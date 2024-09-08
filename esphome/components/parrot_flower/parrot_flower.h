#pragma once

#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/core/component.h"

#include <vector>

#ifdef USE_ESP32

namespace esphome {
namespace parrot_flower {

struct ParrotParseResult {
  enum {
    TYPE_POT,
    TYPE_POWER,
  } type;
  std::string name;
  optional<float> moisture;
  optional<float> sunlight;
  optional<float> soiltemperature;
  optional<float> battery_level;
  optional<float> airtemperature;
  optional<float> calibratedsoilmoisture;
  optional<float> calibratedairtemperature;
  optional<float> calibratedsunlight;
  optional<float> calibratedea;
  optional<float> calibratedecb;
  optional<float> calibratedecporous;
  optional<bool> is_active;
  optional<bool> is_light;
  optional<bool> button_press;
  bool has_data;        // 0x40
  bool has_capability;  // 0x20
  bool has_encryption;  // 0x08
  bool is_duplicate;
  int raw_offset;
};

struct ParrotAESVector {
  uint8_t key[16];
  uint8_t plaintext[16];
  uint8_t ciphertext[16];
  uint8_t authdata[16];
  uint8_t iv[16];
  uint8_t tag[16];
  size_t keysize;
  size_t authsize;
  size_t datasize;
  size_t tagsize;
  size_t ivsize;
};

bool parse_parrot_value(uint16_t value_type, const uint8_t *data, uint8_t value_length, ParrotParseResult &result);
bool parse_parrot_message(const std::vector<uint8_t> &message, ParrotParseResult &result);
optional<ParrotParseResult> parse_parrot_header(const esp32_ble_tracker::ServiceData &service_data);
bool report_parrot_results(const optional<ParrotParseResult> &result, const std::string &address);

class ParrotListener : public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
};

}  // namespace parrot_flower
}  // namespace esphome

#endif
