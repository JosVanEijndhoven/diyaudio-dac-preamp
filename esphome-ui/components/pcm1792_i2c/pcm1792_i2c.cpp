#include "pcm1792_i2c.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <cinttypes>
 
namespace esphome {
namespace pcm1792_i2c {
 
static const char *const TAG = "pcm1792";
 
void Pcm1792I2C::dump_config() {
  ESP_LOGCONFIG(TAG, "Pcm1792");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Mode: 0x%08x {%s}", mode_, mode_to_string().c_str());
}
 
ErrorCode Pcm1792I2C::set_mode(uint32_t mode) {
  mode_ = mode;
  ESP_LOGI(TAG, "Init PCM1792 mode=0x%08x on i2c bus_addr=0x%02x", mode, address_);
  std::array<uint8_t, 4> dacmode = {0, 0, 0, 0};
  for (int i = 0; i < 4; i++, mode >>= 8) {
    dacmode[i] = mode & 0xff;
  }
  return write_register(REG_MODE, dacmode.data(), dacmode.size());
}

ErrorCode Pcm1792I2C::set_volume64(uint8_t volume) {
  volume = std::min(volume, (uint8_t)64u);  // protect against out-of-bound argument
  const uint8_t vol_dac = (volume == 0) ? 0 : 2 * volume + 127; // pcm1792 uses 0..255
  const std::array<uint8_t, 2> i2c_data = {vol_dac, vol_dac};
  ESP_LOGI(TAG, "Set PCM1792 volume=%02d on i2c bus_addr=0x%02x", volume, address_);
  return write_register(REG_VOLUME, i2c_data.data(), i2c_data.size());
}

std::string Pcm1792I2C::mode_to_string() const {
  uint32_t mode = mode_;
  std::string names;
  std::string sep = "";
  // for multi-bit fields
  for (const auto& field: mode_names_field_zero) {
    uint32_t field_mask = field.first;
    if (mode & field_mask) {
      auto it = mode_names.find(mode & field_mask);
      if (it != mode_names.end()) {
        names += sep + it->second;
      }
      mode &= ~field_mask;  // remove processed non-zero field
    } else {
      names += sep + field.second;
    }
    sep = ",";
  }
  // for single-bit fields
  for (const auto& field: mode_names) {
    uint32_t field_mask = field.first;  // presumably has a only single bit raised
    if (mode & field_mask) {
      names += sep + field.second;
    }
  }
  return names;
}

}  // namespace pcm1792_i2c
}  // namespace esphome
