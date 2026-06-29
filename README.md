# RetroClock
Pixel-art clock / weather / crypto ticker / egg timer firmware for the ESP32 Cheap Yellow Display, no API Key required, web-configurable.

A retro-style clock, weather and crypto/asset-ticker display for the ESP32 Cheap Yellow Display (CYD — board `2432S028`), built with PlatformIO and LovyanGFX.

All graphics are drawn from an 8×8-pixel block grid with a 32-color palette, giving everything a consistent pixel-art look.

## Install
A one-click browser flasher for [RetroClock](https://seizu.github.io/RetroClockInstaller) Works only in Chrome or Edge on desktop (Web Serial)

## Configuration

On first boot, RetroClock will:
1. Start in Access Point (AP) mode
2. Create a WiFi network e.g. named SSB178C1
3. Display the configuration IP address on the LCD screen

### Web Interface Access

**Default AP Mode IP:** `10.100.10.1`
**Default AP Password:** `RetroClock`

1. Connect to the RetroClock WiFi network
2. Open a web browser and navigate to `http://10.100.10.1`
3. Adjust the network setting
4. Save & reboot

## Features

- **Retro block graphics** — every visual is rendered from an 8×8 px grid with a 32-color palette loaded from a BMP color matrix at boot
- **Four swappable screens** (tap the left/right edge to switch):
  1. **Main** — time (HH:MM) in a large pixel font, seconds as a dot grid, and a 5-hour weather forecast (two pages: +0..+4 ↔ +5..+9)
  2. **Big clock** — oversized clock plus the current temperature
  3. **Crypto / assets** — up to 6 configurable symbols with live price and percentage change over a selectable timeframe; tap the status row to cycle 9 timeframes (1H · 4H · 12H · 1D · 1W · 1M · 3M · 6M · 1Y)
  4. **Egg timer** — MM:SS countdown with M / S / START / STOP buttons (tap-to-+1, hold for accelerating auto-repeat) and a WebPrefs-selectable alarm tone at 00:00 (1–5). A small non-blocking engine plays note/duration arrays straight through the LEDC peripheral (no `tone()` task → no heap churn); tones are converted from RTTTL ringtone strings.
- **Live data**
  - Weather from [open-meteo.com](https://open-meteo.com) (via the `ApiClient` helper)
  - Prices & history from the Binance Futures API (`fapi.binance.com`): `indexPriceKlines` for 1h/1d/1w history buckets (100 bars each) and `ticker/price` per symbol for live quotes
- **Robust networking** — a single keep-alive TLS session per fetch batch (one handshake instead of many) plus a contiguous-heap guard avoid the mbedtls fragmentation failures (`-32512`) the CYD is prone to; fetches run on a FreeRTOS background task (Core 0) so the render loop never blocks
- **Power / display control** — configurable backlight brightness (1–100 %), optional auto screen-off after inactivity (10–300 s, 0 = never), a night mode that dims (or fully blanks at 0 %) during a configurable HH:MM window (wraps past midnight), and a manual screen-off touch gesture; the screen wakes on any touch and on the egg-timer alarm
- **Web settings UI** — EEPROM-backed configuration (colors, brightness, night mode, screen-off timeout, alarm tone, symbols, location, Wi-Fi, NTP, …) served over an async web server; hold touch at boot for AP/restore mode
- **Touch navigation** — edge zones switch screens; tapping a widget slot cycles its candidates

## Hardware

| Component | Detail |
|---|---|
| Board | ESP32 `2432S028` (Cheap Yellow Display) |
| Display | 320×240 ST7789, landscape, driven by LovyanGFX |
| Touch | XPT2046 resistive (configured in `lib/RetroGfx`) |
| Speaker | Passive buzzer on GPIO 26 (egg-timer alarm) |

> **Note:** newer `2432S028` batches ship with an **ST7789** panel (not ILI9341). `RetroGfx` is configured accordingly.

## Software

| Dependency | Purpose |
|---|---|
| [LovyanGFX](https://github.com/lovyan03/LovyanGFX) | Display + touch driver |
| [ArduinoJson](https://arduinojson.org) | Weather API JSON parsing |
| [ESPAsyncWebServer](https://github.com/esphome/ESPAsyncWebServer) | Web-based settings UI |

## Grid Coordinate System

The display is a **40×30 grid of 8×8 px cells** (landscape 320×240). All widget positions and sizes are specified in grid cells, not pixels.

## Touch Zones

| Area | Action |
|---|---|
| Left edge (grid x 0–9) | Previous screen |
| Right edge (grid x 30–39) | Next screen |
| Weather area (main screen) | Toggle forecast page (+0..+4 ↔ +5..+9) |
| Status row (crypto screen) | Cycle timeframe |
| MIN / SEC / START / STOP (egg-timer) | Set/start/stop the countdown |
| Anywhere, while the alarm sounds | Silence the egg-timer alarm |
| Centre 8×8-cell area, held ~3 s | Turn the screen (backlight) off |
| Anywhere, while the screen is off | Turn the screen back on |

Hold touch during the first ~2 s after boot to enter AP/restore mode.
