#include "parrot_flower.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

#include <vector>
#include "mbedtls/ccm.h"

namespace esphome {
namespace parrot_flower {

static const char *const TAG = "parrot_flower";

bool parse_parrot_value(uint16_t value_type, const uint8_t *data, uint8_t value_length, ParrotParseResult &result) {
  // button pressed, 3 bytes, only byte 3 is used for supported devices so far
  if ((value_type == 0x1001) && (value_length == 3)) {
    result.button_press = data[2] == 0;
    return true;
  }
 // Battery Level, range is 0 - 100
  else if ((value_type == 0x4C02) && (value_length == 1)) {
    result.battery_level = data[0];
  }
  // Sunlight, units are photons per square meter
  else if ((value_type == 0x4C02) && (value_length == 1)) {
    result.sunlight = data[0];
  }
  // Soil Temperature,  
  else if ((value_type == 0x4C02) && (value_length == 1)) {
    result.soiltemperature = data[0];
  }
  // Air Temperature,  
  else if ((value_type == 0x4C02) && (value_length == 1)) {
    result.airtemperature = data[0];
  }
  // Soil Moisture, units is percentage (%)
  else if ((value_type == 0x4C02) && (value_length == 1)) {
    result.moisture = data[0];
  }
  // Calibrated Value soil moisture %
  else if ((value_type == 0x4C02) && (value_length == 1)) {
    result.calibratedsoilmoisture = data[0];
  }
  // Calibrated Value temperature C
  else if ((value_type == 0x4C02) && (value_length == 1)) {
    result.calibratedairtemperature = data[0];
  }
  // Calibrated Value sunlight photons per square meter (mol/m²/d)
  else if ((value_type == 0x4C02) && (value_length == 1)) {
    result.calibratedsunlight = data[0];
  }
  // Calibrated Value ea no unites
  else if ((value_type == 0x4C02) && (value_length == 1)) {
    result.calibratedea = data[0];
  }
  // Calibrated Value ecb ds/m
  else if ((value_type == 0x4C02) && (value_length == 1)) {
    result.calibratedecb = data[0];
  }
  // Calibrated Value ecPorous ds/m
  else if ((value_type == 0x4C02) && (value_length == 1)) {
    result.calibratedecporous = data[0];
  } else {
    return false;
  }

  return true;
}

bool parse_parrot_message(const std::vector<uint8_t> &message, ParrotParseResult &result) {
  result.has_encryption = message[0] & 0x08;  // update encryption status
  if (result.has_encryption) {
    ESP_LOGVV(TAG, "parse_parrot_message(): payload is encrypted, stop reading message.");
    return false;
  }

  // Data point specs
  // Byte 0: type
  // Byte 1: fixed 0x10
  // Byte 2: length
  // Byte 3..3+len-1: data point value

  const uint8_t *payload = message.data() + result.raw_offset;
  uint8_t payload_length = message.size() - result.raw_offset;
  uint8_t payload_offset = 0;
  bool success = false;

  if (payload_length < 4) {
    ESP_LOGVV(TAG, "parse_parrot_message(): payload has wrong size (%d)!", payload_length);
    return false;
  }

  while (payload_length > 3) {
    if (payload[payload_offset + 1] != 0x10 && payload[payload_offset + 1] != 0x00 &&
        payload[payload_offset + 1] != 0x4C && payload[payload_offset + 1] != 0x48) {
      ESP_LOGVV(TAG, "parse_parrot_message(): fixed byte not found, stop parsing residual data.");
      break;
    }

    const uint8_t value_length = payload[payload_offset + 2];
    if ((value_length < 1) || (value_length > 4) || (payload_length < (3 + value_length))) {
      ESP_LOGVV(TAG, "parse_parrot_message(): value has wrong size (%d)!", value_length);
      break;
    }

    const uint16_t value_type = encode_uint16(payload[payload_offset + 1], payload[payload_offset + 0]);
    const uint8_t *data = &payload[payload_offset + 3];

    if (parse_parrot_value(value_type, data, value_length, result))
      success = true;

    payload_length -= 3 + value_length;
    payload_offset += 3 + value_length;
  }

  return success;
}

optional<ParrotParseResult> parse_parrot_header(const esp32_ble_tracker::ServiceData &service_data) {
  ParrotParseResult result;
  if (!service_data.uuid.contains(0x95, 0xFE)) {
    ESP_LOGVV(TAG, "parse_parrot_header(): no service data UUID magic bytes.");
    return {};
  }

  auto raw = service_data.data;
  result.has_data = raw[0] & 0x40;
  result.has_capability = raw[0] & 0x20;
  result.has_encryption = raw[0] & 0x08;

  if (!result.has_data) {
    ESP_LOGVV(TAG, "parse_parrot_header(): service data has no DATA flag.");
    return {};
  }

  static uint8_t last_frame_count = 0;
  if (last_frame_count == raw[4]) {
    ESP_LOGVV(TAG, "parse_parrot_header(): duplicate data packet received (%d).", static_cast<int>(last_frame_count));
    result.is_duplicate = true;
    return {};
  }
  last_frame_count = raw[4];
  result.is_duplicate = false;
  result.raw_offset = result.has_capability ? 12 : 11;

  const uint16_t device_uuid = encode_uint16(raw[3], raw[2]);

  if (device_uuid == 0x01aa) {  // Flower Pot
    result.type = ParrotParseResult::TYPE_POT;
    result.name = "POT";
  } else if (device_uuid == 0x015d) {  // Flower Power Service UUID: 39E1FA00-84A8-11E2-AFBA-0002A5D5C51B
    result.type = ParrotParseResult::TYPE_POWER;
    result.name = "POWER";
    result.raw_offset -= 6;
  } else {
    ESP_LOGVV(TAG, "parse_parrot_header(): unknown device, no magic bytes.");
    return {};
  }

  return result;
}

bool decrypt_parrot_payload(std::vector<uint8_t> &raw, const uint8_t *bindkey, const uint64_t &address) {
  if (!((raw.size() == 19) || ((raw.size() >= 22) && (raw.size() <= 24)))) {
    ESP_LOGVV(TAG, "decrypt_parrot_payload(): data packet has wrong size (%d)!", raw.size());
    ESP_LOGVV(TAG, "  Packet : %s", format_hex_pretty(raw.data(), raw.size()).c_str());
    return false;
  }

  uint8_t mac_reverse[6] = {0};
  mac_reverse[5] = (uint8_t) (address >> 40);
  mac_reverse[4] = (uint8_t) (address >> 32);
  mac_reverse[3] = (uint8_t) (address >> 24);
  mac_reverse[2] = (uint8_t) (address >> 16);
  mac_reverse[1] = (uint8_t) (address >> 8);
  mac_reverse[0] = (uint8_t) (address >> 0);

  ParrotAESVector vector{.key = {0},
                         .plaintext = {0},
                         .ciphertext = {0},
                         .authdata = {0x11},
                         .iv = {0},
                         .tag = {0},
                         .keysize = 16,
                         .authsize = 1,
                         .datasize = 0,
                         .tagsize = 4,
                         .ivsize = 12};

  vector.datasize = (raw.size() == 19) ? raw.size() - 12 : raw.size() - 18;
  int cipher_pos = (raw.size() == 19) ? 5 : 11;

  const uint8_t *v = raw.data();

  memcpy(vector.key, bindkey, vector.keysize);
  memcpy(vector.ciphertext, v + cipher_pos, vector.datasize);
  memcpy(vector.tag, v + raw.size() - vector.tagsize, vector.tagsize);
  memcpy(vector.iv, mac_reverse, 6);             // MAC address reverse
  memcpy(vector.iv + 6, v + 2, 3);               // sensor type (2) + packet id (1)
  memcpy(vector.iv + 9, v + raw.size() - 7, 3);  // payload counter

  mbedtls_ccm_context ctx;
  mbedtls_ccm_init(&ctx);

  int ret = mbedtls_ccm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, vector.key, vector.keysize * 8);
  if (ret) {
    ESP_LOGVV(TAG, "decrypt_parrot_payload(): mbedtls_ccm_setkey() failed.");
    mbedtls_ccm_free(&ctx);
    return false;
  }

