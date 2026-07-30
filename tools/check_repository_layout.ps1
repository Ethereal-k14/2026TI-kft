$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

$required = @(
    'README.md',
    'docs\README.md',
    'docs\H题系统集成与验收.md',
    'docs\目录与交付规范.md',
    'H题_车载平衡滚球运动控制系统.pdf',
    'k230\runtime\ball_balance_link.py',
    'k230\tests\test_ball_balance_link.py',
    'mspm0\UPSTREAM.md',
    'mspm0\MSPM0G3507_Project\MSPM0G3507_FreeRTOS\Application\app_main.c',
    'mspm0\MSPM0G3507_Project\MSPM0G3507_FreeRTOS\Config\project_config.h',
    'stm32f4\balance_control\balance_control.ioc',
    'stm32f4\balance_control\User\user_runtime.c',
    'stm32f4\balance_control\User\Config\user_config.h'
)
foreach ($relative in $required) {
    if (-not (Test-Path -LiteralPath (Join-Path $root $relative))) {
        throw "Required repository path is missing: $relative"
    }
}

$authoritativeDocs = @(
    'README.md',
    'docs\README.md',
    'docs\H题系统集成与验收.md',
    'docs\目录与交付规范.md',
    'k230\QUICKSTART.md',
    'k230\README.md',
    'k230\docs\self_training.md',
    'stm32f4\balance_control\User\README.md',
    'stm32f4\balance_control\Docs\firmware_specification.md'
)
foreach ($relative in $authoritativeDocs) {
    $content = Get-Content -Raw -LiteralPath (Join-Path $root $relative)
    if ($content -match 'file:///' -or $content -match '(?i)[A-Z]:/Destop/') {
        throw "Non-portable absolute path found in: $relative"
    }
}

$portableConfigs = @(
    'k230\datasets\real_steel_ball\data.yaml',
    'k230\datasets\test_pipeline_yolo11\data.yaml'
)
foreach ($relative in $portableConfigs) {
    $content = Get-Content -Raw -LiteralPath (Join-Path $root $relative)
    if ($content -match '(?i)path:\s*[A-Z]:[/\\]') {
        throw "Absolute dataset path found in: $relative"
    }
}

$tracked = @(git -C $root ls-files)
if ($LASTEXITCODE -ne 0) { throw 'Unable to read the Git file list.' }
$forbiddenActive = @(
    '^k230/(\.venv|__pycache__|\.pytest_cache|runs)/',
    '^stm32f4/balance_control/(Objects|Listings|__pycache__)/',
    '^mspm0/MSPM0G3507_Project/MSPM0G3507_FreeRTOS/keil/(Objects|Listings)/'
)
foreach ($path in $tracked) {
    foreach ($pattern in $forbiddenActive) {
        if ($path -match $pattern) {
            throw "Generated file is tracked inside an active project: $path"
        }
    }
}

$checks = @(
    @('k230\runtime\ball_balance_link.py', 'MSG_VISION_POSE = 0x20'),
    @('stm32f4\balance_control\User\Protocol\Inc\app_protocol.h', 'MSG_CHASSIS_IMU       = 0x12U'),
    @('stm32f4\balance_control\User\Protocol\Inc\app_protocol.h', 'MSG_VISION_POSE       = 0x20U'),
    @('mspm0\MSPM0G3507_Project\MSPM0G3507_FreeRTOS\Application\app_balance_link.c', '#define LINK_MSG_IMU (0x12U)'),
    @('mspm0\MSPM0G3507_Project\MSPM0G3507_FreeRTOS\Config\project_config.h', '#define PRJ_BALANCE_LINK_WATCHDOG_MS   (250U)'),
    @('mspm0\MSPM0G3507_Project\MSPM0G3507_FreeRTOS\main.c', 'bsp_motor_power_disable();'),
    @('stm32f4\balance_control\User\Config\user_config.h', '#define USER_CHASSIS_START_ACK_TIMEOUT_MS (300U)'),
    @('stm32f4\balance_control\User\App\Src\app_safety.c', 'App_Safety_ControlledStop(SAFETY_WARN_SENSOR_FEEDBACK);'),
    @('mspm0\MSPM0G3507_Project\MSPM0G3507_FreeRTOS\Config\empty.syscfg', 'UART2.targetBaudRate                   = 115200;'),
    @('stm32f4\balance_control\balance_control.ioc', 'Mcu.Package=LQFP100')
)
foreach ($check in $checks) {
    $content = Get-Content -Raw -LiteralPath (Join-Path $root $check[0])
    if (-not $content.Contains($check[1])) {
        throw "Interface contract drift in $($check[0]): expected '$($check[1])'"
    }
}

$nestedGit = @(Get-ChildItem -LiteralPath $root -Directory -Recurse -Force -Filter '.git' |
    Where-Object { $_.FullName -ne (Join-Path $root '.git') })
if ($nestedGit.Count -ne 0) {
    throw "Nested Git metadata is not allowed: $($nestedGit[0].FullName)"
}

Write-Output "Repository layout validation passed: required=$($required.Count), tracked=$($tracked.Count), contracts=$($checks.Count)"
