/**
 * @file    lsm6dsr.c
 * @brief   LSM6DSR 椹卞姩灞傚疄鐜?
 *
 * 鎻愪緵 lsm6dsr.h 涓０鏄庣殑鎵€鏈夊嚱鏁扮殑鍏蜂綋瀹炵幇銆?
 * 鎵€鏈?I/O 閫氳繃 lsm6dsr_io_t 鍥炶皟鍑芥暟瀹屾垚锛屽钩鍙版棤鍏炽€?
 *
 * 鍔熻兘瑕嗙洊锛?
 *   - 瀵勫瓨鍣ㄥ崟瀛楄妭/澶氬瓧鑺傝鍐?
 *   - ACC/GYRO/TEMP 鏁版嵁璇诲彇 (raw + float)
 *   - FIFO 鍒濆鍖栥€佹ā寮忚缃€佺姸鎬佹煡璇€佹暟鎹鍙?
 *   - 鑷銆佸姛鑰楁ā寮忋€丅DU/IF_INC 鎺у埗
 */
#include "lsm6dsr.h"

/**
 * @brief  鍐欏崟瀛楄妭瀵勫瓨鍣?
 * @param  io  I/O 鎶借薄灞傛寚閽?
 * @param  reg 瀵勫瓨鍣ㄥ湴鍧€
 * @param  val 鍐欏叆鍊?
 * @return LSM6DSR_OK 鎴愬姛 / LSM6DSR_NULL_PTR 绌烘寚閽?/ LSM6DSR_ERROR 閫氫俊澶辫触
 */
lsm6dsr_status_t lsm6dsr_write_reg(lsm6dsr_io_t *io, uint8_t reg, uint8_t val)
{
    if (io == NULL || io->write == NULL) return LSM6DSR_NULL_PTR;
    return (io->write(io->ctx, reg, &val, 1) == 0) ? LSM6DSR_OK : LSM6DSR_ERROR;
}

/**
 * @brief  璇诲崟瀛楄妭瀵勫瓨鍣?
 * @param  io  I/O 鎶借薄灞傛寚閽?
 * @param  reg 瀵勫瓨鍣ㄥ湴鍧€
 * @param  val 杈撳嚭缂撳啿鍖?
 * @return LSM6DSR_OK 鎴愬姛 / LSM6DSR_NULL_PTR 绌烘寚閽?/ LSM6DSR_ERROR 閫氫俊澶辫触
 */
lsm6dsr_status_t lsm6dsr_read_reg(lsm6dsr_io_t *io, uint8_t reg, uint8_t *val)
{
    if (io == NULL || io->read == NULL || val == NULL) return LSM6DSR_NULL_PTR;
    return (io->read(io->ctx, reg, val, 1) == 0) ? LSM6DSR_OK : LSM6DSR_ERROR;
}

/**
 * @brief  澶氬瓧鑺傝 (闇€ IF_INC 鑷姩閫掑鍦板潃)
 * @param  io   I/O 鎶借薄灞傛寚閽?
 * @param  reg  璧峰瀵勫瓨鍣ㄥ湴鍧€
 * @param  data 杈撳嚭缂撳啿鍖?
 * @param  len  璇诲彇瀛楄妭鏁?
 * @return LSM6DSR_OK 鎴愬姛 / LSM6DSR_NULL_PTR 绌烘寚閽?/ LSM6DSR_ERROR 閫氫俊澶辫触
 */
lsm6dsr_status_t lsm6dsr_read_multi(lsm6dsr_io_t *io, uint8_t reg, uint8_t *data, uint16_t len)
{
    if (io == NULL || io->read == NULL || data == NULL) return LSM6DSR_NULL_PTR;
    return (io->read(io->ctx, reg, data, len) == 0) ? LSM6DSR_OK : LSM6DSR_ERROR;
}

/**
 * @brief  閫愬瓧鑺傚璇?(鏃犻渶 IF_INC锛屼絾鏁堢巼浣?
 * @details 鏌愪簺骞冲彴涓嶆敮鎸佸瀛楄妭杩炵画璇诲彇鏃朵娇鐢ㄦ鍑芥暟銆?
 *          姣忓瓧鑺傚湴鍧€閫掑 reg+i锛岄€傜敤浜庡瘎瀛樺櫒涓嶈嚜澧炵殑鎯呭喌銆?
 * @param  io   I/O 鎶借薄灞傛寚閽?
 * @param  reg  璧峰瀵勫瓨鍣ㄥ湴鍧€
 * @param  data 杈撳嚭缂撳啿鍖?
 * @param  len  璇诲彇瀛楄妭鏁?
 * @return LSM6DSR_OK 鎴愬姛 / 鍏朵粬 澶辫触
 */
