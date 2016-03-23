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

const i18n_datedef  datedefs[] = {
  {"de_DE", "%a, %e. %B %Y"},
  {"fr_FR", "%a, %e %B %Y"},
  {"es_ES", "%a, %e de %B de %Y"},
  {"it_IT", "%a, %e %B %Y"},
  {"pt_PT", "%a, %e de %B de %Y"},
  {"en_CN", "%a, %B %e, %Y"},
  {"en_TW", "%a, %B %e, %Y"},
  {"en_US", "%a, %B %e, %Y"},
  {"DEF",   "%a, %B %e, %Y"}
};



char              *sys_locale;
char               now_text[256];

Window            *now_window;
Layer             *now_center_layer;
TextLayer         *now_text_layer;
TextStyle          now_text_style;
Layer             *now_battery_layer;
BatteryStyle       now_battery_style;

char               clock_text[16],
                   clock_date[64];

Window            *clock_window;
Layer             *clock_center_layer;
Layer             *clock_battery_layer;
BatteryStyle       clock_battery_style;
TextLayer         *clock_text_layer;
TextStyle          clock_text_style;
TextLayer         *clock_date_layer;
TextStyle          clock_date_style;
bool               clock_show;
unsigned long int  clock_show_timeout;
AppTimer          *clock_show_timer;

void blink_battery(void* layer) {
  layer_mark_dirty((Layer*)layer);
}

void draw_battery(Layer *layer, GContext *ctx) {
  GRect                     bounds = layer_get_bounds(layer);
  const int16_t             nip_space = (bounds.size.h * 35) / 100;
  BatteryStyle             *style = *((BatteryStyle**)layer_get_data(layer));
  const BatteryChargeState  state = battery_state_service_peek();

  if(style->blink_timer) {
    app_timer_cancel(style->blink_timer);
    style->blink_timer = NULL;
  }
  
  if ( ! style->draw)
    return;
  
  if (state.charge_percent <= 10) {
    if(style->low_blink) {
      style->blink_state = ! style->blink_state;
      style->blink_timer = app_timer_register(style->blink_delay, &blink_battery, layer);
    }
    
    graphics_context_set_stroke_color(ctx, style->low);
    graphics_context_set_fill_color(ctx, style->low);
    if (( ! style->low_blink) || style->blink_state) {
      if (style->low_blink)
        graphics_fill_rect(ctx, GRect(bounds.origin.x, bounds.origin.y, bounds.size.w * 90 / 100, bounds.size.h), 0, GCornerNone);
      else
        graphics_draw_rect(ctx, GRect(bounds.origin.x, bounds.origin.y, bounds.size.w * 90 / 100, bounds.size.h));
      graphics_fill_rect(ctx, GRect(bounds.origin.x + (bounds.size.w * 90 / 100), bounds.origin.y + nip_space, bounds.size.w * 10 / 100, bounds.size.h - (2 * nip_space)), 2, GCornersRight);
    }
  } else if ( ! style->low_only) {
    graphics_context_set_stroke_color(ctx, style->normal);
    graphics_context_set_fill_color(ctx, style->normal);
    graphics_draw_rect(ctx, GRect(bounds.origin.x, bounds.origin.y, bounds.size.w * 90 / 100, bounds.size.h));
    graphics_fill_rect(ctx, GRect(bounds.origin.x + (bounds.size.w * 90 / 100), bounds.origin.y + nip_space, bounds.size.w * 10 / 100, bounds.size.h - (2 * nip_space)), 2, GCornersRight);
    for(int16_t i = 3; i <= MAX(4, (bounds.size.w * 90 * state.charge_percent / (100 * 100))); i += 4) {
      graphics_fill_rect(ctx, GRect(bounds.origin.x + i, bounds.origin.y + 2, 2, bounds.size.h - 4), 2, GCornersAll);
    }
  }
}

void layer_center_vertical(Layer *layer, Layer *parent, Layer* children[]) {
  GRect bounds_parent = layer_get_bounds(parent);
  GSize content = { 0, 0 };
  
  for(uint8_t i = 0; children[i] != NULL; i++) {
    GRect frame = layer_get_frame(children[i]);
    layer_set_frame(children[i], GRect(0, content.h, bounds_parent.size.w, content.h + frame.size.h));
    content.h = (content.h ? content.h + 5: content.h) + frame.size.h;
  }
  
  layer_set_frame(layer, GRect(bounds_parent.origin.x,
                               bounds_parent.origin.y + ((bounds_parent.size.h - content.h) / 2) - 5,
                               bounds_parent.size.w,
                               content.h));
}

