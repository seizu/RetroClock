#include <Arduino.h>
#include <esp_heap_caps.h>
#include <freertos/task.h>
#include "globals.h"
#include "network.h"
#include "storage.h"
#include "weather.h"
#include "widgets.h"
#include "crypto.h"
#include "eggtimer.h"
#include "alarmclock.h"
#include "screens.h"
#include "fonts/Font_alphanumeric_7x11.h"
#include "fonts/Font_alphanumeric_7x22.h"
#include "fonts/Font_numeric_7x16.h"
#include "icons/icons.h"

Display display;

constexpr uint8_t MAIN_SCREEN_INDEX      = 0;
constexpr uint8_t BIG_CLOCK_SCREEN_INDEX = 1;
constexpr uint8_t CRYPTO_SCREEN_INDEX    = 2;
constexpr uint8_t EGG_TIMER_SCREEN_INDEX = 3;
constexpr uint8_t SCREENS                = 4;   // total number of screens

PixelFont wartrisFont;
PixelFont wartrisFont1x2;
PixelFont wartrisFont2x3;
// BSS buffers no heap alloc for fonts.
static constexpr char WARTRIS_CHARSET[] =
    " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]°_";
// Glyph count of the charset above; sizing the buffers from it keeps them exactly
// as large as needed (they are the two biggest static allocations in the build).
static constexpr size_t WARTRIS_CHARS = sizeof(WARTRIS_CHARSET) - 1;
static uint16_t wartrisFont1x2Buf[WARTRIS_CHARS *  8 * 16];  // 1x2 scale
static uint16_t wartrisFont2x3Buf[WARTRIS_CHARS * 16 * 24];  // 2x3 scale