lsm6dsr_status_t lsm6dsr_read_multi_bytewise(lsm6dsr_io_t *io, uint8_t reg, uint8_t *data, uint16_t len)
{
    if (io == NULL || io->read == NULL || data == NULL) return LSM6DSR_NULL_PTR;
    for (uint16_t i = 0; i < len; i++) {
        if (io->read(io->ctx, reg + i, &data[i], 1) != 0)
            return LSM6DSR_ERROR;
    }
    return LSM6DSR_OK;
}

/**
 * @brief  楠岃瘉 WHO_AM_I
 * @param  io I/O 鎶借薄灞傛寚閽?
 * @return LSM6DSR_OK 鍖归厤 / LSM6DSR_NOT_FOUND 涓嶅尮閰?/ 鍏朵粬 閫氫俊澶辫触
 */
lsm6dsr_status_t lsm6dsr_verify_id(lsm6dsr_io_t *io)
{
    uint8_t id = 0;
    lsm6dsr_status_t st = lsm6dsr_read_reg(io, LSM6DSR_REG_WHO_AM_I, &id);
    if (st != LSM6DSR_OK) return st;
    return (id == LSM6DSR_WHO_AM_I_VAL) ? LSM6DSR_OK : LSM6DSR_NOT_FOUND;
}

/**
 * @brief  杞欢澶嶄綅 (SW_RESET)
 * @details 鍐?CTRL3_C bit0=1锛岃疆璇㈢瓑寰呯‖浠惰嚜鍔ㄦ竻闆?
 * @param  io I/O 鎶借薄灞傛寚閽?
 * @return LSM6DSR_OK 鎴愬姛 / LSM6DSR_TIMEOUT 瓒呮椂 / 鍏朵粬 閫氫俊澶辫触
 */
lsm6dsr_status_t lsm6dsr_reset(lsm6dsr_io_t *io)
{
    lsm6dsr_status_t st = lsm6dsr_write_reg(io, LSM6DSR_REG_CTRL3_C, CTRL3_C_SW_RESET);
    if (st != LSM6DSR_OK) return st;
    uint8_t rst = 1;
    uint32_t timeout = 1000;
    while (rst != 0 && timeout-- > 0) {
        lsm6dsr_read_reg(io, LSM6DSR_REG_CTRL3_C, &rst);
        rst &= CTRL3_C_SW_RESET;
    }
    return (rst == 0) ? LSM6DSR_OK : LSM6DSR_TIMEOUT;
}

/**
 * @brief  閲嶆柊鍔犺浇 BOOT (鏍″噯鍙傛暟)
 * @param  io I/O 鎶借薄灞傛寚閽?
 * @return LSM6DSR_OK 鎴愬姛 / LSM6DSR_TIMEOUT 瓒呮椂 / 鍏朵粬 閫氫俊澶辫触
 */
lsm6dsr_status_t lsm6dsr_boot(lsm6dsr_io_t *io)
{
    uint8_t val;
    lsm6dsr_status_t st = lsm6dsr_read_reg(io, LSM6DSR_REG_CTRL3_C, &val);
    if (st != LSM6DSR_OK) return st;
    st = lsm6dsr_write_reg(io, LSM6DSR_REG_CTRL3_C, val | CTRL3_C_BOOT);
    if (st != LSM6DSR_OK) return st;
    uint8_t boot = 1;
    uint32_t timeout = 500;
    while (boot != 0 && timeout-- > 0) {
        lsm6dsr_read_reg(io, LSM6DSR_REG_CTRL3_C, &boot);
        boot &= CTRL3_C_BOOT;
    }
    return (boot == 0) ? LSM6DSR_OK : LSM6DSR_TIMEOUT;
}

/**
 * @brief  绂佺敤 I3C 鎺ュ彛 (寮哄埗浣跨敤 I2C/SPI)
 * @param  io I/O 鎶借薄灞傛寚閽?
 * @return lsm6dsr_status_t
 */
