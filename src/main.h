#ifndef MAIN_H
#define MAIN_H MAIN_H

#include <pebble.h>

#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))

typedef enum  {
  MSG_NOW_TEXT = 0,
  MSG_NOW_COLOR = 1,
  MSG_NOW_BACKGROUND = 2,
  MSG_NOW_FONT = 3,
  MSG_NOW_BATTERY_SHOW = 4,
  MSG_NOW_BATTERY_BLINKS = 5,
  MSG_CLOCK_COLOR = 6,
  MSG_CLOCK_BACKGROUND = 7,
  MSG_CLOCK_FONT = 8,
  MSG_CLOCK_DATE_FONT = 9,
  MSG_CLOCK_BATTERY_SHOW = 10,
  MSG_CLOCK_BATTERY_BLINKS = 11,
  MSG_CLOCK_DELAY = 12,
  MSG_NOW_BATTERY_COLOR = 13,
  MSG_NOW_BATTERY_LOW = 14,
  MSG_CLOCK_BATTERY_COLOR = 15,
  MSG_CLOCK_BATTERY_LOW = 16,
} AppMessageKeys;

typedef enum  {
  STORE_NOW_TEXT = 0,
  STORE_NOW_COLOR = 1,
  STORE_NOW_BACKGROUND = 2,
  STORE_NOW_FONT = 3,
  STORE_NOW_BATTERY_DRAW = 4,
  STORE_NOW_BATTERY_LOW_ONLY = 5,
  STORE_NOW_BATTERY_BLINKS = 6,
  STORE_CLOCK_COLOR = 7,
  STORE_CLOCK_BACKGROUND = 8,
  STORE_CLOCK_FONT = 9,
  STORE_CLOCK_DATE_FONT = 10,
  STORE_CLOCK_BATTERY_DRAW = 11,
  STORE_CLOCK_BATTERY_LOW_ONLY = 12,
  STORE_CLOCK_BATTERY_BLINKS = 13,
  STORE_CLOCK_DELAY = 14,
  STORE_NOW_BATTERY_COLOR = 15,
  STORE_NOW_BATTERY_LOW = 16,
  STORE_CLOCK_BATTERY_COLOR = 17,
  STORE_CLOCK_BATTERY_LOW = 18,
} StorageKeys;

typedef struct {
    char               font[64];
    GColor             color,
                       background;
    GTextOverflowMode  overflow;
    GTextAlignment     alignment;
  } TextStyle;

typedef struct {
    GColor     normal,
               low;
    bool       draw,
               low_only,
               low_blink,
               blink_state;
    uint32_t   blink_delay;
    AppTimer  *blink_timer;
  } BatteryStyle;

typedef struct {
  char  lang[6],
        datestr[32];
  } i18n_datedef;


// UI helpers
void        layer_center_vertical(Layer *layer, Layer *parent, Layer* children[]);
GFont       get_font(const char* name);
void        update_now();

// clock display
const char* get_localized_date_format();
void        update_clock();
void        handle_clock_show_timeout();

// battery symbol
bool        battery_draw(const char* value);
bool        battery_low_only(const char* value);
void        blink_battery(void* layer);
void        draw_battery(Layer *layer, GContext *ctx);
void        handle_battery_state();
void        notify_battery_layer(Layer *layer);

// settings management
void        in_received_handler(DictionaryIterator *iter, void *context);
void        load_config();
void        save_config();

// events
void        handle_tap(AccelAxisType axis, int32_t direction);

// app management
void        handle_init(void);
void        handle_deinit(void);
int         main(void);

#endif
