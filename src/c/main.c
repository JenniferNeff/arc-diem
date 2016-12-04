#include <pebble.h>
#include <pdc-transform/pdc-transform.h>
#include "enamel.h"
#include <pebble-events/pebble-events.h>

static Window *s_main_window;
static TextLayer *s_time_layer;
static TextLayer *s_day_layer;
static TextLayer *s_date_layer;
static TextLayer *s_pm_layer;
static GFont s_time_font;
static GFont s_date_font;

static Layer *s_canvas_layer;
static GPath *hour_hand;
static GPath *inner_sun;
static GPath *outer_sun;

// These are for the battery level
static int s_battery_level;
static bool s_battery_charging;
static Layer *s_battery_layer;
static BitmapLayer *s_battery_icon_layer;
static GBitmap *s_battery_icon, *s_battery_icon_dark;
static GBitmap *s_battery_icon_plus, *s_battery_icon_plus_dark;

static bool daytime;
static GColor foreground_color;
static GColor background_color;

// For the bitmap background

static BitmapLayer *s_background_layer;
static GBitmap *s_background_bitmap_day;
static GBitmap *s_background_bitmap_night;

// For the hour hand

static GDrawCommandImage *s_command_image;

#if PBL_DISPLAY_WIDTH == 200
static const GPathInfo SUN_INNER_RAYS_INFO = {
  .num_points = 8,
  .points = (GPoint[]) {{7,7}, {21,14}, {35,7}, {28, 21}, {35, 35}, {21,28}, {7, 35}, {14,21}}
};
static const GPathInfo SUN_OUTER_RAYS_INFO = {
  .num_points = 8,
  .points = (GPoint[]) {{21, 0}, {28, 14}, {42, 21}, {28, 28}, {21, 42}, {14, 28}, {0, 21}, {14,14}}
};
static int sun_offset = 21;
static int small_sun_radius = 10;

static int moon_outer_radius = 19;
static int moon_inner_radius = 11;

#else
static const GPathInfo SUN_INNER_RAYS_INFO = {
  .num_points = 8,
  .points = (GPoint[]) {{5,5}, {15,10}, {25,5}, {20, 15}, {25, 25}, {15,20}, {5, 25}, {10,15}}
};
static const GPathInfo SUN_OUTER_RAYS_INFO = {
  .num_points = 8,
  .points = (GPoint[]) {{15, 0}, {20, 10}, {30, 15}, {20, 20}, {15, 30}, {10, 20}, {0, 15}, {10,10}}
};
static int sun_offset = 15;
static int small_sun_radius = 7;

static int moon_outer_radius = 14;
static int moon_inner_radius = 8;
#endif

// Bluetooth

static Layer *s_bt_layer;
static BitmapLayer *s_bt_icon_layer;
static GBitmap *s_bt_icon_bitmap, *s_bt_icon_bitmap_dark;
static GBitmap *s_bt_icon_on_bitmap, *s_bt_icon_on_bitmap_dark;
static GBitmap *s_bt_icon_off_bitmap, *s_bt_icon_off_bitmap_dark;

static const VibePattern SIGNAL_LOST = {
  .durations = (uint32_t[]) {200, 300, 500},
  .num_segments = 3
};

static const VibePattern SIGNAL_FOUND = {
  .durations = (uint32_t[]) {100, 200, 100},
  .num_segments = 3
};

// Track the day start and end options
static int start_hour;
static int end_hour;

static EventHandle s_boundary_handle;

// Update daytime from anywhere

static void check_daytime() {
  time_t start_stamp = clock_to_timestamp(TODAY, start_hour, 0);
  time_t end_stamp = clock_to_timestamp(TODAY, end_hour, 0);
  
  daytime = end_stamp < start_stamp; // Check if this is correct; needs to be more elegant to swap colors right at 7:00, not 7:01
  foreground_color = daytime ? GColorBlack : GColorWhite;
  background_color = daytime ? GColorWhite : GColorBlack;
  bitmap_layer_set_bitmap(s_background_layer, daytime ? s_background_bitmap_day : s_background_bitmap_night);
  
  text_layer_set_text_color(s_time_layer, foreground_color);
  text_layer_set_text_color(s_day_layer, foreground_color);
  text_layer_set_text_color(s_date_layer, foreground_color);
  text_layer_set_text_color(s_pm_layer, foreground_color);
}


