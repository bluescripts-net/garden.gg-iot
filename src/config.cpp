#include "config.h"
#include <Preferences.h>
#include <Arduino.h>

const char* Config::NS_NAME = "gardengg";

bool Config::load(ConfigData& config) {
  Serial.println("Config::load starting");
  Serial.flush();

  Preferences prefs;
  prefs.begin(NS_NAME, true);

  if (!prefs.isKey("wifi_ssid")) {
    Serial.println("Config::load: no wifi_ssid found");
    Serial.flush();
    prefs.end();
    return false;
  }

  String ssid = prefs.getString("wifi_ssid", "");
  String pass = prefs.getString("wifi_pass", "");
  String api_key = prefs.getString("api_key", "");
  String plot_id = prefs.getString("plot_id", "");
  config.capture_interval_ms = prefs.getUInt("capture_interval", 900000);

  prefs.end();

  strncpy(config.wifi_ssid, ssid.c_str(), sizeof(config.wifi_ssid) - 1);
  config.wifi_ssid[sizeof(config.wifi_ssid) - 1] = '\0';
  strncpy(config.wifi_pass, pass.c_str(), sizeof(config.wifi_pass) - 1);
  config.wifi_pass[sizeof(config.wifi_pass) - 1] = '\0';
  strncpy(config.api_key, api_key.c_str(), sizeof(config.api_key) - 1);
  config.api_key[sizeof(config.api_key) - 1] = '\0';
  strncpy(config.plot_id, plot_id.c_str(), sizeof(config.plot_id) - 1);
  config.plot_id[sizeof(config.plot_id) - 1] = '\0';

  Serial.println("Config::load success");
  Serial.flush();
  return true;
}

void Config::save(const ConfigData& config) {
  Serial.println("Config::save starting");
  Serial.flush();

  Preferences prefs;
  prefs.begin(NS_NAME, false);

  prefs.putString("wifi_ssid", config.wifi_ssid);
  prefs.putString("wifi_pass", config.wifi_pass);
  prefs.putString("api_key", config.api_key);
  prefs.putString("plot_id", config.plot_id);
  prefs.putUInt("capture_interval", config.capture_interval_ms);

  prefs.end();
  Serial.println("Config::save complete");
  Serial.flush();
}

void Config::reset() {
  Preferences prefs;
  prefs.begin(NS_NAME, false);
  prefs.clear();
  prefs.end();
  Serial.println("Config reset to factory defaults");
  Serial.flush();
}

bool Config::isConfigured() {
  Serial.println("Config::isConfigured checking");
  Serial.flush();

  Preferences prefs;
  prefs.begin(NS_NAME, true);
  bool configured = prefs.isKey("wifi_ssid");
  prefs.end();

  Serial.printf("Config::isConfigured = %d\n", configured);
  Serial.flush();
  return configured;
}
