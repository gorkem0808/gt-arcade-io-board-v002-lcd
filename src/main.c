#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include "lcd_i2c.h"
#include "config_store.h"

#ifndef PLAYER_ID
#define PLAYER_ID 1
#endif

#define PIN_MENU_UP       2
#define PIN_MENU_DOWN     3
#define PIN_MENU_LEFT     4
#define PIN_MENU_RIGHT    5
#define PIN_SELECT_BOMB   6
#define PIN_TRIGGER       7
#define PIN_RELOAD        8
#define PIN_COIN          17
#define PIN_START         18
#define PIN_MENU_CAL      19
#define PIN_ACTIVE        20

#define ADC_X_GPIO        26
#define ADC_Y_GPIO        27
#define ADC_X_CH          0
#define ADC_Y_CH          1

#define KEY_R             0x15
#define KEY_T             0x17
#define KEY_SPACE         0x2C
#define KEY_RIGHT_CTRL    0xE4
#define KEY_ARROW_RIGHT   0x4F
#define KEY_ARROW_LEFT    0x50
#define KEY_ARROW_DOWN    0x51
#define KEY_ARROW_UP      0x52
#define KEY_KEYPAD_1      0x59
#define KEY_KEYPAD_2      0x5A
#define KEY_KEYPAD_5      0x5D
#define KEY_KEYPAD_6      0x5E

typedef enum {
    SCREEN_NORMAL = 0,
    SCREEN_MAIN_MENU,
    SCREEN_CALIBRATION,
    SCREEN_BUTTON_TEST,
    SCREEN_AIM_TEST,
    SCREEN_FILTER,
    SCREEN_SAVED_MSG
} screen_t;

static gt_config_t cfg;
static screen_t screen = SCREEN_NORMAL;
static uint8_t menu_index = 0;
static uint8_t cal_step = 0;
static uint16_t cal_x[4];
static uint16_t cal_y[4];
static uint8_t button_test_index = 0;
static uint32_t saved_msg_until = 0;
static uint32_t last_lcd_ms = 0;
static uint32_t last_hid_ms = 0;
static uint32_t last_key_ms = 0;
static uint16_t last_x = 16384;
static uint16_t last_y = 16384;
static uint8_t last_buttons = 0;
static bool last_pins[29];

static const char *menu_items[] = {
    "KALIBRASYON",
    "BUTON TESTI",
    "NISANGAH TEST",
    "TITRESIM AZALT",
    "AYAR KAYDET",
    "CIKIS"
};

static const char *button_names[] = {
    "TETIK",
    "JARJOR DEGISTIR",
    "BOMBA / SEC",
    "COIN / KREDI",
    "START",
    "TEST / MENU",
    "SISTEM AKTIF",
    "YON YUKARI",
    "YON ASAGI",
    "YON SOL",
    "YON SAG"
};

static const uint8_t button_pins[] = {
    PIN_TRIGGER,
    PIN_RELOAD,
    PIN_SELECT_BOMB,
    PIN_COIN,
    PIN_START,
    PIN_MENU_CAL,
    PIN_ACTIVE,
    PIN_MENU_UP,
    PIN_MENU_DOWN,
    PIN_MENU_LEFT,
    PIN_MENU_RIGHT
};

static void setup_pin(uint pin) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
}

static bool pressed(uint pin) {
    return !gpio_get(pin);
}

static bool edge_pressed(uint pin) {
    bool p = pressed(pin);
    bool e = p && !last_pins[pin];
    last_pins[pin] = p;
    return e;
}

static uint16_t adc_avg(uint ch, uint samples) {
    adc_select_input(ch);
    uint32_t sum = 0;
    for (uint i = 0; i < samples; i++) {
        sum += adc_read();
        sleep_us(50);
    }
    return (uint16_t)(sum / samples);
}

static uint16_t map_adc(uint16_t v, uint16_t mn, uint16_t mx) {
    if (mx <= mn + 10) return 16384;
    if (v < mn) v = mn;
    if (v > mx) v = mx;
    return (uint16_t)(((uint32_t)(v - mn) * 32767u) / (uint32_t)(mx - mn));
}