lsm6dsr_status_t lsm6dsr_i3c_disable(lsm6dsr_io_t *io)
{
    return lsm6dsr_write_reg(io, LSM6DSR_REG_CTRL9_XL, 0xE2);
}

/**
 * @brief  鑾峰彇 ACC 鐏垫晱搴︾郴鏁?(mg/LSB)
 * @param  fs 婊￠噺绋嬮€夋嫨
 * @return 鐏垫晱搴﹀€?(mg/LSB)
 */
static float accel_sensitivity(lsm6dsr_accel_fs_t fs)
{
    switch (fs) {
        case LSM6DSR_ACCEL_FS_2G:  return LSM6DSR_ACCEL_SENS_2G;
        case LSM6DSR_ACCEL_FS_4G:  return LSM6DSR_ACCEL_SENS_4G;
        case LSM6DSR_ACCEL_FS_8G:  return LSM6DSR_ACCEL_SENS_8G;
        case LSM6DSR_ACCEL_FS_16G: return LSM6DSR_ACCEL_SENS_16G;
        default:                   return LSM6DSR_ACCEL_SENS_2G;
    }
}

/**
 * @brief  鑾峰彇 GYRO 鐏垫晱搴︾郴鏁?(dps/LSB)
 * @param  fs 婊￠噺绋嬮€夋嫨
 * @return 鐏垫晱搴﹀€?(dps/LSB)
 */
static float gyro_sensitivity(lsm6dsr_gyro_fs_t fs)
{
    switch (fs) {
        case LSM6DSR_GYRO_FS_250DPS:  return LSM6DSR_GYRO_SENS_250DPS;
        case LSM6DSR_GYRO_FS_500DPS:  return LSM6DSR_GYRO_SENS_500DPS;
        case LSM6DSR_GYRO_FS_1000DPS: return LSM6DSR_GYRO_SENS_1000DPS;
        case LSM6DSR_GYRO_FS_2000DPS: return LSM6DSR_GYRO_SENS_2000DPS;
        default:                      return LSM6DSR_GYRO_SENS_250DPS;
    }
}

/**
 * @brief  閰嶇疆 ACC (ODR + 婊￠噺绋?
 * @param  io  I/O 鎶借薄灞傛寚閽?
 * @param  odr 杈撳嚭鏁版嵁閫熺巼
 * @param  fs  婊￠噺绋?
 * @return lsm6dsr_status_t
 * @note  淇濈暀 CTRL1_XL 鐨勪綆 2 浣?(婊ゆ尝閰嶇疆涓嶅彉)
 */
lsm6dsr_status_t lsm6dsr_accel_config(lsm6dsr_io_t *io, lsm6dsr_accel_odr_t odr, lsm6dsr_accel_fs_t fs)
{
    uint8_t val;
    lsm6dsr_status_t st = lsm6dsr_read_reg(io, LSM6DSR_REG_CTRL1_XL, &val);
    if (st != LSM6DSR_OK) return st;
    val = (val & 0x03) | ((uint8_t)fs << 2) | ((uint8_t)odr << 4);
    return lsm6dsr_write_reg(io, LSM6DSR_REG_CTRL1_XL, val);
}

/**
 * @brief  璇诲彇 ACC 鍘熷鏁版嵁 (int16)
 * @details 浠?0x28 杩炵画璇诲彇 6 瀛楄妭 X/Y/Z (LSB 鍦ㄥ厛)
 * @param  io    I/O 鎶借薄灞傛寚閽?
 * @param  accel 杈撳嚭涓夎酱 raw 鏁版嵁
 * @return lsm6dsr_status_t
 */
lsm6dsr_status_t lsm6dsr_read_accel_raw(lsm6dsr_io_t *io, lsm6dsr_axis_t *accel)
{
    if (accel == NULL) return LSM6DSR_NULL_PTR;
    uint8_t buf[6];
    lsm6dsr_status_t st = lsm6dsr_read_multi(io, LSM6DSR_REG_OUTX_L_XL, buf, 6);
    if (st != LSM6DSR_OK) return st;
    accel->x = (int16_t)(buf[1] << 8 | buf[0]);
    accel->y = (int16_t)(buf[3] << 8 | buf[2]);
    accel->z = (int16_t)(buf[5] << 8 | buf[4]);
    return LSM6DSR_OK;
}