void update_text(TextLayer *layer, Layer *parent, const TextStyle *style, const char *text) {
  GRect bounds = layer_get_bounds(parent);
  GSize size = graphics_text_layout_get_content_size(text,
                                                     fonts_get_system_font(style->font),
                                                     GRect(bounds.origin.x, 0, bounds.size.w, 32767),
                                                     style->overflow,
                                                     style->alignment
                                                    );
  layer_set_frame((Layer*)layer, GRect(0, 0, bounds.size.w, size.h));
  text_layer_set_text(layer, text);
  text_layer_set_font(layer, fonts_get_system_font(style->font));
  text_layer_set_overflow_mode(layer, style->overflow);
  text_layer_set_text_alignment(layer, style->alignment);
  text_layer_set_text_color(layer, style->color);
  text_layer_set_background_color(layer, style->background);
}

void update_now() {
  Layer* layers[] = {
    (Layer*)now_text_layer,
    NULL
  };
  
  window_set_background_color(now_window, now_text_style.background);
  update_text(now_text_layer, now_center_layer, &now_text_style, now_text);
  layer_center_vertical(now_center_layer, window_get_root_layer(now_window), layers);
  
  layer_mark_dirty(window_get_root_layer(now_window));
}

const char* get_localized_date_format() {
  const char *format = NULL,
             *locale = i18n_get_system_locale();
  
  if (strcmp(locale, sys_locale) != 0) {
    sys_locale = setlocale(LC_ALL, "");
  }
    
  for(uint8_t i=0;; i++) {
    format = datedefs[i].datestr;
    if ((strcmp(datedefs[i].lang, locale) == 0) || (strcmp(datedefs[i].lang, "DEF") == 0))
      break;
  }
  return format;
}

void update_clock() {
  Layer* layers[] = {
    (Layer*)clock_text_layer,
    (Layer*)clock_date_layer,
    NULL
  };

  time_t temp = time(NULL);
  struct tm *t = localtime(&temp);

  clock_copy_time_string(clock_text, sizeof(clock_text));
  strftime(clock_date, sizeof(clock_date), get_localized_date_format(), t);
  
  window_set_background_color(clock_window, clock_text_style.background);
  update_text(clock_text_layer, clock_center_layer, &clock_text_style, clock_text);
  update_text(clock_date_layer, clock_center_layer, &clock_date_style, clock_date);
  layer_center_vertical(clock_center_layer, window_get_root_layer(clock_window), layers);
  
  layer_mark_dirty(window_get_root_layer(clock_window));
}

void handle_clock_show_timeout() {
  if(clock_show) {
    window_stack_remove(clock_window, false);
    if(clock_show_timer) {
      app_timer_cancel(clock_show_timer);
      clock_show_timer = NULL;
    }
    clock_show = false;
  }
}

void handle_tap(AccelAxisType axis, int32_t direction) {
  if (!clock_show) {
    update_clock();
    window_stack_push(clock_window, false);
    clock_show = true;
    clock_show_timer = app_timer_register(clock_show_timeout, &handle_clock_show_timeout, NULL);
  } else if (clock_show_timer) {
    update_clock();
    app_timer_reschedule(clock_show_timer, clock_show_timeout);
  }
}

void notify_battery_layer(Layer *layer) {
  BatteryStyle *style = NULL;

  if(now_battery_layer) {
    style = (BatteryStyle*)layer_get_data(layer);
    if(style->low_blink && ( ! style->blink_timer ))
      layer_mark_dirty(layer);
  }
}

void handle_battery_state() {
  notify_battery_layer(now_battery_layer);
  notify_battery_layer(clock_battery_layer);
}

