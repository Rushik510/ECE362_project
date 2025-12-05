#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "ili9341.h"

#define BUZZER_PIN 18

// TFT object
ILI9341 tft;

// ---------------- WATER LEVEL LOGIC ----------------

int get_water_level(uint16_t raw) {
    if (raw < 2200) return 2;        // HIGH (Wet)
    else if (raw < 3800) return 1;   // MODERATE (Some moisture)
    else return 0;                  // NO WATER (Dry)
}

uint16_t smooth_adc() {
    uint32_t sum = 0;
    for (int i = 0; i < 20; i++) {
        sum += adc_read();
        sleep_us(1000);
    }
    return sum / 20;
}

int classify_water(uint16_t raw) {
    if (raw < 2200) return 2;
    else if (raw < 3800) return 1;
    else return 0;
}

// Buzzer 
void buzzer_on() {
     gpio_put(BUZZER_PIN, 1); 
    } 
void buzzer_off() { 
    gpio_put(BUZZER_PIN, 0); }

    void buzzer_no_water() {
    buzzer_on();
    sleep_ms(1000);      // 1-second long beep
    buzzer_off();
    sleep_ms(200);
}

void buzzer_mid() {
    buzzer_on();
    sleep_ms(150);       // short beep
    buzzer_off();
    sleep_ms(300);
}

void buzzer_high() {
    for (int i = 0; i < 3; i++) {
        buzzer_on();
        sleep_ms(80);
        buzzer_off();
        sleep_ms(80);
    }
}

void buzzer_play(int level) {
    switch(level) {
        case 0: buzzer_no_water(); break;
        case 1: buzzer_mid(); break;
        case 2: buzzer_high(); break;
    }
}

// ---------------- TFT TEXT FOR ONE SENSOR ----------------

void tft_show_readings(ILI9341 *lcd, uint16_t raw) {
    char raw_str[16];
    char volt_str[16];
    const char *status_str;

    float voltage = (3.3f * raw) / 4095.0f;
    int level = get_water_level(raw);

    snprintf(raw_str, sizeof(raw_str), "%u", raw);
    snprintf(volt_str, sizeof(volt_str), "%.2f", voltage);

    switch (level) {
        case 0: status_str = "DRY"; break;
        case 1: status_str = "MOIST";    break;
        case 2: status_str = "TOO WET";     break;
    }

    int scale = 2;

    // RAW
    ili9341_draw_string_big(lcd, 10,  70,  "RAW",   ILI9341_GREEN,  ILI9341_BLACK, scale);
    ili9341_draw_string_big(lcd, 150, 70,  raw_str, ILI9341_GREEN,  ILI9341_BLACK, scale);

    // VOLTAGE
    ili9341_draw_string_big(lcd, 10,  120, "VOLT",  ILI9341_CYAN,   ILI9341_BLACK, scale);
    ili9341_draw_string_big(lcd, 150, 120, volt_str, ILI9341_CYAN,  ILI9341_BLACK, scale);

    // STATUS on its own line
    ili9341_draw_string_big(lcd, 10, 170, "STATUS:", 
                        ILI9341_YELLOW, ILI9341_BLACK, 2);

    // Value printed BELOW STATUS
    ili9341_draw_string_big(lcd, 10, 210, status_str, 
                        ILI9341_YELLOW, ILI9341_BLACK, 2);
}

// ---------------------- MAIN -----------------------

int main() {
    stdio_init_all();
    sleep_ms(2000);

    // --- ADC SETUP ---
    adc_init();

    // Adjust these if your actual GPIOs / ADC channels differ
    const int sensor_gpio[4]   = {45, 44, 43, 42};
    const int sensor_adc_ch[4] = {5,  4,  3,  2};  // matching 45→ch5, 44→ch4, etc.

    for (int i = 0; i < 4; i++) {
        adc_gpio_init(sensor_gpio[i]);
    }

    // --- BUZZER SETUP ---
    gpio_init(BUZZER_PIN);
    gpio_set_dir(BUZZER_PIN, GPIO_OUT);
    gpio_put(BUZZER_PIN, 0);

    // --- TFT SETUP ---
    ILI9341 lcd;
    // spi0, CS=9, DC=8, RST=12  (your wiring)
    ili9341_init(&lcd, spi0, 9, 8, 12);

    ili9341_fill_screen(&lcd, ILI9341_BLACK);

    int current_sensor = 0;

    while (1) {
        // Select channel for current sensor
        adc_select_input(sensor_adc_ch[current_sensor]);

        // Read + process
        uint16_t raw  = smooth_adc();
        int      level = classify_water(raw);

        // Clear entire screen
        ili9341_fill_screen(&lcd, ILI9341_BLACK);

        // ------- HEADER: which sensor & which GPIO -------
        char header[16];

        // Text like: S1 (45), S2 (44), ...
        int gpio_pin = sensor_gpio[current_sensor];
        snprintf(header, sizeof(header), "SENS%d (%d)",
                 current_sensor + 1, gpio_pin);

        // Blue header bar
        ili9341_fill_rect(&lcd, 0, 0, 320, 55, ILI9341_BLUE);
        // Big yellow header text
        ili9341_draw_string_big(&lcd, 40, 10, header,
                                ILI9341_WHITE, ILI9341_BLUE, 2);

        // ------- SENSOR READINGS -------
        tft_show_readings(&lcd, raw);

        // ------- BUZZER ALERT -------
        buzzer_play(level);

        // Long delay so buzzer doesn’t spam, and you can see each sensor
        sleep_ms(3000);

        // Next sensor
        current_sensor = (current_sensor + 1) % 4;
    }
}
