#include "akeron.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#include <esp_gattc_api.h>
#include <esp_bt_defs.h>

namespace esphome {
namespace akeron {

// ──────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ──────────────────────────────────────────────────────────────────────────────

void AkeronComponent::setup() {
  if (this->parent() != nullptr) {
    this->parent()->set_enabled(false);
  }
  this->publish_connection_status_("disconnected");
  this->publish_disconnect_reason_(this->disconnect_reason_);
  this->set_internal_state_(InternalState::DISCONNECTED_IDLE);
  this->set_timeout("boot_connect", this->reconnect_delay_ms_, [this]() {
    this->begin_connection_attempt_();
  });
}

void AkeronComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Akeron BLE component");
  ESP_LOGCONFIG(TAG, "  Service UUID : %s", SERVICE_UUID);
  ESP_LOGCONFIG(TAG, "  Char UUID    : %s", CHAR_UUID);
  ESP_LOGCONFIG(TAG, "  Reconnect delay: %u ms", this->reconnect_delay_ms_);
  ESP_LOGCONFIG(TAG, "  Watchdog timeout: %u ms", this->watchdog_timeout_ms_);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "pH",            ph_);
  LOG_SENSOR("  ", "Redox",         redox_);
  LOG_SENSOR("  ", "Temperature",   temperature_);
  LOG_SENSOR("  ", "Salt",          salt_);
  LOG_SENSOR("  ", "pH Setpoint",   ph_setpoint_);
  LOG_SENSOR("  ", "Redox Setpoint",redox_setpoint_);
  LOG_SENSOR("  ", "ELX Production",elx_production_);
  LOG_SENSOR("  ", "Boost Duration",boost_duration_);
  LOG_BINARY_SENSOR("  ", "pH Pump Active",  ph_pump_active_);
  LOG_BINARY_SENSOR("  ", "ELX Active",      elx_active_);
  LOG_BINARY_SENSOR("  ", "Pumps Forced",    pumps_forced_);
  LOG_BINARY_SENSOR("  ", "Cover Active",    cover_active_);
  LOG_BINARY_SENSOR("  ", "Flow Switch",     flow_switch_);
  LOG_BINARY_SENSOR("  ", "Boost Active",    boost_active_);
  LOG_TEXT_SENSOR("  ", "Alarm ELX",         alarm_elx_);
  LOG_TEXT_SENSOR("  ", "Alarm Regulator",   alarm_regulator_);
  LOG_TEXT_SENSOR("  ", "Warning",           warning_);
  LOG_TEXT_SENSOR("  ", "Connection Status", connection_status_);
  LOG_TEXT_SENSOR("  ", "Last Disconnect Reason", last_disconnect_reason_);
}

// ──────────────────────────────────────────────────────────────────────────────
// Polling — sends four request frames staggered 500 ms apart
// ──────────────────────────────────────────────────────────────────────────────

void AkeronComponent::update() {
  if (!this->is_connected_()) {
    return;
  }

  this->publish_connection_status_("connected");

  this->send_request_(MNEMO_M);
  this->set_timeout("req_s", 500,  [this]() { this->send_request_(MNEMO_S); });
  this->set_timeout("req_a", 1000, [this]() { this->send_request_(MNEMO_A); });
  this->set_timeout("req_e", 1500, [this]() { this->send_request_(MNEMO_E); });
}

// ──────────────────────────────────────────────────────────────────────────────
// BLE GATTC event handler
// ──────────────────────────────────────────────────────────────────────────────