const char* get_font(const char* name) {
  if (strncmp("GOTHIC_14", name, 32) == 0) { return FONT_KEY_GOTHIC_14; };
  if (strncmp("GOTHIC_14_BOLD", name, 32) == 0) { return FONT_KEY_GOTHIC_14_BOLD; };
  if (strncmp("GOTHIC_18", name, 32) == 0) { return FONT_KEY_GOTHIC_18; };
  if (strncmp("GOTHIC_18_BOLD", name, 32) == 0) { return FONT_KEY_GOTHIC_18_BOLD; };
  if (strncmp("GOTHIC_24", name, 32) == 0) { return FONT_KEY_GOTHIC_24; };
  if (strncmp("GOTHIC_24_BOLD", name, 32) == 0) { return FONT_KEY_GOTHIC_24_BOLD; };
  if (strncmp("GOTHIC_28", name, 32) == 0) { return FONT_KEY_GOTHIC_28; };
  if (strncmp("GOTHIC_28_BOLD", name, 32) == 0) { return FONT_KEY_GOTHIC_28_BOLD; };
  if (strncmp("BITHAM_30_BLACK", name, 32) == 0) { return FONT_KEY_BITHAM_30_BLACK; };
  if (strncmp("BITHAM_34_MEDIUM_NUMBERS", name, 32) == 0) { return FONT_KEY_BITHAM_34_MEDIUM_NUMBERS; };
  if (strncmp("BITHAM_42_BOLD", name, 32) == 0) { return FONT_KEY_BITHAM_42_BOLD; };
  if (strncmp("BITHAM_42_LIGHT", name, 32) == 0) { return FONT_KEY_BITHAM_42_LIGHT; };
  if (strncmp("BITHAM_42_MEDIUM_NUMBERS", name, 32) == 0) { return FONT_KEY_BITHAM_42_MEDIUM_NUMBERS; };
  if (strncmp("ROBOTO_CONDENSED_21", name, 32) == 0) { return FONT_KEY_ROBOTO_CONDENSED_21; };
  if (strncmp("ROBOTO_BOLD_SUBSET_49", name, 32) == 0) { return FONT_KEY_ROBOTO_BOLD_SUBSET_49; };
  if (strncmp("DROID_SERIF_28_BOLD", name, 32) == 0) { return FONT_KEY_DROID_SERIF_28_BOLD; };
  if (strncmp("LECO_20_BOLD_NUMBERS", name, 32) == 0) { return FONT_KEY_LECO_20_BOLD_NUMBERS; };
  if (strncmp("LECO_26_BOLD_NUMBERS_AM_PM", name, 32) == 0) { return FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM; };
  if (strncmp("LECO_28_LIGHT_NUMBERS", name, 32) == 0) { return FONT_KEY_LECO_28_LIGHT_NUMBERS; };
  if (strncmp("LECO_32_BOLD_NUMBERS", name, 32) == 0) { return FONT_KEY_LECO_32_BOLD_NUMBERS; };
  if (strncmp("LECO_36_BOLD_NUMBERS", name, 32) == 0) { return FONT_KEY_LECO_36_BOLD_NUMBERS; };
  if (strncmp("LECO_38_BOLD_NUMBERS", name, 32) == 0) { return FONT_KEY_LECO_38_BOLD_NUMBERS; };
  if (strncmp("LECO_42_NUMBERS", name, 32) == 0) { return FONT_KEY_LECO_42_NUMBERS; };
  return FONT_KEY_GOTHIC_18;
}

