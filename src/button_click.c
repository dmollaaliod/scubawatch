#include <pebble.h>
#define NBARITEMS 3
#define LOGITEMS 20

#define PERSIST_BAR_I 1
#define PERSIST_ITEM 10
#define PERSIST_SECONDS 2
#define PERSIST_BAR 3
  
// Main window
static Window *window;
static TextLayer *text_layer[NBARITEMS];
static TextLayer *time_layer;
static TextLayer *bar_layer;

// Log window
static Window *log_window;
static ScrollLayer *log_scroll_layer;
static TextLayer *log_text_layer[LOGITEMS];

// Other data
static int seconds_elapsed = 0;
static int bar = 200;
static bool bar_changed = false;
static time_t bar_changed_time;
static char* bar_readings[LOGITEMS];
static int bar_i = 0;

static void render_time() {
  
  // Create a long-lived buffer
  static char buffer[] = "00:00:00";
  
  // Write the elapsed time into the buffer
  strftime(buffer, sizeof("00:00:00"), "%X", localtime((time_t *) &seconds_elapsed));

  // Display this time on the TextLayer
  text_layer_set_text(time_layer, buffer);
}

static void render_bar() {
  static char buf[] = "000 bar";
  snprintf(buf, sizeof(buf), "%i bar", bar);
  text_layer_set_text(bar_layer, buf);
}

static void render_text() {
                           
  for (int i=0; i<NBARITEMS && bar_i-i >= 0; i++) {
    int offset = bar_i - NBARITEMS +1;
    if (offset<0)
      offset = 0;
    text_layer_set_text(text_layer[i],bar_readings[offset+i]);    
  }
}

char *get_bar_reading() {
  char *result;
  char buftime[] = "00:00:00";
  
  result = malloc(sizeof("00:00:00 000 bar"));
  strftime(buftime, sizeof("00:00:00"), "%X", localtime((time_t *) &seconds_elapsed));
  snprintf(result, sizeof("00:00:00 000 bar"), "%s %i bar", buftime, bar);  
  return result;
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  seconds_elapsed += 1;
  render_time();
  if (bar_changed && difftime(time(NULL),bar_changed_time) > 3) {
    bar_changed = false;
    bar_readings[bar_i] = get_bar_reading();
    render_text();
    if (bar_i < LOGITEMS-1)
      bar_i += 1;
  }
}
  
static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  bar_readings[bar_i] = get_bar_reading();
  bar_changed = false;
  render_text();
  if (bar_i < LOGITEMS-1)
    bar_i += 1;
}

static void select_multi_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Reset timer
  seconds_elapsed = 0;
  render_time();
  bar = 200;
  render_bar();
  for (int i=0; i<bar_i; i++) {
    free(bar_readings[bar_i]);
  }
  bar_i = 0;
  for (int i=0; i<NBARITEMS; i++)
    text_layer_set_text(text_layer[i], "");      
    
}

static void select_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Log window
  window_stack_push(log_window,true);
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  bar += 5;
  bar_changed = true;
  bar_changed_time = time(NULL);
  render_bar();
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  bar -= 5;
  bar_changed = true;
  bar_changed_time = time(NULL);
  render_bar();
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_multi_click_subscribe(BUTTON_ID_SELECT, 2, 10, 0, true, select_multi_click_handler);
  window_long_click_subscribe(BUTTON_ID_SELECT, 1000, select_long_click_handler, NULL);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  window_set_click_config_provider(window, click_config_provider);

  // Time layer
  time_layer = text_layer_create((GRect) { .origin = { 0, 110 }, .size = { bounds.size.w, 40 } });
  text_layer_set_background_color(time_layer, GColorBlack);
  text_layer_set_text_color(time_layer, GColorClear);
  text_layer_set_font(time_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(time_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(time_layer));
  render_time();
  
  // Bar layer
  bar_layer = text_layer_create((GRect) { .origin = { 0, 0 }, .size = { bounds.size.w, 40 } });
  text_layer_set_font(bar_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
  text_layer_set_text_alignment(bar_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(bar_layer));
  render_bar();
  
  // Text layer
  for (int i=0; i<NBARITEMS; i++) {
    text_layer[i] = text_layer_create((GRect) { .origin = { 0, 30+i*25 }, .size = { bounds.size.w, 25 } });
    text_layer_set_text(text_layer[i], "");
    text_layer_set_font(text_layer[i], fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_text_alignment(text_layer[i], GTextAlignmentLeft);
    layer_add_child(window_layer, text_layer_get_layer(text_layer[i]));
  }
  render_text();
}

static void window_unload(Window *window) {
  for (int i=0; i<NBARITEMS; i++)
    text_layer_destroy(text_layer[i]);
  text_layer_destroy(time_layer);
  text_layer_destroy(bar_layer);
}

static void log_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  log_scroll_layer = scroll_layer_create(bounds);
  scroll_layer_set_content_size(log_scroll_layer, (GSize) { bounds.size.w, 25*bar_i });
  scroll_layer_set_click_config_onto_window(log_scroll_layer, window);

  for (int i=0; i<bar_i; i++) {
    log_text_layer[i] = text_layer_create((GRect) { .origin = { 0, i*25 }, .size = { bounds.size.w, 25 } });
    text_layer_set_font(log_text_layer[i], fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_text(log_text_layer[i], bar_readings[i]);
    scroll_layer_add_child(log_scroll_layer, text_layer_get_layer(log_text_layer[i]));
  }
  layer_add_child(window_layer, scroll_layer_get_layer(log_scroll_layer));  
}

static void log_window_unload(Window *window) {
  for (int i=0; i<bar_i; i++)
    text_layer_destroy(log_text_layer[i]);
  scroll_layer_destroy(log_scroll_layer);
}

static void init(void) {
  // Register with TickTimerService
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);

  // Create window
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
	.load = window_load,
    .unload = window_unload,
  });
  
  // Create log window
  log_window = window_create();
  window_set_window_handlers(log_window, (WindowHandlers) {
	.load = log_window_load,
    .unload = log_window_unload,
  });
  
  // Load persistent data
  if (persist_exists(PERSIST_BAR_I)) {
    bar_i = persist_read_int(PERSIST_BAR_I);
    for (int i=0; i<bar_i; i++) {
      if (persist_exists(PERSIST_ITEM+i)) {
        char *s = malloc(sizeof("00:00:00 000 bar"));
        bar_readings[i] = s;
        persist_read_string(PERSIST_ITEM+i,bar_readings[i],sizeof("00:00:00 000 bar"));
      }
    }
  }
  
  if (persist_exists(PERSIST_SECONDS)) {
    seconds_elapsed = persist_read_int(PERSIST_SECONDS);//+time(NULL)-persist_read_int(PERSIST_TIME);
  }

  if (persist_exists(PERSIST_BAR)) {
    bar = persist_read_int(PERSIST_BAR);
  }

  const bool animated = true;
  window_stack_push(window, animated);
}

static void deinit(void) {
  // Store persistent data
  persist_write_int(PERSIST_BAR_I, bar_i);
  for (int i=0; i<bar_i; i++) {
    persist_write_string(PERSIST_ITEM+i,bar_readings[i]);
  }
  persist_write_int(PERSIST_SECONDS, seconds_elapsed);
  persist_write_int(PERSIST_BAR, bar);
  //persist_write_int(PERSIST_TIME, time(NULL));
  
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