void AkeronComponent::gattc_event_handler(esp_gattc_cb_event_t event,
                                           esp_gatt_if_t gattc_if,
                                           esp_ble_gattc_cb_param_t *param) {
  switch (event) {

    // Connection opened — save remote BDA for later use
    case ESP_GATTC_OPEN_EVT: {
      if (param->open.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "BLE open failed, status=%d", param->open.status);
        this->force_reconnect_(DisconnectReason::ERROR);
        break;
      }
      ESP_LOGI(TAG, "Connected to Akeron BLE box");
      memcpy(this->remote_bda_, param->open.remote_bda, sizeof(esp_bd_addr_t));
      this->rx_len_ = 0;
      this->publish_connection_status_("connecting");
      this->set_internal_state_(InternalState::CONNECTING);
      break;
    }

    // Service discovery complete — find characteristic and register for indications
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      if (param->search_cmpl.conn_id != this->parent()->get_conn_id()) break;

      auto *chr = this->parent()->get_characteristic(
          espbt::ESPBTUUID::from_raw(SERVICE_UUID),
          espbt::ESPBTUUID::from_raw(CHAR_UUID));

      if (chr == nullptr) {
        ESP_LOGE(TAG, "BLE characteristic not found");
        this->set_internal_state_(InternalState::ERROR);
        this->force_reconnect_(DisconnectReason::ERROR);
        break;
      }

      this->char_handle_ = chr->handle;

      auto status = esp_ble_gattc_register_for_notify(
          gattc_if, this->remote_bda_, this->char_handle_);
      if (status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "BLE register_for_notify failed for 0x%04X, status=%d",
                 this->char_handle_, status);
        this->set_internal_state_(InternalState::ERROR);
        this->force_reconnect_(DisconnectReason::ERROR);
      }
      break;
    }

    // Registered for notify/indicate — write CCCD to enable indications (0x0002)
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      if (param->reg_for_notify.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "BLE register_for_notify failed for 0x%04X, status=%d",
                 param->reg_for_notify.handle, param->reg_for_notify.status);
        this->set_internal_state_(InternalState::ERROR);
        this->force_reconnect_(DisconnectReason::ERROR);
        break;
      }

      // Find the Client Characteristic Configuration Descriptor (UUID 0x2902)
      esp_bt_uuid_t cccd_uuid;
      cccd_uuid.len        = ESP_UUID_LEN_16;
      cccd_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;  // 0x2902

      esp_gattc_descr_elem_t descr_elem{};
      uint16_t count = 1;
      esp_gatt_status_t ret = esp_ble_gattc_get_descr_by_char_handle(
          gattc_if, this->parent()->get_conn_id(),
          this->char_handle_, cccd_uuid, &descr_elem, &count);

      if (ret != ESP_GATT_OK || count == 0) {
        ESP_LOGW(TAG, "BLE CCCD not found for 0x%04X, status=%d", this->char_handle_, ret);
        this->set_internal_state_(InternalState::ERROR);
        this->force_reconnect_(DisconnectReason::ERROR);
        break;
      }

      // Write 0x0002 to enable indications (0x0001 = notifications, 0x0002 = indications)
      uint8_t cccd_val[2] = {0x02, 0x00};
      esp_ble_gattc_write_char_descr(
          gattc_if, this->parent()->get_conn_id(),
          descr_elem.handle,
          sizeof(cccd_val), cccd_val,
          ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
      break;
    }

    // CCCD write complete — we are now subscribed
    case ESP_GATTC_WRITE_DESCR_EVT: {
      if (param->write.conn_id != this->parent()->get_conn_id()) {
        break;
      }
      if (param->write.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "BLE CCCD write failed, status=%d", param->write.status);
        this->set_internal_state_(InternalState::ERROR);
        this->force_reconnect_(DisconnectReason::ERROR);
        break;
      }
      if (this->node_state == espbt::ClientState::ESTABLISHED ||
          this->internal_state_ == InternalState::CONNECTED_IDLE) {
        break;
      }
      this->node_state = espbt::ClientState::ESTABLISHED;
      this->last_notify_ms_ = millis();
      this->cancel_timeout("connect_attempt");
      this->publish_connection_status_("connected");
      this->publish_disconnect_reason_(DisconnectReason::NONE);
      this->set_internal_state_(InternalState::CONNECTED_IDLE);
      ESP_LOGI(TAG, "Akeron BLE session ready");
      this->on_subscribed_();
      break;
    }

    // Indication / notification received — buffer and parse
    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle != this->char_handle_) break;
      this->last_notify_ms_ = millis();

      auto hex = this->format_hex_(param->notify.value, param->notify.value_len);
      ESP_LOGV(TAG, "BLE notify (%u bytes): %s", param->notify.value_len, hex.c_str());

      size_t avail = RX_BUF_SIZE - this->rx_len_;
      size_t copy  = std::min((size_t) param->notify.value_len, avail);
      if (copy < param->notify.value_len) {
        // Buffer nearly full — reset to avoid stale data
        ESP_LOGW(TAG, "BLE RX buffer overflow, resetting");
        this->rx_len_ = 0;
        copy = std::min((size_t) param->notify.value_len, RX_BUF_SIZE);
      }
      memcpy(this->rx_buf_ + this->rx_len_, param->notify.value, copy);
      this->rx_len_ += copy;
      this->parse_buffer_();
      break;
    }

    // Disconnected — mark sensors unavailable and reset state
    case ESP_GATTC_DISCONNECT_EVT: {
      ESP_LOGI(TAG, "Disconnected from Akeron BLE box");
      if (this->disconnect_reason_ == DisconnectReason::NONE) {
        this->disconnect_reason_ = DisconnectReason::LINK_LOSS;
      }
      this->reset_connection_state_();
      this->publish_connection_status_("disconnected");
      this->publish_disconnect_reason_(this->disconnect_reason_);
      this->set_internal_state_(InternalState::DISCONNECTED_IDLE);
      // Cancel pending watchdog — no point firing it after disconnect
      this->cancel_timeout("watchdog");
      this->cancel_timeout("connect_attempt");
      this->cancel_timeout("req_s");
      this->cancel_timeout("req_a");
      this->cancel_timeout("req_e");
      this->cancel_timeout("post_write_poll");
      this->mark_unavailable_();
      if (this->parent() != nullptr) {
        this->parent()->set_enabled(false);
      }
      this->schedule_reconnect_();
      break;
    }

    default:
      break;
  }
}