/**
 * @brief  璇诲彇 ACC 鏁版嵁骞惰浆鎹负閲嶅姏鍔犻€熷害 (g)
 * @param  io I/O 鎶借薄灞傛寚閽?
 * @param  ax X 杞磋緭鍑?(g)
 * @param  ay Y 杞磋緭鍑?(g)
 * @param  az Z 杞磋緭鍑?(g)
 * @param  fs 婊￠噺绋?
 * @return lsm6dsr_status_t
 */
lsm6dsr_status_t lsm6dsr_read_accel_float(lsm6dsr_io_t *io, float *ax, float *ay, float *az, lsm6dsr_accel_fs_t fs)
{
    if (ax == NULL || ay == NULL || az == NULL) return LSM6DSR_NULL_PTR;
    lsm6dsr_axis_t raw;
    lsm6dsr_status_t st = lsm6dsr_read_accel_raw(io, &raw);
    if (st != LSM6DSR_OK) return st;
    float sens = accel_sensitivity(fs);
    *ax = raw.x * sens;
    *ay = raw.y * sens;
    *az = raw.z * sens;
    return LSM6DSR_OK;
}

/**
 * @brief  閰嶇疆 GYRO (ODR + 婊￠噺绋?
 * @param  io  I/O 鎶借薄灞傛寚閽?
 * @param  odr 杈撳嚭鏁版嵁閫熺巼
 * @param  fs  婊￠噺绋?
 * @return lsm6dsr_status_t
 */
lsm6dsr_status_t lsm6dsr_gyro_config(lsm6dsr_io_t *io, lsm6dsr_gyro_odr_t odr, lsm6dsr_gyro_fs_t fs)
{
    uint8_t val = ((uint8_t)fs & 0x0F) | ((uint8_t)odr << 4);
    return lsm6dsr_write_reg(io, LSM6DSR_REG_CTRL2_G, val);
}

/**
 * @brief  璇诲彇 GYRO 鍘熷鏁版嵁 (int16)
 * @details 浠?0x22 杩炵画璇诲彇 6 瀛楄妭 X/Y/Z (LSB 鍦ㄥ厛)
 * @param  io   I/O 鎶借薄灞傛寚閽?
 * @param  gyro 杈撳嚭涓夎酱 raw 鏁版嵁
 * @return lsm6dsr_status_t
 */
lsm6dsr_status_t lsm6dsr_read_gyro_raw(lsm6dsr_io_t *io, lsm6dsr_axis_t *gyro)
{
    if (gyro == NULL) return LSM6DSR_NULL_PTR;
    uint8_t buf[6];
    lsm6dsr_status_t st = lsm6dsr_read_multi(io, LSM6DSR_REG_OUTX_L_G , buf, 6);
    if (st != LSM6DSR_OK) return st;
    gyro->x = (int16_t)(buf[1] << 8 | buf[0]);
    gyro->y = (int16_t)(buf[3] << 8 | buf[2]);
    gyro->z = (int16_t)(buf[5] << 8 | buf[4]);
    return LSM6DSR_OK;
}

/**
 * @brief  璇诲彇 GYRO 鏁版嵁骞惰浆鎹负瑙掗€熷害 (dps)
 * @param  io I/O 鎶借薄灞傛寚閽?
 * @param  wx X 杞磋緭鍑?(dps)
 * @param  wy Y 杞磋緭鍑?(dps)
 * @param  wz Z 杞磋緭鍑?(dps)
 * @param  fs 婊￠噺绋?
 * @return lsm6dsr_status_t
 */
lsm6dsr_status_t lsm6dsr_read_gyro_float(lsm6dsr_io_t *io, float *wx, float *wy, float *wz, lsm6dsr_gyro_fs_t fs)
{
    if (wx == NULL || wy == NULL || wz == NULL) return LSM6DSR_NULL_PTR;
    lsm6dsr_axis_t raw;
    lsm6dsr_status_t st = lsm6dsr_read_gyro_raw(io, &raw);
    if (st != LSM6DSR_OK) return st;
    float sens = gyro_sensitivity(fs);
    *wx = raw.x * sens;
    *wy = raw.y * sens;
    *wz = raw.z * sens;
    return LSM6DSR_OK;
}

/**
 * @brief  璇诲彇娓╁害浼犳劅鍣?
 * @details 娓╁害鍏紡: T = raw/256 + 25 (掳C)锛?5掳C 鏃惰緭鍑?0
 * @param  io           I/O 鎶借薄灞傛寚閽?
 * @param  temp_celsius 杈撳嚭娓╁害 (掳C)
 * @return lsm6dsr_status_t
 */
