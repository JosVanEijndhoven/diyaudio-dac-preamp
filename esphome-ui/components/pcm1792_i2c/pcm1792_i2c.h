#pragma once

#include <map>
#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
 
namespace esphome {
namespace pcm1792_i2c {
 
// pcm1792 operation modes, corresponding to i2c reg 18 (lsb) to reg 21 (msb)
// For MOXO_XXX field names, the XXX name corrsponds to the name in the datasheet
enum Mode: uint32_t {
  MODE_MUTE = 0x000001,
  MODE_DME  = 0x000002,
  MODE_DMF  = 0x00000c,
  MODE_DMF_NO   = 0x000000,
  MODE_DMF_48   = 0x000004,
  MODE_DMF_44   = 0x000008,
  MODE_DMF_32   = 0x00000c,
  MODE_FMT  = 0x000070,
  MODE_FMT_16R  = 0x000000,
  MODE_FMT_20R  = 0x000010,
  MODE_FMT_24R  = 0x000020,
  MODE_FMT_24L  = 0x000030,
  MODE_FMT_16I  = 0x000040,
  MODE_FMT_24I  = 0x000050,
  MODE_ATLD = 0x000080,
  MODE_INZD = 0x000100,
  MODE_FLT  = 0x000200,
  MODE_DFMS = 0x000400,
  MODE_OPE  = 0x001000,
  MODE_ATS  = 0x006000,
  MODE_ATS_LR1  = 0x000000,
  MODE_ATS_LR2  = 0x002000,
  MODE_ATS_LR4  = 0x004000,
  MODE_ATS_LR8  = 0x006000,
  MODE_OS   = 0x030000,
  MODE_OS_64    = 0x000000,
  MODE_OS_32    = 0x010000,
  MODE_OS_128   = 0x020000,
  MODE_CHSL = 0x040000,
  MODE_MONO = 0x080000,
  MODE_DFTH = 0x100000,
  MODE_DSD  = 0x200000,
  MODE_SRST = 0x400000,
  MODE_RSV  = 0x800000
};

const static std::map<uint32_t, const char *> mode_names = {
  {MODE_MUTE,    "Mute"},
  {MODE_DME,     "Dme"},
  {MODE_DMF_48,  "Dmf48"},
  {MODE_DMF_44,  "Dmf44"},
  {MODE_DMF_32,  "Dmf32"},
  {MODE_FMT_20R, "Fmt20R"},
  {MODE_FMT_24R, "Fmt24R"},
  {MODE_FMT_24L, "Fmt24L"},
  {MODE_FMT_16I, "Fmt16I"},
  {MODE_FMT_24I, "Fmt24I"},
  {MODE_ATLD,    "Atld"},
  {MODE_INZD,    "Inzd"},
  {MODE_FLT,     "Flt"},
  {MODE_DFMS,    "Dfms"},
  {MODE_OPE,     "Ope"},
  {MODE_ATS_LR2, "AtsLr2"},
  {MODE_ATS_LR4, "AtsLr4"},
  {MODE_ATS_LR8, "AtsLr8"},
  {MODE_OS_32,   "Os32"},
  {MODE_OS_128,  "Os128"},
  {MODE_CHSL,    "Right"},
  {MODE_MONO,    "Mono"},
  {MODE_DFTH,    "Dfth"},
  {MODE_DSD,     "Dsd"},
  {MODE_SRST,    "Srst"},
  {MODE_RSV,     "Rsv"}
};

const static std::map<uint32_t, const char *> mode_names_field_zero = {
  {MODE_DMF,     "DmfNo"},
  {MODE_FMT,     "Fmt16R"},
  {MODE_ATS,     "AtsLr1"},
  {MODE_OS,      "Os64"}
};

enum Reg: uint8_t {
  REG_MODE   = 18,
  REG_VOLUME = 16
};

using ErrorCode = i2c::ErrorCode;

class Pcm1792I2C : public Component, public i2c::I2CDevice {
  public:
    void dump_config() override;
    float get_setup_priority() const override { return setup_priority::DATA; }
    /**
     * Set operating mode of the dac chip. Although not common,
     * this could be modified at run-time.
     *
     * @param mode Provides a bit-wise OR of various 'enum Mode' constants.
     * @return Result of the I2C bus operation, with 0 indicating success.
     */
    ErrorCode set_mode(uint32_t mode);

    /**
     * Set the volume of the dac chip output audio, to both channels.
     *
     * @param volume 0: silent, 1: lowest volume, 64: max volume, with 1dB steps
     * @return Result of the I2C bus operation, with 0 indicating success.
     */
    ErrorCode set_volume64(uint8_t volume);
 
  protected:
    uint32_t mode_;
    std::string mode_to_string() const;
};
 
}  // namespace pcm1792_i2c
}  // namespace esphome
