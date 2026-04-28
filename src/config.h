#ifndef CONFIG_H
#define CONFIG_H

#include <string>

struct ConfigData {
  char wifi_ssid[64];
  char wifi_pass[64];
  char api_key[128];
  char plot_id[64];
  uint32_t capture_interval_ms;
};

class Config {
public:
  static bool load(ConfigData& config);
  static void save(const ConfigData& config);
  static void reset();
  static bool isConfigured();

private:
  static const char* NS_NAME;
};

#endif
