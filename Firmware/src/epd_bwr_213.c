#include <stdint.h>
#include "tl_common.h"
#include "main.h"
#include "epd.h"
#include "epd_spi.h"
#include "epd_bwr_213.h"
#include "drivers.h"
#include "stack/ble/ble.h"

// IL3895 专用 LUT
const uint8_t LUTDefault_full[] = {
    0x32, 0x22, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x11, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const uint8_t LUTDefault_part[] = {
    0x32, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

_attribute_ram_code_ uint8_t EPD_BWR_213_detect(void) { return 1; }
_attribute_ram_code_ uint8_t EPD_BWR_213_read_temp(void) { return 0; }

static void EPD_Send_LUT(const uint8_t *lut, int size) {
    for (int i = 0; i < size; i++) EPD_WriteData(lut[i]);
}

static void EPD_IL3895_Init(void) {
    EPD_WriteCmd(0x12); EPD_CheckStatus(100); 

    EPD_WriteCmd(0x01);
    EPD_WriteData(0xF9); EPD_WriteData(0x00); EPD_WriteData(0x00);

    EPD_WriteCmd(0x11);
    EPD_WriteData(0x03); // X加，Y加。如果发现字反了，换成 0x01

    EPD_WriteCmd(0x44);
    EPD_WriteData(0x00); EPD_WriteData(0x0F); 

    EPD_WriteCmd(0x45);
    EPD_WriteData(0x00); EPD_WriteData(0x00); EPD_WriteData(0xF9); EPD_WriteData(0x00);

    EPD_WriteCmd(0x4E); EPD_WriteData(0x00);
    EPD_WriteCmd(0x4F); EPD_WriteData(0x00); EPD_WriteData(0x00);
}

_attribute_ram_code_ uint8_t EPD_BWR_213_Display(unsigned char *image, int size, uint8_t full_or_partial)
{
    EPD_IL3895_Init();

    if (full_or_partial) { // 全刷
        EPD_WriteCmd(LUTDefault_full[0]); 
        EPD_Send_LUT(&LUTDefault_full[1], sizeof(LUTDefault_full) - 1); 
        EPD_LoadImage(image, size, 0x24);
    } else { // 局刷
        EPD_WriteCmd(LUTDefault_part[0]); 
        EPD_Send_LUT(&LUTDefault_part[1], sizeof(LUTDefault_part) - 1);
        
        // 【核心修复】把当前图像作为“历史数据”发给芯片，防止对比花屏
        EPD_LoadImage(image, size, 0x10);
        EPD_LoadImage(image, size, 0x24);
    }

    EPD_WriteCmd(0x22);
    if (full_or_partial) {
        EPD_WriteData(0xC7); // 全刷参数
    } else {
        EPD_WriteData(0xCF); // 局刷参数 (部分屏幕可能需要改回 0xC7)
    }
    EPD_WriteCmd(0x20);
    EPD_CheckStatus(100);

    return 0;
}

_attribute_ram_code_ uint8_t EPD_BWR_213_Display_BWR(unsigned char *image, unsigned char *red_image, int size, uint8_t full_or_partial) {
    return EPD_BWR_213_Display(image, size, full_or_partial); 
}

_attribute_ram_code_ void EPD_BWR_213_set_sleep(void) {
    EPD_WriteCmd(0x10); EPD_WriteData(0x01);
}
