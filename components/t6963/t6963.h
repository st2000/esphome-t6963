#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/display/display_buffer.h"

namespace esphome {
namespace t6963 {

// ─── T6963C command bytes ─────────────────────────────────────────────────────
static const uint8_t T6963_SET_CURSOR_POINTER       = 0x21;
static const uint8_t T6963_SET_OFFSET_REGISTER      = 0x22;
static const uint8_t T6963_SET_ADDRESS_POINTER       = 0x24;

static const uint8_t T6963_SET_TEXT_HOME_ADDRESS     = 0x40;
static const uint8_t T6963_SET_TEXT_AREA             = 0x41;
static const uint8_t T6963_SET_GRAPHIC_HOME_ADDRESS  = 0x42;
static const uint8_t T6963_SET_GRAPHIC_AREA          = 0x43;

static const uint8_t T6963_MODE_SET                  = 0x80;  // OR mode, internal CG
static const uint8_t T6963_DISPLAY_MODE              = 0x90;  // display off
static const uint8_t T6963_DISPLAY_GRAPHIC_ON        = 0x98;  // graphic layer on

static const uint8_t T6963_DATA_WRITE_INC            = 0xC0;  // write data, auto-increment
static const uint8_t T6963_DATA_READ_INC             = 0xC1;
static const uint8_t T6963_DATA_WRITE                = 0xC4;  // write, no increment

// Status byte masks (read from DB0-DB7 when CD=HIGH)
static const uint8_t T6963_STATUS_STA0 = 0x01;  // ready to accept command/data
static const uint8_t T6963_STATUS_STA1 = 0x02;  // ready for memory read/write

// ─── GRAM layout constants ────────────────────────────────────────────────────
// Graphic Home Address in T6963C GRAM.  Text area placed above so it doesn't
// overlap; for graphics-only use we can put graphic RAM at 0x0000.
static const uint16_t T6963_GRAPHIC_HOME  = 0x0000;
static const uint16_t T6963_TEXT_HOME     = 0x1000;  // above graphic area

class T6963Display : public display::DisplayBuffer, public PollingComponent {
 public:
  // ── Pin setters (called from display.py → to_code) ─────────────────────────
  void set_cs_pin(GPIOPin *pin)  { cs_pin_  = pin; }
  void set_wr_pin(GPIOPin *pin)  { wr_pin_  = pin; }
  void set_rd_pin(GPIOPin *pin)  { rd_pin_  = pin; }
  void set_cd_pin(GPIOPin *pin)  { cd_pin_  = pin; }
  void set_rst_pin(GPIOPin *pin) { rst_pin_ = pin; }
  void set_data_pin(uint8_t idx, GPIOPin *pin) { data_pins_[idx] = pin; }

  void set_width(int w)   { width_  = w; }
  void set_height(int h)  { height_ = h; }
  void set_writer(display::DisplayBufferWriter writer) { writer_ = writer; }

  // ── ESPHome component lifecycle ─────────────────────────────────────────────
  void setup() override;
  void update() override;
  void dump_config() override;

  // ── display::DisplayBuffer interface ────────────────────────────────────────
  int  get_width_internal()  override { return width_;  }
  int  get_height_internal() override { return height_; }

 protected:
  void draw_absolute_pixel_internal(int x, int y, Color color) override;

 private:
  // ── Control pins ─────────────────────────────────────────────────────────────
  GPIOPin *cs_pin_   = nullptr;
  GPIOPin *wr_pin_   = nullptr;
  GPIOPin *rd_pin_   = nullptr;
  GPIOPin *cd_pin_   = nullptr;
  GPIOPin *rst_pin_  = nullptr;
  GPIOPin *data_pins_[8] = {};

  int width_  = 240;
  int height_ = 128;

  // ── Frame buffer ─────────────────────────────────────────────────────────────
  // 1 bit per pixel, packed 8 pixels per byte, row-major.
  // Size = (width/8) * height bytes.
  uint8_t *framebuf_ = nullptr;

  display::DisplayBufferWriter writer_{};

  // ── Low-level bus helpers ─────────────────────────────────────────────────────
  void     wait_for_ready_();
  void     write_data_(uint8_t data);
  void     write_command_(uint8_t cmd);
  void     write_data_command_(uint8_t data, uint8_t cmd);
  void     write_data2_command_(uint8_t lo, uint8_t hi, uint8_t cmd);
  void     set_data_pins_as_output_();
  void     set_data_pins_as_input_();
  uint8_t  read_data_bus_();
  void     write_data_bus_(uint8_t value);

  // ── T6963C init helpers ───────────────────────────────────────────────────────
  void t6963_init_();
  void flush_framebuf_();
};

}  // namespace t6963
}  // namespace esphome