lsm6dsr_status_t lsm6dsr_read_temp(lsm6dsr_io_t *io, float *temp_celsius)
{
    if (temp_celsius == NULL) return LSM6DSR_NULL_PTR;
    uint8_t buf[2];
    lsm6dsr_status_t st = lsm6dsr_read_multi(io, LSM6DSR_REG_OUT_TEMP_L , buf, 2);
    if (st != LSM6DSR_OK) return st;
    int16_t raw = (int16_t)(buf[1] << 8 | buf[0]);
    *temp_celsius = (raw / LSM6DSR_TEMP_SENSITIVITY) + LSM6DSR_TEMP_OFFSET;
    return LSM6DSR_OK;
}

/**
 * @brief  鍒濆鍖?FIFO (璁剧疆姘村嵃闃堝€?+ 鎵归€熺巼)
 * @param  io        I/O 鎶借薄灞傛寚閽?
 * @param  threshold 姘村嵃闃堝€?(9-bit)
 * @param  bdr_xl    ACC 鎵归€熺巼
 * @param  bdr_gy    GYRO 鎵归€熺巼
 * @return lsm6dsr_status_t
 */
lsm6dsr_status_t lsm6dsr_fifo_init(lsm6dsr_io_t *io, uint16_t threshold, uint8_t bdr_xl, uint8_t bdr_gy)
{
    lsm6dsr_status_t st;
    st = lsm6dsr_write_reg(io, LSM6DSR_REG_FIFO_CTRL1, (uint8_t)(threshold & 0xFF));
    if (st != LSM6DSR_OK) return st;
    st = lsm6dsr_write_reg(io, LSM6DSR_REG_FIFO_CTRL2,
                            (uint8_t)(((threshold >> 8) & 0x01) | (0x03 << 1)));
    if (st != LSM6DSR_OK) return st;
    st = lsm6dsr_write_reg(io, LSM6DSR_REG_FIFO_CTRL3, (bdr_gy << 4) | bdr_xl);
    if (st != LSM6DSR_OK) return st;
    return LSM6DSR_OK;
}

/**
 * @brief  璁剧疆 FIFO 妯″紡
 * @param  io   I/O 鎶借薄灞傛寚閽?
 * @param  mode FIFO 妯″紡 (FIFO_MODE_*)
 * @return lsm6dsr_status_t
 */
lsm6dsr_status_t lsm6dsr_fifo_set_mode(lsm6dsr_io_t *io, uint8_t mode)
{
    uint8_t val;
    lsm6dsr_status_t st;

    st = lsm6dsr_read_reg(io, LSM6DSR_REG_FIFO_CTRL4, &val);
    if (st != LSM6DSR_OK) return st;
    val = (val & ~FIFO_CTRL4_FIFO_MODE_MASK) | (mode & FIFO_CTRL4_FIFO_MODE_MASK);
    st = lsm6dsr_write_reg(io, LSM6DSR_REG_FIFO_CTRL4, val);
    if (st != LSM6DSR_OK) return st;

    st = lsm6dsr_read_reg(io, LSM6DSR_REG_FIFO_CTRL2, &val);
    if (st != LSM6DSR_OK) return st;
    val &= ~((1 << 7) | (1 << 6));
    return lsm6dsr_write_reg(io, LSM6DSR_REG_FIFO_CTRL2, val);
}

/**
 * @brief  璇诲彇 FIFO 鏍囩 + 鏁版嵁 (6 瀛楄妭)
 * @param  io   I/O 鎶借薄灞傛寚閽?
 * @param  tag  杈撳嚭鏍囩
 * @param  data 杈撳嚭鏁版嵁缂撳啿鍖?(6 瀛楄妭 X/Y/Z)
 * @return lsm6dsr_status_t
 */
lsm6dsr_status_t lsm6dsr_fifo_read_tag_data(lsm6dsr_io_t *io, uint8_t *tag, uint8_t *data)
{
    if (tag == NULL || data == NULL) return LSM6DSR_NULL_PTR;
    lsm6dsr_status_t st = lsm6dsr_read_reg(io, LSM6DSR_REG_FIFO_DATA_OUT_TAG, tag);
    if (st != LSM6DSR_OK) return st;
    st = lsm6dsr_read_multi(io, LSM6DSR_REG_FIFO_DATA_OUT_XL, data, 6);
    return st;
}

