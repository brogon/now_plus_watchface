#include <pebble.h>

#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))

typedef struct {
    GFont              font;
    GColor             color,
                       background;
    GTextOverflowMode  overflow;
    GTextAlignment     alignment;
  } TextStyle;

typedef struct {
    GColor     normal,
               low,
               background;
    bool       low_only,
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
const char        *now_text_default = "NOW";

Window            *now_window;
Layer             *now_center_layer;
TextLayer         *now_text_layer;
Layer             *now_battery_layer;
TextStyle          now_text_style;


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
  BatteryStyle             *style = (BatteryStyle*)layer_get_data(layer);
  const BatteryChargeState  state = battery_state_service_peek();

  if(style->blink_timer) {
    app_timer_cancel(style->blink_timer);
    style->blink_timer = NULL;
  }
  
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
    for(int16_t i = 3; i < MAX(4, (bounds.size.w * 90 * state.charge_percent / (100 * 100)) - 3); i += 4) {
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
                                                     style->font,
                                                     bounds,
                                                     style->overflow,
                                                     style->alignment
                                                    );
  layer_set_frame((Layer*)layer, GRect(0, 0, bounds.size.w, size.h));
  text_layer_set_text(layer, text);
  text_layer_set_font(layer, style->font);
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
  
  update_text(now_text_layer, now_center_layer, &now_text_style, now_text_default);
  layer_center_vertical(now_center_layer, window_get_root_layer(now_window), layers);
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
  
  update_text(clock_text_layer, clock_center_layer, &clock_text_style, clock_text);
  update_text(clock_date_layer, clock_center_layer, &clock_date_style, clock_date);
  layer_center_vertical(clock_center_layer, window_get_root_layer(clock_window), layers);
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

void handle_init(void) {
  GRect bounds;
  BatteryStyle battery_style;
  
  sys_locale = setlocale(LC_ALL, "");
  
  battery_style.normal      = GColorFromRGB(255,255,255);
  battery_style.low         = GColorFromRGB(255,0,0);
  battery_style.background  = GColorFromRGB(0,0,0);
  battery_style.low_only    = true;
  battery_style.low_blink   = true;
  battery_style.blink_state = false;
  battery_style.blink_delay = 1000;
    
  now_text_style.font        = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  now_text_style.color       = GColorFromRGB(255,255,255);
  now_text_style.background  = GColorFromRGB(0,0,0);
  now_text_style.overflow    = GTextOverflowModeWordWrap;
  now_text_style.alignment   = GTextAlignmentCenter;
  
  now_window = window_create();
  window_set_background_color(now_window, now_text_style.background);
  
  bounds = layer_get_bounds(window_get_root_layer(now_window));
  
  now_battery_layer = layer_create_with_data(GRect(bounds.size.w - 28, 5, 23, 8), sizeof(BatteryStyle));
  memcpy(layer_get_data(now_battery_layer), &battery_style, sizeof(BatteryStyle));
  layer_set_update_proc(now_battery_layer, &draw_battery);
  layer_add_child(window_get_root_layer(now_window), now_battery_layer);
  
  now_center_layer = layer_create(GRect(0, 0, bounds.size.w, 20));
  layer_add_child(window_get_root_layer(now_window), (Layer*)now_center_layer);
  
  now_text_layer = text_layer_create(GRect(0, 0, bounds.size.w, 20));
  layer_add_child(now_center_layer, (Layer*)now_text_layer);
  
  update_now();
  window_stack_push(now_window, true);
  
  
  clock_text_style.font           = fonts_get_system_font(FONT_KEY_BITHAM_42_MEDIUM_NUMBERS);
  clock_text_style.color          = GColorFromRGB(255,255,255);
  clock_text_style.background     = GColorFromRGB(0,0,0);
  clock_text_style.overflow       = GTextOverflowModeWordWrap;
  clock_text_style.alignment      = GTextAlignmentCenter;
  clock_date_style.font           = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  clock_date_style.color          = GColorFromRGB(255,255,255);
  clock_date_style.background     = GColorFromRGB(0,0,0);
  clock_date_style.overflow       = GTextOverflowModeWordWrap;
  clock_date_style.alignment      = GTextAlignmentCenter;
  clock_show                      = false;
  clock_show_timeout              = 4000;
  clock_show_timer                = NULL;
  battery_style.low_only          = false;
  
  clock_window = window_create();
  window_set_background_color(clock_window, clock_text_style.background);
  
  bounds = layer_get_bounds(window_get_root_layer(clock_window));
  
  clock_battery_layer = layer_create_with_data(GRect(bounds.size.w - 28, 5, 23, 8), sizeof(BatteryStyle));
  memcpy(layer_get_data(clock_battery_layer), &battery_style, sizeof(BatteryStyle));
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
}

void handle_deinit(void) {
  battery_state_service_unsubscribe();
  accel_tap_service_unsubscribe();
  
  text_layer_destroy(clock_text_layer);
  layer_destroy(clock_battery_layer);
  window_destroy(clock_window);
  
  text_layer_destroy(now_text_layer);
  window_destroy(now_window);
}

int main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}