  ret = mbedtls_ccm_auth_decrypt(&ctx, vector.datasize, vector.iv, vector.ivsize, vector.authdata, vector.authsize,
                                 vector.ciphertext, vector.plaintext, vector.tag, vector.tagsize);
  if (ret) {
    uint8_t mac_address[6] = {0};
    memcpy(mac_address, mac_reverse + 5, 1);
    memcpy(mac_address + 1, mac_reverse + 4, 1);
    memcpy(mac_address + 2, mac_reverse + 3, 1);
    memcpy(mac_address + 3, mac_reverse + 2, 1);
    memcpy(mac_address + 4, mac_reverse + 1, 1);
    memcpy(mac_address + 5, mac_reverse, 1);
    ESP_LOGVV(TAG, "decrypt_parrot_payload(): authenticated decryption failed.");
    ESP_LOGVV(TAG, "  MAC address : %s", format_hex_pretty(mac_address, 6).c_str());
    ESP_LOGVV(TAG, "       Packet : %s", format_hex_pretty(raw.data(), raw.size()).c_str());
    ESP_LOGVV(TAG, "          Key : %s", format_hex_pretty(vector.key, vector.keysize).c_str());
    ESP_LOGVV(TAG, "           Iv : %s", format_hex_pretty(vector.iv, vector.ivsize).c_str());
    ESP_LOGVV(TAG, "       Cipher : %s", format_hex_pretty(vector.ciphertext, vector.datasize).c_str());
    ESP_LOGVV(TAG, "          Tag : %s", format_hex_pretty(vector.tag, vector.tagsize).c_str());
    mbedtls_ccm_free(&ctx);
    return false;
  }

