#ifndef OTA_H
#define OTA_H

#include <Arduino.h>

struct OTAInfo {
  String currentVersion;
  String latestVersion;
  String changelog;
  String downloadUrl;
  bool hasUpdate;
  String error;
};

class OTA {
public:
  // Query GitHub releases/latest. Network-bound; ~1-3s on a healthy WiFi.
  // Sets `error` in the returned info on any failure.
  static OTAInfo check();

  // Stream firmware.bin from GitHub and flash it. On success, the device
  // reboots and this never returns. Returns false on failure (with reason
  // in `error`).
  static bool install(const String& downloadUrl, String& error);
};

#endif