/**
 * @brief  鑾峰彇 FIFO 宸茬敤娣卞害
 * @param  io I/O 鎶借薄灞傛寚閽?
 * @return FIFO 鏉＄洰鏁?(0-1023)锛岄€氫俊澶辫触鏃惰繑鍥?0
 */
uint16_t lsm6dsr_fifo_get_level(lsm6dsr_io_t *io)
{
    uint8_t buf[2];
    if (lsm6dsr_read_multi(io, LSM6DSR_REG_FIFO_STATUS1 , buf, 2) != LSM6DSR_OK)
        return 0;
    return (uint16_t)((buf[1] & 0x03) << 8 | buf[0]) & 0x03FF;
}

/**
 * @brief  鏌ヨ鏁版嵁灏辩华鐘舵€?
 * @param  io         I/O 鎶借薄灞傛寚閽?
 * @param  accel_drdy ACC 灏辩华鏍囧織 (1=灏辩华)
 * @param  gyro_drdy  GYRO 灏辩华鏍囧織 (1=灏辩华)
 * @return lsm6dsr_status_t
 */
lsm6dsr_status_t lsm6dsr_get_drdy(lsm6dsr_io_t *io, uint8_t *accel_drdy, uint8_t *gyro_drdy)
{
    if (accel_drdy == NULL || gyro_drdy == NULL) return LSM6DSR_NULL_PTR;
    uint8_t status;
    lsm6dsr_status_t st = lsm6dsr_read_reg(io, LSM6DSR_REG_STATUS_REG, &status);
    if (st != LSM6DSR_OK) return st;
    *accel_drdy = (status & STATUS_REG_DRDY_XL) ? 1 : 0;
    *gyro_drdy  = (status & STATUS_REG_DRDY_G)  ? 1 : 0;
    return LSM6DSR_OK;
}

/*============================================================================*
 * FIFO Status Flags
 *============================================================================*/
/**
 * @brief  鏌ヨ FIFO 姘村嵃鏍囧織
 * @param  io I/O 鎶借薄灞傛寚閽?
 * @return 1=杈惧埌姘村嵃闃堝€?/ 0=鏈揪鍒?
 */
uint8_t lsm6dsr_fifo_wtm_flag(lsm6dsr_io_t *io)
{
    uint8_t status = 0;
    lsm6dsr_read_reg(io, LSM6DSR_REG_FIFO_STATUS2, &status);
    return (status >> 7) & 1;
}

/**
 * @brief  鏌ヨ FIFO 婧㈠嚭鏍囧織
 * @param  io I/O 鎶借薄灞傛寚閽?
 * @return 1=婧㈠嚭 / 0=姝ｅ父
 */
uint8_t lsm6dsr_fifo_ovr_flag(lsm6dsr_io_t *io)
{
    uint8_t status = 0;
    lsm6dsr_read_reg(io, LSM6DSR_REG_FIFO_STATUS2, &status);
    return (status >> 6) & 1;
}

/**
 * @brief  鏌ヨ FIFO 婊℃爣蹇?
 * @param  io I/O 鎶借薄灞傛寚閽?
 * @return 1=婊?/ 0=鏈弧
 */
uint8_t lsm6dsr_fifo_full_flag(lsm6dsr_io_t *io)
{
    uint8_t status = 0;
    lsm6dsr_read_reg(io, LSM6DSR_REG_FIFO_STATUS2, &status);
    return (status >> 5) & 1;
}

/**
 * @brief  鍐插埛 FIFO (鍒囨崲涓?Bypass 妯″紡)
 * @param  io I/O 鎶借薄灞傛寚閽?
 * @return lsm6dsr_status_t
 */
lsm6dsr_status_t lsm6dsr_fifo_flush(lsm6dsr_io_t *io)
{
    return lsm6dsr_fifo_set_mode(io, FIFO_MODE_BYPASS);
}

/**
 * @brief  璁剧疆 FIFO 姘村嵃闃堝€?
 * @param  io        I/O 鎶借薄灞傛寚閽?
 * @param  threshold 闃堝€?(9-bit, 0-511)
 * @return lsm6dsr_status_t
 */
