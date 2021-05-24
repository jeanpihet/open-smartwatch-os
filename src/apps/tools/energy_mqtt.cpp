
#include "./apps/tools/energy_mqtt.h"

#include <gfx_util.h>
#include <osw_app.h>
#include <osw_hal.h>

#include <WiFi.h>
#include <PubSubClient.h>

#include <ArduinoJson.h>

#define POWER_MAX_W       12000
#define HOUR_TICKS_RADIUS 112

static long lastUITick;
IPAddress broker(192,168,2,33);
WiFiClient wclient;
PubSubClient client(wclient);
bool connecting;

StaticJsonDocument<512> doc;
uint32_t predpv_power = 0;
uint32_t pv_power = 0;
uint32_t ac_power = 0;
int32_t grid_power = 0;
int32_t battery_percent = 0;
float battery_current = 0;
float battery_temp = 0;

typedef struct {
  int32_t target;
  int32_t value;
} animation_t;

animation_t grid_anim, pv_anim, pvpred_anim, ac_anim, batt_anim;

int animation_value(animation_t *anim)
{
  #define STEPS     5
  #define THRESHOLD 0.02f

  if (anim->value == anim->target)
    return 0;

  float diff = ((float) anim->target - anim->value) / anim->target;
  if (abs(diff) < THRESHOLD) {
    anim->value = anim->target;
  } else {
    float new_value = (float) anim->value + ((float) anim->target - anim->value) / STEPS;
    anim->value = new_value;
  }

  return 1;
}

float clamp(float in, float min, float max)
{
  if (in > max)
    return max;
  if (in < min)
    return min;

  return in;
}

int32_t clamp(int32_t in, int32_t min, int32_t max)
{
  if (in > max)
    return max;
  if (in < min)
    return min;

  return in;
}

uint32_t clamp(uint32_t in, uint32_t min, uint32_t max)
{
  if (in > max)
    return max;
  if (in < min)
    return min;

  return in;
}

// Handle incomming messages from the MQTT broker
void callback(char* topic, byte* payload, unsigned int length) {
  //printf("MQTT RX, topic=%s, len=%d\n", topic, length);
  if (!strcmp(topic, "house/irradiance")) {
    deserializeJson(doc, payload);
    predpv_power = doc["pv_power_pred"];
  } else if (!strcmp(topic, "house/inverter")) {
    deserializeJson(doc, payload);
    pv_power = doc["pv_power"];
    ac_power = doc["ac_power"];
    grid_power = doc["grid_power"];
  } else if (!strcmp(topic, "house/battery")) {
    deserializeJson(doc, payload);
    battery_percent = doc["batt_soc"];
    battery_current = doc["batt_current"];
    battery_temp = doc["batt_temp"];
  }

#if 0
    // Generate random data, for testing
    predpv_power = random(POWER_MAX_W);
    pv_power = predpv_power * (33 + random(67)) / 100;
    ac_power = pv_power * random(100) / 150;
    grid_power = random(2 * POWER_MAX_W) - POWER_MAX_W;
    battery_percent = random(100);
    battery_current = (random(240) - 120) / 10.0;
    battery_temp = 15.0 + (random(350) / 10.0);
#endif
}

