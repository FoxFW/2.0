#include <furi.h>
#include <furi_hal.h>
#include "notification.h"
#include "notification_messages.h"
#include "notification_settings_filename.h"
#include <SK6805.h>
#include <math.h>

#define NOTIFICATION_LED_COUNT      3
#define NOTIFICATION_EVENT_COMPLETE 0x00000001U

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NotificationLayerMessage,
    InternalLayerMessage,
    SaveSettingsMessage,
    LoadSettingsMessage,
} NotificationAppMessageType;

typedef struct {
    const NotificationSequence* sequence;
    NotificationAppMessageType type;
    FuriEventFlag* back_event;
} NotificationAppMessage;

typedef enum {
    LayerInternal = 0,
    LayerNotification = 1,
    LayerMAX = 2,
} NotificationLedLayerIndex;

typedef struct {
    uint8_t value_last[LayerMAX];
    uint8_t value[LayerMAX];
    NotificationLedLayerIndex index;
    Light light;
} NotificationLedLayer;

#define NOTIFICATION_SETTINGS_VERSION 0x06
#define NOTIFICATION_SETTINGS_PATH    INT_PATH(NOTIFICATION_SETTINGS_FILE_NAME)

typedef struct {
    //Common settings
    uint8_t rgb_backlight_installed;

    // 0 = RGB only (stock white LED off, matches Momentum firmware's
    // behavior), 1 = RGB + White (stock white LED still ramps alongside
    // the RGB LEDs - FoxFW's original/default behavior)
    uint8_t white_backlight_mode;

    // static gradient mode settings
    uint8_t led_2_color_index;
    uint8_t led_1_color_index;
    uint8_t led_0_color_index;

    // rainbow mode setings
    uint32_t rainbow_mode;
    uint32_t rainbow_speed_ms;
    uint16_t rainbow_step;
    uint8_t rainbow_saturation;
    uint8_t rainbow_wide;
} RGBBacklightSettings;

typedef struct {
    uint8_t version;
    float display_brightness;
    float led_brightness;
    float speaker_volume;
    uint32_t display_off_delay_ms;
    int8_t contrast;
    bool vibro_on;
    float night_shift;
    uint32_t night_shift_start;
    uint32_t night_shift_end;
    bool lcd_inversion;
    RGBBacklightSettings rgb;
} NotificationSettings;

struct NotificationApp {
    FuriMessageQueue* queue;
    FuriPubSub* event_record;
    FuriTimer* display_timer;

    NotificationLedLayer display;
    NotificationLedLayer led[NOTIFICATION_LED_COUNT];
    bool display_led_lock;

    NotificationSettings settings;

    FuriTimer* night_shift_timer;
    FuriTimer* night_shift_demo_timer;
    float current_night_shift;

    FuriTimer* rainbow_timer;
    uint16_t rainbow_hue;
    uint8_t rainbow_red;
    uint8_t rainbow_green;
    uint8_t rainbow_blue;

    // Fox Alarm Clock - drives the "beep beep beep... beep beep beep..."
    // pattern + matching vibration while an alarm is ringing. Owned here
    // (not Desktop) since sound/vibration hardware access is this service's
    // job; Desktop just calls notification_alarm_start()/_stop() when it
    // decides an alarm is due.
    FuriTimer* alarm_timer;
    uint16_t alarm_pattern_step;
    bool alarm_beep_enabled;
    bool alarm_vibrate_enabled;
};

void notification_message_save_settings(NotificationApp* app);
void night_shift_timer_start(NotificationApp* app);
void night_shift_timer_stop(NotificationApp* app);
void rgb_backlight_update(float brightness);
void rgb_backlight_blank(void);
void rgb_backlight_set_led_static_color(uint8_t led, uint8_t index);
void rainbow_timer_start(NotificationApp* app);
void rainbow_timer_stop(NotificationApp* app);
void rainbow_timer_starter(NotificationApp* app);
const char* rgb_backlight_get_color_text(uint8_t index);
void rgb_backlight_get_color_rgb(uint8_t index, uint8_t* r, uint8_t* g, uint8_t* b);
uint8_t rgb_backlight_get_color_count(void);
void set_rgb_backlight_installed_variable(uint8_t var);
void set_rgb_backlight_white_mode_variable(uint8_t var);
bool rgb_backlight_should_drive_white_led(void);

void notification_alarm_start(NotificationApp* app, bool beep_enabled, bool vibrate_enabled);
void notification_alarm_stop(NotificationApp* app);

#ifdef __cplusplus
}
#endif
