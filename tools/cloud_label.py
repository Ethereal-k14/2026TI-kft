"""
tools/cloud_label.py — 云端视觉 API 辅助打标工具
支持三大 Provider（均无需本地 GPU）：
  - stepfun  : 阶跃星辰 step-3.7-flash（全模态，无代理，国内首选）
  - qwen     : 阿里云百炼 Qwen3-VL（无代理，性价比极高）
  - gemini   : Google Gemini 2.5 Flash（海外 / Google AI Studio 免费额度）

用法:
  # 阶跃星辰（用户自提供 API Key）
  python tools/cloud_label.py --images datasets/raw --classes person car \
      --provider stepfun --api-key YOUR_STEPFUN_KEY

  # 阿里云百炼（无代理）
  python tools/cloud_label.py --images datasets/raw --classes person car \
      --provider qwen --api-key YOUR_DASHSCOPE_KEY

  # Google Gemini（海外免费）
  python tools/cloud_label.py --images datasets/raw --classes person car \
      --provider gemini --api-key YOUR_GEMINI_KEY
"""

import argparse
import base64
import json
import os
import sys
import time
from io import BytesIO
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    sys.exit("[ERROR] 请先安装 Pillow: pip install Pillow")

IMAGE_EXTS = {".jpg", ".jpeg", ".png", ".bmp", ".webp"}
MAX_SIDE = 1024          # 压缩长边，节省 token（StepFun 上限 2048，Gemini/Qwen 建议 1024）
CONFIDENCE_THRESHOLD = 0.72
MAX_RETRIES = 3


# ---------------------------------------------------------------------------
# Prompt 工厂
# ---------------------------------------------------------------------------

def build_system_prompt(classes: list[str]) -> str:
    class_lines = "\n".join(f"  {i}: {c}" for i, c in enumerate(classes))
    return f"""你是专业的计算机视觉数据标注专家。找出图像中所有属于以下类别的物体并输出边界框。

【类别列表】
{class_lines}

【规则】
1. 边界框坐标归一化到 0.000~1.000（相对图像宽/高），格式 [xmin, ymin, xmax, ymax]。
2. 每个目标输出 confidence（0.0~1.0）。
3. 图中无目标则 objects 为空列表。
4. 只输出合法 JSON，无任何 Markdown 包裹。

【输出格式】
{{"objects": [{{"class_id": 1, "class_name": "car", "bbox": [0.25, 0.40, 0.50, 0.60], "confidence": 0.95}}]}}"""


def build_user_text(classes: list[str]) -> str:
    return f"请检测图中所有目标（类别：{', '.join(classes)}），返回 JSON。"


# ---------------------------------------------------------------------------
# 图片预处理
# ---------------------------------------------------------------------------

def encode_image(image_path: Path) -> tuple[str, int, int]:
    """返回 (base64_str, original_width, original_height)"""
    with Image.open(image_path) as img:
        if img.mode not in ("RGB", "L"):
            img = img.convert("RGB")
        orig_w, orig_h = img.size
        img.thumbnail((MAX_SIDE, MAX_SIDE), Image.Resampling.LANCZOS)
        buf = BytesIO()
        img.save(buf, format="JPEG", quality=85)
        return base64.b64encode(buf.getvalue()).decode(), orig_w, orig_h


# ---------------------------------------------------------------------------
# API 调用：StepFun step-3.7-flash（OpenAI 兼容接口）
# ---------------------------------------------------------------------------

