/**
 * @file    lsm6dsr.h
 * @brief   LSM6DSR 椹卞姩灞?鈥?瀵勫瓨鍣ㄦ槧灏勩€両/O 鎶借薄銆佹灇涓剧被鍨嬩笌鍑芥暟鍘熷瀷
 *
 * 涓夊眰鏋舵瀯涓殑鏈€搴曞眰锛屾彁渚涘钩鍙版棤鍏崇殑 LSM6DSR 浼犳劅鍣ㄩ┍鍔細
 *   - lsm6dsr_io_t 鎶借薄 I2C/SPI 璇诲啓
 *   - 瀹屾暣鐨勫瘎瀛樺櫒鍦板潃鏄犲皠
 *   - ACC/GYRO/TEMP 鏁版嵁璇诲彇锛坮aw + float锛?
 *   - FIFO 鍏ㄩ儴妯″紡鎿嶄綔
 *   - 鑷銆佸姛鑰楁ā寮忋€丅DU/IF_INC 鎺у埗
 *
 * 涓婂眰锛圔SP 灞傦級閫氳繃 lsm6dsr_io_t 瀹炰緥璋冪敤鏈眰鍑芥暟銆?
 */
#ifndef LSM6DSR_H
#define LSM6DSR_H

#include <stdint.h>
#include <stddef.h>

/** @brief Platform I/O abstraction 鈥?娉ㄥ唽璇诲啓鍥炶皟鍑芥暟 */
typedef struct {
    int8_t (*read)(void *ctx, uint8_t reg, uint8_t *buf, uint16_t len);   /**< 澶氬瓧鑺傝鍥炶皟 */
    int8_t (*write)(void *ctx, uint8_t reg, const uint8_t *buf, uint16_t len); /**< 鍐欏洖璋?*/
    void *ctx; /**< 骞冲彴涓婁笅鏂囨寚閽?(濡?I2C_HandleTypeDef*) */
} lsm6dsr_io_t;

