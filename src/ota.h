#ifndef OTA_H
#define OTA_H

class OTA {
public:
  static void init();
  static void checkAndUpdate();

private:
  static unsigned long lastCheckTime;
  static const unsigned long CHECK_INTERVAL = 86400000UL;  // 24 hours
};

#endif
