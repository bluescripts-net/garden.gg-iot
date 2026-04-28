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

Place firmware binary at `https://garden.gg/firmware/esp32cam/latest.json` with this structure:
```json
{
  "version": "1.1.0",
  "url": "https://garden.gg/firmware/esp32cam/v1.1.0.bin"
}
```

Device checks for updates every 24 hours. On reboot, firmware version is printed to serial.

### Build OTA Binary

```bash
pio run
# Binary is at .pio/build/freenove_esp32s3_cam/firmware.bin
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
- Ensure device has internet connectivity
- Check OTA manifest URL is reachable (browser test)
- Serial output shows error code — compare against ESP error codes

## Configuration Storage

All settings stored in ESP32 NVS (non-volatile storage / flash):
- Namespace: `gardengg`
- Keys: `wifi_ssid`, `wifi_pass`, `api_key`, `plot_id`, `intvl_ms`

To inspect/edit manually, use `espefuse.py` or a dedicated NVS editor tool.