void load_config() {
  char  font[256];
  
  if (persist_exists(STORE_NOW_TEXT))
    persist_read_string(STORE_NOW_TEXT, now_text, sizeof(now_text));
  else
    strncpy(now_text, "NOW", sizeof(now_text));
  
  now_battery_style.normal.argb = persist_exists(STORE_NOW_BATTERY_COLOR) ? persist_read_int(STORE_NOW_BATTERY_COLOR) : GColorFromRGB(255,255,255).argb;
  now_battery_style.low.argb    = persist_exists(STORE_NOW_BATTERY_LOW) ? persist_read_int(STORE_NOW_BATTERY_LOW) : GColorFromRGB(255,0,0).argb;
  now_battery_style.draw        = persist_exists(STORE_NOW_BATTERY_DRAW) ? persist_read_bool(STORE_NOW_BATTERY_DRAW) : true;
  now_battery_style.low_only    = persist_exists(STORE_NOW_BATTERY_LOW_ONLY) ? persist_read_bool(STORE_NOW_BATTERY_LOW_ONLY) : true;
  now_battery_style.low_blink   = persist_exists(STORE_NOW_BATTERY_BLINKS) ? persist_read_bool(STORE_NOW_BATTERY_BLINKS) : true;
  now_battery_style.blink_state = false;
  now_battery_style.blink_delay = 1000;
  
  if (persist_exists(STORE_NOW_FONT))
    persist_read_string(STORE_NOW_FONT, now_text_style.font, sizeof(now_text_style.font));
  else
    strncpy(now_text_style.font, FONT_KEY_GOTHIC_28_BOLD, sizeof(now_text_style.font));
  now_text_style.color.argb       = persist_exists(STORE_NOW_COLOR) ? persist_read_int(STORE_NOW_COLOR) : GColorFromRGB(255,255,255).argb;
  now_text_style.background.argb  = persist_exists(STORE_NOW_BACKGROUND) ? persist_read_int(STORE_NOW_BACKGROUND) : GColorFromRGB(0,0,0).argb;
  now_text_style.overflow         = GTextOverflowModeWordWrap;
  now_text_style.alignment        = GTextAlignmentCenter;
  
  clock_battery_style.normal.argb = persist_exists(STORE_CLOCK_BATTERY_COLOR) ? persist_read_int(STORE_CLOCK_BATTERY_COLOR) : GColorFromRGB(255,255,255).argb;
  clock_battery_style.low.argb    = persist_exists(STORE_CLOCK_BATTERY_LOW) ? persist_read_int(STORE_CLOCK_BATTERY_LOW) : GColorFromRGB(255,0,0).argb;
  clock_battery_style.draw        = persist_exists(STORE_CLOCK_BATTERY_DRAW) ? persist_read_bool(STORE_CLOCK_BATTERY_DRAW) : true;
  clock_battery_style.low_only    = persist_exists(STORE_CLOCK_BATTERY_LOW_ONLY) ? persist_read_bool(STORE_CLOCK_BATTERY_LOW_ONLY) : false;
  clock_battery_style.low_blink   = persist_exists(STORE_CLOCK_BATTERY_BLINKS) ? persist_read_bool(STORE_CLOCK_BATTERY_BLINKS) : true;
  clock_battery_style.blink_state = false;
  clock_battery_style.blink_delay = 1000;

  if (persist_exists(STORE_CLOCK_FONT))
    persist_read_string(STORE_CLOCK_FONT, clock_text_style.font, sizeof(clock_text_style.font));
  else
    strncpy(clock_text_style.font, FONT_KEY_BITHAM_42_MEDIUM_NUMBERS, sizeof(clock_text_style.font));
  clock_text_style.color.argb      = persist_exists(STORE_CLOCK_COLOR) ? persist_read_int(STORE_CLOCK_COLOR) : GColorFromRGB(255,255,255).argb;
  clock_text_style.background.argb = persist_exists(STORE_CLOCK_BACKGROUND) ? persist_read_int(STORE_CLOCK_BACKGROUND) : GColorFromRGB(0,0,0).argb;
  clock_text_style.overflow        = GTextOverflowModeWordWrap;
  clock_text_style.alignment       = GTextAlignmentCenter;

  if (persist_exists(STORE_CLOCK_DATE_FONT))
    persist_read_string(STORE_CLOCK_DATE_FONT, clock_date_style.font, sizeof(clock_date_style.font));
  else
    strncpy(clock_date_style.font, FONT_KEY_GOTHIC_18, sizeof(clock_date_style.font));
  clock_date_style.color          = clock_text_style.color;
  clock_date_style.background     = clock_text_style.background;
  clock_date_style.overflow       = GTextOverflowModeWordWrap;
  clock_date_style.alignment      = GTextAlignmentCenter;

  clock_show                      = false;
  clock_show_timeout              = persist_exists(STORE_CLOCK_DELAY) ? persist_read_int(STORE_CLOCK_DELAY) : 4000;
  clock_show_timer                = NULL;
}