// Prepare display: background etc.
void prepare_display(OswHal* hal) {
  uint16_t color;

  // Background
  hal->gfx()->fill(OswUI::getInstance()->getBackgroundColor());
  hal->gfx()->setTextColor(OswUI::getInstance()->getForegroundColor(),OswUI::getInstance()->getBackgroundColor());
  // Hour ticks
  hal->gfx()->drawHourTicks(120, 120, HOUR_TICKS_RADIUS + 5, HOUR_TICKS_RADIUS - 5,
                            OswUI::getInstance()->getForegroundDimmedColor());

  // Time
  uint32_t second = 0;
  uint32_t minute = 0;
  uint32_t hour = 0;
  hal->getLocalTime(&hour, &minute, &second);
  // hours
  float angle = 3.1415 - (6.283 / 12.0 * (1.0 * hour + minute / 60.0));
  double x = 120.0 + sin(angle) * HOUR_TICKS_RADIUS;
  double y = 120.0 + cos(angle) * HOUR_TICKS_RADIUS;
  hal->gfx()->fillCircle(x, y, 7, rgb565(220, 220, 220));
  // minutes
  angle = 3.1415 - (6.283 / 60.0 * (minute + second / 60.0));
  x = 120.0 + sin(angle) * HOUR_TICKS_RADIUS;
  y = 120.0 + cos(angle) * HOUR_TICKS_RADIUS;
  hal->gfx()->fillCircle(x, y, 5, rgb565(252,255, 55));
  // seconds
  angle = 3.1415 - (6.283 / 60.0 * second);
  x = 120.0 + sin(angle) * HOUR_TICKS_RADIUS;
  y = 120.0 + cos(angle) * HOUR_TICKS_RADIUS;
  hal->gfx()->fillCircle(x, y, 3, rgb565(252, 94, 57));

  // Background arcs
  if (grid_anim.value > 0) {
    color = rgb565(210, 50, 66);
  } else {
    color = rgb565(66,50,210);
  }
  hal->gfx()->drawArc(120, 120, 0, 360, 90, 95, 3, changeColor(color, 0.33));
  hal->gfx()->drawArc(120, 120, 0, 360, 90, 82, 3, changeColor(rgb565(117, 235, 10), 0.33));
  hal->gfx()->drawArc(120, 120, 0, 360, 90, 69, 3, changeColor(rgb565(25, 193, 202), 0.33));

  // Text
  if (!hal->getWiFi()->isConnected()) {
    hal->gfx()->setTextSize(2);
    hal->gfx()->setTextCenterAligned();
    hal->gfx()->setTextTopAligned();
    hal->gfx()->setTextCursor(120, 108);
    hal->gfx()->print("Energy monitor");
  }

  // Buttons
  OswUI::getInstance()->setTextCursor(BUTTON_3);
  if (!hal->getWiFi()->isConnected()) {
    if (!connecting)
      hal->gfx()->print(LANG_CONNECT);
    else
      hal->gfx()->print(LANG_BMC_CONNECTING);
  }
}

void OswAppEnergyMqtt::setup(OswHal* hal) {
  hal->getWiFi()->setDebugStream(&Serial);
  connecting = false;
  prepare_display(hal);
  hal->requestFlush();
  lastUITick = millis();
}

