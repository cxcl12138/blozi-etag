#include <stdint.h>
#include "tl_common.h"
#include "main.h"
#include "epd.h"
#include "epd_spi.h"
#include "epd_bwr_213.h"
#include "drivers.h"
#include "stack/ble/ble.h"

// ==================== IL3895 专用 LUT 波形表 ====================
// 全刷 LUT (Full Update)
const uint8_t LUTDefault_full[] = {
    0x32, // 命令
    0x22, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x11, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// 局刷 LUT (Partial Update)
const uint8_t LUTDefault_part[] = {
    0x32, // 命令
    0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
// ===============================================================

_attribute_ram_code_ uint8_t EPD_BWR_213_detect(void)
{
    EPD_WriteCmd(0x12);
    WaitMs(10);
    EPD_WriteCmd(0x32);
    for (int i = 0; i < 153; i++) {
        EPD_WriteData(0xA5);
    }
    EPD_WriteCmd(0x33);
    for (int i = 0; i < 153; i++) {
        if (EPD_SPI_read() != 0xA5) return 0;
    }
    return 1;
}

_attribute_ram_code_ uint8_t EPD_BWR_213_read_temp(void)
{
    // IL3895 温度读取机制不同，直接返回默认值，防止卡死
    uint8_t epd_temperature = 0;
    return epd_temperature;
}

// 内部辅助函数：发送 LUT 数据
static void EPD_Send_LUT(const uint8_t *lut, int size)
{
    for (int i = 0; i < size; i++) {
        EPD_WriteData(lut[i]);
    }
}

// 内部辅助函数：IL3895 标准初始化
static void EPD_IL3895_Init(void)
{
    // 1. 软件复位
    EPD_WriteCmd(0x12);
    EPD_CheckStatus(100); 

    // 2. 驱动输出控制 (MUX 249, 250行)
    EPD_WriteCmd(0x01);
    EPD_WriteData(0xF9);
    EPD_WriteData(0x00);
    EPD_WriteData(0x00);

    // 3. 数据入口模式设置 (扫描方向)
    EPD_WriteCmd(0x11);
    EPD_WriteData(0x01);

    // 4. 设置 RAM X 窗口 (0-15, 128像素对齐)
    EPD_WriteCmd(0x44);
    EPD_WriteData(0x00);
    EPD_WriteData(0x0F); 

    // 5. 设置 RAM Y 窗口 (0-249)
    EPD_WriteCmd(0x45);
    EPD_WriteData(0x00);
    EPD_WriteData(0x00);
    EPD_WriteData(0xF9); 
    EPD_WriteData(0x00);

    // 6. 设置起始游标 (光标归零)
    EPD_WriteCmd(0x4E);
    EPD_WriteData(0x00);
    EPD_WriteCmd(0x4F);
    EPD_WriteData(0x00);
    EPD_WriteData(0x00);
}

_attribute_ram_code_ uint8_t EPD_BWR_213_Display(unsigned char *image, int size, uint8_t full_or_partial)
{
    uint8_t epd_temperature = 0;

    // 1. 执行初始化
    EPD_IL3895_Init();

    // 2. 加载 LUT
    if (full_or_partial) {
        EPD_WriteCmd(LUTDefault_full[0]); 
        EPD_Send_LUT(&LUTDefault_full[1], sizeof(LUTDefault_full) - 1); 
    } else {
        EPD_WriteCmd(LUTDefault_part[0]); 
        EPD_Send_LUT(&LUTDefault_part[1], sizeof(LUTDefault_part) - 1);
    }

    // 3. 写入图像数据 (黑白单色，只用 0x24)
    EPD_LoadImage(image, size, 0x24);

    // 4. 触发刷新
    EPD_WriteCmd(0x22);
    EPD_WriteData(0xC7);
    EPD_WriteCmd(0x20);
    EPD_CheckStatus(100);

    return epd_temperature;
}

// 三色显示函数保留空壳，忽略红色通道
_attribute_ram_code_ uint8_t EPD_BWR_213_Display_BWR(unsigned char *image, unsigned char *red_image, int size, uint8_t full_or_partial)
{
    return EPD_BWR_213_Display(image, size, full_or_partial); 
}

_attribute_ram_code_ void EPD_BWR_213_set_sleep(void)
{
    EPD_WriteCmd(0x10);
    EPD_WriteData(0x01);
}