static void update_time() {
  check_daytime();
  // Get a tm structure
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  // Write the current hours and minutes into a buffer
  static char s_buffer[16];
  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ?
                                          "%H:%M" : "%I:%M", tick_time);
  
  if('0' == s_buffer[0]) {
    memmove(s_buffer, &s_buffer[1], sizeof(s_buffer)-1);
  } // thanks morris https://forums.pebble.com/t/remove-padding-from-12-hour-time/15700
  
  // Write the current day and month into a buffer
  static char dd_buffer[16];
  strftime(dd_buffer, sizeof(dd_buffer), "%A", tick_time);
  
  static char m_buffer[16];
  strftime(m_buffer, sizeof(m_buffer), "%b ", tick_time);

  // And also the numbered date
  static char date_buffer[16];
  strftime(date_buffer, sizeof(date_buffer), "%d", tick_time);

  if('0' == date_buffer[0]) {
    memmove(date_buffer, &date_buffer[1], sizeof(date_buffer)-1);
  } // thanks morris https://forums.pebble.com/t/remove-padding-from-12-hour-time/15700
  
  strncat(m_buffer, date_buffer, 2);
  
  // Write AM/PM into a buffer
  static char pm_buffer[8];
  strftime(pm_buffer, sizeof(pm_buffer), "%P", tick_time);
  
  // Display this time on their TextLayers
  text_layer_set_text(s_time_layer, s_buffer);
  text_layer_set_text(s_day_layer, dd_buffer);
  text_layer_set_text(s_date_layer, m_buffer);
  if (!clock_is_24h_style()) {
    text_layer_set_text(s_pm_layer, pm_buffer);
  }
}

