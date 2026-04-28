#include "provisioning.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Arduino.h>
#include <WString.h>

static WebServer webServer(80);
static DNSServer dnsServer;
static ConfigData provisioningConfig;

void Provisioning::start() {
  Serial.println("P: Starting AP...");
  Serial.flush();
  setupAP();
  Serial.println("P: Starting DNS...");
  Serial.flush();
  setupDNS();
  Serial.println("P: Starting web server...");
  Serial.flush();
  setupWeb();
  Serial.println("P: Ready! Connect to WiFi AP at 192.168.4.1");
  Serial.flush();
}

void Provisioning::setupAP() {
  String apName = "GardenGG-Setup-";
  uint64_t chipid = ESP.getEfuseMac();
  apName += String((uint32_t)(chipid >> 32), 16);
  apName += String((uint32_t)chipid, 16);
  apName = apName.substring(0, 32);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(apName.c_str());
  Serial.printf("AP started: %s\n", apName.c_str());
  Serial.printf("IP: %s\n", WiFi.softAPIP().toString().c_str());
}

void Provisioning::setupDNS() {
  dnsServer.start(53, "*", WiFi.softAPIP());
}

void Provisioning::setupWeb() {
  webServer.on("/", HTTP_GET, []() { handleRoot(); });
  webServer.on("/save", HTTP_POST, []() { handleSave(); });

  webServer.on("/generate_204", HTTP_GET, []() {
    webServer.send(200, "text/plain", "");
  });
  webServer.on("/connecttest.txt", HTTP_GET, []() {
    webServer.send(200, "text/plain", "OK");
  });
  webServer.on("/hotspot-detect.html", HTTP_GET, []() {
    webServer.send(200, "text/html", "<html><body>OK</body></html>");
  });

  webServer.begin();
}

String Provisioning::generateHTML() {
  return R"(
<!DOCTYPE html>
<html>
<head>
  <title>GardenGG Setup</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial, sans-serif; max-width: 500px; margin: 50px auto; padding: 20px; }
    input { width: 100%; padding: 8px; margin: 8px 0; box-sizing: border-box; }
    button { width: 100%; padding: 10px; background: #4CAF50; color: white; border: none; border-radius: 4px; cursor: pointer; }
  </style>
</head>
<body>
  <h1>GardenGG Setup</h1>
  <form action="/save" method="POST">
    <label>WiFi SSID:<br>
      <input type="text" name="ssid" required>
    </label>
    <label>WiFi Password:<br>
      <input type="password" name="pass" required>
    </label>
    <label>API Key:<br>
      <input type="text" name="api_key" required>
    </label>
    <label>Plot ID:<br>
      <input type="text" name="plot_id" required>
    </label>
    <label>Capture Interval (ms):<br>
      <input type="number" name="interval" value="900000" min="10000">
    </label>
    <button type="submit">Save & Reboot</button>
  </form>
</body>
</html>
  )";
}

void Provisioning::handleRoot() {
  webServer.send(200, "text/html", generateHTML());
}

void Provisioning::handleSave() {
  if (!webServer.hasArg("ssid") || !webServer.hasArg("pass") || !webServer.hasArg("api_key") || !webServer.hasArg("plot_id")) {
    webServer.send(400, "text/plain", "Missing parameters");
    return;
  }

  String ssid = webServer.arg("ssid");
  String pass = webServer.arg("pass");
  String api_key = webServer.arg("api_key");
  String plot_id = webServer.arg("plot_id");
  uint32_t interval = webServer.arg("interval").toInt();

  if (interval < 10000) interval = 900000;

  strncpy(provisioningConfig.wifi_ssid, ssid.c_str(), sizeof(provisioningConfig.wifi_ssid) - 1);
  provisioningConfig.wifi_ssid[sizeof(provisioningConfig.wifi_ssid) - 1] = '\0';

  strncpy(provisioningConfig.wifi_pass, pass.c_str(), sizeof(provisioningConfig.wifi_pass) - 1);
  provisioningConfig.wifi_pass[sizeof(provisioningConfig.wifi_pass) - 1] = '\0';

  strncpy(provisioningConfig.api_key, api_key.c_str(), sizeof(provisioningConfig.api_key) - 1);
  provisioningConfig.api_key[sizeof(provisioningConfig.api_key) - 1] = '\0';

  strncpy(provisioningConfig.plot_id, plot_id.c_str(), sizeof(provisioningConfig.plot_id) - 1);
  provisioningConfig.plot_id[sizeof(provisioningConfig.plot_id) - 1] = '\0';

  provisioningConfig.capture_interval_ms = interval;

  Config::save(provisioningConfig);

  webServer.send(200, "text/html", "<h1>Saved!</h1><p>Rebooting...</p>");
  delay(1000);
  ESP.restart();
}

void Provisioning::handleClient() {
  dnsServer.processNextRequest();
  webServer.handleClient();
}

void Provisioning::stop() {
  webServer.stop();
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
}