void setup() {
    Serial.begin(115200);
    LOG_SDEBUG("[main] booting");


    // Config first: display.begin() autodetects the panel driver and needs
    // dc.panel_toggle to know whether to use the other one.
    initWebPrefs();

    if (!display.begin("/ColorMap.bmp", dc.panel_toggle)) {
        LOG_SERROR("[main] display init failed");
        return;
    }

    delay(1000);
    bool touch_restore = false;
    for (uint8_t i = 0; i < 10 && !touch_restore; i++) {
        touch_restore = display.getTouch();
        delay(30);
    }

    // Restore mode: back to the defaults (open AP, no UI login). The panel driver
    // keeps the stored setting for this boot, it is already initialized by now.
    if (touch_restore) {
        LOG_SWARNING("[main] touch held at boot, restore mode...");
        loadDefaultConfig();
    }

    initWifi(touch_restore);

    display.setRotation(dc.flip_screen ? 3 : 1);
    cbSetBrightness();   // apply the configured backlight brightness
    cbNTPConfigUpdate();

    display.registerFont(0, Font_alphanumeric_7x22);
    display.registerFont(1, Font_alphanumeric_7x11);
    display.registerFont(2, Font_numeric_7x16);
    display.registerIconSet(projectIcons);

    if (!wartrisFont.loadFromFile("/fonts/wartris128x32.bmp", 8, 8, WARTRIS_CHARSET)) {
        LOG_SERROR("[main] wartris font load failed");
    }

    wartrisFont1x2.copyScaledFrom(wartrisFont, 1, 2,
                              wartrisFont1x2Buf,
                              sizeof(wartrisFont1x2Buf) / sizeof(uint16_t));

    wartrisFont2x3.copyScaledFrom(wartrisFont, 2, 3,
                              wartrisFont2x3Buf,
                              sizeof(wartrisFont2x3Buf) / sizeof(uint16_t));

    weatherInit();
    cryptoInit();
    // LEDC ch 0; pin attaches only while a melody plays. The ILI9341 revision needs
    // its amp switched on (floating SHUTDOWN input), the ST7789 one has it wired on.
    buzzer.begin(SPEAKER_PIN, 0,
                 display.panelIsST7789() ? MelodyPlayer::NO_PIN : AMP_ENABLE_PIN);
    eggTimerInit();

    // === Screen 1: main (clock + seconds + weather) ===
    // The seconds style is a setting (dc.hourglass), not a touch target: only the
    // configured widget goes into the slot, and w == 0 switches its hit-test off.
    // Its old rect (34,5,5,16) claimed four rows more than it ever draws and
    // overlapped the weather slot, which sits first in this array and therefore
    // swallowed every tap in the lower right corner.
    Widget* secondsWidget = dc.hourglass ? static_cast<Widget*>(&widget_seconds)
                                         : static_cast<Widget*>(&widget_seconds02);
    static Slot mainSlots[] = {
        { {1,  1,  33, 3},  {&widget_clock},   1, 0 },
        { {34, 5,  0,  0},  {secondsWidget},   1, 0 },
        { {0,  18, 40, 9},  {&widget_weather, &widget_weather2}, 2, 0 },
    };
    static Screen mainScreen = {
        "main", mainSlots, 3,
        {0,  0, 10, 18},
        {30, 0, 10, 18},
        grey0
    };

    // === Screen 2: big clock + current temperature ===
    static Slot bigClockSlots[] = {
        { {4, 1,  33, 3}, {&widget_clock_big},    1, 0 },
        { {0, 20, 15, 2}, {&widget_temp_current}, 1, 0 },
    };
    static Screen bigClockScreen = {
        "big-clock", bigClockSlots, 2,
        {0,  0, 10, 18},
        {30, 0, 10, 18},
        grey0
    };

    // === Screen 3: crypto ===
    // Slot order matters: widget_crypto (display) MUST come before
    // widget_crypto_status. The display widget sets s_screenJustEntered on
    // activate, the status widget reads+clears it to distinguish a screen entry
    // from a real touch. Reversing the order breaks the timeframe-cycle logic.
    // Rects are hit-test only (drawing uses just x/y): 39 wide from x=1 keeps them
    // inside the 40-cell grid, and 17 rows stop the display slot one row short of
    // the status slot instead of swallowing its top row.
    static Slot cryptoSlots[] = {
        { {1,  1, 39, 17}, {&widget_crypto},        1, 0 },  // 6 asset rows
        { {1, 18, 39, 12}, {&widget_crypto_status}, 1, 0 },  // status + timeframe touch
    };
    static Screen cryptoScreen = {
        "crypto", cryptoSlots, 2,
        {0,  0, 10, 18},
        {30, 0, 10, 18},
        grey0
    };

    // === Screen 4: egg timer (countdown + M/S/START/STOP buttons) ===
    static Slot eggTimerSlots[] = {
        { {4, 1, 33, 3},          {&widget_eggtimer_display}, 1, 0 },
        { EGGTIMER_BUTTON_SLOT,   {&widget_eggtimer_buttons}, 1, 0 },  // derived from kButtons
    };
    static Screen eggTimerScreen = {
        "egg-timer", eggTimerSlots, 2,
        {0,  0, 10, 18},
        {30, 0, 10, 18},
        grey0
    };

    static Screen* screens[] = { &mainScreen, &bigClockScreen, &cryptoScreen, &eggTimerScreen };
    drawNetworkInfo();

    // show netWorkInfo Screen for 5sec while trying NTP update
    for(int i=0;i<25;i++) {
        updateNTP();
        delay(200);
    }

    uint8_t initialScreen = (dc.init_screen >= 1 && dc.init_screen <= SCREENS) ? (uint8_t)(dc.init_screen - 1) : 0;
    screensInit(screens, SCREENS, initialScreen);
}

// Screen-off gesture: hold the centre 8x8 grid-cell area (64x64 px) for ≥ 5 s.
// Wake: touch anywhere. Centre of the 40x30 grid → cells x16..23, y11..18.
constexpr Rect     SCREEN_OFF_ZONE    = { 16, 11, 8, 8 };
constexpr uint32_t SCREEN_OFF_HOLD_MS = 3000;

