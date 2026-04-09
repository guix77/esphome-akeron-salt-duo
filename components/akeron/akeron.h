#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/number/number.h"
#include "esphome/components/switch/switch.h"
#include "akeron_protocol.h"

#include <cstring>

namespace esphome {
namespace akeron {

namespace espbt = esphome::esp32_ble_tracker;

static const char *const TAG = "akeron";

// ── Forward declarations ──────────────────────────────────────────────────────
class AkeronPhSetpointNumber;
class AkeronElxProductionNumber;
class AkeronCoverForceSwitch;

// ─────────────────────────────────────────────────────────────────────────────
// Main component
// ─────────────────────────────────────────────────────────────────────────────
class AkeronComponent : public PollingComponent, public ble_client::BLEClientNode {
 public:
  // ── Read-only sensor setters ─────────────────────────────────────────────────
  void set_ph(sensor::Sensor *s)            { ph_ = s; }
  void set_redox(sensor::Sensor *s)         { redox_ = s; }
  void set_temperature(sensor::Sensor *s)   { temperature_ = s; }
  void set_salt(sensor::Sensor *s)          { salt_ = s; }
  void set_ph_setpoint(sensor::Sensor *s)   { ph_setpoint_ = s; }
  void set_redox_setpoint(sensor::Sensor *s){ redox_setpoint_ = s; }
  void set_elx_production(sensor::Sensor *s){ elx_production_ = s; }
  void set_boost_duration(sensor::Sensor *s){ boost_duration_ = s; }

  // ── Binary sensor setters ────────────────────────────────────────────────────
  void set_ph_pump_active(binary_sensor::BinarySensor *s)  { ph_pump_active_ = s; }
  void set_elx_active(binary_sensor::BinarySensor *s)      { elx_active_ = s; }
  void set_pumps_forced(binary_sensor::BinarySensor *s)    { pumps_forced_ = s; }
  void set_cover_active(binary_sensor::BinarySensor *s)    { cover_active_ = s; }
  void set_flow_switch(binary_sensor::BinarySensor *s)     { flow_switch_ = s; }
  void set_boost_active(binary_sensor::BinarySensor *s)    { boost_active_ = s; }

  // ── Text sensor setters ──────────────────────────────────────────────────────
  void set_alarm_elx(text_sensor::TextSensor *s)       { alarm_elx_ = s; }
  void set_alarm_regulator(text_sensor::TextSensor *s) { alarm_regulator_ = s; }
  void set_warning(text_sensor::TextSensor *s)         { warning_ = s; }

  // ── Writable control setters ─────────────────────────────────────────────────
  void set_ph_setpoint_number(AkeronPhSetpointNumber *n)       { ph_setpoint_number_ = n; }
  void set_elx_production_number(AkeronElxProductionNumber *n) { elx_production_number_ = n; }
  void set_cover_force_switch(AkeronCoverForceSwitch *s)       { cover_force_switch_ = s; }

  // ── Diagnostic sensor setters ────────────────────────────────────────────────
  void set_connection_status(text_sensor::TextSensor *s) { connection_status_ = s; }
  void set_last_update(sensor::Sensor *s)                { last_update_ = s; }

  // ── Write commands (called by number/switch control() / write_state()) ───────
  void write_ph_setpoint(float value);
  void write_elx_production(float value);
  void write_cover_force(bool state);

  // ── ESPHome component lifecycle ──────────────────────────────────────────────
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // ── BLE client node ──────────────────────────────────────────────────────────
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

 protected:
  // ── BLE state ────────────────────────────────────────────────────────────────
  uint16_t char_handle_{0};
  esp_bd_addr_t remote_bda_{};

  // ── RX buffer (sliding window, 512 bytes) ────────────────────────────────────
  static const size_t RX_BUF_SIZE = 512;
  uint8_t rx_buf_[RX_BUF_SIZE]{};
  size_t  rx_len_{0};

  // ── Last raw byte[10] from trame A (needed for cover_force command) ───────────
  uint8_t raw_field_a10_{0xFF};

  // ── Diagnostics ───────────────────────────────────────────────────────────────
  uint32_t frame_count_{0};  // total valid frames received since boot

  // ── Internal methods ─────────────────────────────────────────────────────────
  bool is_connected_() {
    return this->parent() != nullptr && this->char_handle_ != 0;
  }
  void send_request_(uint8_t mnemo);
  void write_command_(const uint8_t *frame);
  void trigger_post_write_poll_();
  void on_subscribed_();
  void parse_buffer_();
  void dispatch_frame_(const uint8_t *frame);
  void mark_unavailable_();
  void reset_watchdog_();
  void publish_connection_status_(const char *status);

  // ── Read-only sensors ─────────────────────────────────────────────────────────
  sensor::Sensor *ph_{nullptr};
  sensor::Sensor *redox_{nullptr};
  sensor::Sensor *temperature_{nullptr};
  sensor::Sensor *salt_{nullptr};
  sensor::Sensor *ph_setpoint_{nullptr};
  sensor::Sensor *redox_setpoint_{nullptr};
  sensor::Sensor *elx_production_{nullptr};
  sensor::Sensor *boost_duration_{nullptr};

  // ── Binary sensors ────────────────────────────────────────────────────────────
  binary_sensor::BinarySensor *ph_pump_active_{nullptr};
  binary_sensor::BinarySensor *elx_active_{nullptr};
  binary_sensor::BinarySensor *pumps_forced_{nullptr};
  binary_sensor::BinarySensor *cover_active_{nullptr};
  binary_sensor::BinarySensor *flow_switch_{nullptr};
  binary_sensor::BinarySensor *boost_active_{nullptr};

  // ── Text sensors ──────────────────────────────────────────────────────────────
  text_sensor::TextSensor *alarm_elx_{nullptr};
  text_sensor::TextSensor *alarm_regulator_{nullptr};
  text_sensor::TextSensor *warning_{nullptr};

  // ── Writable controls ────────────────────────────────────────────────────────
  AkeronPhSetpointNumber    *ph_setpoint_number_{nullptr};
  AkeronElxProductionNumber *elx_production_number_{nullptr};
  AkeronCoverForceSwitch    *cover_force_switch_{nullptr};

  // ── Diagnostic sensors ────────────────────────────────────────────────────────
  text_sensor::TextSensor *connection_status_{nullptr};
  sensor::Sensor          *last_update_{nullptr};
};

// ─────────────────────────────────────────────────────────────────────────────
// Number entities
// ─────────────────────────────────────────────────────────────────────────────

/// Writable pH setpoint (6.80–7.80, step 0.05).
class AkeronPhSetpointNumber : public number::Number, public Parented<AkeronComponent> {
 protected:
  void control(float value) override;
};

/// Writable ELX production % (0–100, step 10).
class AkeronElxProductionNumber : public number::Number, public Parented<AkeronComponent> {
 protected:
  void control(float value) override;
};

// ─────────────────────────────────────────────────────────────────────────────
// Switch entity
// ─────────────────────────────────────────────────────────────────────────────

/// Cover force switch — tells the Akeron the pool cover is closed/open
/// (for installations without a physical cover cable).
class AkeronCoverForceSwitch : public switch_::Switch, public Parented<AkeronComponent> {
 protected:
  void write_state(bool state) override;
};

}  // namespace akeron
}  // namespace esphome
