#include <stdint.h>
#include "tl_common.h"
#include "main.h"
#include "epd.h"
#include "epd_spi.h"
#include "epd_bwr_213.h"
#include "drivers.h"
#include "stack/ble/ble.h"

// ==================== GxEPD2 官方 SSD1680 黑白 LUT ====================
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

static void EPD_SSD1680_Init(void) {
    EPD_WriteCmd(0x12); // 软复位
    EPD_CheckStatus(100); 

    // 1. 驱动输出控制 (MUX 249, 250行)
    EPD_WriteCmd(0x01);
    EPD_WriteData(0xF9); 
    EPD_WriteData(0x00); 
    EPD_WriteData(0x00);

    // 2. 数据入口模式 (X增, Y增)
    EPD_WriteCmd(0x11);
    EPD_WriteData(0x03); 

    // 3. RAM X 窗口 (0-15, 128对齐)
    EPD_WriteCmd(0x44);
    EPD_WriteData(0x00); 
    EPD_WriteData(0x0F); 

    // 4. RAM Y 窗口 (0-249)
    EPD_WriteCmd(0x45);
    EPD_WriteData(0x00); 
    EPD_WriteData(0x00); 
    EPD_WriteData(0xF9); 
    EPD_WriteData(0x00);

    // 5. 边框波形
    EPD_WriteCmd(0x3C); 
    EPD_WriteData(0x05);

    // 6. 设置温度传感器 (GxEPD2 官方必备)
    EPD_WriteCmd(0x18); 
    EPD_WriteData(0x80);

    // 7. 游标归零
    EPD_WriteCmd(0x4E); 
    EPD_WriteData(0x00);
    EPD_WriteCmd(0x4F); 
    EPD_WriteData(0x00); 
    EPD_WriteData(0x00);
}

_attribute_ram_code_ uint8_t EPD_BWR_213_Display(unsigned char *image, int size, uint8_t full_or_partial)
{
    EPD_SSD1680_Init();

    if (full_or_partial) { // ===== 全刷 =====
        EPD_WriteCmd(LUTDefault_full[0]); 
        EPD_Send_LUT(&LUTDefault_full[1], sizeof(LUTDefault_full) - 1); 
        
        // GxEPD2 标准全刷指令
        EPD_WriteCmd(0x22); 
        EPD_WriteData(0xB1); // 开启升压
        EPD_WriteCmd(0x20);
        EPD_CheckStatus(100); // 等待 BUSY

        // 写入图像数据
        EPD_WriteCmd(0x24);
        for (int i = 0; i < size; i++) {
            EPD_WriteData(image[i]);
        }

        // 触发全屏刷新
        EPD_WriteCmd(0x22); 
        EPD_WriteData(0xC7); 
        EPD_WriteCmd(0x20);
        EPD_CheckStatus(100);

    } else { // ===== 局刷 =====
        EPD_WriteCmd(LUTDefault_part[0]); 
        EPD_Send_LUT(&LUTDefault_part[1], sizeof(LUTDefault_part) - 1);
        
        // SSD1680 局刷：必须先发旧图(0x10)，再发新图(0x24)
        EPD_WriteCmd(0x10); 
        for (int i = 0; i < size; i++) {
            EPD_WriteData(image[i]);
        }
        
        EPD_WriteCmd(0x24); 
        for (int i = 0; i < size; i++) {
            EPD_WriteData(image[i]);
        }
        
        // 触发局刷
        EPD_WriteCmd(0x22); 
        EPD_WriteData(0xCF); 
        EPD_WriteCmd(0x20);
        EPD_CheckStatus(100);
    }

    return 0;
}

_attribute_ram_code_ uint8_t EPD_BWR_213_Display_BWR(unsigned char *image, unsigned char *red_image, int size, uint8_t full_or_partial) {
    return EPD_BWR_213_Display(image, size, full_or_partial); 
}

_attribute_ram_code_ void EPD_BWR_213_set_sleep(void) {
    EPD_WriteCmd(0x10); 
    EPD_WriteData(0x01);
}