void loop() {
    handleWiFi();

    // A crypto price alert fired (raised by the crypto bg task): wake the screen,
    // jump to the crypto screen and play the dedicated alert sound (last-wins).
    if (cryptoTakeAlert()) {
        display.displayOn();
        screensSwitchTo(CRYPTO_SCREEN_INDEX);
        uint16_t s = dc.crypto_alert_sound;
        if (s < 1 || s > buzzer.count()) s = 1;
        buzzer.play(s - 1, dc.volume, 5UL * 60UL * 1000UL);
    }

    // No point pushing pixels while the backlight is off (content is retained in
    // the panel's RAM and reappears on wake).
    if (display.isDisplayOn()) screensRender();

    static bool     _touchPrev    = false;
    static uint32_t _pressStart   = 0;
    static int16_t  _pressX       = 0, _pressY = 0;
    static bool     _wakePress    = false;  // this press only woke the screen
    static bool     _offDone      = false;  // off already triggered this press
    static bool     _centerTapPending = false;  // crypto centre press: decide on release
    static uint32_t _lastActivity = millis();  // last touch (idle timer); seeded at first loop = boot end

    int16_t tx = 0, ty = 0;
    bool     touched = display.getTouch(&tx, &ty);
    uint32_t now     = millis();
    bool pressEdge   = touched && !_touchPrev;

    // Any ongoing touch counts as activity, not just the press edge otherwise a
    // long hold (e.g. egg-timer M/S auto-repeat) would let the idle timer expire
    // and blank the screen mid-hold. Idle now counts only from finger-up.
    if (touched) _lastActivity = now;

    if (pressEdge) {
        _pressStart = now; _pressX = tx; _pressY = ty; _offDone = false;
        if (!display.isDisplayOn()) {            // any touch wakes the screen…
            display.displayOn();
            _wakePress = true;                   // …and does nothing else
            LOG_SDEBUG("[main] wake screen");
        } else {
            _wakePress = false;
            LOG_SDEBUG("[main] pressed x=%d y=%d", (int)tx, (int)ty);
            if (eggTimerAlarmActive())   eggTimerSilenceAlarm();
            else if (buzzer.isPlaying()) buzzer.stop();   // silence the alarm clock
            else if (screensActiveIndex() == CRYPTO_SCREEN_INDEX &&
                     rectContainsPx(SCREEN_OFF_ZONE, tx, ty)) {
                // Centre press on the crypto screen: decide on release a short
                // tap clears the alert highlights (ack), a long hold turns the
                // screen off (SCREEN_OFF_ZONE hold logic below).
                _centerTapPending = true;
            }
            else                         screensHandleTouch(tx, ty);
        }
    }

    // Hold the centre cell long enough → turn the screen off.
    if (touched && !_wakePress && !_offDone && display.isDisplayOn() &&
        rectContainsPx(SCREEN_OFF_ZONE, _pressX, _pressY) &&
        (now - _pressStart) >= SCREEN_OFF_HOLD_MS) {
        display.displayOff();
        _offDone = true;
        LOG_SDEBUG("[main] screen off (centre hold)");
    }

    // Auto-off after inactivity (dc.display_off_sec: 0 = never, otherwise 10-300 s;
    // values below 10 are treated as 10). Suppressed while the alarm sounds so the
    // timer screen stays visible.
    uint16_t idleSec = dc.display_off_sec;
    if (idleSec > 0 && idleSec < 10) idleSec = 10;
    if (idleSec > 0 && display.isDisplayOn() && !eggTimerAlarmActive() && !buzzer.isPlaying() &&
        (now - _lastActivity) >= (uint32_t)idleSec * 1000) {
        display.displayOff();
        LOG_SDEBUG("[main] screen off (idle %us)", idleSec);
    }

    // Release of a centre press on the crypto screen: a short tap (no screen-off
    // hold) acknowledges the alerts and clears their highlights.
    if (_centerTapPending && !touched && _touchPrev) {
        if (!_offDone && (now - _pressStart) < SCREEN_OFF_HOLD_MS) {
            cryptoClearAlertHighlights();
            LOG_SDEBUG("[main] crypto: clear alert highlights (centre tap)");
        }
        _centerTapPending = false;
    }

    if (!touched) _wakePress = false;
    _touchPrev = touched;

    // Egg-timer hold ignores a wake press (and anything while the screen is off).
    eggTimerHandleHold(screensActiveIndex() == EGG_TIMER_SCREEN_INDEX,
                       touched && display.isDisplayOn() && !_wakePress, tx, ty);

    updateNTP();
    tickNightMode();   // day/night brightness on window transitions
    tickAlarmClock();  // fire the scheduled alarm when its time/weekday matches
    eggTimerTick();
    buzzer.tick();     // drive melody playback regardless of the active screen
    delay(10);
}