// ──────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ──────────────────────────────────────────────────────────────────────────────

void AkeronComponent::on_subscribed_() {
  this->update();
  // Arm the stale-data watchdog
  this->reset_watchdog_();
}

void AkeronComponent::send_request_(uint8_t mnemo) {
  if (this->parent() == nullptr || this->char_handle_ == 0) return;

  uint8_t frame[REQUEST_LEN];
  build_request(mnemo, frame);

  auto status = esp_ble_gattc_write_char(
      this->parent()->get_gattc_if(),
      this->parent()->get_conn_id(),
      this->char_handle_,
      REQUEST_LEN,
      frame,
      ESP_GATT_WRITE_TYPE_RSP,
      ESP_GATT_AUTH_REQ_NONE);

  if (status != ESP_GATT_OK) {
    ESP_LOGW(TAG, "BLE request send failed for mnemo 0x%02X, status=%d", mnemo, status);
  } else {
    ESP_LOGV(TAG, "BLE request sent for mnemo 0x%02X", mnemo);
  }
}

void AkeronComponent::parse_buffer_() {
  // Sliding-window scan: look for 0x2A at i and i+16
  if (this->rx_len_ < FRAME_LEN) return;

  size_t i = 0;
  while (i + FRAME_LEN <= this->rx_len_) {
    if (this->rx_buf_[i] == FRAME_MARKER &&
        this->rx_buf_[i + 16] == FRAME_MARKER) {
      const uint8_t *candidate = this->rx_buf_ + i;
      if (validate_frame(candidate)) {
        this->dispatch_frame_(candidate);
        // Consume everything up to and including this frame
        size_t consumed = i + FRAME_LEN;
        this->rx_len_ -= consumed;
        if (this->rx_len_ > 0) {
          memmove(this->rx_buf_, this->rx_buf_ + consumed, this->rx_len_);
        }
        i = 0;  // restart scan from the beginning of remaining data
        continue;
      }
    }
    i++;
  }

  // Discard leading bytes that can never be the start of a frame
  // (everything before the last possible frame start position)
  if (this->rx_len_ > 0 && i > 0) {
    size_t keep = this->rx_len_ - std::min(i, this->rx_len_);
    if (keep < this->rx_len_) {
      memmove(this->rx_buf_, this->rx_buf_ + (this->rx_len_ - keep), keep);
      this->rx_len_ = keep;
    }
  }
}

