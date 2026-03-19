#include "t6963.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <cstring>

namespace esphome {
namespace t6963 {

static const char *const TAG = "t6963";

// ─────────────────────────────────────────────────────────────────────────────
//  ESPHome lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void T6963Display::setup() {
  ESP_LOGCONFIG(TAG, "Setting up T6963 display (%dx%d)...", width_, height_);

  // Allocate frame buffer (1 bit/pixel, packed)
  size_t buf_size = (width_ / 8) * height_;
  framebuf_ = new uint8_t[buf_size];
  memset(framebuf_, 0x00, buf_size);

  // Initialise control pins
  cs_pin_->setup();   cs_pin_->digital_write(true);   // deselect
  wr_pin_->setup();   wr_pin_->digital_write(true);
  rd_pin_->setup();   rd_pin_->digital_write(true);
  cd_pin_->setup();   cd_pin_->digital_write(true);
  rst_pin_->setup();  rst_pin_->digital_write(true);

  // Data bus pins
  set_data_pins_as_output_();

  // Hardware reset: pull RST low ≥1 ms, then release
  rst_pin_->digital_write(false);
  delay(2);
  rst_pin_->digital_write(true);
  delay(10);

  t6963_init_();
}

void T6963Display::update() {
  // Run the user lambda to draw into our frame buffer
  this->do_update_();

  // Push frame buffer to GRAM
  flush_framebuf_();
}

void T6963Display::dump_config() {
  LOG_DISPLAY("", "T6963", this);
  ESP_LOGCONFIG(TAG, "  Dimensions: %dx%d", width_, height_);
}

// ─────────────────────────────────────────────────────────────────────────────
//  display::DisplayBuffer interface
// ─────────────────────────────────────────────────────────────────────────────

void T6963Display::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x < 0 || x >= width_ || y < 0 || y >= height_)
    return;

  // Byte index: each row has (width_/8) bytes; pixel x lives in byte x/8
  int byte_idx = y * (width_ / 8) + (x / 8);
  uint8_t bit_mask = 0x80 >> (x % 8);  // MSB = leftmost pixel

  if (color.is_on()) {
    framebuf_[byte_idx] |=  bit_mask;
  } else {
    framebuf_[byte_idx] &= ~bit_mask;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
//  T6963C initialisation sequence
// ─────────────────────────────────────────────────────────────────────────────

void T6963Display::t6963_init_() {
  uint16_t columns = width_ / 8;  // e.g. 240/8 = 30 for the AG240128B

  // 1. Set graphic home address (where GRAM starts)
  write_data2_command_(
      (uint8_t)(T6963_GRAPHIC_HOME & 0xFF),
      (uint8_t)(T6963_GRAPHIC_HOME >> 8),
      T6963_SET_GRAPHIC_HOME_ADDRESS);

  // 2. Set graphic area (columns wide)
  write_data2_command_(columns, 0x00, T6963_SET_GRAPHIC_AREA);

  // 3. Set text home address (unused but must be set)
  write_data2_command_(
      (uint8_t)(T6963_TEXT_HOME & 0xFF),
      (uint8_t)(T6963_TEXT_HOME >> 8),
      T6963_SET_TEXT_HOME_ADDRESS);

  // 4. Set text area (same column width)
  write_data2_command_(columns, 0x00, T6963_SET_TEXT_AREA);

  // 5. OR mode, internal CG ROM
  write_command_(T6963_MODE_SET | 0x00);

  // 6. Turn on graphic display only (no text layer)
  write_command_(T6963_DISPLAY_GRAPHIC_ON);

  // 7. Clear GRAM: write 0x00 to every byte
  uint16_t gram_size = columns * height_;
  write_data2_command_(
      (uint8_t)(T6963_GRAPHIC_HOME & 0xFF),
      (uint8_t)(T6963_GRAPHIC_HOME >> 8),
      T6963_SET_ADDRESS_POINTER);

  for (uint16_t i = 0; i < gram_size; i++) {
    write_data_command_(0x00, T6963_DATA_WRITE_INC);
  }

  ESP_LOGD(TAG, "T6963 init complete");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Flush frame buffer to GRAM
// ─────────────────────────────────────────────────────────────────────────────

void T6963Display::flush_framebuf_() {
  // Set address pointer to graphic home
  write_data2_command_(
      (uint8_t)(T6963_GRAPHIC_HOME & 0xFF),
      (uint8_t)(T6963_GRAPHIC_HOME >> 8),
      T6963_SET_ADDRESS_POINTER);

  size_t buf_size = (width_ / 8) * height_;
  for (size_t i = 0; i < buf_size; i++) {
    write_data_command_(framebuf_[i], T6963_DATA_WRITE_INC);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Low-level bus helpers
// ─────────────────────────────────────────────────────────────────────────────

// Poll status byte until STA0 and STA1 are both high (controller ready).
void T6963Display::wait_for_ready_() {
  set_data_pins_as_input_();
  cs_pin_->digital_write(false);
  cd_pin_->digital_write(true);  // command/status mode

  uint32_t timeout = millis() + 10;  // 10 ms safety timeout
  while (millis() < timeout) {
    rd_pin_->digital_write(false);
    delayMicroseconds(1);
    uint8_t status = read_data_bus_();
    rd_pin_->digital_write(true);
    if ((status & (T6963_STATUS_STA0 | T6963_STATUS_STA1)) ==
        (T6963_STATUS_STA0 | T6963_STATUS_STA1))
      break;
  }

  cs_pin_->digital_write(true);
  set_data_pins_as_output_();
}

// Write a single data byte (CD=LOW)
void T6963Display::write_data_(uint8_t data) {
  wait_for_ready_();
  cd_pin_->digital_write(false);  // data mode
  cs_pin_->digital_write(false);
  write_data_bus_(data);
  wr_pin_->digital_write(false);
  delayMicroseconds(1);
  wr_pin_->digital_write(true);
  cs_pin_->digital_write(true);
}

// Write a single command byte (CD=HIGH)
void T6963Display::write_command_(uint8_t cmd) {
  wait_for_ready_();
  cd_pin_->digital_write(true);   // command mode
  cs_pin_->digital_write(false);
  write_data_bus_(cmd);
  wr_pin_->digital_write(false);
  delayMicroseconds(1);
  wr_pin_->digital_write(true);
  cs_pin_->digital_write(true);
}

// Write one data byte then one command byte (the standard T6963C sequence)
void T6963Display::write_data_command_(uint8_t data, uint8_t cmd) {
  write_data_(data);
  write_command_(cmd);
}

// Write two data bytes (lo, hi) then one command byte
void T6963Display::write_data2_command_(uint8_t lo, uint8_t hi, uint8_t cmd) {
  write_data_(lo);
  write_data_(hi);
  write_command_(cmd);
}

// ─── GPIO data bus helpers ───────────────────────────────────────────────────

void T6963Display::set_data_pins_as_output_() {
  for (int i = 0; i < 8; i++) {
    data_pins_[i]->pin_mode(gpio::FLAG_OUTPUT);
  }
}

void T6963Display::set_data_pins_as_input_() {
  for (int i = 0; i < 8; i++) {
    data_pins_[i]->pin_mode(gpio::FLAG_INPUT);
  }
}

void T6963Display::write_data_bus_(uint8_t value) {
  for (int i = 0; i < 8; i++) {
    data_pins_[i]->digital_write((value >> i) & 0x01);
  }
}

uint8_t T6963Display::read_data_bus_() {
  uint8_t value = 0;
  for (int i = 0; i < 8; i++) {
    if (data_pins_[i]->digital_read())
      value |= (1 << i);
  }
  return value;
}

}  // namespace t6963
}  // namespace esphome
