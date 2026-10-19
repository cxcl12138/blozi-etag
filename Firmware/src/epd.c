#include <stdint.h>
#include "etime.h"
#include "tl_common.h"
#include "main.h"
#include "epd.h"
#include "epd_spi.h"
#include "epd_bwr_213.h" // 只保留这一个驱动头文件
#include "drivers.h"
#include "stack/ble/ble.h"

#include "battery.h"

#include "OneBitDisplay.h"
#include "TIFF_G4.h"
extern const uint8_t ucMirror[];
#include "font_60.h"
#include "font16.h"
#include "font16zh.h"
#include "font30.h"

RAM uint8_t epd_model = 2; // 直接写死 2，代表 2.13 寸
const char *epd_model_string[] = {"NC", "BW213", "BWR213", "BWR154", "213ICE", "BWR296"};
RAM uint8_t epd_update_state = 0;

RAM uint8_t epd_scene = 2;
RAM uint8_t epd_wait_update = 0;

RAM uint8_t hour_refresh = 100;
RAM uint8_t minute_refresh = 100;

const char *BLE_conn_string[] = {"BLE 0", "BLE 1"};
RAM uint8_t epd_temperature_is_read = 0;
RAM uint8_t epd_temperature = 0;

RAM uint8_t epd_buffer[epd_buffer_size];
RAM uint8_t epd_temp[epd_buffer_size]; // for OneBitDisplay to draw into
OBDISP obd;                        // virtual display structure
TIFFIMAGE tiff;

void set_EPD_model(uint8_t model_nr)
{
    epd_model = model_nr;
}

void set_EPD_scene(uint8_t scene)
{
    epd_scene = scene;
    set_EPD_wait_flush();
}

void set_EPD_wait_flush() {
    epd_wait_update = 1;
}

_attribute_ram_code_ void EPD_detect_model(void)
{
    EPD_init();
    EPD_POWER_ON();
    WaitMs(10);
    gpio_write(EPD_RESET, 0);
    WaitMs(10);
    gpio_write(EPD_RESET, 1);
    WaitMs(10);

    epd_model = 2; // 暴力指定
    EPD_POWER_OFF();
}

_attribute_ram_code_ uint8_t EPD_read_temp(void)
{
    if (epd_temperature_is_read)
        return epd_temperature;

    EPD_init();
    EPD_POWER_ON();
    WaitMs(5);
    gpio_write(EPD_RESET, 0);
    WaitMs(10);
    gpio_write(EPD_RESET, 1);
    WaitMs(10);

    epd_temperature = EPD_BWR_213_read_temp();

    EPD_POWER_OFF();
    epd_temperature_is_read = 1;
    return epd_temperature;
}

_attribute_ram_code_ void EPD_Display(unsigned char *image, unsigned char *red_image, int size, uint8_t full_or_partial)
{
    EPD_init();
    EPD_POWER_ON();
    WaitMs(5);
    gpio_write(EPD_RESET, 0);
    WaitMs(10);
    gpio_write(EPD_RESET, 1);
    WaitMs(10);

    epd_temperature = EPD_BWR_213_Display(image, size, full_or_partial);

    epd_temperature_is_read = 1;
    epd_update_state = 1;
}

_attribute_ram_code_ void epd_set_sleep(void)
{
    EPD_BWR_213_set_sleep();
    EPD_POWER_OFF();
    epd_update_state = 0;
}

_attribute_ram_code_ uint8_t epd_state_handler(void)
{
    switch (epd_update_state)
    {
    case 0:
        break;
    case 1: 
        if (EPD_IS_BUSY())
            epd_set_sleep();
        break;
    }
    return epd_update_state;
}

// 替换 epd.c 中的 FixBuffer 函数
_attribute_ram_code_ void FixBuffer(uint8_t *pSrc, uint8_t *pDst, uint16_t width, uint16_t height)
{
    int x, y;
    // 先把目标清空为 0x00 (也就是白色)
    memset(pDst, 0x00, 4000);

    for (y = 0; y < height; y++) { // 画布高 128
        for (x = 0; x < width; x++) { // 画布宽 250
            // 严格按 32 字节步长读取画布（因为 epd_width 是 256，256/8 = 32）
            int src_byte = y * 32 + (x / 8);
            int src_bit = 7 - (x % 8);
            int color = (pSrc[src_byte] >> src_bit) & 1;
            
            if (color) { // 如果画布画的是黑色
                // 旋转 90 度映射到屏幕
                int dst_x = 127 - y; 
                int dst_y = x;
                
                int dst_byte = dst_y * 16 + (dst_x / 8); // 屏幕宽 128，128/8 = 16
                int dst_bit = 7 - (dst_x % 8);
                pDst[dst_byte] |= (1 << dst_bit); // 置为黑色
            }
        }
    }
}