static uint8_t filter_to_percent(void) {
    if (cfg.filter_level == 0) return 30;
    if (cfg.filter_level == 2) return 85;
    return 60;
}

static uint16_t smooth_value(uint16_t oldv, uint16_t newv) {
    uint8_t f = filter_to_percent();
    int diff = (int)newv - (int)oldv;
    int dead = 12 + (int)f * 3;
    if (diff > -dead && diff < dead) return oldv;
    uint32_t keep = f;
    uint32_t take = 100 - f;
    return (uint16_t)(((uint32_t)oldv * keep + (uint32_t)newv * take) / 100u);
}

static void show_saved(const char *line2) {
    lcd_print2("KAYDEDILDI", line2);
    screen = SCREEN_SAVED_MSG;
    saved_msg_until = to_ms_since_boot(get_absolute_time()) + 900;
}

static void update_normal_lcd(void) {
    char l1[17];
    char l2[17];
    snprintf(l1, sizeof(l1), "GT IO P%d LCD", PLAYER_ID);
    snprintf(l2, sizeof(l2), "GP19 MENU");
    lcd_print2(l1, l2);
}

static void update_main_menu_lcd(void) {
    char l1[17];
    char l2[17];
    snprintf(l1, sizeof(l1), "MENU P%d", PLAYER_ID);
    snprintf(l2, sizeof(l2), ">%s", menu_items[menu_index]);
    lcd_print2(l1, l2);
}

static void update_calibration_lcd(void) {
    const char *step_text[] = {
        "SOL USTE CEVIR",
        "SAG USTE CEVIR",
        "SAG ALTA CEVIR",
        "SOL ALTA CEVIR"
    };
    lcd_print2(step_text[cal_step], "GP6 KAYDET");
}

static void finish_calibration(void) {
    uint16_t xmin = cal_x[0], xmax = cal_x[0];
    uint16_t ymin = cal_y[0], ymax = cal_y[0];
    for (int i = 1; i < 4; i++) {
        if (cal_x[i] < xmin) xmin = cal_x[i];
        if (cal_x[i] > xmax) xmax = cal_x[i];
        if (cal_y[i] < ymin) ymin = cal_y[i];
        if (cal_y[i] > ymax) ymax = cal_y[i];
    }
    if (xmax > xmin + 50 && ymax > ymin + 50) {
        cfg.x_min = xmin;
        cfg.x_max = xmax;
        cfg.y_min = ymin;
        cfg.y_max = ymax;
        config_save(&cfg);
        show_saved("KALIBRASYON");
    } else {
        lcd_print2("HATA", "POTANS ARALIK");
        screen = SCREEN_SAVED_MSG;
        saved_msg_until = to_ms_since_boot(get_absolute_time()) + 1200;
    }
}

static void handle_calibration_input(void) {
    if (edge_pressed(PIN_SELECT_BOMB)) {
        cal_x[cal_step] = adc_avg(ADC_X_CH, 32);
        cal_y[cal_step] = adc_avg(ADC_Y_CH, 32);
        if (cal_step < 3) {
            cal_step++;
            update_calibration_lcd();
        } else {
            finish_calibration();
        }
    }
    if (edge_pressed(PIN_RELOAD)) {
        screen = SCREEN_MAIN_MENU;
        update_main_menu_lcd();
    }
}

static void update_button_test_lcd(void) {
    char l1[17];
    const char *state = pressed(button_pins[button_test_index]) ? "BASILDI" : "BASILMADI";
    snprintf(l1, sizeof(l1), "%s", button_names[button_test_index]);
    lcd_print2(l1, state);
}

static void update_aim_test_lcd(void) {
    char l1[17];
    char l2[17];
    uint16_t rawx = adc_avg(ADC_X_CH, 8);
    uint16_t rawy = adc_avg(ADC_Y_CH, 8);
    snprintf(l1, sizeof(l1), "X:%4u Y:%4u", rawx, rawy);
    snprintf(l2, sizeof(l2), "AKTIF:%s", pressed(PIN_ACTIVE) ? "EVET" : "HAYIR");
    lcd_print2(l1, l2);
}

