#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "esp_camera.h"
#include "provisioning.h"
#include "ota.h"
#include "firmware_version.h"

#define AUTO_UPDATE_CHECK_INTERVAL_MS (6UL * 60UL * 60UL * 1000UL)  // 6 hours

#define DEFAULT_WIFI_SSID ""
#define DEFAULT_WIFI_PASS ""
#define DEFAULT_API_KEY ""
#define DEFAULT_PLOT_ID ""
#define DEFAULT_INTERVAL_MS (60 * 1000)
#define LED_PIN 21

// XIAO ESP32-S3 Sense camera pins
#define PWDN_GPIO -1
#define RESET_GPIO -1
#define XCLK_GPIO 10
#define SIOD_GPIO 40
#define SIOC_GPIO 39
#define D7_GPIO 48
#define D6_GPIO 11
#define D5_GPIO 12
#define D4_GPIO 14
#define D3_GPIO 16
#define D2_GPIO 18
#define D1_GPIO 17
#define D0_GPIO 15
#define VSYNC_GPIO 38
#define HREF_GPIO 47
#define PCLK_GPIO 13

enum State { CONNECTING, CONNECTED, PROVISIONING };
static State state = CONNECTING;
static unsigned long stateStart = 0;
static unsigned long lastBlink = 0;
static unsigned long lastCapture = 0;
static unsigned long lastAutoUpdateCheck = 0;
static bool cameraOk = false;

static WebServer webServer(80);
static String wifiSsid = DEFAULT_WIFI_SSID;
static String wifiPass = DEFAULT_WIFI_PASS;
static String apiKey = DEFAULT_API_KEY;
static String plotId = DEFAULT_PLOT_ID;
static uint32_t captureInterval = DEFAULT_INTERVAL_MS;
static bool autoUpdate = false;

void loadSettings() {
  Preferences prefs;
  if (prefs.begin("gardengg", true)) {
    wifiSsid = prefs.getString("wifi_ssid", DEFAULT_WIFI_SSID);
    wifiPass = prefs.getString("wifi_pass", DEFAULT_WIFI_PASS);
    apiKey = prefs.getString("api_key", DEFAULT_API_KEY);
    plotId = prefs.getString("plot_id", DEFAULT_PLOT_ID);
    captureInterval = prefs.getUInt("intvl_ms", DEFAULT_INTERVAL_MS);
    autoUpdate = prefs.getBool("auto_upd", false);
    prefs.end();
    Serial.printf("Settings: wifi=%s plot=%s interval=%ums auto_update=%d\n",
                  wifiSsid.c_str(), plotId.c_str(), captureInterval, autoUpdate);
    Serial.flush();
  }
}

void saveSettings() {
  Preferences prefs;
  if (prefs.begin("gardengg", false)) {
    prefs.putString("wifi_ssid", wifiSsid);
    prefs.putString("wifi_pass", wifiPass);
    prefs.putString("api_key", apiKey);
    prefs.putString("plot_id", plotId);
    prefs.putUInt("intvl_ms", captureInterval);
    prefs.putBool("auto_upd", autoUpdate);
    prefs.end();
    Serial.println("Settings saved");
    Serial.flush();
  }
}

bool initCamera() {
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_pwdn = PWDN_GPIO;
  config.pin_reset = RESET_GPIO;
  config.pin_xclk = XCLK_GPIO;
  config.pin_sccb_sda = SIOD_GPIO;
  config.pin_sccb_scl = SIOC_GPIO;
  config.pin_d7 = D7_GPIO;
  config.pin_d6 = D6_GPIO;
  config.pin_d5 = D5_GPIO;
  config.pin_d4 = D4_GPIO;
  config.pin_d3 = D3_GPIO;
  config.pin_d2 = D2_GPIO;
  config.pin_d1 = D1_GPIO;
  config.pin_d0 = D0_GPIO;
  config.pin_vsync = VSYNC_GPIO;
  config.pin_href = HREF_GPIO;
  config.pin_pclk = PCLK_GPIO;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.jpeg_quality = 10;
  config.frame_size = FRAMESIZE_SVGA;
  config.fb_location = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_count = psramFound() ? 2 : 1;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    Serial.println("PSRAM: found - UXGA");
  } else {
    Serial.println("PSRAM: none - SVGA");
  }
  Serial.flush();

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera: FAILED 0x%x\n", err);
    Serial.flush();
    return false;
  }

  Serial.println("Camera: OK");
  Serial.flush();
  return true;
}