static void battery_callback(BatteryChargeState state) {
  // Record the new battery level
  s_battery_level = state.charge_percent;
  s_battery_charging = state.is_charging;
  // Update meter
  layer_mark_dirty(s_battery_layer);

}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
  layer_mark_dirty(s_canvas_layer);
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  // Special Thanks To https://forums.pebble.com/t/watchface-graphic-stops-drawing-after-watchface-loaded-for-a-while/18982
  // Custom drawing happens here!
  GRect bounds = layer_get_bounds(layer);
  //GRect dial_bounds = GRect(0, bounds.size.h - bounds.size.w/2, bounds.size.w, bounds.size.w);
  GRect dial_hand_bounds = GRect(2, bounds.size.h - bounds.size.w/2 + 2, bounds.size.w - 4, bounds.size.w - 4);
  GRect dial_trim_bounds = GRect(12, bounds.size.h - bounds.size.w/2 + 12, bounds.size.w - 24, bounds.size.w - 24);
  GRect center_line_bounds = GRect(bounds.size.w/4, bounds.size.h - bounds.size.w/4, bounds.size.w/2, bounds.size.w/2);
  GPoint center = GPoint(bounds.size.w/2, bounds.size.h);
  //uint16_t radius = bounds.size.w/2;
  
  // Draw rectangles to test
  //graphics_draw_rect(ctx, dial_bounds);
  //graphics_draw_rect(ctx, dial_trim_bounds);
  //graphics_draw_rect(ctx, center_line_bounds);
  
  // Get the user's preferred start and end times of the day
  time_t start_stamp = clock_to_timestamp(TODAY, start_hour, 0);
  time_t end_stamp = clock_to_timestamp(TODAY, end_hour, 0);
  time_t now = time(NULL);
  
  check_daytime();
  int daylight_minutes = daytime ? ((end_hour + 24 - start_hour) % 24) * 60 : ((start_hour + 24 - end_hour) % 24) * 60;
  int daylight_remaining = daytime ? (end_stamp - now) / 60 : (start_stamp - now) / 60;
  
  graphics_context_set_stroke_color(ctx, foreground_color);
  graphics_context_set_stroke_width(ctx, 3);
  
  // Draw the hour hand - simple vector graphics version

  int hour_angle = (TRIG_MAX_ANGLE * 90 / 360) - (TRIG_MAX_ANGLE * daylight_remaining) / (2 * daylight_minutes);
  GPoint hour_hand_end = gpoint_from_polar(dial_hand_bounds, GOvalScaleModeFitCircle, hour_angle); // that last argument corresponds to the hour
  GPoint hour_hand_left = gpoint_from_polar(dial_trim_bounds, GOvalScaleModeFitCircle, hour_angle - 1000); // that last argument corresponds to the hour
  GPoint hour_hand_right = gpoint_from_polar(dial_trim_bounds, GOvalScaleModeFitCircle, hour_angle + 1000); // that last argument corresponds to the hour
  
  GPoint center_of_sun = gpoint_from_polar(center_line_bounds, GOvalScaleModeFitCircle, hour_angle);
  
  
  const GPathInfo BOLT_PATH_INFO = {
    .num_points = 5,
    .points = (GPoint[]) {{bounds.size.w/2, bounds.size.h}, hour_hand_end, hour_hand_left, hour_hand_end, hour_hand_right}
  };
  hour_hand = gpath_create(&BOLT_PATH_INFO);
  gpath_draw_outline_open(ctx, hour_hand);
  gpath_destroy(hour_hand);
  
  graphics_fill_circle(ctx, center, 5);
  graphics_draw_circle(ctx, center, 5);
  
  if (daytime) { // draw the sun on the hour hand
    inner_sun = gpath_create(&SUN_INNER_RAYS_INFO);
    outer_sun = gpath_create(&SUN_OUTER_RAYS_INFO);
  
    gpath_move_to(inner_sun, GPoint(center_of_sun.x - sun_offset, center_of_sun.y - sun_offset));
    gpath_move_to(outer_sun, GPoint(center_of_sun.x - sun_offset, center_of_sun.y - sun_offset));
  
    graphics_context_set_fill_color(ctx, foreground_color);
    gpath_draw_filled(ctx, inner_sun);
    gpath_draw_outline(ctx, inner_sun);
    graphics_context_set_fill_color(ctx, background_color);
    graphics_context_set_stroke_color(ctx, foreground_color);
    gpath_draw_filled(ctx, outer_sun);
    gpath_draw_outline(ctx, outer_sun);
    
    gpath_destroy(inner_sun);
    gpath_destroy(outer_sun);
  
    GRect mid_sun = GRect(center_of_sun.x - small_sun_radius, center_of_sun.y - small_sun_radius, small_sun_radius*2, small_sun_radius*2);
    graphics_context_set_fill_color(ctx, foreground_color);
    graphics_fill_radial(ctx, mid_sun, GOvalScaleModeFitCircle, 3, 0, DEG_TO_TRIGANGLE(360));
    
  } else { // draw the moon on the hour hand - https://www.xkcd.com/1738/ is acknowledged
    graphics_context_set_fill_color(ctx, foreground_color);
    graphics_context_set_stroke_color(ctx, foreground_color);
    graphics_fill_circle(ctx, center_of_sun, moon_outer_radius);
    graphics_draw_circle(ctx, center_of_sun, moon_outer_radius);
    graphics_context_set_fill_color(ctx, background_color);
    graphics_context_set_stroke_color(ctx, background_color);
    graphics_fill_circle(ctx, gpoint_from_polar(center_line_bounds, GOvalScaleModeFitCircle, hour_angle + 2200), moon_inner_radius);
    graphics_draw_circle(ctx, gpoint_from_polar(center_line_bounds, GOvalScaleModeFitCircle, hour_angle + 2200), moon_inner_radius);
  }
  
}