def call_stepfun(api_key: str, b64: str, system_prompt: str, user_text: str, model_name: str = "step-3.7-flash") -> dict | None:
    """调用阶跃星辰 step-3.7-flash 全模态模型。
    Base URL: https://api.stepfun.com/v1  （注意是 .com 不是 .ai）
    完全兼容 OpenAI SDK，建议使用 0~1000 整数刻度归一化，精度与稳定性最高。
    """
    try:
        from openai import OpenAI
    except ImportError:
        sys.exit('[ERROR] 请先安装 openai: pip install openai')

    client = OpenAI(api_key=api_key, base_url='https://api.stepfun.com/v1')

    # StepFun 专用的 0-1000 刻度强化 System Prompt
    stepfun_system_prompt = system_prompt + "\n注：边界框坐标可以使用 0~1000 的整数归一化刻度 [xmin, ymin, xmax, ymax]。"

    for attempt in range(MAX_RETRIES):
        try:
            extra_params = {
                "model": model_name,
                "response_format": {'type': 'json_object'},
                "temperature": 0.05,
                "messages": [
                    {'role': 'system', 'content': stepfun_system_prompt},
                    {'role': 'user', 'content': [
                        {'type': 'text', 'text': user_text},
                        {'type': 'image_url', 'image_url': {
                            'url': f'data:image/jpeg;base64,{b64}'
                        }},
                    ]},
                ],
            }
            # 开启 StepFun Thinking 推理增强模式（若模型支持）
            try:
                extra_params["extra_body"] = {"enable_thinking": True}
                resp = client.chat.completions.create(**extra_params)
            except Exception:
                # 兼容不带 extra_body 的调用
                extra_params.pop("extra_body", None)
                resp = client.chat.completions.create(**extra_params)

            raw = resp.choices[0].message.content.strip()
            raw = raw.removeprefix('```json').removeprefix('```').removesuffix('```').strip()
            data = json.loads(raw)

            # 坐标自适应检查：如果坐标属于 0~1000 整数刻度，归一化到 0~1
            for obj in data.get("objects", []):
                b = obj.get("bbox", [])
                if len(b) == 4 and max(b) > 1.5:
                    obj["bbox"] = [b[0] / 1000.0, b[1] / 1000.0, b[2] / 1000.0, b[3] / 1000.0]

            return data
        except Exception as e:
            print(f'  [StepFun] 第{attempt+1}次失败: {e}')
            time.sleep(2 ** attempt)
    return None


# ---------------------------------------------------------------------------
# API 调用：Qwen3-VL（阿里云百炼）
# ---------------------------------------------------------------------------

def call_qwen(api_key: str, b64: str, system_prompt: str, user_text: str) -> dict | None:
    try:
        from dashscope import MultiModalConversation
    except ImportError:
        sys.exit("[ERROR] 请安装 dashscope: pip install dashscope")

    import dashscope
    dashscope.api_key = api_key

    for attempt in range(MAX_RETRIES):
        try:
            resp = MultiModalConversation.call(
                model="qwen-vl-max",   # 或 qwen-vl-plus（更便宜）
                messages=[
                    {"role": "system", "content": [{"text": system_prompt}]},
                    {"role": "user", "content": [
                        {"image": f"data:image/jpeg;base64,{b64}"},
                        {"text": user_text},
                    ]},
                ],
            )
            if resp.status_code == 200:
                raw = resp.output.choices[0].message.content[0]["text"]
                # 去掉可能的 markdown 代码块
                raw = raw.strip().removeprefix("```json").removeprefix("```").removesuffix("```").strip()
                return json.loads(raw)
        except Exception as e:
            print(f"  [Qwen] 第{attempt+1}次失败: {e}")
            time.sleep(2 ** attempt)
    return None


# ---------------------------------------------------------------------------
# API 调用：Gemini 2.5 Flash（Google）
# ---------------------------------------------------------------------------

