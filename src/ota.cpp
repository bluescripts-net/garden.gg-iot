#include "ota.h"
#include "firmware_version.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <ArduinoJson.h>

unsigned long OTA::lastCheckTime = 0;

void OTA::init() {
  lastCheckTime = millis();
}

void OTA::checkAndUpdate() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  unsigned long now = millis();
  if (now - lastCheckTime < CHECK_INTERVAL) {
    return;
  }
  lastCheckTime = now;

  Serial.println("Checking for firmware updates...");

  WiFiClientSecure client;
  client.setInsecure();

  if (!client.connect("garden.gg", 443)) {
    Serial.println("Failed to connect to update server");
    return;
  }

  String path = "/firmware/esp32cam/latest.json";
  client.print("GET " + path + " HTTP/1.1\r\n");
  client.print("Host: garden.gg\r\n");
  client.print("Connection: close\r\n\r\n");
  client.flush();

  unsigned long timeout = millis();
  while (!client.available() && millis() - timeout < 10000) {
    delay(10);
  }

  if (!client.available()) {
    Serial.println("No response from update server");
    client.stop();
    return;
  }

  String statusLine = client.readStringUntil('\n');
  while (client.available() && client.read() != '\n') {
    // Skip headers
  }

  String response = "";
  while (client.available()) {
    response += (char)client.read();
  }
  client.stop();

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, response);
  if (error) {
    Serial.printf("JSON parse error: %s\n", error.c_str());
    return;
  }

  const char* newVersion = doc["version"];
  const char* downloadUrl = doc["url"];

  if (!newVersion || !downloadUrl) {
    Serial.println("Invalid update manifest");
    return;
  }

  Serial.printf("Current version: %s, Available: %s\n", FIRMWARE_VERSION, newVersion);

  if (strcmp(newVersion, FIRMWARE_VERSION) <= 0) {
    Serial.println("Already on latest version");
    return;
  }

  Serial.printf("Downloading update from: %s\n", downloadUrl);

  WiFiClientSecure updateClient;
  updateClient.setInsecure();

  if (!updateClient.connect("garden.gg", 443)) {
    Serial.println("Failed to download firmware");
    return;
  }

  updateClient.print(String("GET ") + downloadUrl + " HTTP/1.1\r\n");
  updateClient.print("Host: garden.gg\r\n");
  updateClient.print("Connection: close\r\n\r\n");
  updateClient.flush();

  timeout = millis();
  while (!updateClient.available() && millis() - timeout < 10000) {
    delay(10);
  }

  String updateStatusLine = updateClient.readStringUntil('\n');
  while (updateClient.available() && updateClient.read() != '\n') {
    // Skip headers
  }

  if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
    Serial.println("OTA begin failed");
    updateClient.stop();
    return;
  }

  size_t written = 0;
  while (updateClient.available()) {
    uint8_t buf[512];
    size_t len = updateClient.readBytes(buf, sizeof(buf));
    Update.write(buf, len);
    written += len;
  }

  if (Update.end(true)) {
    Serial.printf("OTA successful, wrote %u bytes. Rebooting...\n", written);
    delay(1000);
    ESP.restart();
  } else {
    Serial.printf("OTA failed: %s\n", Update.errorString());
    updateClient.stop();
  }
}