static void battery_update_proc(Layer *layer, GContext *ctx) {
  
  if (strcmp(enamel_get_BatteryStatus(), "yes") == 0 || (strcmp(enamel_get_BatteryStatus(), "low") == 0 && (s_battery_level < 30 || s_battery_charging))) {
    
    GRect bounds = layer_get_bounds(layer);
    GRect back_of_bar = GRect(25, 1, bounds.size.w - 50, 8);
  
    // Find the width of the bar
    int width = (int)(float)(((float)s_battery_level / 100.0F) * 90.0F);

    // Draw the background
    graphics_context_set_fill_color(ctx, foreground_color);
    graphics_fill_rect(ctx, back_of_bar, 0, GCornerNone);

    // Draw the bar
    graphics_context_set_fill_color(ctx, background_color);
    graphics_fill_rect(ctx, GRect(27, 3, width, 4), 0, GCornerNone);
    
    if (s_battery_charging) {
      bitmap_layer_set_bitmap(s_battery_icon_layer, daytime ? s_battery_icon_plus : s_battery_icon_plus_dark);
    } else { //if (s_battery_level < 30) {
      bitmap_layer_set_bitmap(s_battery_icon_layer, daytime ? s_battery_icon : s_battery_icon_dark);
    }
    layer_set_hidden(bitmap_layer_get_layer(s_battery_icon_layer), false);
    
  } else {
    layer_set_hidden(bitmap_layer_get_layer(s_battery_icon_layer), true);
  }

}

static void update_bluetooth_pictures(bool connected) {
  if (connected) {
      bitmap_layer_set_bitmap(s_bt_icon_layer, daytime ? s_bt_icon_on_bitmap : s_bt_icon_on_bitmap_dark);
      layer_set_hidden(bitmap_layer_get_layer(s_bt_icon_layer), !strcmp(enamel_get_BluetoothStatus(), "yes") == 0);
  } else {
    if (strcmp(enamel_get_BluetoothStatus(), "yes") == 0) {
      bitmap_layer_set_bitmap(s_bt_icon_layer, daytime ? s_bt_icon_off_bitmap : s_bt_icon_off_bitmap_dark);
      layer_set_hidden(bitmap_layer_get_layer(s_bt_icon_layer), false);
    } else if (strcmp(enamel_get_BluetoothStatus(), "disconnected") == 0) {
      bitmap_layer_set_bitmap(s_bt_icon_layer, daytime ? s_bt_icon_bitmap : s_bt_icon_bitmap_dark);
      layer_set_hidden(bitmap_layer_get_layer(s_bt_icon_layer), false);
    } else {
      layer_set_hidden(bitmap_layer_get_layer(s_bt_icon_layer), true);
    }
  }
}

static void bluetooth_callback(bool connected) {
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "Bluetooth callback");
  
  if(!connected && strcmp(enamel_get_BluetoothDisconnect(), "yes") == 0) {
    vibes_enqueue_custom_pattern(SIGNAL_LOST);
  } else if (connected && strcmp(enamel_get_BluetoothConnect(), "yes") == 0) {
    vibes_enqueue_custom_pattern(SIGNAL_FOUND);
  }
  update_bluetooth_pictures(connected);
  layer_mark_dirty(s_bt_layer);
}

static void enamel_settings_received_boundary_handler(void *context){
  APP_LOG(0, "Settings received %d", (int)enamel_get_DayStart());
  APP_LOG(0, "Settings received %d", (int)enamel_get_DayEnd());
  start_hour = enamel_get_DayStart();
  end_hour = enamel_get_DayEnd();
  check_daytime();
  update_bluetooth_pictures(connection_service_peek_pebble_app_connection());
}