bool uploadPhoto(camera_fb_t* fb) {
  WiFiClientSecure client;
  client.setInsecure();

  if (!client.connect("garden.gg", 443)) {
    Serial.println("Upload: connect FAILED");
    Serial.flush();
    return false;
  }

  String boundary = "----ESP32";
  String header = "--" + boundary + "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"photo.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
  String footer = "\r\n--" + boundary + "--\r\n";
  size_t contentLen = header.length() + fb->len + footer.length();

  String path = "/api/v1/iot/photo?api_key=" + apiKey + "&plot_id=" + plotId;

  client.print("POST " + path + " HTTP/1.1\r\n");
  client.print("Host: garden.gg\r\n");
  client.print("Content-Type: multipart/form-data; boundary=" + boundary + "\r\n");
  client.print("Content-Length: " + String(contentLen) + "\r\n");
  client.print("Connection: close\r\n\r\n");
  client.print(header);

  const size_t CHUNK = 4096;
  size_t sent = 0;
  while (sent < fb->len) {
    size_t toSend = fb->len - sent;
    if (toSend > CHUNK) toSend = CHUNK;
    client.write(fb->buf + sent, toSend);
    sent += toSend;
  }

  client.print(footer);
  client.flush();

  unsigned long timeout = millis();
  while (!client.available() && millis() - timeout < 15000) delay(10);

  bool success = false;
  if (client.available()) {
    String response = client.readStringUntil('\n');
    Serial.println(response);
    Serial.flush();
    if (response.indexOf("201") > 0 || response.indexOf("200") > 0) {
      success = true;
    }
  }

  client.stop();
  return success;
}

bool captureAndUpload() {
  camera_fb_t* fb = nullptr;
  for (int i = 0; i < 3; i++) {
    fb = esp_camera_fb_get();
    if (fb && fb->len > 1000) break;
    Serial.printf("Capture attempt %d: %s (len=%u)\n", i + 1,
                  fb ? "too small" : "null", fb ? fb->len : 0);
    Serial.flush();
    if (fb) esp_camera_fb_return(fb);
    fb = nullptr;
    delay(500);
  }

  if (!fb) {
    Serial.println("Capture: FAILED");
    Serial.flush();
    return false;
  }

  Serial.printf("Capture: %u bytes\n", fb->len);
  Serial.flush();

  bool ok = uploadPhoto(fb);
  esp_camera_fb_return(fb);
  return ok;
}

