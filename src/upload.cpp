#include "upload.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Arduino.h>

const char* Upload::GARDENGG_HOST = "garden.gg";
const int Upload::GARDENGG_PORT = 443;
const char* Upload::BOUNDARY = "ESP32CamBoundary";

bool Upload::send(const ConfigData& config, camera_fb_t* fb) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, skipping upload");
    return false;
  }

  String partHeader = String("--") + BOUNDARY + "\r\n"
    "Content-Disposition: form-data; name=\"file\"; filename=\"esp32-capture.jpg\"\r\n"
    "Content-Type: image/jpeg\r\n\r\n";
  String partFooter = String("\r\n--") + BOUNDARY + "--\r\n";

  size_t totalLen = partHeader.length() + fb->len + partFooter.length();
  String path = String("/api/v1/iot/photo?api_key=") + config.api_key + "&plot_id=" + config.plot_id;

  WiFiClientSecure client;
  client.setInsecure();

  Serial.printf("Connecting to %s:%d...\n", GARDENGG_HOST, GARDENGG_PORT);
  if (!client.connect(GARDENGG_HOST, GARDENGG_PORT)) {
    Serial.println("Connection failed");
    return false;
  }

  client.print("POST " + path + " HTTP/1.1\r\n");
  client.print(String("Host: ") + GARDENGG_HOST + "\r\n");
  client.print(String("Content-Type: multipart/form-data; boundary=") + BOUNDARY + "\r\n");
  client.print("Content-Length: " + String(totalLen) + "\r\n");
  client.print("Connection: close\r\n\r\n");

  client.print(partHeader);

  const size_t CHUNK_SIZE = 4096;
  size_t sent = 0;
  while (sent < fb->len) {
    size_t toSend = fb->len - sent;
    if (toSend > CHUNK_SIZE) toSend = CHUNK_SIZE;
    client.write(fb->buf + sent, toSend);
    sent += toSend;
  }
  Serial.printf("Sent %u bytes of image data\n", sent);

  client.print(partFooter);
  client.flush();

  unsigned long timeout = millis();
  while (!client.available() && millis() - timeout < 30000) {
    delay(10);
  }

  if (!client.available()) {
    Serial.println("No response from server");
    client.stop();
    return false;
  }

  String statusLine = client.readStringUntil('\n');
  Serial.println(statusLine);

  int statusCode = 0;
  int spaceIdx = statusLine.indexOf(' ');
  if (spaceIdx > 0) {
    statusCode = statusLine.substring(spaceIdx + 1, spaceIdx + 4).toInt();
  }

  while (client.available()) {
    String line = client.readStringUntil('\n');
    Serial.println(line);
  }
  client.stop();

  if (statusCode == 201) {
    Serial.println("Upload successful!");
    return true;
  }

  if (statusCode == 429) {
    Serial.println("Rate limited — will retry next interval");
  } else {
    Serial.printf("Upload failed with status %d\n", statusCode);
  }
  return false;
}
