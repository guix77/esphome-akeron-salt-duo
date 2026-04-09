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
  // Nothing to do here — the BLE client parent manages connection.
}

void AkeronComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Akeron BLE component");
  ESP_LOGCONFIG(TAG, "  Service UUID : %s", SERVICE_UUID);
  ESP_LOGCONFIG(TAG, "  Char UUID    : %s", CHAR_UUID);
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
  LOG_TEXT_SENSOR("  ", "Alarm ELX",       alarm_elx_);
  LOG_TEXT_SENSOR("  ", "Alarm Regulator", alarm_regulator_);
  LOG_TEXT_SENSOR("  ", "Warning",         warning_);
}

// ──────────────────────────────────────────────────────────────────────────────
// Polling — sends four request frames staggered 500 ms apart
// ──────────────────────────────────────────────────────────────────────────────

void AkeronComponent::update() {
  if (!this->is_connected_()) {
    ESP_LOGD(TAG, "update() called but not connected, skipping");
    return;
  }
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
        ESP_LOGW(TAG, "Connection failed, status=%d", param->open.status);
        break;
      }
      ESP_LOGI(TAG, "Connected to Akeron BLE device");
      memcpy(this->remote_bda_, param->open.remote_bda, sizeof(esp_bd_addr_t));
      this->rx_len_ = 0;
      break;
    }

    // Service discovery complete — find characteristic and register for indications
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      if (param->search_cmpl.conn_id != this->parent()->get_conn_id()) break;

      auto *chr = this->parent()->get_characteristic(
          espbt::ESPBTUUID::from_raw(SERVICE_UUID),
          espbt::ESPBTUUID::from_raw(CHAR_UUID));

      if (chr == nullptr) {
        ESP_LOGE(TAG, "Akeron characteristic not found — check UUIDs");
        break;
      }

      this->char_handle_ = chr->handle;
      ESP_LOGD(TAG, "Found characteristic, handle=0x%04X", this->char_handle_);

      auto status = esp_ble_gattc_register_for_notify(
          gattc_if, this->remote_bda_, this->char_handle_);
      if (status != ESP_GATT_OK) {
        ESP_LOGE(TAG, "esp_ble_gattc_register_for_notify failed, status=%d", status);
      }
      break;
    }

    // Registered for notify/indicate — write CCCD to enable indications (0x0002)
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      if (param->reg_for_notify.status != ESP_GATT_OK) {
        ESP_LOGE(TAG, "register_for_notify failed, status=%d",
                 param->reg_for_notify.status);
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
        ESP_LOGE(TAG, "CCCD descriptor not found (status=%d)", ret);
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
      if (param->write.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Failed to write CCCD, status=%d", param->write.status);
        break;
      }
      ESP_LOGI(TAG, "Subscribed to Akeron indications");
      this->node_state = espbt::ClientState::ESTABLISHED;
      this->on_subscribed_();
      break;
    }

    // Indication / notification received — buffer and parse
    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle != this->char_handle_) break;

      size_t avail = RX_BUF_SIZE - this->rx_len_;
      size_t copy  = std::min((size_t) param->notify.value_len, avail);
      if (copy < param->notify.value_len) {
        // Buffer nearly full — reset to avoid stale data
        ESP_LOGW(TAG, "RX buffer overflow, resetting");
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
      ESP_LOGW(TAG, "Disconnected from Akeron BLE device");
      this->char_handle_ = 0;
      this->rx_len_      = 0;
      this->node_state   = espbt::ClientState::IDLE;
      this->mark_unavailable_();
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
  // Immediately trigger a poll cycle so we get fresh data on connect
  this->update();
}

void AkeronComponent::send_request_(uint8_t mnemo) {
  if (!this->is_connected_()) return;

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
    ESP_LOGW(TAG, "Failed to send request for mnemo 0x%02X (status=%d)", mnemo, status);
  } else {
    ESP_LOGV(TAG, "Sent request for mnemo 0x%02X", mnemo);
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
  ESP_LOGV(TAG, "Received frame mnemo=0x%02X (%c)", mnemo, mnemo);

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
      if (ph_setpoint_) ph_setpoint_->publish_state(s.ph_setpoint);
      break;
    }

    case MNEMO_E: {
      auto e = parse_trame_e(frame);
      if (redox_setpoint_) redox_setpoint_->publish_state(e.redox_setpoint);
      break;
    }

    case MNEMO_A: {
      auto a = parse_trame_a(frame);
      if (elx_production_) elx_production_->publish_state((float) a.elx_production);
      if (boost_duration_) boost_duration_->publish_state((float) a.boost_duration);
      if (cover_active_)   cover_active_->publish_state(a.cover_active);
      if (flow_switch_)    flow_switch_->publish_state(a.flow_switch);
      if (boost_active_)   boost_active_->publish_state(a.boost_active);
      break;
    }

    default:
      ESP_LOGD(TAG, "Unknown/unhandled frame mnemo=0x%02X", mnemo);
      break;
  }
}

void AkeronComponent::mark_unavailable_() {
  // Numeric sensors: there's no "unavailable" publish in ESPHome sensor API,
  // but not publishing keeps the last value. We just log a warning.
  // Text sensors can publish a meaningful "unavailable" string.
  ESP_LOGW(TAG, "BLE disconnected — sensor values are stale");
  if (alarm_elx_)       alarm_elx_->publish_state("unavailable");
  if (alarm_regulator_) alarm_regulator_->publish_state("unavailable");
  if (warning_)         warning_->publish_state("unavailable");
  // Binary sensors
  if (ph_pump_active_)  ph_pump_active_->publish_state(false);
  if (elx_active_)      elx_active_->publish_state(false);
  if (pumps_forced_)    pumps_forced_->publish_state(false);
  if (cover_active_)    cover_active_->publish_state(false);
  if (flow_switch_)     flow_switch_->publish_state(false);
  if (boost_active_)    boost_active_->publish_state(false);
}

}  // namespace akeron
}  // namespace esphome