void AkeronComponent::dispatch_frame_(const uint8_t *frame) {
  uint8_t mnemo = frame[1];
  this->reset_watchdog_();

  switch (mnemo) {
    case MNEMO_M: {
      auto m = parse_trame_m(frame);
      if (m.ph_valid       && ph_ != nullptr)          ph_->publish_state(m.ph);
      if (m.redox_valid    && redox_ != nullptr)        redox_->publish_state(m.redox);
      if (m.temp_valid     && temperature_ != nullptr)  temperature_->publish_state(m.temperature);
      if (m.salt_valid     && salt_ != nullptr)         salt_->publish_state(m.salt);
      if (ph_pump_active_)  ph_pump_active_->publish_state(m.ph_pump_active);
      if (elx_active_)      elx_active_->publish_state(m.elx_active);
      if (pumps_forced_)    pumps_forced_->publish_state(m.pumps_forced);
      if (alarm_elx_)       alarm_elx_->publish_state(alarm_elx_text(m.alarm_elx));
      if (alarm_regulator_) alarm_regulator_->publish_state(alarm_rdx_text(m.alarm_rdx));
      if (warning_)         warning_->publish_state(warning_text(m.warning));
      break;
    }

    case MNEMO_S: {
      auto s = parse_trame_s(frame);
      if (ph_setpoint_)        ph_setpoint_->publish_state(s.ph_setpoint);
      if (ph_setpoint_number_) ph_setpoint_number_->publish_state(s.ph_setpoint);
      break;
    }

    case MNEMO_E: {
      auto e = parse_trame_e(frame);
      if (redox_setpoint_) redox_setpoint_->publish_state(e.redox_setpoint);
      break;
    }

    case MNEMO_A: {
      auto a = parse_trame_a(frame);
      this->raw_field_a10_ = a.raw_byte10;
      if (elx_production_)        elx_production_->publish_state((float) a.elx_production);
      if (elx_production_number_) elx_production_number_->publish_state((float) a.elx_production);
      if (boost_duration_) boost_duration_->publish_state((float) a.boost_duration);
      if (cover_active_)   cover_active_->publish_state(a.cover_active);
      if (cover_force_switch_) cover_force_switch_->publish_state(a.cover_forced);
      if (flow_switch_)    flow_switch_->publish_state(a.flow_switch);
      if (boost_active_)   boost_active_->publish_state(a.boost_active);
      break;
    }

    default:
      break;
  }
}

void AkeronComponent::mark_unavailable_() {
  // Numeric sensors: publish NaN → HA shows "Unavailable"
  if (ph_)              ph_->publish_state(NAN);
  if (redox_)           redox_->publish_state(NAN);
  if (temperature_)     temperature_->publish_state(NAN);
  if (salt_)            salt_->publish_state(NAN);
  if (ph_setpoint_)     ph_setpoint_->publish_state(NAN);
  if (redox_setpoint_)  redox_setpoint_->publish_state(NAN);
  if (elx_production_)  elx_production_->publish_state(NAN);
  if (boost_duration_)  boost_duration_->publish_state(NAN);

  // Text sensors
  if (alarm_elx_)       alarm_elx_->publish_state("Unavailable");
  if (alarm_regulator_) alarm_regulator_->publish_state("Unavailable");
  if (warning_)         warning_->publish_state("Unavailable");

  // Binary sensors — default to off (safe state)
  if (ph_pump_active_)  ph_pump_active_->publish_state(false);
  if (elx_active_)      elx_active_->publish_state(false);
  if (pumps_forced_)    pumps_forced_->publish_state(false);
  if (cover_active_)    cover_active_->publish_state(false);
  if (flow_switch_)     flow_switch_->publish_state(false);
  if (boost_active_)    boost_active_->publish_state(false);
}