_attribute_ram_code_ void TIFFDraw(TIFFDRAW *pDraw)
{
    uint8_t uc = 0, ucSrcMask, ucDstMask, *s, *d;
    int x, y;

    s = pDraw->pPixels;
    y = pDraw->y;                          
    d = &epd_buffer[(249 * 16) + (y / 8)]; 
    ucDstMask = 0x80 >> (y & 7);           
    ucSrcMask = 0;                         
    for (x = 0; x < pDraw->iWidth; x++)
    {
        if (ucSrcMask == 0)
        { 
            ucSrcMask = 0x80;
            uc = *s++;
        }
        if (!(uc & ucSrcMask))
        { 
            d[-(x * 16)] &= ~ucDstMask;
        }
        ucSrcMask >>= 1;
    }
}

_attribute_ram_code_ void epd_display_tiff(uint8_t *pData, int iSize)
{
    epd_clear();
    TIFF_openRAW(&tiff, 250, 122, BITDIR_MSB_FIRST, pData, iSize, TIFFDraw);
    TIFF_setDrawParameters(&tiff, 65536, TIFF_PIXEL_1BPP, 0, 0, 250, 122, NULL);
    TIFF_decode(&tiff);
    TIFF_close(&tiff);
    EPD_Display(epd_buffer, NULL, 4000, 1); // 强制 4000 字节
}

extern uint8_t mac_public[6];

_attribute_ram_code_ void epd_display(struct date_time _time, uint16_t battery_mv, int16_t temperature, uint8_t full_or_partial)
{
    uint8_t battery_level;

    if (epd_update_state)
        return;

    uint16_t resolution_w = 250;
    uint16_t resolution_h = 128; 

    epd_clear();

    obdCreateVirtualDisplay(&obd, resolution_w, resolution_h, epd_temp);
    obdFill(&obd, 0, 0); // fill with white

    char buff[100];
    battery_level = get_battery_level(battery_mv);
    sprintf(buff, "BLZ_%02X%02X%02X %s", mac_public[2], mac_public[1], mac_public[0], epd_model_string[epd_model]);
    obdWriteStringCustom(&obd, (GFXfont *)&Dialog_plain_16, 1, 17, (char *)buff, 1);
    sprintf(buff, "%s", BLE_conn_string[ble_get_connected()]);
    obdWriteStringCustom(&obd, (GFXfont *)&Dialog_plain_16, 232, 20, (char *)buff, 1);
    sprintf(buff, "%02d:%02d", _time.tm_hour, _time.tm_min);
    obdWriteStringCustom(&obd, (GFXfont *)&DSEG14_Classic_Mini_Regular_40, 50, 65, (char *)buff, 1);
    sprintf(buff, "%d'C", EPD_read_temp());
    obdWriteStringCustom(&obd, (GFXfont *)&Special_Elite_Regular_30, 10, 95, (char *)buff, 1);
    sprintf(buff, "Battery %dmV  %d%%", battery_mv, battery_level);
    obdWriteStringCustom(&obd, (GFXfont *)&Dialog_plain_16, 10, 120, (char *)buff, 1);
    
    FixBuffer(epd_temp, epd_buffer, resolution_w, resolution_h);
    
    EPD_Display(epd_buffer, NULL, resolution_w * resolution_h / 8, full_or_partial);
}

_attribute_ram_code_ void epd_display_char(uint8_t data)
{
    int i;
    for (i = 0; i < epd_buffer_size; i++)
    {
        epd_buffer[i] = data;
    }
    EPD_Display(epd_buffer, NULL, epd_buffer_size, 1);
}

_attribute_ram_code_ void epd_clear(void)
{
    memset(epd_buffer, 0x00, epd_buffer_size);
    memset(epd_temp, 0x00, epd_buffer_size);
}