lsm6dsr_status_t lsm6dsr_fifo_set_wtm(lsm6dsr_io_t *io, uint16_t threshold)
{
    lsm6dsr_status_t st;
    st = lsm6dsr_write_reg(io, LSM6DSR_REG_FIFO_CTRL1, (uint8_t)(threshold & 0xFF));
    if (st != LSM6DSR_OK) return st;
    uint8_t ctrl2;
    st = lsm6dsr_read_reg(io, LSM6DSR_REG_FIFO_CTRL2, &ctrl2);
    if (st != LSM6DSR_OK) return st;
    ctrl2 = (ctrl2 & 0xFE) | ((threshold >> 8) & 0x01);
    return lsm6dsr_write_reg(io, LSM6DSR_REG_FIFO_CTRL2, ctrl2);
}

/*============================================================================*
 * BDU / IF_INC Control
 *============================================================================*/
/**
 * @brief  浣胯兘/绂佺敤鍧楁暟鎹洿鏂?(BDU)
 * @details BDU 浣胯兘鏃讹紝杈撳嚭瀵勫瓨鍣ㄥ湪璇诲彇瀹屾垚鍓嶄笉鏇存柊锛岄伩鍏嶈鍙栬繃绋嬩腑鏁版嵁鏀瑰彉銆?
 * @param  io     I/O 鎶借薄灞傛寚閽?
 * @param  enable 1=浣胯兘 / 0=绂佺敤
 * @return lsm6dsr_status_t
 */
lsm6dsr_status_t lsm6dsr_set_bdu(lsm6dsr_io_t *io, uint8_t enable)
{
    uint8_t val;
    lsm6dsr_status_t st = lsm6dsr_read_reg(io, LSM6DSR_REG_CTRL3_C, &val);
    if (st != LSM6DSR_OK) return st;
    if (enable) val |= CTRL3_C_BDU;
    else        val &= ~CTRL3_C_BDU;
    return lsm6dsr_write_reg(io, LSM6DSR_REG_CTRL3_C, val);
}

/**
 * @brief  浣胯兘/绂佺敤瀵勫瓨鍣ㄥ湴鍧€鑷姩閫掑 (IF_INC)
 * @details IF_INC 浣胯兘鏃讹紝澶氬瓧鑺傝鍙栦細鑷姩閫掑瀵勫瓨鍣ㄥ湴鍧€銆?
 *          蹇呴』浣胯兘浠ヤ娇鐢?lsm6dsr_read_multi 涓€娆¤鍙?6 瀛楄妭涓夎酱鏁版嵁銆?
 * @param  io     I/O 鎶借薄灞傛寚閽?
 * @param  enable 1=浣胯兘 / 0=绂佺敤
 * @return lsm6dsr_status_t
 */
lsm6dsr_status_t lsm6dsr_set_if_inc(lsm6dsr_io_t *io, uint8_t enable)
{
    uint8_t val;
    lsm6dsr_status_t st = lsm6dsr_read_reg(io, LSM6DSR_REG_CTRL3_C, &val);
    if (st != LSM6DSR_OK) return st;
    if (enable) val |= CTRL3_C_IF_INC;
    else        val &= ~CTRL3_C_IF_INC;
    return lsm6dsr_write_reg(io, LSM6DSR_REG_CTRL3_C, val);
}

/*============================================================================*
 * FIFO Entry Read with Sensor Type
 *============================================================================*/
/**
 * @brief  璇诲彇 FIFO 鏉＄洰骞跺垽鏂紶鎰熷櫒绫诲瀷
 * @param  io     I/O 鎶借薄灞傛寚閽?
 * @param  sensor 杈撳嚭浼犳劅鍣ㄧ被鍨?(GYRO/ACC)
 * @param  data   杈撳嚭涓夎酱鏁版嵁
 * @return lsm6dsr_status_t
 */
