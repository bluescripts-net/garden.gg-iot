# Arduino IDE → PlatformIO Migration Complete

## What's Changed

✅ **PlatformIO setup** — `platformio.ini` replaces Arduino IDE configuration  
✅ **Modular codebase** — 7 modules in `src/` (camera, upload, config, provisioning, OTA, firmware_version, main)  
✅ **NVS config storage** — WiFi, API key, plot ID saved to device flash (no more hardcoding)  
✅ **Captive portal provisioning** — On first boot, configure via AP `GardenGG-Setup-XXXX`  
✅ **OTA updates** — Device polls `garden.gg/firmware/esp32cam/latest.json` every 24h  
✅ **VS Code tasks** — `Ctrl+Shift+B` = build + upload  
✅ **Auto-upload script** — `watch-port.ps1` triggers upload when device is plugged in  

## Next Steps

1. **Install PlatformIO:**
   ```bash
   pip install platformio
   ```

2. **Build & upload:**
   ```bash
   pio run -t upload
   ```

3. **First boot** — device enters provisioning mode if no config exists. See `SETUP.md` for details.

4. **OTA server** — You need to host `latest.json` + `.bin` files at `garden.gg/firmware/esp32cam/`.  
   Example manifest:
   ```json
   {
     "version": "1.0.0",
     "url": "https://garden.gg/firmware/esp32cam/v1.0.0.bin"
   }
   ```

## Files

- `platformio.ini` — build config
- `src/main.cpp` — orchestrates all modules
- `src/config.*` — NVS persistence
- `src/camera.*` — camera init + capture
- `src/upload.*` — HTTPS multipart POST
- `src/provisioning.*` — AP + captive portal
- `src/ota.*` — firmware update check/download
- `src/firmware_version.h` — version + OTA URL
- `.vscode/tasks.json` — VS Code build tasks
- `watch-port.ps1` — auto-upload on plug-in
- `SETUP.md` — user guide

## Original Files

The original `.ino` file is preserved at `gardengg-esp32-cam/gardengg-esp32-cam.ino` for reference.
