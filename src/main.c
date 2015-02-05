#include <pebble.h>
#include "pebble_fonts.h"
  
// forked from https://github.com/MaikJoosten/DutchFuzzyTimeSource
// License: GPLv3

// If this is defined, the face will use minutes and seconds instead of hours and minutes
// to make debugging faster.
//#define DEBUG_FAST

static Window *window;
static GFont font_small;
static GFont font_big;
static InverterLayer *inverter;
static PropertyAnimation *inverter_anim;

typedef struct {
	TextLayer * layer;
	PropertyAnimation *anim;
	const char * text;
	const char * old_text;
} word_t;

static word_t first_word;
static word_t first_word_between;
static word_t second_word;
static word_t second_word_between;
static word_t third_word;
static const char *hours[] = {"twaalf","een","twee","drie","vier","vijf","zes","zeven","acht","negen","tien","elf","twaalf"};
static int minute_offset;

static bool use_twintig = false;
//static const int setting_twintig = 1;
static enum SettingTwintig { twintig_off = 0, twintig_on, twintig_count } twintig;
static AppSync app;
static uint8_t buffer[256];

static void tuple_changed_callback(const uint32_t key, const Tuple* tuple_new, const Tuple* tuple_old, void* context) {
  // we know these values are uint8 format
  int value = tuple_new->value->uint8;
    switch (key) {
    case 1: // Use twintig
      if ((value >= 0) && (value < twintig_count) && (twintig != value)) {
        // update value
        twintig = value;
        use_twintig = twintig==0 ? false : true;
      }
      break;
    }
}

static void app_error_callback(DictionaryResult dict_error, AppMessageResult app_message_error, void* context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "app error %d", app_message_error);
}
  
TextLayer *text_layer_setup(Window * window, GRect frame, GFont font) {
	TextLayer *layer = text_layer_create(frame);
	text_layer_set_text(layer, "");
	text_layer_set_text_color(layer, GColorWhite);
	text_layer_set_background_color(layer, GColorClear);
	text_layer_set_text_alignment(layer, GTextAlignmentCenter);
	text_layer_set_font(layer, font);
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(layer));
	return layer;
}

static void update_word(word_t * const word) {
	if(word->text != word->old_text) {
		// Move the layer offscreen before changing text.
		// Without this, the new text will be visible briefly before the animation starts.
		layer_set_frame(text_layer_get_layer(word->layer), word->anim->values.from.grect);
		animation_schedule(&word->anim->animation);
	}
	text_layer_set_text(word->layer, word->text);
}

static const char* hour_string(int h) {
  return hours[h % 12];
}

static const char* minute_string(int m){
  switch(m){
    case 0:
    case 60:
      return "";
    case 10:
    case 50:
      return "tien";
    case 15:
    case 45:
      return "kwart";
    case 20:
    case 40:
      if (use_twintig)
        return "twintig";
      else
        return "tien";
    default: 
      return "vijf";
  }
}

static void render_minute_offset() {
  GRect frame;
	GRect frame_right;
	switch(minute_offset) {
		case -2:
		frame = GRect(116, 167, 28, 1);
		frame_right = frame;
		frame_right.origin.x = 0;
		break;
		case -1:
		frame = GRect(0, 167, 28, 1);
		frame_right = frame;
		frame_right.origin.x = 28;
		break;
		case 0:
		frame = GRect(28, 167, 28, 1);
		frame_right = frame;
		frame_right.origin.x = 56;
		frame_right.size.w = 32;
		break;
		case 1:
		frame = GRect(56, 167, 32, 1);
		frame_right = frame;
		frame_right.origin.x = 84;
		frame_right.size.w = 28;
		break;
		case 2:
		frame = GRect(84, 167, 28, 1);
		frame_right = frame;
		frame_right.origin.x = 116;
		break;
	}
	inverter_anim->values.from.grect = frame;
	inverter_anim->values.to.grect = frame_right;
	animation_schedule(&inverter_anim->animation);
}