static void main_window_load(Window *window) {
  // Get information about the Window
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  // Draw the pictures
  
  // Create GBitmap
  s_background_bitmap_day = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_DAY_ON_WHITE);
  s_background_bitmap_night = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_NIGHT_ON_BLACK);

  // Create BitmapLayer to display the GBitmap
  s_background_layer = bitmap_layer_create(bounds);

  // Set the bitmap onto the layer and add to the window
  bitmap_layer_set_bitmap(s_background_layer, daytime ? s_background_bitmap_day : s_background_bitmap_night);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_background_layer));

  // Create canvas layer
  s_canvas_layer = layer_create(bounds);
  
  // Create the time TextLayer with specific bounds
  s_time_layer = text_layer_create(
      //clock_is_24h_style() ? GRect(0, 48, bounds.size.w, 50) : GRect(0, 48, bounds.size.w-50, 50));
      clock_is_24h_style() ? GRect(0, bounds.size.h*(6.0/21), bounds.size.w, 50) : GRect(0, bounds.size.h*(6.0/21), bounds.size.w-50, 50));
  
  // Create the day and date TextLayer with specific bounds
  s_day_layer = text_layer_create(
      GRect(0, bounds.size.h*(5.0/84), bounds.size.w, 35));
  
  s_date_layer = text_layer_create(
      GRect(0, bounds.size.h*(15.0/84), bounds.size.w, 35));
  
  // Create AM/PM layer
  s_pm_layer = text_layer_create(
      GRect(bounds.size.w - 46, bounds.size.h*(6.0/21) + 13, 30, 25));
  
  // Create GFont
  #if PBL_DISPLAY_WIDTH == 200
  s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ECZAR_SEMIBOLD_LARGE_44));
  s_date_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ECZAR_SEMIBOLD_MEDIUM_25));
  #else
  s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ECZAR_SEMIBOLD_32));
  s_date_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ECZAR_SEMIBOLD_SMALL_18));
  #endif

  // Improve the layout to be more like a watchface
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, foreground_color);
  text_layer_set_font(s_time_layer, s_time_font);
  text_layer_set_text_alignment(s_time_layer, clock_is_24h_style() ? GTextAlignmentCenter: GTextAlignmentRight);
  
  text_layer_set_background_color(s_day_layer, GColorClear);
  text_layer_set_text_color(s_day_layer, foreground_color);
  text_layer_set_font(s_day_layer, s_date_font);
  text_layer_set_text_alignment(s_day_layer, GTextAlignmentCenter);
  
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, foreground_color);
  text_layer_set_font(s_date_layer, s_date_font);
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  
  text_layer_set_background_color(s_pm_layer, GColorClear);
  text_layer_set_text_color(s_pm_layer, foreground_color);
  text_layer_set_font(s_pm_layer, s_date_font);
  text_layer_set_text_alignment(s_pm_layer, GTextAlignmentLeft);
  
  // Assign the custom drawing procedure
  layer_set_update_proc(s_canvas_layer, canvas_update_proc);

  // Add to Window
  layer_add_child(window_get_root_layer(window), s_canvas_layer);
  
  // Add text fields to Window
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_day_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_pm_layer));

  // Create battery meter Layer
  s_battery_layer = layer_create(GRect(0, 0, bounds.size.w, bounds.size.h/4));
  layer_set_update_proc(s_battery_layer, battery_update_proc);

  // Add to Window
  layer_add_child(window_get_root_layer(window), s_battery_layer);

  // Create the battery images from resource file
  s_battery_icon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY_ICON);
  s_battery_icon_plus = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY_ICON_PLUS);
  s_battery_icon_dark = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY_ICON_DARK);
  s_battery_icon_plus_dark = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY_ICON_PLUS_DARK);
  
  // Create the BitmapLayer to display the battery icon
  s_battery_icon_layer = bitmap_layer_create(GRect(0, 0, 21, 9));
  bitmap_layer_set_bitmap(s_battery_icon_layer, s_battery_icon);
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(s_battery_icon_layer));
  
  // Create the Bluetooth icon GBitmap
  s_bt_icon_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BLUETOOTH);
  s_bt_icon_bitmap_dark = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BLUETOOTH_DARK);
  s_bt_icon_on_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BLUETOOTH_ON);
  s_bt_icon_on_bitmap_dark = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BLUETOOTH_ON_DARK);
  s_bt_icon_off_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BLUETOOTH_OFF);
  s_bt_icon_off_bitmap_dark = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BLUETOOTH_OFF_DARK);

  // Create the BitmapLayer to display the GBitmap
  s_bt_layer = layer_create(GRect(122, 4, 18, 18));
  s_bt_icon_layer = bitmap_layer_create(GRect(122, 4, 18, 18));
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(s_bt_icon_layer));
  }