static const char *filter_name(void) {
    if (cfg.filter_level == 0) return "AZ";
    if (cfg.filter_level == 2) return "YUKSEK";
    return "ORTA";
}

static void update_filter_lcd(void) {
    char l2[17];
    snprintf(l2, sizeof(l2), "SEVIYE:%s", filter_name());
    lcd_print2("TITRESIM AZALT", l2);
}

static void handle_menu(void) {
    if (edge_pressed(PIN_MENU_UP)) {
        menu_index = (menu_index == 0) ? 5 : menu_index - 1;
        update_main_menu_lcd();
    }
    if (edge_pressed(PIN_MENU_DOWN)) {
        menu_index = (menu_index + 1) % 6;
        update_main_menu_lcd();
    }
    if (edge_pressed(PIN_SELECT_BOMB)) {
        switch (menu_index) {
            case 0:
                cal_step = 0;
                screen = SCREEN_CALIBRATION;
                update_calibration_lcd();
                break;
            case 1:
                button_test_index = 0;
                screen = SCREEN_BUTTON_TEST;
                update_button_test_lcd();
                break;
            case 2:
                screen = SCREEN_AIM_TEST;
                update_aim_test_lcd();
                break;
            case 3:
                screen = SCREEN_FILTER;
                update_filter_lcd();
                break;
            case 4:
                config_save(&cfg);
                show_saved("AYARLAR");
                break;
            default:
                screen = SCREEN_NORMAL;
                update_normal_lcd();
                break;
        }
    }
    if (edge_pressed(PIN_RELOAD)) {
        screen = SCREEN_NORMAL;
        update_normal_lcd();
    }
}

static void handle_button_test(void) {
    if (edge_pressed(PIN_MENU_UP)) {
        button_test_index = (button_test_index == 0) ? 10 : button_test_index - 1;
        update_button_test_lcd();
    }
    if (edge_pressed(PIN_MENU_DOWN)) {
        button_test_index = (button_test_index + 1) % 11;
        update_button_test_lcd();
    }
    if (edge_pressed(PIN_RELOAD)) {
        screen = SCREEN_MAIN_MENU;
        update_main_menu_lcd();
    }
}

static void handle_filter(void) {
    if (edge_pressed(PIN_MENU_LEFT) && cfg.filter_level > 0) {
        cfg.filter_level--;
        update_filter_lcd();
    }
    if (edge_pressed(PIN_MENU_RIGHT) && cfg.filter_level < 2) {
        cfg.filter_level++;
        update_filter_lcd();
    }
    if (edge_pressed(PIN_SELECT_BOMB)) {
        config_save(&cfg);
        show_saved("TITRESIM");
    }
    if (edge_pressed(PIN_RELOAD)) {
        screen = SCREEN_MAIN_MENU;
        update_main_menu_lcd();
    }
}

static void handle_aim_test(void) {
    if (edge_pressed(PIN_RELOAD)) {
        screen = SCREEN_MAIN_MENU;
        update_main_menu_lcd();
    }
}

static void ui_task(void) {
    if (screen == SCREEN_NORMAL && edge_pressed(PIN_MENU_CAL)) {
        screen = SCREEN_MAIN_MENU;
        menu_index = 0;
        update_main_menu_lcd();
        return;
    }

    switch (screen) {
        case SCREEN_MAIN_MENU:
            handle_menu();
            break;
        case SCREEN_CALIBRATION:
            handle_calibration_input();
            break;
        case SCREEN_BUTTON_TEST:
            handle_button_test();
            break;
        case SCREEN_AIM_TEST:
            handle_aim_test();
            break;
        case SCREEN_FILTER:
            handle_filter();
            break;
        case SCREEN_SAVED_MSG:
            if (to_ms_since_boot(get_absolute_time()) > saved_msg_until) {
                screen = SCREEN_MAIN_MENU;
                update_main_menu_lcd();
            }
            break;
        default:
            break;
    }
}

static void lcd_refresh_task(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - last_lcd_ms < 250) return;
    last_lcd_ms = now;

    if (screen == SCREEN_BUTTON_TEST) update_button_test_lcd();
    if (screen == SCREEN_AIM_TEST) update_aim_test_lcd();
}

