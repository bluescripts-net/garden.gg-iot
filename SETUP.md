# GardenGG ESP32-CAM Setup Guide

## Installation

### 1. Install PlatformIO
```bash
pip install platformio
```

Or use VS Code extension: search "PlatformIO IDE" in extensions.

### 2. Build & Upload

**Default (Ctrl+Shift+B):** Builds and uploads via USB
```bash
pio run -t upload
```

**Monitor serial output:**
```bash
pio device monitor
```

## First Boot (Provisioning)

On a fresh flash with no saved config:
1. Device broadcasts AP: `GardenGG-Setup-XXXX` (XXXX = last 4 digits of MAC address)
2. Connect your phone/laptop to this AP
3. Browser should auto-pop a captive portal; if not, navigate to `192.168.4.1`
4. Fill in:
   - **WiFi SSID** — your home/office WiFi
   - **WiFi Password**
   - **API Key** — from garden.gg
   - **Plot ID** — from garden.gg
   - **Capture Interval** — milliseconds between photos (default 900000 = 15 min)
5. Click "Save & Reboot"

Device will save config to NVS flash, reboot, and begin capturing/uploading.

## Factory Reset

Hold the **BOOT button** (on the ESP32) for 5 seconds at startup. Device will erase config and re-enter provisioning mode.

## OTA Updates

Updates are served from [GitHub releases](https://github.com/bluescripts-net/garden.gg-iot/releases). See the OTA section of [README.md](README.md) for the full release workflow. In short: tag with CalVer (`v2026.04.28.0`), push the tag, GitHub Actions builds and attaches `firmware.bin`, and devices pick it up via the **Check for update** button or the auto-update toggle.

### Building locally

```bash
pio run
# Binary is at .pio/build/xiao_esp32s3_sense/firmware.bin
```

The version baked in matches `git describe --tags --always --dirty`. To inspect it without flashing:

```bash
pio run 2>&1 | grep FIRMWARE_VERSION
```

## Auto-Upload on Plug-In

On Windows, run the watch script to auto-upload when device is plugged in:
```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File watch-port.ps1
```

## Troubleshooting

**Camera won't initialize:**
- Check pin definitions in `initCamera()` (top of `src/main.cpp`) match your board
- Verify PSRAM is enabled in `platformio.ini` (`-DBOARD_HAS_PSRAM`)

**WiFi provisioning portal doesn't appear:**
- Ensure device is in AP mode (check serial output)
- Try navigating manually to `192.168.4.1`
- iOS requires "Ask to Join Network" dialog — accept it

**Upload fails with 401:**
- API key is invalid or expired
- Reset and re-enter provisioning

**OTA fails:**
- Ensure device has internet connectivity (and that DNS/HTTPS to api.github.com works on its WiFi)
- Confirm the latest GitHub release actually has a `firmware.bin` asset attached — releases without one won't trigger the install button
- Watch the serial output during install: `OTA: downloading N bytes` should appear, followed by `OTA: flashed N bytes, rebooting`

## Configuration Storage

All settings stored in ESP32 NVS (non-volatile storage / flash):
- Namespace: `gardengg`
- Keys: `wifi_ssid`, `wifi_pass`, `api_key`, `plot_id`, `intvl_ms`, `auto_upd`

To inspect/edit manually, use `espefuse.py` or a dedicated NVS editor tool.