static void main_window_unload(Window *window) {
  // Destroy TextLayer
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_day_layer);
  text_layer_destroy(s_date_layer);
  text_layer_destroy(s_pm_layer);
  layer_destroy(s_canvas_layer);
  layer_destroy(s_battery_layer);
  //APP_LOG(APP_LOG_LEVEL_ERROR, "Destroyed text layers");
  
  
  // Destroy GBitmap
  gbitmap_destroy(s_background_bitmap_day);
  gbitmap_destroy(s_background_bitmap_night);
  
  gbitmap_destroy(s_battery_icon);
  gbitmap_destroy(s_battery_icon_plus);
  gbitmap_destroy(s_battery_icon_dark);
  gbitmap_destroy(s_battery_icon_plus_dark);
  bitmap_layer_destroy(s_battery_icon_layer);
  //APP_LOG(APP_LOG_LEVEL_ERROR, "Destroyed bitmaps");

  // Destroy the PDC image
  gdraw_command_image_destroy(s_command_image);
  //APP_LOG(APP_LOG_LEVEL_ERROR, "Destroyed s_command_image");

  // Destroy BitmapLayer
  bitmap_layer_destroy(s_background_layer);
  //APP_LOG(APP_LOG_LEVEL_ERROR, "Destroyed s_background_layer");
  
  // Unload GFont
  fonts_unload_custom_font(s_time_font);
  fonts_unload_custom_font(s_date_font);
  //APP_LOG(APP_LOG_LEVEL_ERROR, "Destroyed fonts");
  
  // Unload the bluetooth stuff
  gbitmap_destroy(s_bt_icon_bitmap);
  gbitmap_destroy(s_bt_icon_bitmap_dark);
  gbitmap_destroy(s_bt_icon_on_bitmap);
  gbitmap_destroy(s_bt_icon_on_bitmap_dark);
  gbitmap_destroy(s_bt_icon_off_bitmap);
  gbitmap_destroy(s_bt_icon_off_bitmap_dark);
  bitmap_layer_destroy(s_bt_icon_layer);
  layer_destroy(s_bt_layer);

}

static void init() {
  // Create main Window element and assign to pointer
  s_main_window = window_create();

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  
  // Register with TickTimerService
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  
  // Register for battery level updates
  battery_state_service_subscribe(battery_callback);

  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);
  
  // Make sure the time is displayed from the start
  update_time();
  
  // Ensure battery level is displayed from the start
  battery_callback(battery_state_service_peek());
  
  // Initialize Enamel to register App Message handlers and restores settings
  enamel_init();

  // call pebble-events app_message_open function
  events_app_message_open(); 

  // Get Start and End hour preferences
  start_hour = enamel_get_DayStart(); // change this so it only happens when the app starts or the user submits changes
  end_hour = enamel_get_DayEnd(); // https://github.com/gregoiresage/enamel Step 5, https://developer.pebble.com/guides/user-interfaces/app-configuration/ Persisting Settings
  s_boundary_handle = enamel_settings_received_subscribe(enamel_settings_received_boundary_handler, s_main_window);
  
  // Register for Bluetooth connection updates
  connection_service_subscribe((ConnectionHandlers) {
  .pebble_app_connection_handler = bluetooth_callback
  });

  // Show the correct state of the BT connection from the start
  check_daytime();
  update_bluetooth_pictures(connection_service_peek_pebble_app_connection());
  APP_LOG(APP_LOG_LEVEL_DEBUG, "First callback");
}

static void deinit() {
  // Destroy Window
  window_destroy(s_main_window);
  //APP_LOG(APP_LOG_LEVEL_ERROR, "Destroyed s_main_window");
  
  // Deinit Enamel to unregister App Message handlers and save settings
  enamel_settings_received_unsubscribe(s_boundary_handle);
  enamel_deinit();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}