static void send_keyboard_report(void) {
    uint8_t keycode[6] = {0};
    uint8_t mod = 0;
    uint8_t n = 0;
    bool active = pressed(PIN_ACTIVE);

    if (screen != SCREEN_NORMAL) {
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
        return;
    }

    if (pressed(PIN_MENU_UP) && n < 6) keycode[n++] = KEY_ARROW_UP;
    if (pressed(PIN_MENU_DOWN) && n < 6) keycode[n++] = KEY_ARROW_DOWN;
    if (pressed(PIN_MENU_LEFT) && n < 6) keycode[n++] = KEY_ARROW_LEFT;
    if (pressed(PIN_MENU_RIGHT) && n < 6) keycode[n++] = KEY_ARROW_RIGHT;
    if (pressed(PIN_COIN) && n < 6) keycode[n++] = (PLAYER_ID == 1) ? KEY_KEYPAD_5 : KEY_KEYPAD_6;
    if (pressed(PIN_START) && n < 6) keycode[n++] = (PLAYER_ID == 1) ? KEY_KEYPAD_1 : KEY_KEYPAD_2;

    if (active) {
        if (pressed(PIN_SELECT_BOMB) && n < 6) keycode[n++] = (PLAYER_ID == 1) ? KEY_SPACE : KEY_RIGHT_CTRL;
        if (pressed(PIN_RELOAD) && n < 6) keycode[n++] = (PLAYER_ID == 1) ? KEY_R : KEY_T;
    }

    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, mod, keycode);
}

static void send_mouse_report(void) {
    uint8_t buttons = 0;
    bool active = pressed(PIN_ACTIVE);

    if (screen == SCREEN_NORMAL && active) {
        uint16_t rawx = adc_avg(ADC_X_CH, 16);
        uint16_t rawy = adc_avg(ADC_Y_CH, 16);
        uint16_t x = map_adc(rawx, cfg.x_min, cfg.x_max);
        uint16_t y = map_adc(rawy, cfg.y_min, cfg.y_max);
        last_x = smooth_value(last_x, x);
        last_y = smooth_value(last_y, y);

        if (pressed(PIN_TRIGGER)) buttons |= 0x01;
        if (pressed(PIN_SELECT_BOMB)) buttons |= 0x02;
    }

    last_buttons = buttons;
    uint8_t report[5] = {
        last_buttons,
        (uint8_t)(last_x & 0xFF),
        (uint8_t)(last_x >> 8),
        (uint8_t)(last_y & 0xFF),
        (uint8_t)(last_y >> 8)
    };

    tud_hid_report(REPORT_ID_MOUSE, report, sizeof(report));
}

static void hid_task(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (!tud_hid_ready()) return;

    if (now - last_hid_ms >= 5) {
        last_hid_ms = now;
        send_mouse_report();
    }

    if (now - last_key_ms >= 20) {
        last_key_ms = now;
        send_keyboard_report();
    }
}

int main(void) {
    stdio_init_all();

    adc_init();
    adc_gpio_init(ADC_X_GPIO);
    adc_gpio_init(ADC_Y_GPIO);

    setup_pin(PIN_MENU_UP);
    setup_pin(PIN_MENU_DOWN);
    setup_pin(PIN_MENU_LEFT);
    setup_pin(PIN_MENU_RIGHT);
    setup_pin(PIN_SELECT_BOMB);
    setup_pin(PIN_TRIGGER);
    setup_pin(PIN_RELOAD);
    setup_pin(PIN_COIN);
    setup_pin(PIN_START);
    setup_pin(PIN_MENU_CAL);
    setup_pin(PIN_ACTIVE);

    config_load(&cfg);

    lcd_init_auto();
    if (lcd_is_present()) {
        char line1[17];
        snprintf(line1, sizeof(line1), "GT IO P%d V0.02", PLAYER_ID);
        lcd_print2(line1, "LCD HAZIR");
        sleep_ms(900);
        update_normal_lcd();
    }

    tusb_init();

    while (true) {
        tud_task();
        ui_task();
        lcd_refresh_task();
        hid_task();
        sleep_ms(1);
    }
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize) {
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen) {
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}