void save_config() {
  persist_write_string(STORE_NOW_TEXT, now_text);
  
  persist_write_int(STORE_NOW_BATTERY_COLOR, now_battery_style.normal.argb);
  persist_write_int(STORE_NOW_BATTERY_LOW, now_battery_style.low.argb);
  persist_write_bool(STORE_NOW_BATTERY_DRAW, now_battery_style.draw);
  persist_write_bool(STORE_NOW_BATTERY_LOW_ONLY, now_battery_style.low_only);
  persist_write_bool(STORE_NOW_BATTERY_BLINKS, now_battery_style.low_blink);
    
  persist_write_string(STORE_NOW_FONT, now_text_style.font);
  persist_write_int(STORE_NOW_COLOR, now_text_style.color.argb);
  persist_write_int(STORE_NOW_BACKGROUND, now_text_style.background.argb);
  
  persist_write_int(STORE_CLOCK_BATTERY_COLOR, clock_battery_style.normal.argb);
  persist_write_int(STORE_CLOCK_BATTERY_LOW, clock_battery_style.low.argb);
  persist_write_bool(STORE_CLOCK_BATTERY_DRAW, clock_battery_style.draw);
  persist_write_bool(STORE_CLOCK_BATTERY_LOW_ONLY, clock_battery_style.low_only);
  persist_write_bool(STORE_CLOCK_BATTERY_BLINKS, clock_battery_style.low_blink);

  persist_write_string(STORE_CLOCK_FONT, clock_text_style.font);
  persist_write_int(STORE_CLOCK_COLOR, clock_text_style.color.argb);
  persist_write_int(STORE_CLOCK_BACKGROUND, clock_text_style.background.argb);

  persist_write_string(STORE_CLOCK_DATE_FONT, clock_date_style.font);

  persist_write_int(STORE_CLOCK_DELAY, clock_show_timeout);
}

bool battery_draw(const char* value) {
  if ((strncmp("ALWAYS", value, 8) == 0) || (strncmp("LOW", value, 8) == 0))
    return true;
  else
    return false;
}

bool battery_low_only(const char* value) {
  if (strncmp("LOW", value, 8) == 0)
    return true;
  else
    return false;
}

void in_received_handler(DictionaryIterator *iter, void *context) {
  Tuple *now_text_t = dict_find(iter, MSG_NOW_TEXT),
        *now_color_t = dict_find(iter, MSG_NOW_COLOR),
        *now_background_t = dict_find(iter, MSG_NOW_BACKGROUND),
        *now_font_t = dict_find(iter, MSG_NOW_FONT),
        *now_battery_show_t = dict_find(iter, MSG_NOW_BATTERY_SHOW),
        *now_battery_blinks_t = dict_find(iter, MSG_NOW_BATTERY_BLINKS),
        *now_battery_color_t = dict_find(iter, MSG_NOW_BATTERY_COLOR),
        *now_battery_low_t = dict_find(iter, MSG_NOW_BATTERY_LOW),
        *clock_color_t = dict_find(iter, MSG_CLOCK_COLOR),
        *clock_background_t = dict_find(iter, MSG_CLOCK_BACKGROUND),
        *clock_font_t = dict_find(iter, MSG_CLOCK_FONT),
        *clock_date_font_t = dict_find(iter, MSG_CLOCK_DATE_FONT),
        *clock_battery_show_t = dict_find(iter, MSG_CLOCK_BATTERY_SHOW),
        *clock_battery_blinks_t = dict_find(iter, MSG_CLOCK_BATTERY_BLINKS),
        *clock_battery_color_t = dict_find(iter, MSG_CLOCK_BATTERY_COLOR),
        *clock_battery_low_t = dict_find(iter, MSG_CLOCK_BATTERY_LOW),
        *clock_delay_t = dict_find(iter, MSG_CLOCK_DELAY);
  
  if (now_text_t) {
    strncpy(now_text, now_text_t->value->cstring, sizeof(now_text));
  }
  if (now_color_t) {
    now_text_style.color = GColorFromHEX(now_color_t->value->uint32);
  }
  if (now_background_t) {
    now_text_style.background = GColorFromHEX(now_background_t->value->uint32);
  }
  if (now_font_t) {
    strncpy(now_text_style.font, get_font(now_font_t->value->cstring), sizeof(now_text_style.font));
  }
  if (now_battery_show_t) {
    now_battery_style.draw = battery_draw(now_battery_show_t->value->cstring);
    now_battery_style.low_only = battery_low_only(now_battery_show_t->value->cstring);
  }
  if (now_battery_blinks_t) {
    now_battery_style.low_blink = now_battery_blinks_t->value->int16 ? true : false;
  }
  if (now_battery_color_t) {
    now_battery_style.normal = GColorFromHEX(now_battery_color_t->value->uint32);
  }
  if (now_battery_low_t) {
    now_battery_style.low = GColorFromHEX(now_battery_low_t->value->uint32);
  }
  
  if (clock_color_t) {
    clock_text_style.color = clock_date_style.color = GColorFromHEX(clock_color_t->value->uint32);
  }
  if (clock_background_t) {
    clock_text_style.background = clock_date_style.background = GColorFromHEX(clock_background_t->value->uint32);
  }
  if (clock_font_t) {
    strncpy(clock_text_style.font, get_font(clock_font_t->value->cstring), sizeof(clock_text_style.font));
  }
  if (clock_date_font_t) {
    strncpy(clock_date_style.font, get_font(clock_date_font_t->value->cstring), sizeof(clock_date_style.font));
  }
  if (clock_battery_show_t) {
    clock_battery_style.draw = battery_draw(clock_battery_show_t->value->cstring);
    clock_battery_style.low_only = battery_low_only(clock_battery_show_t->value->cstring);
  }
  if (clock_battery_blinks_t) {
    clock_battery_style.low_blink = clock_battery_blinks_t->value->int16 ? true : false;
  }
  if (clock_battery_color_t) {
    clock_battery_style.normal = GColorFromHEX(clock_battery_color_t->value->uint32);
  }
  if (clock_battery_low_t) {
    clock_battery_style.low = GColorFromHEX(clock_battery_low_t->value->uint32);
  }
  if (clock_delay_t) {
    clock_show_timeout = clock_delay_t->value->uint32 * 1000;
  }
  
  save_config();
  update_now();
  update_clock();
}

