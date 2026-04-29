#include "ota.h"
#include "firmware_version.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>

static const char* RELEASES_URL =
    "https://api.github.com/repos/" GITHUB_OWNER "/" GITHUB_REPO "/releases/latest";

OTAInfo OTA::check() {
  OTAInfo info;
  info.currentVersion = FIRMWARE_VERSION;
  info.hasUpdate = false;

  if (WiFi.status() != WL_CONNECTED) {
    info.error = "WiFi not connected";
    return info;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setUserAgent("garden.gg-iot/" FIRMWARE_VERSION);
  http.setTimeout(10000);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

  if (!http.begin(client, RELEASES_URL)) {
    info.error = "HTTPClient begin failed";
    return info;
  }
  http.addHeader("Accept", "application/vnd.github+json");

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    info.error = "GitHub API HTTP " + String(code);
    http.end();
    return info;
  }

  JsonDocument filter;
  filter["tag_name"] = true;
  filter["body"] = true;
  filter["assets"][0]["name"] = true;
  filter["assets"][0]["browser_download_url"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(
      doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();

  if (err) {
    info.error = String("JSON parse: ") + err.c_str();
    return info;
  }

  String tag = doc["tag_name"].as<String>();
  if (tag.startsWith("v")) tag = tag.substring(1);
  info.latestVersion = tag;
  info.changelog = doc["body"].as<String>();

  for (JsonObject asset : doc["assets"].as<JsonArray>()) {
    if (asset["name"] == FIRMWARE_ASSET_NAME) {
      info.downloadUrl = asset["browser_download_url"].as<String>();
      break;
    }
  }

  if (info.downloadUrl.length() == 0) {
    info.error = "Release has no " FIRMWARE_ASSET_NAME " asset yet";
    return info;
  }

  info.hasUpdate = strcmp(info.latestVersion.c_str(), info.currentVersion.c_str()) > 0;
  return info;
}

bool OTA::install(const String& downloadUrl, String& error) {
  if (WiFi.status() != WL_CONNECTED) {
    error = "WiFi not connected";
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setUserAgent("garden.gg-iot/" FIRMWARE_VERSION);
  http.setTimeout(15000);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

  if (!http.begin(client, downloadUrl)) {
    error = "HTTPClient begin failed";
    return false;
  }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    error = "Download HTTP " + String(code);
    http.end();
    return false;
  }

  int size = http.getSize();
  Serial.printf("OTA: downloading %d bytes\n", size);
  Serial.flush();

  if (!Update.begin(size > 0 ? size : UPDATE_SIZE_UNKNOWN)) {
    error = String("Update.begin: ") + Update.errorString();
    http.end();
    return false;
  }

  WiFiClient* stream = http.getStreamPtr();
  size_t written = Update.writeStream(*stream);
  http.end();

  if (size > 0 && (int)written != size) {
    error = "Wrote " + String(written) + "/" + String(size) + " bytes";
    Update.abort();
    return false;
  }

  if (!Update.end(true)) {
    error = String("Update.end: ") + Update.errorString();
    return false;
  }

  Serial.printf("OTA: flashed %u bytes, rebooting\n", written);
  Serial.flush();
  delay(500);
  ESP.restart();
  return true;  // unreachable
}
