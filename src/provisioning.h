#ifndef PROVISIONING_H
#define PROVISIONING_H

#include "config.h"
#include <WString.h>

class Provisioning {
public:
  static void start();
  static void handleClient();
  static void stop();

private:
  static void setupAP();
  static void setupDNS();
  static void setupWeb();
  static String generateHTML();
  static void handleRoot();
  static void handleSave();
};

#endif