void handleRoot() {
  String html = R"HTML(<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>garden🍅gg camera</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="preconnect" href="https://fonts.googleapis.com">
  <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
  <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700;800&display=swap" rel="stylesheet">
  <style>
    * { box-sizing: border-box; }
    body {
      font-family: 'Inter', -apple-system, sans-serif;
      max-width: 800px;
      margin: 0 auto;
      padding: 24px;
      background: #fefdfb;
      color: #2a2520;
      line-height: 1.5;
    }
    h1 {
      font-size: 2rem;
      font-weight: 800;
      margin: 0 0 4px;
      letter-spacing: -0.02em;
    }
    h1 .garden { color: #16a34a; }
    h1 .tomato { font-size: 0.7em; vertical-align: middle; }
    h1 .sub { color: #6b5d4f; font-weight: 500; margin-left: 8px; }
    .subtitle { color: #857664; margin: 0 0 20px; font-size: 0.95rem; }

    .status-bar {
      background: #fef8f0;
      border: 1px solid #f0e6d6;
      border-radius: 12px;
      padding: 14px 18px;
      margin-bottom: 20px;
      display: flex;
      flex-wrap: wrap;
      gap: 18px;
      font-size: 0.9rem;
    }
    .stat { display: flex; align-items: center; gap: 6px; }
    .stat-value { font-weight: 600; color: #2a2520; }
    .stat-label { color: #857664; }

    .image-wrap {
      position: relative;
      background: #f5efe4;
      border-radius: 14px;
      overflow: hidden;
      border: 1px solid #e8ddc8;
    }
    img { display: block; width: 100%; height: auto; }

    .controls {
      display: flex;
      gap: 10px;
      margin: 16px 0 24px;
      flex-wrap: wrap;
      align-items: center;
    }
    button {
      background: #16a34a;
      color: white;
      border: none;
      padding: 10px 18px;
      border-radius: 10px;
      cursor: pointer;
      font-family: inherit;
      font-size: 0.9rem;
      font-weight: 600;
      transition: background 0.15s;
    }
    button:hover { background: #15803d; }
    button.secondary {
      background: white;
      color: #2a2520;
      border: 1px solid #e8ddc8;
    }
    button.secondary:hover { background: #f5efe4; }
    .toggle { display: flex; align-items: center; gap: 6px; color: #6b5d4f; font-size: 0.9rem; }
    .toggle input { width: auto; margin: 0; }

    .card {
      background: white;
      border: 1px solid #e8ddc8;
      border-radius: 14px;
      padding: 24px;
      margin-top: 20px;
    }
    .card h2 {
      margin: 0 0 16px;
      font-size: 1.2rem;
      font-weight: 700;
      color: #2a2520;
    }
    label {
      display: block;
      margin: 14px 0 6px;
      font-size: 0.85rem;
      font-weight: 600;
      color: #6b5d4f;
    }
    label:first-child { margin-top: 0; }
    input, select {
      width: 100%;
      padding: 10px 12px;
      border: 1px solid #e8ddc8;
      border-radius: 10px;
      background: #fefdfb;
      color: #2a2520;
      font-family: inherit;
      font-size: 0.95rem;
      transition: border-color 0.15s;
    }
    input:focus, select:focus {
      outline: none;
      border-color: #16a34a;
    }
    .save-btn { margin-top: 20px; width: 100%; padding: 12px; }
    @keyframes spin { from { transform: rotate(0deg); } to { transform: rotate(360deg); } }
  </style>
</head>
<body>
  <h1>🌱 <span class="garden">garden</span><span class="tomato">🍅</span>gg <span class="sub">camera</span></h1>
  <p class="subtitle">Timelapse uploader for your plot</p>

  <div class="status-bar">
    <div class="stat"><span>📡</span> <span class="stat-label">IP</span> <span class="stat-value">)HTML" + WiFi.localIP().toString() + R"HTML(</span></div>
    <div class="stat"><span>📶</span> <span class="stat-label">Signal</span> <span class="stat-value" id="rssi">)HTML" + String(WiFi.RSSI()) + R"HTML( dBm</span></div>
    <div class="stat"><span>🌡️</span> <span class="stat-label">Chip</span> <span class="stat-value" id="temp">)HTML" + String(temperatureRead(), 1) + R"HTML(&deg;C</span></div>
    <div class="stat"><span>⏱️</span> <span class="stat-label">Uptime</span> <span class="stat-value" id="uptime">)HTML" + String(millis() / 1000) + R"HTML(s</span></div>
    <div class="stat"><span>🏷️</span> <span class="stat-label">Firmware</span> <span class="stat-value">)HTML" FIRMWARE_VERSION R"HTML(</span></div>
  </div>

  <div class="image-wrap">
    <img id="preview" src="/capture?t=0" alt="Camera view">
  </div>

  <div class="controls">
    <button onclick="refresh()">🔄 Refresh</button>
    <button onclick="captureNow()" class="secondary">📸 Capture &amp; Upload</button>
    <label class="toggle">
      <input type="checkbox" id="autorefresh" onchange="toggleAuto()">
      <span>Auto-refresh (2s)</span>
    </label>
  </div>

  <div class="card">
    <h2>🚀 Firmware</h2>
    <div style="font-size: 0.9rem; color: #6b5d4f;">
      Current version: <strong style="color: #2a2520;">)HTML" FIRMWARE_VERSION R"HTML(</strong>
    </div>
    <div style="display: flex; gap: 10px; margin: 14px 0; flex-wrap: wrap;">
      <button type="button" onclick="checkUpdate()" class="secondary" id="checkBtn">Check for update</button>
      <button type="button" onclick="installUpdate()" id="installBtn" style="display:none;">Install update</button>
    </div>
    <div id="otaStatus" style="font-size: 0.9rem; color: #857664;"></div>
    <div id="otaChangelog" style="display:none; margin-top: 12px; padding: 12px; background: #f5efe4; border-radius: 8px; font-family: ui-monospace, monospace; font-size: 0.85rem; white-space: pre-wrap; max-height: 240px; overflow-y: auto;"></div>
  </div>

  <div class="card">
    <h2>⚙️ Configuration</h2>
    <form action="/config" method="POST">
      <label>📶 WiFi SSID</label>
      <input type="text" name="wifi_ssid" value=")HTML" + wifiSsid + R"HTML(" required>

      <label>🔒 WiFi Password</label>
      <input type="password" name="wifi_pass" value=")HTML" + wifiPass + R"HTML(">

      <label>🔑 API Key</label>
      <input type="text" name="api_key" value=")HTML" + apiKey + R"HTML(" required>

      <label>🌿 Plot</label>
      <div style="display: flex; gap: 8px; align-items: stretch;">
        <select name="plot_id" id="plotSelect" required style="flex: 1;">
          <option value=")HTML" + plotId + R"HTML(" selected>)HTML" + plotId + R"HTML(</option>
        </select>
        <button type="button" onclick="loadPlots()" class="secondary" style="padding: 0 12px; min-width: 44px;" title="Refresh plots">
          <span id="refreshIcon">↻</span>
        </button>
      </div>
      <div id="plotStatus" style="font-size: 0.8rem; color: #857664; margin-top: 4px;"></div>

      <label>⏲️ Capture Interval</label>
      <select name="interval">
        <option value="30000")HTML" + (captureInterval == 30000 ? " selected" : "") + R"HTML(>30 seconds</option>
        <option value="60000")HTML" + (captureInterval == 60000 ? " selected" : "") + R"HTML(>1 minute</option>
        <option value="300000")HTML" + (captureInterval == 300000 ? " selected" : "") + R"HTML(>5 minutes</option>
        <option value="600000")HTML" + (captureInterval == 600000 ? " selected" : "") + R"HTML(>10 minutes</option>
        <option value="900000")HTML" + (captureInterval == 900000 ? " selected" : "") + R"HTML(>15 minutes</option>
        <option value="1800000")HTML" + (captureInterval == 1800000 ? " selected" : "") + R"HTML(>30 minutes</option>
        <option value="3600000")HTML" + (captureInterval == 3600000 ? " selected" : "") + R"HTML(>1 hour</option>
        <option value="7200000")HTML" + (captureInterval == 7200000 ? " selected" : "") + R"HTML(>2 hours</option>
        <option value="21600000")HTML" + (captureInterval == 21600000 ? " selected" : "") + R"HTML(>6 hours</option>
      </select>

      <label class="toggle" style="margin-top: 16px;">
        <input type="checkbox" name="auto_update")HTML" + (autoUpdate ? " checked" : "") + R"HTML(>
        <span>Automatically install firmware updates (checks every 6 hours)</span>
      </label>

      <button type="submit" class="save-btn">💾 Save Configuration</button>
    </form>
  </div>

  <script>
    let autoTimer = null;
    function refresh() {
      document.getElementById('preview').src = '/capture?t=' + Date.now();
    }
    function toggleAuto() {
      if (document.getElementById('autorefresh').checked) {
        autoTimer = setInterval(refresh, 2000);
      } else {
        clearInterval(autoTimer);
      }
    }
    function captureNow() {
      fetch('/snap').then(r => r.text()).then(t => { alert(t); refresh(); });
    }
    async function updateStats() {
      try {
        const r = await fetch('/stats');
        const s = await r.json();
        document.getElementById('rssi').textContent = s.rssi + ' dBm';
        document.getElementById('temp').innerHTML = s.temp.toFixed(1) + '&deg;C';
        document.getElementById('uptime').textContent = s.uptime + 's';
      } catch(e) {}
    }
    setInterval(updateStats, 3000);

    const currentPlotId = ")HTML" + plotId + R"HTML(";

    async function loadPlots() {
      const apiKey = document.querySelector('input[name="api_key"]').value.trim();
      const status = document.getElementById('plotStatus');
      const icon = document.getElementById('refreshIcon');
      const select = document.getElementById('plotSelect');

      if (!apiKey) {
        status.textContent = 'Enter an API key to load plots';
        return;
      }

      icon.style.display = 'inline-block';
      icon.style.animation = 'spin 1s linear infinite';
      status.textContent = 'Loading plots…';

      try {
        const r = await fetch('/plots?api_key=' + encodeURIComponent(apiKey));
        const data = await r.json();

        if (data.error) {
          status.textContent = '⚠️ ' + data.error;
          icon.style.animation = '';
          return;
        }

        const plots = Array.isArray(data) ? data : (data.plots || data.data || []);

        if (plots.length === 0) {
          status.textContent = 'No plots found for this API key';
          icon.style.animation = '';
          return;
        }

        select.innerHTML = '';
        plots.forEach(p => {
          const opt = document.createElement('option');
          opt.value = p.id || p.plot_id || p.uuid || '';
          const name = p.name || p.title || p.label || opt.value;
          opt.textContent = name + (p.id ? ' (' + p.id.substring(0, 8) + '…)' : '');
          if (opt.value === currentPlotId) opt.selected = true;
          select.appendChild(opt);
        });

        status.textContent = '✓ Loaded ' + plots.length + ' plot' + (plots.length !== 1 ? 's' : '');
      } catch(e) {
        status.textContent = '⚠️ Failed: ' + e.message;
      }
      icon.style.animation = '';
    }

    // Load plots on startup and when API key changes
    loadPlots();
    let apiKeyTimer = null;
    document.querySelector('input[name="api_key"]').addEventListener('input', () => {
      clearTimeout(apiKeyTimer);
      apiKeyTimer = setTimeout(loadPlots, 600);
    });

    let pendingDownloadUrl = null;

    async function checkUpdate() {
      const status = document.getElementById('otaStatus');
      const changelog = document.getElementById('otaChangelog');
      const installBtn = document.getElementById('installBtn');
      const checkBtn = document.getElementById('checkBtn');

      checkBtn.disabled = true;
      status.textContent = 'Checking GitHub for releases…';
      changelog.style.display = 'none';
      installBtn.style.display = 'none';

      try {
        const r = await fetch('/ota/check');
        const data = await r.json();

        if (data.error) {
          status.textContent = '⚠️ ' + data.error;
          return;
        }

        if (data.hasUpdate) {
          status.innerHTML = '✨ <strong>' + data.latest + '</strong> available (you are on ' + data.current + ')';
          changelog.textContent = data.changelog || '(no changelog)';
          changelog.style.display = 'block';
          installBtn.style.display = 'inline-block';
          installBtn.textContent = 'Install ' + data.latest;
          pendingDownloadUrl = data.downloadUrl;
        } else {
          status.textContent = '✓ Up to date (' + data.current + ')';
        }
      } catch(e) {
        status.textContent = '⚠️ ' + e.message;
      } finally {
        checkBtn.disabled = false;
      }
    }

    async function installUpdate() {
      const status = document.getElementById('otaStatus');
      const installBtn = document.getElementById('installBtn');
      const checkBtn = document.getElementById('checkBtn');

      if (!confirm('Install the new firmware? The device will reboot.')) return;

      installBtn.disabled = true;
      checkBtn.disabled = true;
      status.textContent = 'Downloading and flashing… do not unplug. Page will reload in ~45s.';

      try {
        const r = await fetch('/ota/install', { method: 'POST' });
        const data = await r.json().catch(() => ({}));
        if (data.error) {
          status.textContent = '⚠️ ' + data.error;
          installBtn.disabled = false;
          checkBtn.disabled = false;
          return;
        }
        // Device is rebooting. Wait then reload.
        setTimeout(() => window.location.reload(), 45000);
      } catch(e) {
        // Connection drop is expected during reboot — treat as success.
        status.textContent = 'Device rebooting… reloading shortly.';
        setTimeout(() => window.location.reload(), 45000);
      }
    }
  </script>
</body>
</html>)HTML";

  webServer.send(200, "text/html; charset=utf-8", html);
}

void handleCapture() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    webServer.send(500, "text/plain", "Capture failed");
    return;
  }

  webServer.setContentLength(fb->len);
  webServer.send(200, "image/jpeg", "");
  WiFiClient client = webServer.client();

  const size_t CHUNK = 4096;
  size_t sent = 0;
  while (sent < fb->len) {
    size_t toSend = fb->len - sent;
    if (toSend > CHUNK) toSend = CHUNK;
    client.write(fb->buf + sent, toSend);
    sent += toSend;
  }

  esp_camera_fb_return(fb);
}

void handleSnap() {
  bool ok = captureAndUpload();
  if (ok) {
    lastCapture = millis();
    webServer.send(200, "text/plain", "Photo captured and uploaded!");
  } else {
    webServer.send(500, "text/plain", "Capture/upload failed");
  }
}

void handleConfig() {
  if (!webServer.hasArg("api_key") || !webServer.hasArg("plot_id")) {
    webServer.send(400, "text/plain", "Missing parameters");
    return;
  }

  bool wifiChanged = false;
  if (webServer.hasArg("wifi_ssid")) {
    String newSsid = webServer.arg("wifi_ssid");
    String newPass = webServer.arg("wifi_pass");
    if (newSsid.length() > 0 && (newSsid != wifiSsid || newPass != wifiPass)) {
      wifiSsid = newSsid;
      wifiPass = newPass;
      wifiChanged = true;
    }
  }

  apiKey = webServer.arg("api_key");
  plotId = webServer.arg("plot_id");

  if (webServer.hasArg("interval")) {
    uint32_t newInterval = webServer.arg("interval").toInt();
    if (newInterval >= 10000) {
      captureInterval = newInterval;
    }
  }

  autoUpdate = webServer.hasArg("auto_update");

  saveSettings();

  if (wifiChanged) {
    webServer.send(200, "text/html",
      "<html><body style='font-family:Inter,sans-serif;padding:40px;text-align:center;'>"
      "<h2>✅ Saved — rebooting to connect to new WiFi</h2>"
      "<p>The device will reconnect to <strong>" + wifiSsid + "</strong>.</p>"
      "<p>Find its new IP on your router or reload after a minute.</p>"
      "</body></html>");
    delay(1500);
    ESP.restart();
    return;
  }

  webServer.sendHeader("Location", "/");
  webServer.send(303);
}

void handleStats() {
  String json = "{\"rssi\":" + String(WiFi.RSSI()) +
                ",\"temp\":" + String(temperatureRead(), 2) +
                ",\"uptime\":" + String(millis() / 1000) +
                ",\"heap\":" + String(ESP.getFreeHeap()) + "}";
  webServer.send(200, "application/json", json);
}

void handlePlots() {
  String key = webServer.hasArg("api_key") ? webServer.arg("api_key") : apiKey;

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10);

  if (!client.connect("garden.gg", 443)) {
    webServer.send(502, "application/json", "{\"error\":\"connect failed\"}");
    return;
  }

  String path = "/api/v1/iot/plots?api_key=" + key;
  client.print("GET " + path + " HTTP/1.1\r\n");
  client.print("Host: garden.gg\r\n");
  client.print("Accept: application/json\r\n");
  client.print("Connection: close\r\n\r\n");
  client.flush();

  unsigned long timeout = millis();
  while (!client.available() && millis() - timeout < 10000) delay(10);

  if (!client.available()) {
    webServer.send(504, "application/json", "{\"error\":\"timeout\"}");
    client.stop();
    return;
  }

  String statusLine = client.readStringUntil('\n');
  int statusCode = 0;
  int sp = statusLine.indexOf(' ');
  if (sp > 0) statusCode = statusLine.substring(sp + 1, sp + 4).toInt();

  bool chunked = false;
  long contentLength = -1;

  while (client.connected() || client.available()) {
    String line = client.readStringUntil('\n');
    if (line == "\r" || line.length() < 2) break;
    line.toLowerCase();
    if (line.startsWith("transfer-encoding:") && line.indexOf("chunked") > 0) {
      chunked = true;
    } else if (line.startsWith("content-length:")) {
      contentLength = line.substring(15).toInt();
    }
  }

  String body;
  if (chunked) {
    while (client.connected() || client.available()) {
      String sizeLine = client.readStringUntil('\n');
      sizeLine.trim();
      if (sizeLine.length() == 0) continue;
      long chunkSize = strtol(sizeLine.c_str(), NULL, 16);
      if (chunkSize <= 0) break;

      long read = 0;
      unsigned long rtimeout = millis();
      while (read < chunkSize && millis() - rtimeout < 10000) {
        if (client.available()) {
          body += (char)client.read();
          read++;
          rtimeout = millis();
        } else {
          delay(1);
        }
      }
      client.read(); // trailing \r
      client.read(); // trailing \n
    }
  } else {
    unsigned long rtimeout = millis();
    long read = 0;
    while (millis() - rtimeout < 5000) {
      if (client.available()) {
        body += (char)client.read();
        read++;
        rtimeout = millis();
        if (contentLength > 0 && read >= contentLength) break;
      } else if (!client.connected()) {
        break;
      } else {
        delay(1);
      }
    }
  }

  client.stop();

  if (statusCode < 200 || statusCode >= 300) {
    String err = "{\"error\":\"status " + String(statusCode) + "\"}";
    webServer.send(statusCode, "application/json", err);
    return;
  }

  webServer.send(200, "application/json", body);
}

void handleOtaCheck() {
  OTAInfo info = OTA::check();
  JsonDocument doc;
  doc["current"] = info.currentVersion;
  doc["latest"] = info.latestVersion;
  doc["changelog"] = info.changelog;
  doc["downloadUrl"] = info.downloadUrl;
  doc["hasUpdate"] = info.hasUpdate;
  if (info.error.length() > 0) doc["error"] = info.error;

  String out;
  serializeJson(doc, out);
  int code = info.error.length() > 0 ? 502 : 200;
  webServer.send(code, "application/json", out);
}

void handleOtaInstall() {
  OTAInfo info = OTA::check();
  if (info.error.length() > 0) {
    JsonDocument doc;
    doc["error"] = info.error;
    String out;
    serializeJson(doc, out);
    webServer.send(502, "application/json", out);
    return;
  }
  if (!info.hasUpdate) {
    webServer.send(200, "application/json",
                   "{\"installing\":false,\"message\":\"already on latest\"}");
    return;
  }

  // Reply before flashing — the browser will see the connection drop on reboot.
  JsonDocument doc;
  doc["installing"] = true;
  doc["version"] = info.latestVersion;
  String out;
  serializeJson(doc, out);
  webServer.send(200, "application/json", out);
  webServer.client().flush();
  delay(200);

  Serial.printf("OTA: installing %s\n", info.latestVersion.c_str());
  Serial.flush();

  String error;
  bool ok = OTA::install(info.downloadUrl, error);
  if (!ok) {
    Serial.printf("OTA install failed: %s\n", error.c_str());
    Serial.flush();
  }
  // On success, OTA::install reboots and we never reach here.
}

void setupWebServer() {
  webServer.on("/", handleRoot);
  webServer.on("/capture", handleCapture);
  webServer.on("/snap", handleSnap);
  webServer.on("/config", HTTP_POST, handleConfig);
  webServer.on("/stats", handleStats);
  webServer.on("/plots", handlePlots);
  webServer.on("/ota/check", handleOtaCheck);
  webServer.on("/ota/install", HTTP_POST, handleOtaInstall);
  webServer.begin();
  Serial.println("Web server started on port 80");
  Serial.flush();
}

void setup() {
  delay(500);
  Serial.begin(115200);
  delay(1000);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println("\n=== GardenGG Timelapse ===");
  Serial.flush();

  loadSettings();

  if (wifiSsid.length() == 0) {
    Serial.println("No config - entering provisioning");
    Serial.flush();
    state = PROVISIONING;
    Provisioning::start();
    stateStart = millis();
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
    stateStart = millis();
  }
}

void loop() {
  unsigned long now = millis();

  if (state == CONNECTING) {
    if (WiFi.status() == WL_CONNECTED) {
      state = CONNECTED;
      Serial.printf("WiFi: %s\n", WiFi.localIP().toString().c_str());
      Serial.flush();

      if (!cameraOk) cameraOk = initCamera();
      setupWebServer();

      lastCapture = now - captureInterval + 5000;
      // First auto-update check fires ~2 minutes after connect, not on the
      // full 6-hour interval, so freshly flashed devices pull updates promptly.
      lastAutoUpdateCheck = now - AUTO_UPDATE_CHECK_INTERVAL_MS + 120000;
      return;
    }

    if (now - stateStart > 30000) {
      Serial.println("WiFi: timeout - provisioning");
      Serial.flush();
      state = PROVISIONING;
      Provisioning::start();
      stateStart = now;
      return;
    }

    if (now - lastBlink > 500) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      lastBlink = now;
    }
  }
  else if (state == PROVISIONING) {
    Provisioning::handleClient();

    if (now - lastBlink > 2000) {
      digitalWrite(LED_PIN, HIGH);
      delay(100);
      digitalWrite(LED_PIN, LOW);
      lastBlink = now;
    }
  }
  else if (state == CONNECTED) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi: lost");
      Serial.flush();
      state = PROVISIONING;
      Provisioning::start();
      return;
    }

    webServer.handleClient();

    if (cameraOk && now - lastCapture >= captureInterval) {
      Serial.printf("\nCapture cycle at %lu ms\n", now);
      Serial.flush();

      if (captureAndUpload()) {
        Serial.println("Upload: SUCCESS\n");
        lastCapture = now;
      } else {
        Serial.println("Upload: FAILED\n");
      }
      Serial.flush();
    }

    if (autoUpdate && now - lastAutoUpdateCheck >= AUTO_UPDATE_CHECK_INTERVAL_MS) {
      lastAutoUpdateCheck = now;
      Serial.println("Auto-update: checking GitHub releases");
      Serial.flush();
      OTAInfo info = OTA::check();
      if (info.error.length() > 0) {
        Serial.printf("Auto-update check failed: %s\n", info.error.c_str());
      } else if (info.hasUpdate) {
        Serial.printf("Auto-update: installing %s (was %s)\n",
                      info.latestVersion.c_str(), info.currentVersion.c_str());
        String error;
        OTA::install(info.downloadUrl, error);
        if (error.length() > 0) {
          Serial.printf("Auto-update install failed: %s\n", error.c_str());
        }
      } else {
        Serial.printf("Auto-update: on latest (%s)\n", info.currentVersion.c_str());
      }
      Serial.flush();
    }

    if (now - lastBlink > 5000) {
      digitalWrite(LED_PIN, HIGH);
      delay(50);
      digitalWrite(LED_PIN, LOW);
      delay(50);
      digitalWrite(LED_PIN, HIGH);
      delay(50);
      digitalWrite(LED_PIN, LOW);
      lastBlink = now;
    }
  }

  delay(10);
}