void update_time_scene(struct date_time _time, uint16_t battery_mv, int16_t temperature, void (*scene)(struct date_time, uint16_t, int16_t,  uint8_t)) {
    if (epd_update_state)
        return;

    if (epd_wait_update) {
        scene(_time, battery_mv, temperature, 1);
        epd_wait_update = 0;
    }
    else if (_time.tm_min != minute_refresh)
    {
        minute_refresh = _time.tm_min;
        if (_time.tm_hour != hour_refresh)
        {
            hour_refresh = _time.tm_hour;
            scene(_time, battery_mv, temperature, 1); // 跨小时全刷
        }
        else
        {
            scene(_time, battery_mv, temperature, 0); // 跨分钟局刷
        }
    }
}

void epd_update(struct date_time _time, uint16_t battery_mv, int16_t temperature) {
    switch(epd_scene) {
        case 1:
            update_time_scene(_time, battery_mv, temperature, epd_display);
            break;
        case 2:
            update_time_scene(_time, battery_mv, temperature, epd_display_time_with_date);
            break;
        default:
            break;
    }
}

void epd_display_time_with_date(struct date_time _time, uint16_t battery_mv, int16_t temperature, uint8_t full_or_partial)
{
    uint16_t battery_level;

    epd_clear();

    uint16_t resolution_w = 250;
    uint16_t resolution_h = 128;
    
    obdCreateVirtualDisplay(&obd, resolution_w, resolution_h, epd_temp);
    obdFill(&obd, 0, 0); // fill with white

    char buff[100];
    battery_level = get_battery_level(battery_mv);

    sprintf(buff, "BLZ_%02X%02X%02X", mac_public[2], mac_public[1], mac_public[0]);
    obdWriteStringCustom(&obd, (GFXfont *)&Dialog_plain_16, 10, 17, (char *)buff, 1);

    if (ble_get_connected())
    {
        sprintf(buff, "%s", "234");
    }
    else
    {
        sprintf(buff, "%s", "56");
    }

    obdWriteStringCustom(&obd, (GFXfont *)&Dialog_plain_16_zh, 128, 21, (char *)buff, 1);

    obdRectangle(&obd, 212, 10, 215, 14, 1, 1);
    obdRectangle(&obd, 215, 2, 249, 22, 1, 1);

    sprintf(buff, "%d", battery_level);
    obdWriteStringCustom(&obd, (GFXfont *)&Dialog_plain_16, 216, 18, (char *)buff, 0);

    obdRectangle(&obd, 0, 25, 252, 27, 1, 1);

    sprintf(buff, "%02d:%02d", _time.tm_hour, _time.tm_min);
    obdWriteStringCustom(&obd, (GFXfont *)&DSEG14_Classic_Mini_Regular_40, 25, 85, (char *)buff, 1);

    sprintf(buff, "   %d'C", EPD_read_temp());
    obdWriteStringCustom(&obd, (GFXfont *)&Dialog_plain_16, 180, 50, (char *)buff, 1);

    obdRectangle(&obd, 180, 60, 252, 62, 1, 1);

    sprintf(buff, " %dmV", battery_mv);
    obdWriteStringCustom(&obd, (GFXfont *)&Dialog_plain_16, 180, 84, (char *)buff, 1);

    obdRectangle(&obd, 178, 27, 180, 99, 1, 1);
    obdRectangle(&obd, 0, 97, 252, 99, 1, 1);

    sprintf(buff, "%d-%02d-%02d", _time.tm_year, _time.tm_month, _time.tm_day);
    obdWriteStringCustom(&obd, (GFXfont *)&Dialog_plain_16, 8, 118, (char *)buff, 1);

    if (_time.tm_week == 7)
    {
        sprintf(buff, "9:%c", _time.tm_week + 0x20 + 6);
    }
    else
    {
        sprintf(buff, "9:%c", _time.tm_week + 0x20);
    }
    obdWriteStringCustom(&obd, (GFXfont *)&Dialog_plain_16_zh, 105, 122, (char *)buff, 1);

    if (_time.tm_hour > 7 && _time.tm_hour < 20)
    {
        sprintf(buff, "%s", "EFGH");
    }
    else
    {
        sprintf(buff, "%s", "ABCD");
    }
    obdWriteStringCustom(&obd, (GFXfont *)&Dialog_plain_16_zh, 170, 122, (char *)buff, 1);

    FixBuffer(epd_temp, epd_buffer, resolution_w, resolution_h);

    // 强制发送 4000 字节（250 * 128 / 8），防止显存越界导致花屏
    EPD_Display(epd_buffer, NULL, resolution_w * resolution_h / 8, full_or_partial);
}
