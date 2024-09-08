#include "parrot_power.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace parrot_power {

static const char *const TAG = "parrot_power";

void ParrotPower::dump_config() {
  ESP_LOGCONFIG(TAG, "Parrot Power");
  LOG_SENSOR("  ", "Soil Temperature", this->soiltemperature_);
  LOG_SENSOR("  ", "Air Temperature", this->airtemperature_);
  LOG_SENSOR("  ", "Moisture", this->moisture_);
  LOG_SENSOR("  ", "Calibrated Soil Moisture", this->calibratedsoilmoisture_);
  LOG_SENSOR("  ", "sunlight", this->sunlight_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
}

bool ParrotPower::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  if (device.address_uint64() != this->address_) {
    ESP_LOGVV(TAG, "parse_device(): unknown MAC address.");
    return false;
  }
  ESP_LOGVV(TAG, "parse_device(): MAC address %s found.", device.address_str().c_str());

  bool success = false;
  for (auto &service_data : device.get_service_datas()) {
    auto res = parrot_flower::parse_parrot_header(service_data);
    if (!res.has_value()) {
      continue;
    }
    if (res->is_duplicate) {
      continue;
    }
    if (!(parrot_flower::parse_parrot_message(service_data.data, *res))) {
      continue;
    }
    if (!(parrot_flower::report_parrot_results(res, device.address_str()))) {
      continue;
    }
    if (res->airtemperature.has_value() && this->airtemperature_ != nullptr)
      this->airtemperature_->publish_state(*res->airtemperature);
    if (res->soiltemperature.has_value() && this->soiltemperature_ != nullptr)
      this->soiltemperature_->publish_state(*res->soiltemperature);
    if (res->moisture.has_value() && this->moisture_ != nullptr)
      this->moisture_->publish_state(*res->moisture);
    if (res->calibratedsoilmoisture.has_value() && this->calibratedsoilmoisture_ != nullptr)
      this->calibratedsoilmoisture_->publish_state(*res->calibratedsoilmoisture);
    if (res->sunlight.has_value() && this->sunlight_ != nullptr)
      this->sunlight_->publish_state(*res->sunlight);
    if (res->battery_level.has_value() && this->battery_level_ != nullptr)
      this->battery_level_->publish_state(*res->battery_level);
    success = true;
  }

  return success;
}

}  // namespace parrot_power
}  // namespace esphome

#endif