  // replace encrypted payload with plaintext
  uint8_t *p = vector.plaintext;
  for (std::vector<uint8_t>::iterator it = raw.begin() + cipher_pos; it != raw.begin() + cipher_pos + vector.datasize;
       ++it) {
    *it = *(p++);
  }

  // clear encrypted flag
  raw[0] &= ~0x08;

  ESP_LOGVV(TAG, "decrypt_parrot_payload(): authenticated decryption passed.");
  ESP_LOGVV(TAG, "  Plaintext : %s, Packet : %d", format_hex_pretty(raw.data() + cipher_pos, vector.datasize).c_str(),
            static_cast<int>(raw[4]));

  mbedtls_ccm_free(&ctx);
  return true;
}

bool report_parrot_results(const optional<ParrotParseResult> &result, const std::string &address) {
  if (!result.has_value()) {
    ESP_LOGVV(TAG, "report_parrot_results(): no results available.");
    return false;
  }

  ESP_LOGD(TAG, "Got Parrot %s (%s):", result->name.c_str(), address.c_str());

  if (result->soiltemperature.has_value()) {
    ESP_LOGD(TAG, "  Soil Temperature: %.1f°C", *result->soiltemperature);
  }
  if (result->airtemperature.has_value()) {
    ESP_LOGD(TAG, "  Air Temperature: %.1f°C", *result->airtemperature);
  }
  if (result->moisture.has_value()) {
    ESP_LOGD(TAG, "  Moisture: %.1f%%", *result->moisture);
  }
  if (result->battery_level.has_value()) {
    ESP_LOGD(TAG, "  Battery Level: %.0f%%", *result->battery_level);
  }
  if (result->sunlight.has_value()) {
    ESP_LOGD(TAG, "  Sunlighte: %.0flx", *result->sunlight);
  }
  if (result->calibratedsoilmoisture.has_value()) {
    ESP_LOGD(TAG, "  Calibrated Soil Moisture: %.0f%%", *result->calibratedsoilmoisture);
  }
  if (result->calibratedairtemperature.has_value()) {
    ESP_LOGD(TAG, "  calibratedairtemperature: %.0f%%", *result->calibratedairtemperature);
  }
  if (result->calibratedsunlight.has_value()) {
    ESP_LOGD(TAG, "  calibratedsunlight: %.0f%%", *result->calibratedsunlight);
  }
  if (result->calibratedea.has_value()) {
    ESP_LOGD(TAG, "  Calibrated ea: %.0f%%", *result->calibratedea);
  }
  if (result->calibratedecb.has_value()) {
    ESP_LOGD(TAG, "  Calibrated ecb: %.0f%%", *result->calibratedecb);
  }
  if (result->calibratedecporous.has_value()) {
    ESP_LOGD(TAG, "  Calibrated ecb: %.0f%%", *result->calibratedecporous);
  }
  if (result->is_light.has_value()) {
    ESP_LOGD(TAG, "  Light: %s", (*result->is_light) ? "on" : "off");
  }
  if (result->button_press.has_value()) {
    ESP_LOGD(TAG, "  Button: %s", (*result->button_press) ? "pressed" : "");
  }

  return true;
}

bool ParrotListener::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  // Previously the message was parsed twice per packet, once by ParrotListener::parse_device()
  // and then again by the respective device class's parse_device() function. Parsing the header
  // here and then for each device seems to be unnecessary and complicates the duplicate packet filtering.
  // Hence I disabled the call to parse_parrot_header() here and the message parsing is done entirely
  // in the respective device instance. The ParrotListener class is defined in __init__.py and I was not
  // able to remove it entirely.

  return false;  // with true it's not showing device scans
}

}  // namespace parrot_flower
}  // namespace esphome

#endif
