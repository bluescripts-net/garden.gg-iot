# Garden.gg ESP32-CAM Timelapse

Periodically captures photos with an AI-Thinker ESP32-CAM and uploads them to your garden.gg plot.

## Hardware

- **AI-Thinker ESP32-CAM** (with OV2640 camera)
- **FTDI USB-to-Serial adapter** (3.3V) for programming

### Wiring (for flashing)

| FTDI   | ESP32-CAM |
|--------|-----------|
| GND    | GND       |
| VCC    | 5V        |
| TX     | U0R       |
| RX     | U0T       |
| —      | IO0 → GND *(only during upload — disconnect after flashing)* |

> **Important:** Connect IO0 to GND before powering on to enter flash mode. Remove the jumper after uploading, then press RST to boot normally.

## Setup

### 1. Get your credentials

- **API Key:** garden.gg → Settings → API Keys → Create Key
- **Plot ID:** from the URL when viewing your plot (`garden.gg/plots/{PLOT_ID}`)

### 2. Configure the sketch

Open `gardengg-esp32-cam.ino` and edit the defines at the top:

```cpp
#define WIFI_SSID          "your-network"
#define WIFI_PASS          "your-password"
#define API_KEY            "gg_live_xxxxxxxx..."
#define PLOT_ID            "your-plot-uuid"
```

Optional settings:

| Define | Default | Description |
|--------|---------|-------------|
| `CAPTURE_INTERVAL_MS` | `900000` (15 min) | Time between captures |
| `IMAGE_QUALITY` | `12` | JPEG quality (10–63, lower = better quality) |
| `FRAME_SIZE` | `FRAMESIZE_SVGA` | Resolution (800×600) |
| `USE_DEEP_SLEEP` | `false` | Enable deep sleep for battery power |
| `GARDENGG_USE_HTTPS` | `true` | Use HTTPS (disable for local testing) |

### 3. Flash

**Arduino IDE:**
1. Install the ESP32 board package: File → Preferences → add `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json` to Board Manager URLs
2. Tools → Board → "AI Thinker ESP32-CAM"
3. Tools → Port → select your FTDI adapter
4. Upload

**PlatformIO:**
```ini
[env:esp32cam]
platform = espressif32
board = esp32cam
framework = arduino
monitor_speed = 115200
```

### 4. Local testing

Point at your dev server:

```cpp
#define GARDENGG_HOST      "192.168.1.37"
#define GARDENGG_PORT      8080
#define GARDENGG_USE_HTTPS false
```

## Operation

- On boot, captures and uploads one photo immediately
- Then repeats every `CAPTURE_INTERVAL_MS` (default 15 minutes)
- LED on GPIO 4 flashes briefly during capture
- Open Serial Monitor at 115200 baud for status/debug output
- With `USE_DEEP_SLEEP` enabled, the board resets and runs `setup()` each cycle (lower power, but slower wake)