void AkeronComponent::reset_watchdog_() {
  if (this->watchdog_timeout_ms_ == 0) {
    this->cancel_timeout("watchdog");
    return;
  }
  this->set_timeout("watchdog", this->watchdog_timeout_ms_, [this]() {
    if (!this->is_connected_()) return;
    ESP_LOGW(TAG, "BLE watchdog: no valid trame for %u ms, forcing BLE reconnect",
             this->watchdog_timeout_ms_);
    this->force_reconnect_(DisconnectReason::WATCHDOG);
  });
}

void AkeronComponent::reset_connection_state_() {
  this->cancel_timeout("connect_attempt");
  this->char_handle_ = 0;
  this->rx_len_ = 0;
  this->node_state = espbt::ClientState::IDLE;
  this->last_notify_ms_ = 0;
}

void AkeronComponent::schedule_reconnect_() {
  this->cancel_timeout("reconnect");
  if (this->parent() == nullptr) {
    return;
  }
  ESP_LOGI(TAG, "BLE reconnect scheduled in %u ms", this->reconnect_delay_ms_);
  this->set_timeout("reconnect", this->reconnect_delay_ms_, [this]() {
    this->begin_connection_attempt_();
  });
}

void AkeronComponent::begin_connection_attempt_() {
  if (this->parent() == nullptr) {
    ESP_LOGW(TAG, "BLE connection attempt skipped: parent not ready");
    return;
  }
  this->disconnect_reason_ = DisconnectReason::NONE;
  ESP_LOGI(TAG, "BLE connection attempt started");
  this->publish_connection_status_("connecting");
  this->set_internal_state_(InternalState::CONNECTING);
  this->reset_connection_state_();
  this->set_timeout("connect_attempt", CONNECTION_ATTEMPT_TIMEOUT_MS, [this]() {
    this->handle_connection_attempt_timeout_();
  });
  this->parent()->set_enabled(true);
  if (this->ble_tracker_ != nullptr) {
    ESP_LOGD(TAG, "BLE scan started for connection attempt");
    this->ble_tracker_->start_scan();
  } else {
    ESP_LOGW(TAG, "BLE connection attempt failed: tracker not available");
  }
}

void AkeronComponent::handle_connection_attempt_timeout_() {
  if (this->internal_state_ != InternalState::CONNECTING || this->is_connected_()) {
    return;
  }

  ESP_LOGW(TAG, "BLE connection attempt timed out after %u ms", CONNECTION_ATTEMPT_TIMEOUT_MS);
  this->reset_connection_state_();
  this->publish_connection_status_("disconnected");
  this->set_internal_state_(InternalState::DISCONNECTED_IDLE);
  this->mark_unavailable_();
  this->schedule_reconnect_();
}

void AkeronComponent::force_reconnect_(DisconnectReason reason) {
  this->cancel_timeout("watchdog");
  this->cancel_timeout("connect_attempt");
  this->cancel_timeout("reconnect");
  this->cancel_timeout("req_s");
  this->cancel_timeout("req_a");
  this->cancel_timeout("req_e");
  this->cancel_timeout("post_write_poll");
  this->disconnect_reason_ = reason;
  this->publish_connection_status_("disconnected");
  this->publish_disconnect_reason_(reason);
  this->set_internal_state_(reason == DisconnectReason::ERROR ? InternalState::ERROR
                                                              : InternalState::DISCONNECTED_IDLE);
  this->mark_unavailable_();
  if (this->parent() == nullptr) {
    this->reset_connection_state_();
    return;
  }

  uint16_t conn_id = this->parent()->get_conn_id();
  esp_gatt_if_t gattc_if = this->parent()->get_gattc_if();
  bool had_connection = this->char_handle_ != 0;

  this->reset_connection_state_();
  this->schedule_reconnect_();
  this->parent()->set_enabled(false);

  if (had_connection) {
    esp_ble_gattc_close(gattc_if, conn_id);
  }
}

void AkeronComponent::publish_connection_status_(const char *status) {
  if (this->last_connection_status_ == status) {
    return;
  }
  this->last_connection_status_ = status;
  if (this->connection_status_ != nullptr) {
    this->connection_status_->publish_state(status);
  }
}

