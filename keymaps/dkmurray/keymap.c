/* Copyright 2020 Jay Greco
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include QMK_KEYBOARD_H
#include "action_layer.h"
#include "14seg_animation.h"
#include "remote_kb.h"
#include "bitc_led.h"

#define _BASE     0
#define _FUNC     1
#define _SETT      2

#define DISP_ADDR 0x70
HT16K33 *disp;
animation_t *animation;
char message[16];

enum custom_keycodes {
  PROG = SAFE_RANGE,
};

enum td_keycodes {
    TD_ENTER_LAYER
};

// Tap Dance definitions
qk_tap_dance_action_t tap_dance_actions[] = {
    // Tap once for KP_ENTER, twice for _FUNC layer
    [TD_ENTER_LAYER] = ACTION_TAP_DANCE_LAYER_TOGGLE(KC_KP_ENTER, 1),
};

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
  // Base layer (numpad)
    [_BASE] = LAYOUT(
              _______,    _______,    _______, \
  KC__MUTE,   KC_KP_7,  KC_KP_8,  KC_KP_9, \
  KC_PMNS,    KC_KP_4,  KC_KP_5,  KC_KP_6, \
  KC_PPLS,    KC_KP_1,  KC_KP_2,  KC_KP_3, \
  MO(_FUNC),  KC_KP_0,  KC_KP_DOT,KC_PENT\
  ),
  // Function layer (numpad)
  [_FUNC] = LAYOUT(
              _______,   _______,   _______,
    KC_NO,    KC_NO,    KC_NO, KC_BSPC,
    KC_PSLS,  KC_NO,    KC_NO, KC_NO,
    KC_PAST,  KC_NO,    KC_NO, KC_NO,
    KC_NO,    MO(_SETT),KC_NO, KC_NO
  ),
  // Settings layer (numpad)
  [_SETT] = LAYOUT(
              _______,   _______,   _______,
    KC_NO,    RGB_HUD,  RGB_HUI, RGB_MOD,
    RGB_TOG,  RGB_SAD,  RGB_SAI, RGB_RMOD,
    RGB_M_P,  RGB_VAD,  RGB_VAI, KC_NO,
    KC_NO,    KC_NO,    PROG,    KC_NO
  ),
};

#ifdef OLED_DRIVER_ENABLE
oled_rotation_t oled_init_user(oled_rotation_t rotation) { return OLED_ROTATION_270; }

static void render_logo(void) {
  static const char PROGMEM logo[] = {
  0x3c, 0x3c, 0xc3, 0xc3, 0xc3, 0xc3, 0x03, 0x03, 0x03, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 
  0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x03, 0x03, 0x03, 0xc3, 0xc3, 0xc3, 0xc3, 0x3c, 0x3c, 
  0xfc, 0xfc, 0x03, 0x03, 0x00, 0x00, 0x0f, 0x0f, 0x0f, 0xcf, 0xcf, 0x30, 0x30, 0x30, 0x30, 0x30, 
  0x30, 0x30, 0x30, 0x30, 0x30, 0xcf, 0xcf, 0x0f, 0x0f, 0x0f, 0x00, 0x00, 0x03, 0x03, 0xfc, 0xfc, 
  0x0f, 0x0f, 0xf0, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x0f, 0x30, 0x30, 0x33, 0x33, 0x30, 
  0x30, 0x33, 0x33, 0x30, 0x30, 0x0f, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xf0, 0x0f, 0x0f, 
  0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xc0, 0xc0, 0xc0, 0xc3, 0xc3, 0xfc, 0xfc, 0x0c, 0x0c, 0x0c, 
  0x0c, 0x0c, 0x0c, 0xfc, 0xfc, 0xc3, 0xc3, 0xc0, 0xc0, 0xc0, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00
  };
    // Host Keyboard Layer Status
    oled_write_raw_P(logo, sizeof(logo));
}

// WPM-responsive animation stuff here
#define IDLE_FRAMES 2
#define IDLE_SPEED 40 // below this wpm value your animation will idle

#define ANIM_FRAME_DURATION 200 // how long each frame lasts in ms
// #define SLEEP_TIMER 60000 // should sleep after this period of 0 wpm, needs fixing
#define ANIM_SIZE 636 // number of bytes in array, minimize for adequate firmware size, max is 1024

uint32_t anim_timer = 0;
uint32_t anim_sleep = 0;
uint8_t current_idle_frame = 0;

// Credit to u/Pop-X- for the initial code. You can find his commit here https://github.com/qmk/qmk_firmware/pull/9264/files#diff-303f6e3a7a5ee54be0a9a13630842956R196-R333.
// static void render_anim(void) {
//     static const char PROGMEM idle[IDLE_FRAMES][ANIM_SIZE] = {
//         {
//         0,  0,192,192,192,192,192,192,192,248,248, 30, 30,254,254,248,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  3,  3,  3,  3,  3,255,255,255,255,255,255,255,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,127,127,255,255,255,255,255,159,159,135,135,129,129,129, 97, 97, 25, 25,  7,  7,  1,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1, 97, 97,127,  1,  1, 97, 97,127,  1,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0
//         },
//         {
//         0,  0,128,128,128,128,128,128,128,240,240, 60, 60,252,252,240,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  7,  7,  7,  7,  7,255,255,254,254,255,255,255,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,255,255,255,255,255,255,255, 63, 63, 15, 15,  3,  3,  3,195,195, 51, 51, 15, 15,  3,  3,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  3,  3, 99, 99,127,  3,  3, 99, 99,127,  3,  3,  0,  0,  0,  0,  0,  0,  0,  0,  0
//         }
//     };

//     //assumes 1 frame prep stage
//     void animation_phase(void) {
//             current_idle_frame = (current_idle_frame + 1) % IDLE_FRAMES;
//             oled_write_raw_P(idle[abs((IDLE_FRAMES-1)-current_idle_frame)], ANIM_SIZE);
//     }

//         if(timer_elapsed32(anim_timer) > ANIM_FRAME_DURATION) {
//             anim_timer = timer_read32();
//             animation_phase();
//         }
//     }

void oled_task_user(void) {
    // oled_set_cursor(0,1);
    render_logo();
    // render_anim();
    oled_set_cursor(0,5);
    oled_write_P(PSTR("TROY\nPAD\n"), false);
    oled_write_P(PSTR("-----\n"), false);
    // Host Keyboard Layer Status
    oled_write_P(PSTR("MODE\n"), false);
    // 

    switch (get_highest_layer(layer_state)) {
        case _BASE:
            oled_write_P(PSTR("BASE\n"), false);
            oled_write_P(PSTR("\n"), false);
            oled_write_P(PSTR("m789\n"), false);
            oled_write_P(PSTR("-456\n"), false);
            oled_write_P(PSTR("+123\n"), false);
            oled_write_P(PSTR("f0.E\n"), false);
            break;
        case _FUNC:
            oled_write_P(PSTR("FUNC\n"), false);
            oled_write_P(PSTR("\n"), false);
            oled_write_P(PSTR("___b\n"), false);
            oled_write_P(PSTR("/___\n"), false);
            oled_write_P(PSTR("*___\n"), false);
            oled_write_P(PSTR("_S__\n"), false);
            break;
        case _SETT:
            oled_write_P(PSTR("SETT\n"), false);
            oled_write_P(PSTR(" -+ \n"), false);
            oled_write_P(PSTR("_HHM\n"), false);
            oled_write_P(PSTR("TSSM\n"), false);
            oled_write_P(PSTR("pVV_\n"), false);
            oled_write_P(PSTR("__P_\n"), false);
            break;
    }
}
#endif

void matrix_init_user(void) {
  matrix_init_remote_kb();
  set_bitc_LED(LED_OFF);

  disp = newHT16K33(4, DISP_ADDR);

  animation = newAnimation(disp);
  animation->message = message;
  animation->mode = DISP_MODE_BOUNCE;
  animation->enabled = true;

  char tmp[] = "    ";
  strcpy(message, tmp);
  register_code(KC_NLCK);
}

#ifdef RGBLIGHT_ENABLE
void keyboard_post_init_user(void) {
  // rgblight_enable_noeeprom(); // Enables RGB, without saving settings
  rgblight_sethsv_noeeprom(213, 128, 128); // should be roughly rgb 0x800080
  rgblight_disable_noeeprom();
}
#endif

void matrix_scan_user(void) {
  matrix_scan_remote_kb();
  animation_refresh(animation);
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
  process_record_remote_kb(keycode, record);

  switch(keycode) {
    case PROG:
      if (record->event.pressed) {
        char tmp[] = "PROG";
        strcpy(message, tmp);
        force_animation_refresh(animation); //force 14-seg refresh
        set_bitc_LED(LED_DIM);
        rgblight_disable_noeeprom();
        bootloader_jump(); //jump to bootloader
      }
    break;

    default:
    break;
  }
  return true;
}

void encoder_update_user(uint8_t index, bool clockwise) {
  if (clockwise) {
    tap_code(KC_VOLU);
  } else {
    tap_code(KC_VOLD);
  }  
}

layer_state_t layer_state_set_user(layer_state_t state) {
    switch (get_highest_layer(state)) {
    case _FUNC:
      {
        char tmp[] = "FUNC";
        strcpy(message, tmp);
        force_animation_refresh(animation); //force refresh
        break;
      }
    case _SETT:
      {
        char tmp[] = "SETTINGS";
        strcpy(message, tmp);
        force_animation_refresh(animation); //force refresh
        break;
      }
    default: //  for any other layers, or the default layer
      {
        char tmp[] = "    ";
        strcpy(message, tmp);
        force_animation_refresh(animation); //force refresh
        break;
      }
    }
  return state;
}

void led_set_kb(uint8_t usb_led) {
  if (usb_led & (1<<USB_LED_NUM_LOCK))
    set_bitc_LED(LED_DIM);
  else
    set_bitc_LED(LED_OFF);
}