static void render_dutch_time(int h, int m){
  // reset
  first_word.text = "";
	first_word_between.text = "";
	second_word.text = "";
	second_word_between.text = "";
	third_word.text = "";
  
  bool voor = use_twintig ? m>22 : m>17;
  int fuzzy_hour = voor ? h+1 : h; // add one to the hour, because we're going to say e.g. 'half elf' (in english: 'halfway eleven', which would be 'half ten' in proper english)
  
	float temp_m = m;
	temp_m = temp_m / 5;
	int fuzzy_minute = temp_m + 0.5;
	fuzzy_minute *= 5;
  minute_offset = m - fuzzy_minute; // offset of the minute to a multiple of 5
  
  // whole hour
  if(fuzzy_minute == 0 || fuzzy_minute == 60) {
    first_word_between.text = hour_string(fuzzy_hour); 
    second_word_between.text = "uur";
    return;
  }
  // half hour
  if (fuzzy_minute == 30) {
    first_word_between.text = "half";
    second_word_between.text = hour_string(fuzzy_hour);
    return;
  }
  
  first_word.text = minute_string(fuzzy_minute);
  if (fuzzy_minute == 25 || (!use_twintig && fuzzy_minute == 20)) {
    second_word.text = "voor half";
  }
  else if (fuzzy_minute == 35 || (!use_twintig && fuzzy_minute == 40)) {
    second_word.text = "over half";
  }
  else {
    second_word.text = voor ? "voor" : "over";
  }
    
  third_word.text = hour_string(fuzzy_hour);
}

static void handle_tick(struct tm *tick_time, TimeUnits units_changed) {
	first_word.old_text = first_word.text;
	first_word_between.old_text = first_word_between.text;
	second_word.old_text = second_word.text;
	second_word_between.old_text = second_word_between.text;
	third_word.old_text = third_word.text;

#ifdef DEBUG_FAST
  render_dutch_time(tick_time->tm_min % 24,  tick_time->tm_sec);
#else
  render_dutch_time(tick_time->tm_hour,  tick_time->tm_min);
#endif
  render_minute_offset();

	update_word(&first_word);
	update_word(&first_word_between);
	update_word(&second_word);
	update_word(&second_word_between);
	update_word(&third_word);
}

static void text_layer(word_t * word, GRect frame, GFont font) {
	word->layer = text_layer_setup(window, frame, font);

	GRect frame_right = frame;
	frame_right.origin.x = 150;

	word->anim = property_animation_create_layer_frame(text_layer_get_layer(word->layer), &frame_right, &frame);
	animation_set_duration(&word->anim->animation, 500);
	animation_set_curve(&word->anim->animation, AnimationCurveEaseIn);
}

void word_destroy(word_t * word) {
	property_animation_destroy(word->anim);
	text_layer_destroy(word->layer);
}

static void init() {

	window = window_create();
	window_stack_push(window, false);
	window_set_background_color(window, GColorBlack);

	font_big = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_GEORGIA_40));
	font_small = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_GEORGIA_30));
  //font_big = fonts_get_system_font(FONT_KEY_GOTHIC_28);
  //font_small = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  
	text_layer(&first_word, GRect(0, 12, 143, 50), font_big);
	text_layer(&second_word, GRect(0, 62, 143, 42), font_small);
	text_layer(&third_word, GRect(0, 96, 143, 50), font_big);

	text_layer(&first_word_between, GRect(0, 27, 143, 50), font_big);
	text_layer(&second_word_between, GRect(0, 83, 143, 50), font_big);

	inverter = inverter_layer_create(GRect(0, 166, 36, 1));
	layer_add_child(window_get_root_layer(window), inverter_layer_get_layer(inverter));

	inverter_anim = property_animation_create_layer_frame(inverter_layer_get_layer(inverter), NULL, NULL);
	animation_set_duration(&inverter_anim->animation, 500);
	animation_set_curve(&inverter_anim->animation, AnimationCurveEaseIn);
  // app communication
  Tuplet tuples[] = {
    TupletInteger(1, twintig)
  };
  app_message_open(160, 160);
  app_sync_init(&app, buffer, sizeof(buffer), tuples, ARRAY_LENGTH(tuples),
  tuple_changed_callback, app_error_callback, NULL);

#ifdef DEBUG_FAST
	tick_timer_service_subscribe(SECOND_UNIT, handle_tick);
#else
	tick_timer_service_subscribe(MINUTE_UNIT, handle_tick);
#endif
}

static void deinit() {
	tick_timer_service_unsubscribe();
	property_animation_destroy(inverter_anim);
	inverter_layer_destroy(inverter);
	word_destroy(&second_word_between);
	word_destroy(&first_word_between);
	word_destroy(&third_word);
	word_destroy(&second_word);
	word_destroy(&first_word);
	fonts_unload_custom_font(font_small);
	fonts_unload_custom_font(font_big);
	window_destroy(window);
}

int main(void) {
	init();
	app_event_loop();
	deinit();
}
