#include <pebble.h>
#define LOGITEMS 50
#define VIBES_INTERVAL 300 // 5-minute interval

#define PERSIST_BAR_I 1
#define PERSIST_ITEM 10
#define PERSIST_SECONDS 2
#define PERSIST_SECONDS_NEXT_VIBES 4
#define PERSIST_BAR 3
#define PERSIST_TIME 5

// Compass
static GPath *s_needle_north, *s_needle_south;
static const GPathInfo NEEDLE_NORTH_POINTS = { 
  3,
  (GPoint[]) { { -6, 0 }, { 6, 0 }, { 0, -27 } }
};
static const GPathInfo NEEDLE_SOUTH_POINTS = { 
  3,
  (GPoint[]) { { 5, 0 }, { 0, 25 }, { -5, 0 } }
};
  
// Main window
static Window *window;
static Layer *needles_layer;
static TextLayer *degrees_layer;
static TextLayer *calibrating_layer;
static char calibrating[] = "calibrating";
static char calibrated[] =  "           ";
static char degrees_text[] = "---°";
static TextLayer *time_layer;
static TextLayer *bar_layer;
static TextLayer *bar_label;
static char bar_label_text[] = "bar";

// Log window
static Window *log_window;
static ScrollLayer *log_scroll_layer;
static TextLayer *log_text_layer[LOGITEMS];

// Other data
static int seconds_elapsed = 0;
static int seconds_next_vibes = VIBES_INTERVAL;
static int bar = 200;
static bool bar_changed = false;
static time_t bar_changed_time;
static char* bar_readings[LOGITEMS];
static int bar_i = 0;
static int main_window = true;

static void path_layer_update_callback(Layer *path, GContext *ctx) {
  // Draw the needles
  
  gpath_draw_filled(ctx, s_needle_north);       
  gpath_draw_outline(ctx, s_needle_south);                     

  // creating centerpoint                 
  GRect bounds = layer_get_frame(path);          
  GPoint path_center = GPoint(bounds.size.w / 2, bounds.size.h / 2);  
  graphics_fill_circle(ctx, path_center, 3);       

  // then put a white circle on top               
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, path_center, 2);                      
}

static void render_time() {
  
  // Create a long-lived buffer
  static char buffer[] = "00:00:00";
  
  // Write the elapsed time into the buffer
  strftime(buffer, sizeof("00:00:00"), "%X", localtime((time_t *) &seconds_elapsed));

  // Display this time on the TextLayer
  text_layer_set_text(time_layer, buffer);
}

static void render_bar() {
  static char bar_text[] = "000";
  snprintf(bar_text, sizeof(bar_text), "%i", bar);
  text_layer_set_text(bar_layer, bar_text);
}

static char *get_bar_reading() {
  char *result;
  char buftime[] = "00:00:00";
  
  result = malloc(sizeof("00:00:00 000 bar"));
  strftime(buftime, sizeof("00:00:00"), "%X", localtime((time_t *) &seconds_elapsed));
  snprintf(result, sizeof("00:00:00 000 bar"), "%s %i bar", buftime, bar);  
  return result;
}

static int get_compass_heading() {
  CompassHeadingData data;
  
  compass_service_peek(&data);
  return 365-TRIGANGLE_TO_DEG((int)data.true_heading);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  seconds_elapsed += 1;
  render_time();
  if (bar_changed && difftime(time(NULL),bar_changed_time) > 3) {
    while (seconds_next_vibes < seconds_elapsed) {
      seconds_next_vibes += VIBES_INTERVAL;
    }
    bar_changed = false;
    if (bar_i < LOGITEMS) {
      bar_readings[bar_i] = get_bar_reading();
      bar_i += 1;
      vibes_short_pulse();
    }
  }
  if (main_window && (seconds_next_vibes <= seconds_elapsed)) {
    vibes_long_pulse();
  }
  
  // display calibration and heading
  CompassHeadingData data;
  compass_service_peek(&data);
  if (data.compass_status == CompassStatusCalibrated) {
    text_layer_set_text(calibrating_layer,calibrated);
  } else {
    text_layer_set_text(calibrating_layer,calibrating);    
  }
  if (data.compass_status == CompassStatusDataInvalid) {
    strcpy(degrees_text,"---°");
    text_layer_set_text(degrees_layer, degrees_text);
  } else {
    snprintf(degrees_text,sizeof(degrees_text),"%3i°",get_compass_heading());
    text_layer_set_text(degrees_layer, degrees_text);
  }
  
  // rotate needle accordingly
  gpath_rotate_to(s_needle_north, data.true_heading);
  gpath_rotate_to(s_needle_south, data.true_heading);

}
  
