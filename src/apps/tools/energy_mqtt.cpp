
#include "./apps/tools/energy_mqtt.h"

#include <config.h>
#include <gfx_util.h>
#include <osw_app.h>
#include <osw_hal.h>

#include "osw_ui_util.h"

#define POWER_MAX_W   15000

static long lastTick;

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

void OswAppEnergyMqtt::setup(OswHal* hal) {
  hal->getWiFi()->setDebugStream(&Serial);
  // Display update
  hal->gfx()->fill(rgb565(0, 0, 0));
  hal->gfx()->setTextColor(rgb565(255, 255, 255), rgb565(0, 0, 0));
  hal->gfx()->setTextSize(2);
  hal->gfx()->setTextCenterAligned();
  hal->gfx()->setTextTopAligned();
  hal->gfx()->setTextCursor(120, 103);
  hal->gfx()->print("Energy monitor\nConnecting ...");
  hal->requestFlush();

  lastTick = millis();
  }

void OswAppEnergyMqtt::loop(OswHal* hal) {
  uint32_t predpv_power = 0;
  uint32_t pv_power = 0;
  uint32_t ac_power = 0;
  int32_t grid_power = 0;
  int32_t battery_percent = 0;
  float battery_current = 0;
  float battery_temp = 0;
  char buf[32];

  // Update every 1s
  if (lastTick + 1000 > millis())
    return;
  lastTick = millis();

  if (!hal->getWiFi()->isConnected()) {
    // Connect Wifi
    hal->getWiFi()->checkWifi();
  } else {
    // Generate data
    predpv_power = random(POWER_MAX_W);
    pv_power = predpv_power * (33 + random(67)) / 100;
    ac_power = pv_power * random(100) / 150;
    grid_power = random(2 * POWER_MAX_W) - POWER_MAX_W;
    battery_percent = random(100);
    battery_current = (random(240) - 120) / 10.0;
    battery_temp = 15.0 + (random(350) / 10.0);
  }

  // Clamp values for display
  grid_power = clamp(grid_power, -POWER_MAX_W, POWER_MAX_W);
  ac_power = clamp(ac_power, (uint32_t) 0, (uint32_t) POWER_MAX_W);
  pv_power = clamp(pv_power, (uint32_t) 0, (uint32_t) POWER_MAX_W);
  predpv_power = clamp(predpv_power, (uint32_t) 0, (uint32_t) POWER_MAX_W);
  battery_percent = clamp(battery_percent, 0, 100);

  // Update display
  #define HOUR_TICKS_RADIUS 112
  hal->gfx()->fill(rgb565(0, 0, 0));
  hal->gfx()->setTextColor(rgb565(255, 255, 255), rgb565(0, 0, 0));
  hal->gfx()->drawHourTicks(120, 120, HOUR_TICKS_RADIUS + 5, HOUR_TICKS_RADIUS - 5, rgb565(128, 128, 128));

  #define ANGLE_OFFSET 180
  int32_t stop_angle;
  uint16_t color;

  // Time
  uint32_t second = 0;
  uint32_t minute = 0;
  uint32_t hour = 0;
  hal->getLocalTime(&hour, &minute, &second);
  // hours
  float angle = 3.1415 - (6.283 / 12.0 * (1.0 * hour + minute / 60.0));
  double x = 120.0 + sin(angle) * HOUR_TICKS_RADIUS;
  double y = 120.0 + cos(angle) * HOUR_TICKS_RADIUS;
  hal->gfx()->fillCircle(x, y, 6, rgb565(220, 220, 220));
  // minutes
  angle = 3.1415 - (6.283 / 60.0 * (minute + second / 60.0));
  x = 120.0 + sin(angle) * HOUR_TICKS_RADIUS;
  y = 120.0 + cos(angle) * HOUR_TICKS_RADIUS;
  hal->gfx()->fillCircle(x, y, 4, dimColor(rgb565(117, 235, 10), 25));
  // seconds
  angle = 3.1415 - (6.283 / 60.0 * second);
  x = 120.0 + sin(angle) * HOUR_TICKS_RADIUS;
  y = 120.0 + cos(angle) * HOUR_TICKS_RADIUS;
  hal->gfx()->fillCircle(x, y, 2, rgb565(66,50,210));

  // Arcs
  // Grid power
  stop_angle = ANGLE_OFFSET + ((grid_power * -360) / POWER_MAX_W);
  if (grid_power > 0) {
    color = rgb565(210, 50, 66);
  } else {
    color = rgb565(66,50,210);
  }
  hal->gfx()->drawArc(120, 120, 0, 360, 90, 95, 3, changeColor(color, 0.25));
  hal->gfx()->drawArc(120, 120, ANGLE_OFFSET, stop_angle, 90, 95, 4, dimColor(color, 25));
  hal->gfx()->drawArc(120, 120, ANGLE_OFFSET, stop_angle, 90, 95, 5, color);
  // PV power and prediction
  hal->gfx()->drawArc(120, 120, 0, 360, 90, 82, 3, changeColor(rgb565(117, 235, 10), 0.20));
  stop_angle = ANGLE_OFFSET + ((predpv_power * 360) / POWER_MAX_W);
  hal->gfx()->drawArc(120, 120, ANGLE_OFFSET, stop_angle, 90, 82, 4, rgb565(117, 235, 10));
  stop_angle = ANGLE_OFFSET + ((pv_power * 360) / POWER_MAX_W);
  hal->gfx()->drawArc(120, 120, ANGLE_OFFSET, stop_angle, 90, 82, 5, dimColor(rgb565(117, 235, 10), 50));
  // AC power
  stop_angle = ANGLE_OFFSET + ((ac_power * 360) / POWER_MAX_W);
  hal->gfx()->drawArc(120, 120, 0, 360, 90, 69, 3, changeColor(rgb565(25, 193, 202), 0.25));
  hal->gfx()->drawArc(120, 120, ANGLE_OFFSET, stop_angle, 90, 69, 4, dimColor(rgb565(25, 193, 202), 25));
  hal->gfx()->drawArc(120, 120, ANGLE_OFFSET, stop_angle, 90, 69, 5, rgb565(25, 193, 202));

  // Texts
  hal->gfx()->setTextSize(1);
  hal->gfx()->setTextRightAligned();
  hal->gfx()->setTextCursor(120, 220);
  hal->gfx()->print("Grid ");
  hal->gfx()->setTextLeftAligned();
  hal->gfx()->setTextCursor(120, 220);
  snprintf(buf, sizeof(buf), "%d", grid_power);
  hal->gfx()->print(buf);

  hal->gfx()->setTextCenterAligned();
  hal->gfx()->setTextCursor(120, 198);
  hal->gfx()->print("PV");

  hal->gfx()->setTextRightAligned();
  hal->gfx()->setTextCursor(120, 171);
  hal->gfx()->print("AC ");
  hal->gfx()->setTextLeftAligned();
  hal->gfx()->setTextCursor(120, 172);
  snprintf(buf, sizeof(buf), "%d", ac_power);
  hal->gfx()->print(buf);

  hal->gfx()->setTextSize(1);
  hal->gfx()->setTextCenterAligned();
  hal->gfx()->setTextTopAligned();
  hal->gfx()->setTextCursor(120, 86);
  snprintf(buf, sizeof(buf), "%.1f A %.1f C",
            battery_current, battery_temp);
  hal->gfx()->print(buf);

  hal->gfx()->setTextSize(2);
  hal->gfx()->setTextCenterAligned();
  hal->gfx()->setTextTopAligned();
  hal->gfx()->setTextCursor(120, 103);
    snprintf(buf, sizeof(buf), "Connecting ...");
    snprintf(buf, sizeof(buf), "PV/Pred:\n%d/%d\n W ",
             pv_power, predpv_power);
  hal->gfx()->print(buf);

  // Draw battery
  hal->gfx()->fillFrame(93, 66, 56, 17, rgb565(220, 220, 220));
  hal->gfx()->fillFrame(94, 67, 54, 15, rgb565(0, 0, 0));
  if (battery_current > 0) {
    color = rgb565(200, 40, 56);
  } else {
    color = dimColor(rgb565(117, 235, 10), 50);
  }
  hal->gfx()->fillFrame(94, 67, (54 * battery_percent) / 100, 15, color);
  hal->gfx()->fillFrame(149, 69, 4, 11, rgb565(220, 220, 220));
  hal->gfx()->setTextSize(1);
  hal->gfx()->setTextCenterAligned();
  hal->gfx()->setTextCursor(120, 70);
  snprintf(buf, sizeof(buf), "%d%%", battery_percent);
  hal->gfx()->print(buf);

  hal->requestFlush();
}
void OswAppEnergyMqtt::stop(OswHal* hal) {}