lsm6dsr_status_t lsm6dsr_read_fifo_entry(lsm6dsr_io_t *io,
                                          lsm6dsr_fifo_sensor_t *sensor,
                                          lsm6dsr_axis_t *data)
{
    if (sensor == NULL || data == NULL) return LSM6DSR_NULL_PTR;
    uint8_t tag, buf[6];
    lsm6dsr_status_t st = lsm6dsr_read_reg(io, LSM6DSR_REG_FIFO_DATA_OUT_TAG, &tag);
    if (st != LSM6DSR_OK) return st;
    st = lsm6dsr_read_multi(io, LSM6DSR_REG_FIFO_DATA_OUT_XL, buf, 6);
    if (st != LSM6DSR_OK) return st;
    data->x = (int16_t)((uint16_t)buf[1] << 8 | buf[0]);
    data->y = (int16_t)((uint16_t)buf[3] << 8 | buf[2]);
    data->z = (int16_t)((uint16_t)buf[5] << 8 | buf[4]);
    *sensor = FIFO_TAG_IS_ACC(tag) ? LSM6DSR_FIFO_SENSOR_ACC : LSM6DSR_FIFO_SENSOR_GYRO;
    return LSM6DSR_OK;
}

/*============================================================================*
 * Self-Test
 *============================================================================*/
/**
 * @brief  璁剧疆 ACC 鑷妯″紡
 * @param  io   I/O 鎶借薄灞傛寚閽?
 * @param  mode 鑷妯″紡 (LSM6DSR_XL_ST_DISABLE/POSITIVE/NEGATIVE)
 * @return lsm6dsr_status_t
 */
lsm6dsr_status_t lsm6dsr_xl_self_test(lsm6dsr_io_t *io, uint8_t mode)
{
    if (io == NULL) return LSM6DSR_NULL_PTR;
    uint8_t val;
    lsm6dsr_status_t st = lsm6dsr_read_reg(io, LSM6DSR_REG_CTRL5_C, &val);
    if (st != LSM6DSR_OK) return st;
    val = (val & 0xFC) | (mode & 0x03);
    return lsm6dsr_write_reg(io, LSM6DSR_REG_CTRL5_C, val);
}

/**
 * @brief  璁剧疆 GYRO 鑷妯″紡
 * @param  io   I/O 鎶借薄灞傛寚閽?
 * @param  mode 鑷妯″紡 (LSM6DSR_GY_ST_DISABLE/POSITIVE/NEGATIVE)
 * @return lsm6dsr_status_t
 */
lsm6dsr_status_t lsm6dsr_gy_self_test(lsm6dsr_io_t *io, uint8_t mode)
{
    if (io == NULL) return LSM6DSR_NULL_PTR;
    uint8_t val;
    lsm6dsr_status_t st = lsm6dsr_read_reg(io, LSM6DSR_REG_CTRL5_C, &val);
    if (st != LSM6DSR_OK) return st;
    val = (val & 0xF3) | ((mode & 0x03) << 2);
    return lsm6dsr_write_reg(io, LSM6DSR_REG_CTRL5_C, val);
}

/*============================================================================*
 * Power Mode Control
 *============================================================================*/
/**
 * @brief  璁剧疆 ACC 楂樻€ц兘妯″紡
 * @param  io     I/O 鎶借薄灞傛寚閽?
 * @param  enable 1=楂樻€ц兘 / 0=浣庡姛鑰?
 * @return lsm6dsr_status_t
 */
lsm6dsr_status_t lsm6dsr_xl_set_hm_mode(lsm6dsr_io_t *io, uint8_t enable)
{
    uint8_t val;
    lsm6dsr_status_t st = lsm6dsr_read_reg(io, LSM6DSR_REG_CTRL6_C, &val);
    if (st != LSM6DSR_OK) return st;
    if (enable) val |= CTRL6_C_XL_HM_MODE;
    else        val &= ~CTRL6_C_XL_HM_MODE;
    return lsm6dsr_write_reg(io, LSM6DSR_REG_CTRL6_C, val);
}

/**
 * @brief  璁剧疆 GYRO 楂樻€ц兘妯″紡
 * @param  io     I/O 鎶借薄灞傛寚閽?
 * @param  enable 1=楂樻€ц兘 / 0=浣庡姛鑰?
 * @return lsm6dsr_status_t
 */
lsm6dsr_status_t lsm6dsr_gy_set_hm_mode(lsm6dsr_io_t *io, uint8_t enable)
{
    uint8_t val;
    lsm6dsr_status_t st = lsm6dsr_read_reg(io, LSM6DSR_REG_CTRL7_G, &val);
    if (st != LSM6DSR_OK) return st;
    if (enable) val |= CTRL7_G_GY_HM_MODE;
    else        val &= ~CTRL7_G_GY_HM_MODE;
    return lsm6dsr_write_reg(io, LSM6DSR_REG_CTRL7_G, val);
}