static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  char buftime[] = "00:00:00";
  char *buffer;
  int heading;

  if (bar_i < LOGITEMS) {
    buffer = malloc(sizeof("00:00:00 000°"));
    strftime(buftime, sizeof("00:00:00"), "%X", localtime((time_t *) &seconds_elapsed));
    heading = get_compass_heading();
    snprintf(buffer, sizeof("00:00:00 000°"), "%s %i°", buftime, heading);
    bar_readings[bar_i] = buffer;
    bar_i += 1;
    vibes_short_pulse();
  }
}

static void select_multi_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Reset timer
  seconds_elapsed = 0;
  seconds_next_vibes = VIBES_INTERVAL;
  render_time();
  bar = 200;
  render_bar();
  bar_i = 0;
  text_layer_set_text(degrees_layer, "");      
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
  window_long_click_subscribe(BUTTON_ID_UP, 1000, select_click_handler, NULL);
  window_long_click_subscribe(BUTTON_ID_DOWN, 1000, select_click_handler, NULL);
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
  text_layer_set_font(time_layer, fonts_get_system_font(FONT_KEY_DROID_SERIF_28_BOLD));
  text_layer_set_text_alignment(time_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(time_layer));
  render_time();
  
  // Bar layer
  bar_layer = text_layer_create((GRect) { .origin = { 0, 0 }, .size = { 90, 50 } });
  text_layer_set_font(bar_layer, fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49));
  text_layer_set_text_alignment(bar_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(bar_layer));
  render_bar();
  
  // Bar label
  bar_label = text_layer_create((GRect) { .origin = { 90, 20}, .size = { 100, 40} });
  text_layer_set_font(bar_label, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
  text_layer_set_text(bar_label, bar_label_text);
  layer_add_child(window_layer, text_layer_get_layer(bar_label));
  
  // Compass needles
  needles_layer = layer_create((GRect) { .origin = { 0, 55 }, .size = { 50, 50 } });
  
  //  Define the draw callback to use for this layer
  layer_set_update_proc(needles_layer, path_layer_update_callback);
  layer_add_child(window_layer, needles_layer);

  // Initialize and define the two paths used to draw the needle to north and to south
  s_needle_north = gpath_create(&NEEDLE_NORTH_POINTS);
  s_needle_south = gpath_create(&NEEDLE_SOUTH_POINTS);

  // Move the needles to the center of the screen.
  GPoint center = GPoint(25, 25);
  gpath_move_to(s_needle_north, center);
  gpath_move_to(s_needle_south, center);

  
  // Compass direction
  degrees_layer = text_layer_create((GRect) { .origin = { 60, 55 }, .size = { bounds.size.w, 40 } });
  text_layer_set_text(degrees_layer, "");
  text_layer_set_font(degrees_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
  text_layer_set_text_alignment(degrees_layer, GTextAlignmentLeft);
  layer_add_child(window_layer, text_layer_get_layer(degrees_layer));
  
  // Calibration label
  calibrating_layer = text_layer_create((GRect) { .origin = { 60, 85 }, .size = { bounds.size.w, 20 } });
  text_layer_set_text(calibrating_layer, "");
  text_layer_set_text_alignment(calibrating_layer, GTextAlignmentLeft);
  layer_add_child(window_layer, text_layer_get_layer(calibrating_layer));
}

static void window_unload(Window *window) {
  layer_destroy(needles_layer);
  text_layer_destroy(degrees_layer);
  text_layer_destroy(calibrating_layer);
  text_layer_destroy(time_layer);
  text_layer_destroy(bar_layer);
  text_layer_destroy(bar_label);
}

static void log_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  main_window = false;
  
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
  main_window = true;
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
    seconds_elapsed = persist_read_int(PERSIST_SECONDS)+(time(NULL)-persist_read_int(PERSIST_TIME));
  }

  if (persist_exists(PERSIST_SECONDS_NEXT_VIBES)) {
    seconds_next_vibes = persist_read_int(PERSIST_SECONDS_NEXT_VIBES);
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
  persist_write_int(PERSIST_SECONDS_NEXT_VIBES, seconds_next_vibes);
  persist_write_int(PERSIST_BAR, bar);
  persist_write_int(PERSIST_TIME, time(NULL));
  
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