def call_gemini(api_key: str, b64: str, system_prompt: str, user_text: str) -> dict | None:
    try:
        from google import genai
        from google.genai import types
    except ImportError:
        sys.exit("[ERROR] 请安装 google-genai: pip install google-genai")

    client = genai.Client(api_key=api_key)

    # 强制 JSON Schema 输出
    schema = {
        "type": "OBJECT",
        "properties": {
            "objects": {
                "type": "ARRAY",
                "items": {
                    "type": "OBJECT",
                    "properties": {
                        "class_id":   {"type": "INTEGER"},
                        "class_name": {"type": "STRING"},
                        "bbox":       {"type": "ARRAY", "items": {"type": "NUMBER"}},
                        "confidence": {"type": "NUMBER"},
                    },
                    "required": ["class_id", "class_name", "bbox", "confidence"],
                },
            }
        },
        "required": ["objects"],
    }

    image_bytes = base64.b64decode(b64)

    for attempt in range(MAX_RETRIES):
        try:
            resp = client.models.generate_content(
                model="gemini-2.5-flash",
                contents=[
                    types.Part.from_bytes(data=image_bytes, mime_type="image/jpeg"),
                    user_text,
                ],
                config=types.GenerateContentConfig(
                    system_instruction=system_prompt,
                    response_mime_type="application/json",
                    response_schema=schema,
                    temperature=0.05,
                ),
            )
            data = json.loads(resp.text)

            # Gemini 原生坐标为 [ymin, xmin, ymax, xmax]（0~1000 刻度）
            # 如果模型遵循了 prompt 输出 [xmin,ymin,xmax,ymax] 0~1，则直接用
            # 此处做自适应转换
            for obj in data.get("objects", []):
                b = obj["bbox"]
                if max(b) > 1.5:          # 判断是 0-1000 刻度
                    # [ymin, xmin, ymax, xmax] → [xmin, ymin, xmax, ymax] 0-1
                    ymin, xmin, ymax, xmax = b[0]/1000, b[1]/1000, b[2]/1000, b[3]/1000
                    obj["bbox"] = [xmin, ymin, xmax, ymax]
            return data
        except Exception as e:
            print(f"  [Gemini] 第{attempt+1}次失败: {e}")
            time.sleep(2 ** attempt)
    return None


# ---------------------------------------------------------------------------
# 坐标转换：[xmin,ymin,xmax,ymax] → YOLO [x_c,y_c,w,h]（归一化 0~1）
# ---------------------------------------------------------------------------

def to_yolo(xmin: float, ymin: float, xmax: float, ymax: float) -> tuple:
    xmin, ymin = max(0.0, xmin), max(0.0, ymin)
    xmax, ymax = min(1.0, xmax), min(1.0, ymax)
    w = xmax - xmin
    h = ymax - ymin
    if w <= 0 or h <= 0:
        return None
    return xmin + w / 2, ymin + h / 2, w, h