#ifdef __cplusplus
extern "C" {
#endif

/** @name I2C 鍦板潃涓庡櫒浠?ID */
/**@{*/
#define LSM6DSR_I2C_ADDR         (0x6A << 1) /**< LSM6DSR 7-bit I2C 鍦板潃 (宸︾Щ1浣嶅悗 0xD4) */
#define LSM6DSR_WHO_AM_I_VAL     0x6B        /**< WHO_AM_I 鏈熸湜鍊?*/
/**@}*/

/** @name 瀵勫瓨鍣ㄥ湴鍧€鏄犲皠 */
/**@{*/
#define LSM6DSR_REG_FIFO_CTRL1      0x07 /**< FIFO 姘村嵃鍙?鎵归€熺巼閰嶇疆 */
#define LSM6DSR_REG_FIFO_CTRL2      0x08 /**< FIFO 姘村嵃楂樹綅/鍋滄鏉′欢 */
#define LSM6DSR_REG_FIFO_CTRL3      0x09 /**< FIFO 鎵归€熺巼 ACC/GYRO */
#define LSM6DSR_REG_FIFO_CTRL4      0x0A /**< FIFO 妯″紡 */
#define LSM6DSR_REG_WHO_AM_I        0x0F /**< WHO_AM_I (鏈熸湜 0x6B) */
#define LSM6DSR_REG_CTRL1_XL        0x10 /**< ACC ODR + 婊￠噺绋?*/
#define LSM6DSR_REG_CTRL2_G         0x11 /**< GYRO ODR + 婊￠噺绋?*/
#define LSM6DSR_REG_CTRL3_C         0x12 /**< BOOT/BDU/H_LACTIVE/IF_INC/SW_RESET */
#define LSM6DSR_REG_CTRL4_C         0x13 /**< SLEEP_G/DRDY_MASK/I2C_DISABLE/LPF1_SEL */
#define LSM6DSR_REG_CTRL5_C         0x14 /**< ACC/GYRO 鑷銆丷OUNDING */
#define LSM6DSR_REG_CTRL6_C         0x15 /**< ACC 楂樻€ц兘妯″紡/婊ゆ尝绫诲瀷 */
#define LSM6DSR_REG_CTRL7_G         0x16 /**< GYRO 楂樻€ц兘/楂橀€氭护娉?OIS */
#define LSM6DSR_REG_CTRL8_XL        0x17 /**< ACC 婊ゆ尝璁剧疆 */
#define LSM6DSR_REG_CTRL9_XL        0x18 /**< I3C 绂佺敤绛?*/
#define LSM6DSR_REG_STATUS_REG      0x1E /**< DRDY 鐘舵€?*/
#define LSM6DSR_REG_OUT_TEMP_L      0x20 /**< 娓╁害浣庡瓧鑺?*/
#define LSM6DSR_REG_OUT_TEMP_H      0x21 /**< 娓╁害楂樺瓧鑺?*/
#define LSM6DSR_REG_OUTX_L_G        0x22 /**< GYRO X 浣庡瓧鑺?*/
#define LSM6DSR_REG_OUTX_H_G        0x23 /**< GYRO X 楂樺瓧鑺?*/
#define LSM6DSR_REG_OUTY_L_G        0x24 /**< GYRO Y 浣庡瓧鑺?*/
#define LSM6DSR_REG_OUTY_H_G        0x25 /**< GYRO Y 楂樺瓧鑺?*/
#define LSM6DSR_REG_OUTZ_L_G        0x26 /**< GYRO Z 浣庡瓧鑺?*/
#define LSM6DSR_REG_OUTZ_H_G        0x27 /**< GYRO Z 楂樺瓧鑺?*/
#define LSM6DSR_REG_OUTX_L_XL       0x28 /**< ACC X 浣庡瓧鑺?*/
#define LSM6DSR_REG_OUTX_H_XL       0x29 /**< ACC X 楂樺瓧鑺?*/
#define LSM6DSR_REG_OUTY_L_XL       0x2A /**< ACC Y 浣庡瓧鑺?*/
#define LSM6DSR_REG_OUTY_H_XL       0x2B /**< ACC Y 楂樺瓧鑺?*/
#define LSM6DSR_REG_OUTZ_L_XL       0x2C /**< ACC Z 浣庡瓧鑺?*/
#define LSM6DSR_REG_OUTZ_H_XL       0x2D /**< ACC Z 楂樺瓧鑺?*/
#define LSM6DSR_REG_FIFO_STATUS1     0x3A /**< FIFO 姘村嵃/婊?婧㈠嚭鐘舵€?*/
#define LSM6DSR_REG_FIFO_STATUS2     0x3B /**< FIFO 鏅鸿兘姘村嵃/璁℃暟鍣?*/
#define LSM6DSR_REG_FIFO_DATA_OUT_TAG 0x78 /**< FIFO 鏁版嵁鏍囩 */
#define LSM6DSR_REG_FIFO_DATA_OUT_XL  0x79 /**< FIFO 鏁版嵁浣庡瓧鑺?*/
#define LSM6DSR_REG_INT1_CTRL       0x0D /**< INT1 涓柇鎺у埗 */
#define LSM6DSR_REG_INT2_CTRL       0x0E /**< INT2 涓柇鎺у埗 */
/**@}*/

/** @name CTRL3_C 浣嶆帺鐮?*/
/**@{*/
#define CTRL3_C_SW_RESET    (1<<0)  /**< 杞欢澶嶄綅 */
#define CTRL3_C_BDU         (1<<6)  /**< 鍧楁暟鎹洿鏂?(杈撳嚭瀵勫瓨鍣ㄥ湪璇诲彇鍓嶄笉鏇存柊) */
#define CTRL3_C_IF_INC      (1<<2)  /**< 瀵勫瓨鍣ㄥ湴鍧€鑷姩閫掑 (澶氬瓧鑺傝鍙? */
#define CTRL3_C_BOOT        (1<<7)  /**< 閲嶆柊鍔犺浇鏍″噯鍙傛暟 */
/**@}*/

#define CTRL9_XL_I3C_DISABLE  (1<<1) /**< 绂佺敤 I3C 鎺ュ彛 */

/** @name STATUS_REG 鏁版嵁灏辩华鏍囧織 */
/**@{*/
#define STATUS_REG_DRDY_XL  (1<<0) /**< ACC 鏁版嵁灏辩华 */
#define STATUS_REG_DRDY_G   (1<<1) /**< GYRO 鏁版嵁灏辩华 */
#define STATUS_REG_DRDY_TEMP (1<<2) /**< 娓╁害鏁版嵁灏辩华 */
/**@}*/

/** @name FIFO 妯″紡 */
/**@{*/
#define FIFO_CTRL4_FIFO_MODE_MASK   0x07
#define FIFO_MODE_BYPASS            0x00 /**< 鏃佽矾妯″紡 */
#define FIFO_MODE_FIFO              0x01 /**< FIFO 妯″紡 (婊″垯鍋? */
#define FIFO_MODE_CONT_TO_FIFO      0x03 /**< 杩炵画鈫扚IFO 妯″紡 */
#define FIFO_MODE_CONT              0x06 /**< 杩炵画妯″紡 */
/**@}*/

/** @name FIFO 鎵归€熺巼 (BDR) */
/**@{*/
#define LSM6DSR_BDR_NOT_BATCHED  0x00
#define LSM6DSR_BDR_12Hz5        0x01
#define LSM6DSR_BDR_26Hz         0x02
#define LSM6DSR_BDR_52Hz         0x03
#define LSM6DSR_BDR_104Hz        0x04
#define LSM6DSR_BDR_208Hz        0x05
#define LSM6DSR_BDR_416Hz        0x06
#define LSM6DSR_BDR_833Hz        0x07
/**@}*/

/** @name 鐘舵€佺爜 */
/**@{*/
typedef enum {
    LSM6DSR_OK             = 0x00, /**< 鎿嶄綔鎴愬姛 */
    LSM6DSR_ERROR          = 0x01, /**< 閫氫俊閿欒 */
    LSM6DSR_TIMEOUT        = 0x02, /**< 鎿嶄綔瓒呮椂 */
    LSM6DSR_NOT_FOUND      = 0x03, /**< 鍣ㄤ欢 ID 涓嶅尮閰?*/
    LSM6DSR_INVALID_PARAM  = 0x04, /**< 鏃犳晥鍙傛暟 */
    LSM6DSR_NULL_PTR       = 0x05  /**< 绌烘寚閽?*/
} lsm6dsr_status_t;
/**@}*/

/** @brief ACC 杈撳嚭鏁版嵁閫熺巼 */
typedef enum {
    LSM6DSR_ACCEL_ODR_OFF     = 0x00, /**< 鍏抽棴 */
    LSM6DSR_ACCEL_ODR_12_5HZ  = 0x01, /**< 12.5 Hz */
    LSM6DSR_ACCEL_ODR_26HZ    = 0x02, /**< 26 Hz */
    LSM6DSR_ACCEL_ODR_52HZ    = 0x03, /**< 52 Hz */
    LSM6DSR_ACCEL_ODR_104HZ   = 0x04, /**< 104 Hz */
    LSM6DSR_ACCEL_ODR_208HZ   = 0x05, /**< 208 Hz */
    LSM6DSR_ACCEL_ODR_416HZ   = 0x06, /**< 416 Hz */
    LSM6DSR_ACCEL_ODR_833HZ   = 0x07, /**< 833 Hz */
    LSM6DSR_ACCEL_ODR_1_66KHZ = 0x08, /**< 1.66 kHz */
    LSM6DSR_ACCEL_ODR_3_33KHZ = 0x09, /**< 3.33 kHz */
    LSM6DSR_ACCEL_ODR_6_66KHZ = 0x0A  /**< 6.66 kHz */
} lsm6dsr_accel_odr_t;

/** @brief ACC 婊￠噺绋?*/
typedef enum {
    LSM6DSR_ACCEL_FS_2G  = 0x00, /**< 卤2G */
    LSM6DSR_ACCEL_FS_4G  = 0x02, /**< 卤4G */
    LSM6DSR_ACCEL_FS_8G  = 0x03, /**< 卤8G */
    LSM6DSR_ACCEL_FS_16G = 0x01  /**< 卤16G */
} lsm6dsr_accel_fs_t;

/** @brief GYRO 杈撳嚭鏁版嵁閫熺巼 (澶嶇敤 ACC 鏋氫妇) */
typedef lsm6dsr_accel_odr_t lsm6dsr_gyro_odr_t;
#define LSM6DSR_GYRO_ODR_OFF      LSM6DSR_ACCEL_ODR_OFF
#define LSM6DSR_GYRO_ODR_12_5HZ  LSM6DSR_ACCEL_ODR_12_5HZ
#define LSM6DSR_GYRO_ODR_26HZ    LSM6DSR_ACCEL_ODR_26HZ
#define LSM6DSR_GYRO_ODR_52HZ    LSM6DSR_ACCEL_ODR_52HZ
#define LSM6DSR_GYRO_ODR_104HZ   LSM6DSR_ACCEL_ODR_104HZ
#define LSM6DSR_GYRO_ODR_208HZ   LSM6DSR_ACCEL_ODR_208HZ
#define LSM6DSR_GYRO_ODR_416HZ   LSM6DSR_ACCEL_ODR_416HZ
#define LSM6DSR_GYRO_ODR_833HZ   LSM6DSR_ACCEL_ODR_833HZ
#define LSM6DSR_GYRO_ODR_1_66KHZ LSM6DSR_ACCEL_ODR_1_66KHZ
#define LSM6DSR_GYRO_ODR_3_33KHZ LSM6DSR_ACCEL_ODR_3_33KHZ
#define LSM6DSR_GYRO_ODR_6_66KHZ LSM6DSR_ACCEL_ODR_6_66KHZ

/** @brief GYRO 婊￠噺绋?*/
typedef enum {
    LSM6DSR_GYRO_FS_250DPS  = 0,  /**< 卤250 dps */
    LSM6DSR_GYRO_FS_500DPS  = 4,  /**< 卤500 dps */
    LSM6DSR_GYRO_FS_1000DPS = 8,  /**< 卤1000 dps */
    LSM6DSR_GYRO_FS_2000DPS = 12  /**< 卤2000 dps */
} lsm6dsr_gyro_fs_t;

/** @brief 涓夎酱鏁版嵁缁撴瀯 (int16 raw) */
typedef struct {
    int16_t x; /**< X 杞?*/
    int16_t y; /**< Y 杞?*/
    int16_t z; /**< Z 杞?*/
} lsm6dsr_axis_t;

/** @name 鐏垫晱搴﹀父閲?(mg/LSB 鎴?dps/LSB) */
/**@{*/
#define LSM6DSR_ACCEL_SENS_2G   0.061f   /**< 卤2G  鐏垫晱搴? 0.061 mg/LSB */
#define LSM6DSR_ACCEL_SENS_4G   0.122f   /**< 卤4G  鐏垫晱搴? 0.122 mg/LSB */
#define LSM6DSR_ACCEL_SENS_8G   0.244f   /**< 卤8G  鐏垫晱搴? 0.244 mg/LSB */
#define LSM6DSR_ACCEL_SENS_16G  0.488f   /**< 卤16G 鐏垫晱搴? 0.488 mg/LSB */

#define LSM6DSR_GYRO_SENS_250DPS   0.00875f  /**< 卤250  dps 鐏垫晱搴? 0.00875 dps/LSB */
#define LSM6DSR_GYRO_SENS_500DPS  0.01750f  /**< 卤500  dps 鐏垫晱搴? 0.0175 dps/LSB */
#define LSM6DSR_GYRO_SENS_1000DPS 0.03500f  /**< 卤1000 dps 鐏垫晱搴? 0.035 dps/LSB */
#define LSM6DSR_GYRO_SENS_2000DPS 0.07000f  /**< 卤2000 dps 鐏垫晱搴? 0.07 dps/LSB */
/**@}*/

/** @name 娓╁害浼犳劅鍣?*/
/**@{*/
#define LSM6DSR_TEMP_SENSITIVITY   256.0f /**< 娓╁害鐏垫晱搴?(LSB/掳C) */
#define LSM6DSR_TEMP_OFFSET        25.0f /**< 25掳C 鏃惰緭鍑轰负 0 */
/**@}*/

/** @name FIFO 鏍囩瑙ｇ爜 */
/**@{*/
#define FIFO_TAG_SENSOR(tag)     ((tag) & 0x1F)
#define FIFO_TAG_CNT(tag)        (((tag) >> 5) & 0x03)
#define FIFO_TAG_PARITY(tag)     (((tag) >> 7) & 0x01)
/**
 * @brief 鍒ゆ柇 FIFO 鏍囩鏄惁涓?GYRO
 * @note  LSM6DSR FIFO tag 缂栫爜涓?ST 鏂囨。涓嶅悓:
 *        bit4=0 鈫?GYRO, bit4=1 鈫?ACC
 */
#define FIFO_TAG_IS_GYRO(tag)   (!((tag) & 0x10))
#define FIFO_TAG_IS_ACC(tag)    (((tag) & 0x10) != 0)

#define FIFO_TAG_GYRO  0
#define FIFO_TAG_ACC   1
/**@}*/

/** @name 鑷妯″紡 */
/**@{*/
#define LSM6DSR_XL_ST_DISABLE   0 /**< ACC 鑷鍏抽棴 */
#define LSM6DSR_XL_ST_POSITIVE  1 /**< ACC 姝ｅ悜鑷 */
#define LSM6DSR_XL_ST_NEGATIVE  2 /**< ACC 璐熷悜鑷 */

#define LSM6DSR_GY_ST_DISABLE   0 /**< GYRO 鑷鍏抽棴 */
#define LSM6DSR_GY_ST_POSITIVE  1 /**< GYRO 姝ｅ悜鑷 */
#define LSM6DSR_GY_ST_NEGATIVE  3 /**< GYRO 璐熷悜鑷 */
/**@}*/

/** @name 鍔熻€楁ā寮忔帶鍒?*/
/**@{*/
#define CTRL6_C_XL_HM_MODE  (1<<4) /**< ACC 楂樻€ц兘妯″紡 */
#define CTRL7_G_GY_HM_MODE  (1<<7) /**< GYRO 楂樻€ц兘妯″紡 */
/**@}*/

/**
 * @brief FIFO 鏉＄洰浼犳劅鍣ㄧ被鍨?
 */
typedef enum {
    LSM6DSR_FIFO_SENSOR_GYRO = 1, /**< GYRO 鏁版嵁 */
    LSM6DSR_FIFO_SENSOR_ACC  = 2  /**< ACC 鏁版嵁 */
} lsm6dsr_fifo_sensor_t;

/* ===================================================================
 * 鍑芥暟鍘熷瀷
 * =================================================================== */

/** @name 瀵勫瓨鍣?I/O */
/**@{*/
lsm6dsr_status_t lsm6dsr_write_reg(lsm6dsr_io_t *io, uint8_t reg, uint8_t val);
lsm6dsr_status_t lsm6dsr_read_reg(lsm6dsr_io_t *io, uint8_t reg, uint8_t *val);
lsm6dsr_status_t lsm6dsr_read_multi(lsm6dsr_io_t *io, uint8_t reg, uint8_t *data, uint16_t len);
lsm6dsr_status_t lsm6dsr_read_multi_bytewise(lsm6dsr_io_t *io, uint8_t reg, uint8_t *data, uint16_t len);
/**@}*/

/** @name 鍣ㄤ欢鎺у埗 */
/**@{*/
lsm6dsr_status_t lsm6dsr_verify_id(lsm6dsr_io_t *io);
lsm6dsr_status_t lsm6dsr_reset(lsm6dsr_io_t *io);
lsm6dsr_status_t lsm6dsr_boot(lsm6dsr_io_t *io);
lsm6dsr_status_t lsm6dsr_i3c_disable(lsm6dsr_io_t *io);
/**@}*/

/** @name ACC 鏁版嵁 */
/**@{*/
lsm6dsr_status_t lsm6dsr_accel_config(lsm6dsr_io_t *io, lsm6dsr_accel_odr_t odr, lsm6dsr_accel_fs_t fs);
lsm6dsr_status_t lsm6dsr_read_accel_raw(lsm6dsr_io_t *io, lsm6dsr_axis_t *accel);
lsm6dsr_status_t lsm6dsr_read_accel_float(lsm6dsr_io_t *io, float *ax, float *ay, float *az, lsm6dsr_accel_fs_t fs);
/**@}*/

/** @name GYRO 鏁版嵁 */
/**@{*/
lsm6dsr_status_t lsm6dsr_gyro_config(lsm6dsr_io_t *io, lsm6dsr_gyro_odr_t odr, lsm6dsr_gyro_fs_t fs);
lsm6dsr_status_t lsm6dsr_read_gyro_raw(lsm6dsr_io_t *io, lsm6dsr_axis_t *gyro);
lsm6dsr_status_t lsm6dsr_read_gyro_float(lsm6dsr_io_t *io, float *wx, float *wy, float *wz, lsm6dsr_gyro_fs_t fs);
/**@}*/

/** @name 娓╁害 */
/**@{*/
lsm6dsr_status_t lsm6dsr_read_temp(lsm6dsr_io_t *io, float *temp_celsius);
/**@}*/

/** @name FIFO 鎿嶄綔 */
/**@{*/
lsm6dsr_status_t lsm6dsr_fifo_init(lsm6dsr_io_t *io, uint16_t threshold, uint8_t bdr_xl, uint8_t bdr_gy);
lsm6dsr_status_t lsm6dsr_fifo_set_mode(lsm6dsr_io_t *io, uint8_t mode);
lsm6dsr_status_t lsm6dsr_fifo_read_tag_data(lsm6dsr_io_t *io, uint8_t *tag, uint8_t *data);
uint16_t lsm6dsr_fifo_get_level(lsm6dsr_io_t *io);
uint8_t lsm6dsr_fifo_wtm_flag(lsm6dsr_io_t *io);
uint8_t lsm6dsr_fifo_ovr_flag(lsm6dsr_io_t *io);
uint8_t lsm6dsr_fifo_full_flag(lsm6dsr_io_t *io);
lsm6dsr_status_t lsm6dsr_fifo_flush(lsm6dsr_io_t *io);
lsm6dsr_status_t lsm6dsr_fifo_set_wtm(lsm6dsr_io_t *io, uint16_t threshold);
lsm6dsr_status_t lsm6dsr_read_fifo_entry(lsm6dsr_io_t *io,
                                          lsm6dsr_fifo_sensor_t *sensor,
                                          lsm6dsr_axis_t *data);
/**@}*/

/** @name 鏁版嵁灏辩华 (DRDY) */
/**@{*/
lsm6dsr_status_t lsm6dsr_get_drdy(lsm6dsr_io_t *io, uint8_t *accel_drdy, uint8_t *gyro_drdy);
/**@}*/

/** @name BDU / IF_INC 鎺у埗 */
/**@{*/
lsm6dsr_status_t lsm6dsr_set_bdu(lsm6dsr_io_t *io, uint8_t enable);
lsm6dsr_status_t lsm6dsr_set_if_inc(lsm6dsr_io_t *io, uint8_t enable);
/**@}*/

/** @name 鑷 */
/**@{*/
lsm6dsr_status_t lsm6dsr_xl_self_test(lsm6dsr_io_t *io, uint8_t mode);
lsm6dsr_status_t lsm6dsr_gy_self_test(lsm6dsr_io_t *io, uint8_t mode);
/**@}*/

/** @name 鍔熻€楁ā寮?*/
/**@{*/
lsm6dsr_status_t lsm6dsr_xl_set_hm_mode(lsm6dsr_io_t *io, uint8_t enable);
lsm6dsr_status_t lsm6dsr_gy_set_hm_mode(lsm6dsr_io_t *io, uint8_t enable);
/**@}*/

#ifdef __cplusplus
}
#endif

#endif

