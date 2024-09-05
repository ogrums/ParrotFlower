#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/parrot_flower/parrot_flower.h"

#ifdef USE_ESP32

namespace esphome {
namespace parrot_power {

class ParrotPower : public Component, public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { address_ = address; }

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;

  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_soiltemperature(sensor::Sensor *soiltemperature) { soiltemperature_ = soiltemperature; }
  void set_airtemperature(sensor::Sensor *airtemperature) { airtemperature_ = soiltemperature; }
  void set_moisture(sensor::Sensor *moisture) { moisture_ = moisture; }
  void set_calibratedsoilmoisture(sensor::Sensor *calibratedsoilmoisture) { calibratedsoilmoisture_ = calibratedsoilmoisture; }
  void set_sunlight(sensor::Sensor *sunlight) { sunlight_ = sunlight; }
  void set_battery_level(sensor::Sensor *battery_level) { battery_level_ = battery_level; }

 protected:
  uint64_t address_;
  sensor::Sensor *soiltemperature_{nullptr};
  sensor::Sensor *airtemperature_{nullptr};
  sensor::Sensor *moisture_{nullptr};
  sensor::Sensor *calibratedsoilmoisture_{nullptr};
  sensor::Sensor *sunlight_{nullptr};
  sensor::Sensor *battery_level_{nullptr};
};

}  // namespace parrot_power
}  // namespace esphome

#endif