# ---------------------------------------------------------------------------
# 主流程
# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(
        description='云端视觉 API 辅助打标工具（StepFun step-3.7-flash / Qwen3-VL / Gemini 2.5 Flash）',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            '示例:\n'
            '  stepfun: python tools/cloud_label.py --images datasets/raw --classes person car --provider stepfun --api-key KEY\n'
            '  qwen:    python tools/cloud_label.py --images datasets/raw --classes person car --provider qwen    --api-key KEY\n'
            '  gemini:  python tools/cloud_label.py --images datasets/raw --classes person car --provider gemini  --api-key KEY\n'
        ),
    )
    ap.add_argument('--images',   required=True,  help='原始图片目录')
    ap.add_argument('--output',   default='datasets/auto_labeled', help='输出目录（默认 datasets/auto_labeled）')
    ap.add_argument('--classes',  required=True,  nargs='+', help='类别列表，顺序即 class_id（0开始）')
    ap.add_argument('--provider', choices=['stepfun', 'qwen', 'gemini'], default='stepfun',
                    help='API 提供商：stepfun（阶跃星辰）/ qwen（阿里云）/ gemini（Google）')
    ap.add_argument('--api-key',  required=True,  help='API Key')
    ap.add_argument('--confidence', type=float, default=CONFIDENCE_THRESHOLD,
                    help=f'置信度阈值（低于此值进入 review_labels，默认 {CONFIDENCE_THRESHOLD}）')
    ap.add_argument('--model',    default=None,
                    help='覆盖默认模型名（StepFun: step-3.7-flash；Qwen: qwen-vl-max；Gemini: gemini-2.5-flash）')
    a = ap.parse_args()

    img_dir    = Path(a.images)
    out_dir    = Path(a.output)
    label_dir  = out_dir / "labels"
    review_dir = out_dir / "review_labels"
    img_out    = out_dir / "images"

    for d in (label_dir, review_dir, img_out):
        d.mkdir(parents=True, exist_ok=True)

    class_names = a.classes
    system_prompt = build_system_prompt(class_names)
    user_text     = build_user_text(class_names)

    images = sorted(p for p in img_dir.iterdir() if p.suffix.lower() in IMAGE_EXTS)
    if not images:
        sys.exit(f"[ERROR] {img_dir} 下未找到图片文件")

    print(f"[INFO] 共 {len(images)} 张图片，使用 {a.provider.upper()} API，置信度阈值 {a.confidence}")
    print(f"[INFO] 类别: {class_names}")
    print()

    ok = skip = fail = 0

    for idx, img_path in enumerate(images, 1):
        stem = img_path.stem
        label_ok  = label_dir  / f"{stem}.txt"
        label_rev = review_dir / f"{stem}.txt"

        # 断点续标
        if label_ok.exists() or label_rev.exists():
            skip += 1
            continue

        print(f"[{idx:04d}/{len(images)}] {img_path.name} ... ", end="", flush=True)

        try:
            b64, orig_w, orig_h = encode_image(img_path)
        except Exception as e:
            print(f"图片读取失败: {e}")
            fail += 1
            continue

        if a.provider == 'stepfun':
            result = call_stepfun(a.api_key, b64, system_prompt, user_text, model_name=a.model or "step-3.7-flash")
        elif a.provider == 'qwen':
            result = call_qwen(a.api_key, b64, system_prompt, user_text)
        else:
            result = call_gemini(a.api_key, b64, system_prompt, user_text)

        if result is None:
            print('API 失败，跳过')
            fail += 1
            continue

        objects = result.get("objects", [])
        yolo_lines = []
        needs_review = False

        for obj in objects:
            cid  = obj.get("class_id")
            bbox = obj.get("bbox")
            conf = float(obj.get("confidence", 1.0))

            if cid is None or not isinstance(bbox, list) or len(bbox) != 4:
                continue
            if not (0 <= cid < len(class_names)):
                continue

            coords = to_yolo(*bbox)
            if coords is None:
                continue

            x_c, y_c, w, h = coords
            yolo_lines.append(f"{cid} {x_c:.6f} {y_c:.6f} {w:.6f} {h:.6f}")

            if conf < a.confidence:
                needs_review = True

        save_path = label_rev if needs_review else label_ok
        save_path.write_text("\n".join(yolo_lines), encoding="utf-8")

        tag = "⚠️ review" if needs_review else "✅"
        print(f"{tag}  {len(yolo_lines)} 个目标")
        ok += 1

        # 礼貌性限流
        # StepFun V0（免费/未充值）: 10 RPM → 间隔 6s；充值后可去掉
        # Gemini AI Studio 免费: 15 RPM → 间隔 4s
        if a.provider == 'stepfun':
            time.sleep(6)
        elif a.provider == 'gemini':
            time.sleep(4)

    print()
    print("=" * 50)
    print(f" 完成！处理 {ok} 张  跳过(已标) {skip} 张  失败 {fail} 张")
    print(f" 高置信度标注 → {label_dir}")
    print(f" 低置信度复检 → {review_dir}")
    print("=" * 50)
    print()
    print("下一步：")
    print(f"  1. 用 X-AnyLabeling 打开 {review_dir} 目录进行人工复检")
    print(f"  2. python tools/check_dataset.py --data {out_dir}")
    print(f"  3. 修改 configs/your_dataset.yaml 后开始训练")


if __name__ == "__main__":
    main()