void OswAppEnergyMqtt::loop(OswHal* hal) {
  char buf[32];

  // Check button for connection
  if (hal->btnHasGoneDown(BUTTON_3)) {
    if (hal->getWiFi()->isConnected()) {
      connecting = false;
      hal->getWiFi()->disableWiFi();
    } else {
      // Update display and connect during the next loop()
      connecting = true;
      prepare_display(hal);
      hal->requestFlush();
      return;
    }
  }
  // Connect WiFi
  if (!hal->getWiFi()->isConnected()) {
      if (connecting) {
        hal->getWiFi()->checkWifi();
        // Connect MQTT
        client.setServer(broker, 1883);
        client.setCallback(callback);
      }
  } else {
    // Connect MQTT
    if (!client.connected()) {
      if (connecting) {
        // Subscribe to MQTT topics
        if (client.connect("opensmartwatch")) {
          // MQTT drops messages > 256 bytes, increase the limit
          client.setBufferSize(512);
          client.subscribe("house/#");
        }
      }
    } else {
      // Check MQTT messages
      client.loop();
      grid_anim.target = grid_power;
      pv_anim.target = pv_power;
      pvpred_anim.target = predpv_power;
      ac_anim.target = ac_power;
      batt_anim.target = battery_percent;
    }
  }

  // Update anim, otherwise update UI every 1s
  bool update_anim = false;
  if (animation_value(&grid_anim))
    update_anim = true;
  if (animation_value(&pv_anim))
    update_anim = true;
  if (animation_value(&pvpred_anim))
    update_anim = true;
  if (animation_value(&ac_anim))
    update_anim = true;
  if (animation_value(&batt_anim))
    update_anim = true;

  if (!update_anim && lastUITick + 1000 > millis())
    return;
  lastUITick = millis();

  prepare_display(hal);

  if (client.connected()) {
    #define ANGLE_OFFSET 180
    int32_t stop_angle;
    uint16_t color;

    // Arcs
    // Grid power
    stop_angle = ANGLE_OFFSET + ((clamp(grid_anim.value, -POWER_MAX_W, POWER_MAX_W) * -360) / POWER_MAX_W);
    if (grid_anim.value > 0) {
      color = rgb565(210, 50, 66);
    } else {
      color = rgb565(66,50,210);
    }
    hal->gfx()->drawArc(120, 120, ANGLE_OFFSET, stop_angle, 90, 95, 4, dimColor(color, 25));
    hal->gfx()->drawArc(120, 120, ANGLE_OFFSET, stop_angle, 90, 95, 5, color);
    // PV power and prediction
    stop_angle = ANGLE_OFFSET + ((clamp((uint32_t) pvpred_anim.value, (uint32_t) 0, (uint32_t) POWER_MAX_W) * 360) / POWER_MAX_W);
    hal->gfx()->drawArc(120, 120, ANGLE_OFFSET, stop_angle, 90, 82, 4, rgb565(117, 235, 10));
    stop_angle = ANGLE_OFFSET + ((clamp((uint32_t) pv_anim.value, (uint32_t) 0, (uint32_t) POWER_MAX_W) * 360) / POWER_MAX_W);
    hal->gfx()->drawArc(120, 120, ANGLE_OFFSET, stop_angle, 90, 82, 5, dimColor(rgb565(117, 235, 10), 50));
    // AC power
    stop_angle = ANGLE_OFFSET + ((clamp((uint32_t) ac_anim.value, (uint32_t) 0, (uint32_t) POWER_MAX_W) * 360) / POWER_MAX_W);
    hal->gfx()->drawArc(120, 120, ANGLE_OFFSET, stop_angle, 90, 69, 4, dimColor(rgb565(25, 193, 202), 25));
    hal->gfx()->drawArc(120, 120, ANGLE_OFFSET, stop_angle, 90, 69, 5, rgb565(25, 193, 202));

    // Texts
    hal->gfx()->setTextSize(1);
    hal->gfx()->setTextRightAligned();
    hal->gfx()->setTextCursor(120, 228);
    hal->gfx()->print("Grid ");
    hal->gfx()->setTextLeftAligned();
    hal->gfx()->setTextCursor(120, 228);
    snprintf(buf, sizeof(buf), "%d", grid_power);
    hal->gfx()->print(buf);

    hal->gfx()->setTextCenterAligned();
    hal->gfx()->setTextCursor(120, 206);
    hal->gfx()->print("PV");

    hal->gfx()->setTextRightAligned();
    hal->gfx()->setTextCursor(120, 173);
    hal->gfx()->print("Load ");
    hal->gfx()->setTextLeftAligned();
    hal->gfx()->setTextCursor(120, 173);
    snprintf(buf, sizeof(buf), "%d", ac_power);
    hal->gfx()->print(buf);

    hal->gfx()->setTextSize(1);
    hal->gfx()->setTextCenterAligned();
    hal->gfx()->setTextTopAligned();
    hal->gfx()->setTextCursor(120, 86);
    snprintf(buf, sizeof(buf), "%.1f A %.1f C", battery_current, battery_temp);
    hal->gfx()->print(buf);

    hal->gfx()->setTextSize(2);
    hal->gfx()->setTextCenterAligned();
    hal->gfx()->setTextTopAligned();
    hal->gfx()->setTextCursor(120, 103);
    snprintf(buf, sizeof(buf), "PV/Pred:\n%d/%d\n W ", pv_power, predpv_power);
    hal->gfx()->print(buf);

    // Draw battery
    hal->gfx()->fillFrame(93, 66, 56, 17, OswUI::getInstance()->getForegroundColor());
    hal->gfx()->fillFrame(94, 67, 54, 15, OswUI::getInstance()->getBackgroundColor());
    if (battery_current > 0) {
      color = rgb565(200, 40, 56);
    } else {
      color = dimColor(rgb565(117, 235, 10), 50);
    }
    hal->gfx()->fillFrame(94, 67, (54 * clamp(batt_anim.value, 0, 100)) / 100, 15, color);
    hal->gfx()->fillFrame(149, 69, 4, 11, OswUI::getInstance()->getForegroundColor());
    hal->gfx()->setTextSize(1);
    hal->gfx()->setTextCenterAligned();
    hal->gfx()->setTextCursor(120, 70);
    snprintf(buf, sizeof(buf), "%d%%", battery_percent);
    hal->gfx()->print(buf);
  }
  hal->requestFlush();
}

void OswAppEnergyMqtt::stop(OswHal* hal) {}
