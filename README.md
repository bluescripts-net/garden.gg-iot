# garden.gg Timelapse Camera

Firmware for a Seeed XIAO ESP32-S3 Sense that periodically captures JPEG photos and uploads them to a garden.gg plot. Includes a captive-portal provisioning flow, a built-in web UI for live preview and reconfiguration, and OTA updates.

## Hardware

- **Seeed XIAO ESP32-S3 Sense** (ESP32-S3 + OV2640 camera + 8 MB PSRAM, native USB-C)

No external programmer is needed — the board flashes over its built-in USB-C port.

## Build

This is a PlatformIO project. From the repo root:

```bash
pio run -t upload          # build + flash via USB
pio device monitor         # serial monitor at 115200 baud
```

The full PlatformIO configuration lives in [`platformio.ini`](platformio.ini). VS Code users can also run the bundled tasks (`Ctrl+Shift+B` for build + upload).

## First-boot provisioning

A freshly flashed device boots straight into provisioning mode and broadcasts an open access point named `GardenGG-Setup-XXXX`. Connect to it from a phone or laptop, the captive portal pops automatically (or visit `192.168.4.1`), and enter:

- WiFi SSID and password
- garden.gg API key (`gg_live_…`)
- garden.gg plot ID (the UUID in the plot URL)
- Capture interval

The device saves these to NVS flash, reboots, and starts capturing. See [`SETUP.md`](SETUP.md) for the full walkthrough including factory reset and OTA setup.

## Web UI

Once the device joins your WiFi, the serial log prints its IP. Browse to `http://<device-ip>/` to get:

- Live camera preview with manual refresh and 2-second auto-refresh
- "Capture & upload now" button
- Live stats (RSSI, chip temperature, uptime)
- Configuration form (WiFi, API key, plot picker, interval) — saving WiFi triggers a reboot

## Capture settings

| Setting | Value | Where |
|---|---|---|
| Resolution | UXGA (1600×1200) | [`src/main.cpp`](src/main.cpp) `initCamera()` |
| JPEG quality | 10 (lower = better) | same |
| Default interval | 1 minute | configurable per-device via the web UI |

JPEG quality below 10 at UXGA is unreliable on the OV2640 — frames silently fail to capture, so 10 is the practical floor.

## OTA updates

On boot and every 24 hours afterwards, the device fetches `https://garden.gg/firmware/esp32cam/latest.json`:

```json
{ "version": "1.1.0", "url": "https://garden.gg/firmware/esp32cam/v1.1.0.bin" }
```

If `version` differs from the on-device build, the new `.bin` is downloaded and applied. The current firmware version is defined in [`src/firmware_version.h`](src/firmware_version.h).

## Provisioning automation (optional)

[`provision.py`](provision.py) drives the provisioning portal end-to-end on Windows: it scans for the `GardenGG-Setup-XXXX` AP, joins it, POSTs the form, and reconnects to your main WiFi. Credentials are read from environment variables — never commit them:

```cmd
set GARDENGG_WIFI_SSID=your-network
set GARDENGG_WIFI_PASS=your-password
set GARDENGG_API_KEY=gg_live_...
set GARDENGG_PLOT_ID=your-plot-uuid
python provision.py
```

## Project layout

```
src/
  main.cpp           camera init, web server, capture loop, NVS settings
  provisioning.cpp   captive-portal AP for first-boot setup
  config.cpp         NVS read/write helpers (used by provisioning)
  ota.cpp            OTA update check + apply
  upload.cpp         multipart upload helper
  firmware_version.h current build version + OTA manifest URL
platformio.ini       build configuration (board, USB CDC, PSRAM)
provision.py         optional CLI provisioning helper (Windows)
watch-port.ps1       auto-flash when the device is plugged in
```
