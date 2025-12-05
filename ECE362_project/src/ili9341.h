#ifndef _ILI9341_H_
#define _ILI9341_H_

#include "pico/stdlib.h"
#include "hardware/spi.h"

// 240x320 resolution
#define ILI9341_WIDTH   320
#define ILI9341_HEIGHT  240

// RGB565 colors
#define ILI9341_BLACK    0x0000
#define ILI9341_BLUE     0x001F
#define ILI9341_RED      0xF800
#define ILI9341_GREEN    0x07E0
#define ILI9341_CYAN     0x07FF
#define ILI9341_MAGENTA  0xF81F
#define ILI9341_YELLOW   0xFFE0
#define ILI9341_WHITE    0xFFFF

typedef struct {
    spi_inst_t *spi;
    uint cs_pin;
    uint dc_pin;
    uint rst_pin;
} ILI9341;

void ili9341_init(ILI9341 *lcd,
                  spi_inst_t *spi,
                  uint cs, uint dc, uint rst);

void ili9341_fill_screen(ILI9341 *lcd, uint16_t color);
void ili9341_fill_rect(ILI9341 *lcd, int x, int y, int w, int h, uint16_t color);
void ili9341_draw_pixel(ILI9341 *lcd, int x, int y, uint16_t color);

void ili9341_draw_char(ILI9341 *lcd, int x, int y, char c,
                       uint16_t color, uint16_t bg);

void ili9341_draw_string(ILI9341 *lcd, int x, int y,
                         const char *text,
                         uint16_t color, uint16_t bg);

#endif
