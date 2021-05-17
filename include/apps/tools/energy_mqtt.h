#ifndef OSW_APP_ENERGY_MQTT_H
#define OSW_APP_ENERGY_MQTT_H

#include <osw_hal.h>
#include <osw_ui.h>

#include "osw_app.h"

class OswAppEnergyMqtt : public OswApp {
 public:
  OswAppEnergyMqtt(void) { ui = OswUI::getInstance(); };
  void setup(OswHal * hal);
  void loop(OswHal* hal);
  void stop(OswHal* hal);
  ~OswAppEnergyMqtt(){};

 private:
  OswUI* ui;
};

#endif