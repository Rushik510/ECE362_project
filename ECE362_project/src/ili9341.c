#include "ili9341.h"
#include "font8x8.h"

// Helper functions
static inline void cs_select(ILI9341 *lcd)  { gpio_put(lcd->cs_pin, 0); }
static inline void cs_deselect(ILI9341 *lcd){ gpio_put(lcd->cs_pin, 1); }
static inline void dc_command(ILI9341 *lcd) { gpio_put(lcd->dc_pin, 0); }
static inline void dc_data(ILI9341 *lcd)    { gpio_put(lcd->dc_pin, 1); }

static inline void write_cmd(ILI9341 *lcd, uint8_t cmd) {
    cs_select(lcd);
    dc_command(lcd);
    spi_write_blocking(lcd->spi, &cmd, 1);
    cs_deselect(lcd);
}

static inline void write_data(ILI9341 *lcd, const uint8_t *data, size_t len) {
    cs_select(lcd);
    dc_data(lcd);
    spi_write_blocking(lcd->spi, data, len);
    cs_deselect(lcd);
}

// Set address window
// ---- FIXED ADDRESS WINDOW FOR 240x320 ILI9341 ----
static void set_addr(ILI9341 *lcd, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    // MOST 2.2" TFTs need this vertical offset:
    const uint16_t X_OFFSET = 0;
    const uint16_t Y_OFFSET = 0;   // If still no text, change to 80 later

    uint8_t col_data[] = {
        (x0 + X_OFFSET) >> 8, (x0 + X_OFFSET) & 0xFF,
        (x1 + X_OFFSET) >> 8, (x1 + X_OFFSET) & 0xFF
    };

    uint8_t row_data[] = {
        (y0 + Y_OFFSET) >> 8, (y0 + Y_OFFSET) & 0xFF,
        (y1 + Y_OFFSET) >> 8, (y1 + Y_OFFSET) & 0xFF
    };

    write_cmd(lcd, 0x2A);     // Column Address Set
    write_data(lcd, col_data, 4);

    write_cmd(lcd, 0x2B);     // Row Address Set
    write_data(lcd, row_data, 4);

    write_cmd(lcd, 0x2C);     // Memory Write
}


// Initialize display
void ili9341_init(ILI9341 *lcd,
                  spi_inst_t *spi,
                  uint cs, uint dc, uint rst)
{
    lcd->spi = spi;
    lcd->cs_pin = cs;
    lcd->dc_pin = dc;
    lcd->rst_pin = rst;

    gpio_init(cs);  gpio_set_dir(cs, GPIO_OUT);  gpio_put(cs, 1);
    gpio_init(dc);  gpio_set_dir(dc, GPIO_OUT);
    gpio_init(rst); gpio_set_dir(rst, GPIO_OUT);

    // Reset sequence
    sleep_ms(50);
    gpio_put(rst, 0);
    sleep_ms(50);
    gpio_put(rst, 1);
    sleep_ms(120);

    // Configure SPI0 @ 40MHz
    spi_init(spi, 40000000);
    gpio_set_function(6, GPIO_FUNC_SPI);  // SCK
    gpio_set_function(7, GPIO_FUNC_SPI);  // MOSI

    // ILI9341 init sequence
    write_cmd(lcd, 0x01);  // Software reset
    sleep_ms(120);

    write_cmd(lcd, 0x28);  // Display OFF

    write_cmd(lcd, 0x3A);  // Pixel format
    write_data(lcd, (uint8_t[]){0x55}, 1); // 16-bit

    write_cmd(lcd, 0x36);
    write_data(lcd, (uint8_t[]){0x28}, 1); // Memory Access Control


    write_cmd(lcd, 0x11); // Sleep OUT
    sleep_ms(120);

    write_cmd(lcd, 0x29); // Display ON
}

void ili9341_fill_rect(ILI9341 *lcd, int x, int y, int w, int h, uint16_t color)
{
    uint8_t hi = color >> 8;
    uint8_t lo = color & 0xFF;
    uint8_t px[2] = { hi, lo };

    set_addr(lcd, x, y, x + w - 1, y + h - 1);

    cs_select(lcd);
    dc_data(lcd);

    for (int i = 0; i < w * h; i++)
        spi_write_blocking(lcd->spi, px, 2);

    cs_deselect(lcd);
}

void ili9341_fill_screen(ILI9341 *lcd, uint16_t color) {
    ili9341_fill_rect(lcd, 0, 0, ILI9341_WIDTH, ILI9341_HEIGHT, color);
}

void ili9341_draw_pixel(ILI9341 *lcd, int x, int y, uint16_t color) {

    if (x < 0 || x >= ILI9341_WIDTH || y < 0 || y >= ILI9341_HEIGHT)
        return;

    uint8_t hi = color >> 8;
    uint8_t lo = color & 0xFF;

    // Set column
    write_cmd(lcd, 0x2A);
    uint8_t col_data[] = {0x00, x, 0x00, x};
    write_data(lcd, col_data, 4);

    // Set row
    write_cmd(lcd, 0x2B);
    uint8_t row_data[] = {0x00, y, 0x00, y};
    write_data(lcd, row_data, 4);

    // Write one pixel
    write_cmd(lcd, 0x2C);
    uint8_t px[] = {hi, lo};
    write_data(lcd, px, 2);
}
void ili9341_draw_char_big(ILI9341 *lcd, int x, int y, char c,
                           uint16_t color, uint16_t bg, int scale)
{
    for (int row = 0; row < 8; row++) {
        uint8_t b = font8x8_basic[(uint8_t)c][row];

// Reverse 8 bits (to fix horizontal mirror)
        uint8_t bits = ( (b & 0x01) << 7 ) |
                       ( (b & 0x02) << 5 ) |
                       ( (b & 0x04) << 3 ) |
                       ( (b & 0x08) << 1 ) |
                       ( (b & 0x10) >> 1 ) |
                       ( (b & 0x20) >> 3 ) |
                       ( (b & 0x40) >> 5 ) |
                       ( (b & 0x80) >> 7 );


        for (int col = 0; col < 8; col++) {
            uint16_t pixel_color = (bits & (1 << col)) ? color : bg;

            // Draw scaled block
            for (int dy = 0; dy < scale; dy++) {
                for (int dx = 0; dx < scale; dx++) {
                    ili9341_draw_pixel(lcd,
                                       x + col * scale + dx,
                                       y + row * scale + dy,
                                       pixel_color);
                }
            }
        }
    }
}

void ili9341_draw_string_big(ILI9341 *lcd, int x, int y,
                             const char *text,
                             uint16_t color, uint16_t bg,
                             int scale)
{
    while (*text) {
        ili9341_draw_char_big(lcd, x, y, *text, color, bg, scale);
        x += (8 * scale) + 2;   // next character position
        text++;
    }
}


void ili9341_draw_char(ILI9341 *lcd, int x, int y, char c,
                       uint16_t color, uint16_t bg)
{
    for (int row = 0; row < 8; row++) {
        uint8_t bits = font8x8_basic[(uint8_t)c][row];
        for (int col = 0; col < 8; col++) {
            uint16_t pixel = (bits & (1 << col)) ? color : bg;
            ili9341_draw_pixel(lcd, x + col, y + row, pixel);
        }
    }
}

void ili9341_draw_string(ILI9341 *lcd, int x, int y,
                         const char *text,
                         uint16_t color, uint16_t bg)
{
    while (*text) {
        ili9341_draw_char(lcd, x, y, *text, color, bg);
        x += 8;
        text++;
    }
}