void AkeronComponent::publish_disconnect_reason_(DisconnectReason reason) {
  this->disconnect_reason_ = reason;
  if (this->last_disconnect_reason_ != nullptr) {
    this->last_disconnect_reason_->publish_state(this->disconnect_reason_to_string_(reason));
  }
}

void AkeronComponent::set_internal_state_(InternalState state) {
  this->internal_state_ = state;
}

std::string AkeronComponent::format_hex_(const uint8_t *data, size_t len) const {
  return format_hex_pretty(data, len);
}

const char *AkeronComponent::disconnect_reason_to_string_(DisconnectReason reason) const {
  switch (reason) {
    case DisconnectReason::BOOT:
      return "boot";
    case DisconnectReason::LINK_LOSS:
      return "link_loss";
    case DisconnectReason::WATCHDOG:
      return "watchdog";
    case DisconnectReason::ERROR:
      return "error";
    case DisconnectReason::NONE:
    default:
      return "none";
  }
}

// ──────────────────────────────────────────────────────────────────────────────
// Write commands
// ──────────────────────────────────────────────────────────────────────────────

void AkeronComponent::write_command_(const uint8_t *frame) {
  // Guard 1: parent BLE client must be set and connected
  if (this->parent() == nullptr) {
    ESP_LOGW(TAG, "BLE write skipped: parent is null");
    return;
  }
  // Guard 2: characteristic handle must have been discovered
  if (this->char_handle_ == 0) {
    ESP_LOGW(TAG, "BLE write skipped: characteristic not ready");
    return;
  }
  auto status = esp_ble_gattc_write_char(
      this->parent()->get_gattc_if(),
      this->parent()->get_conn_id(),
      this->char_handle_,
      CMD_FRAME_LEN,
      const_cast<uint8_t *>(frame),
      ESP_GATT_WRITE_TYPE_RSP,
      ESP_GATT_AUTH_REQ_NONE);
  if (status != ESP_GATT_OK) {
    ESP_LOGW(TAG, "BLE write failed, status=%d", status);
  }
}

void AkeronComponent::trigger_post_write_poll_() {
  // Wait 1 s for the Akeron to process the command, then re-poll.
  this->set_timeout("post_write_poll", 1000, [this]() { this->update(); });
}

void AkeronComponent::write_ph_setpoint(float value) {
  ESP_LOGD(TAG, "BLE write pH setpoint: %.2f", value);
  uint8_t frame[CMD_FRAME_LEN];
  build_command_ph_setpoint(value, frame);
  this->write_command_(frame);
  this->trigger_post_write_poll_();
}

void AkeronComponent::write_elx_production(float value) {
  auto percent = (uint8_t) value;
  ESP_LOGD(TAG, "BLE write ELX production: %u%%", percent);
  uint8_t frame[CMD_FRAME_LEN];
  build_command_elx_production(percent, frame);
  this->write_command_(frame);
  this->trigger_post_write_poll_();
}

void AkeronComponent::write_cover_force(bool state) {
  ESP_LOGD(TAG, "BLE write cover force: %s", state ? "ON" : "OFF");
  uint8_t frame[CMD_FRAME_LEN];
  build_command_cover_force(state, this->raw_field_a10_, frame);
  this->write_command_(frame);
  this->trigger_post_write_poll_();
}

// ──────────────────────────────────────────────────────────────────────────────
// Number entity implementations
// ──────────────────────────────────────────────────────────────────────────────

void AkeronPhSetpointNumber::control(float value) {
  this->parent_->write_ph_setpoint(value);
  this->publish_state(value);  // optimistic update; confirmed by next trame S
}

void AkeronElxProductionNumber::control(float value) {
  this->parent_->write_elx_production(value);
  this->publish_state(value);  // optimistic update; confirmed by next trame A
}

// ──────────────────────────────────────────────────────────────────────────────
// Switch entity implementation
// ──────────────────────────────────────────────────────────────────────────────

void AkeronCoverForceSwitch::write_state(bool state) {
  this->parent_->write_cover_force(state);
  this->publish_state(state);  // optimistic update; confirmed by next trame A
}

}  // namespace akeron
}  // namespace esphome