void handle_init(void) {
  GRect bounds;
  
  sys_locale = setlocale(LC_ALL, "");
  
  load_config();
  
  now_window = window_create();
  
  bounds = layer_get_bounds(window_get_root_layer(now_window));
  
  now_battery_layer = layer_create_with_data(GRect(bounds.size.w - 28, 5, 23, 8), sizeof(BatteryStyle*));
  *(BatteryStyle**)layer_get_data(now_battery_layer) = &now_battery_style;
  layer_set_update_proc(now_battery_layer, &draw_battery);
  layer_add_child(window_get_root_layer(now_window), now_battery_layer);
  
  now_center_layer = layer_create(GRect(0, 0, bounds.size.w, 20));
  layer_add_child(window_get_root_layer(now_window), (Layer*)now_center_layer);
  
  now_text_layer = text_layer_create(GRect(0, 0, bounds.size.w, 20));
  layer_add_child(now_center_layer, (Layer*)now_text_layer);
  
  update_now();
  window_stack_push(now_window, true);
  
  
  
  clock_window = window_create();
  window_set_background_color(clock_window, clock_text_style.background);
  
  bounds = layer_get_bounds(window_get_root_layer(clock_window));
  
  clock_battery_layer = layer_create_with_data(GRect(bounds.size.w - 28, 5, 23, 8), sizeof(BatteryStyle*));
  *(BatteryStyle**)layer_get_data(clock_battery_layer) = &clock_battery_style;
  layer_set_update_proc(clock_battery_layer, &draw_battery);
  layer_add_child(window_get_root_layer(clock_window), clock_battery_layer);
  
  clock_center_layer = layer_create(GRect(0, 0, bounds.size.w, bounds.size.h));
  layer_add_child(window_get_root_layer(clock_window), (Layer*)clock_center_layer);

  clock_text_layer = text_layer_create(GRect(0, 0, bounds.size.w, bounds.size.h));
  layer_add_child(clock_center_layer, (Layer*)clock_text_layer);

  clock_date_layer = text_layer_create(GRect(0, 0, bounds.size.w, 20));
  layer_add_child(clock_center_layer, (Layer*)clock_date_layer);
  
  update_clock();
  
  accel_tap_service_subscribe(&handle_tap);
  battery_state_service_subscribe(&handle_battery_state);
  
  // Register message handlers
  app_message_register_inbox_received(in_received_handler);
  app_message_open(1024, 64);
}

void handle_deinit(void) {
  battery_state_service_unsubscribe();
  accel_tap_service_unsubscribe();
  
  text_layer_destroy(clock_text_layer);
  layer_destroy(clock_battery_layer);
  layer_destroy(clock_center_layer);
  window_destroy(clock_window);
  
  text_layer_destroy(now_text_layer);
  layer_destroy(now_center_layer);
  window_destroy(now_window);
}

int main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}
