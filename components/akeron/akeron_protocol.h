#pragma once

#include <cstdint>
#include <cstring>

namespace esphome {
namespace akeron {

// ─── BLE identifiers ──────────────────────────────────────────────────────────
static const char *const SERVICE_UUID = "0bd51666-e7cb-469b-8e4d-2742f1ba77cc";
static const char *const CHAR_UUID    = "e7add780-b042-4876-aae1-112855353cc1";

// ─── Frame constants ──────────────────────────────────────────────────────────
static const uint8_t FRAME_MARKER     = 0x2A;  // '*'
static const uint8_t FRAME_LEN        = 17;
static const uint8_t REQUEST_LEN      = 6;
static const uint8_t REQUEST_CMD      = 0x52;  // 'R'
static const uint8_t REQUEST_ARG      = 0x3F;  // '?'

// Mnemonics (frame type byte [1])
static const uint8_t MNEMO_M = 77;   // Measurements
static const uint8_t MNEMO_S = 83;   // pH setpoints
static const uint8_t MNEMO_E = 69;   // Redox setpoint
static const uint8_t MNEMO_A = 65;   // Electrolysis & accessories

// ─── Valid sensor ranges ───────────────────────────────────────────────────────
static const float PH_MIN      = 3.50f;
static const float PH_MAX      = 9.50f;
static const float REDOX_MIN   = 350.0f;
static const float REDOX_MAX   = 1000.0f;
static const float TEMP_MIN    = 0.0f;
static const float TEMP_MAX    = 50.0f;
static const float SALT_MIN    = 0.0f;
static const float SALT_MAX    = 10.0f;
static const uint8_t ELX_MIN   = 0;
static const uint8_t ELX_MAX   = 100;

// ─── CRC ──────────────────────────────────────────────────────────────────────
/// XOR of bytes [0..count-1]
inline uint8_t calc_crc(const uint8_t *buf, size_t count) {
  uint8_t crc = 0;
  for (size_t i = 0; i < count; i++) {
    crc ^= buf[i];
  }
  return crc;
}

/// Validate a 17-byte response frame.
inline bool validate_frame(const uint8_t *frame) {
  if (frame[0] != FRAME_MARKER || frame[16] != FRAME_MARKER) {
    return false;
  }
  return frame[15] == calc_crc(frame, 15);
}

// ─── Request frame builder ────────────────────────────────────────────────────
/// Build a 6-byte request frame asking the Akeron for mnemo <m>.
inline void build_request(uint8_t mnemo, uint8_t out[6]) {
  out[0] = FRAME_MARKER;
  out[1] = REQUEST_CMD;
  out[2] = REQUEST_ARG;
  out[3] = mnemo;
  out[4] = calc_crc(out, 4);
  out[5] = FRAME_MARKER;
}

// ─── Alarm text maps ──────────────────────────────────────────────────────────

/// Electrolysis alarm codes (byte [10] of trame M, low nibble of trame A byte [12])
inline const char *alarm_elx_text(uint8_t code) {
  switch (code) {
    case 0: return "OK";
    case 1: return "E.01 electrode short/scale";
    case 2: return "E.02 salt or temperature out of range";
    case 3: return "E.03 electrode worn";
    case 4: return "E.04 electrical fault";
    case 6: return "E.06 overtemperature";
    case 7: return "E.07 no flow";
    default: return "E.?? unknown alarm";
  }
}

/// Regulator alarm codes (high nibble of trame M byte [11])
inline const char *alarm_rdx_text(uint8_t code) {
  switch (code) {
    case 0:  return "OK";
    case 1:  return "E.10 pH probe fault";
    case 2:  return "E.11 pH low alarm";
    case 3:  return "E.12 pH high alarm";
    case 4:  return "E.13 redox probe fault";
    case 5:  return "E.14 redox low alarm";
    case 6:  return "E.15 redox high alarm";
    case 7:  return "E.16 pH pump timeout";
    case 8:  return "E.17 redox pump timeout";
    case 9:  return "E.18 no flow (regulator)";
    case 10: return "E.19 temperature fault";
    case 11: return "E.20 salt fault";
    case 12: return "E.21 communication error";
    case 13: return "E.22 configuration error";
    default: return "E.?? unknown alarm";
  }
}

/// Warning status (low nibble of trame M byte [11])
inline const char *warning_text(uint8_t code) {
  switch (code) {
    case 0: return "OK";
    case 1: return "W.01 pH drift";
    case 2: return "W.02 redox drift";
    case 3: return "W.03 low salt";
    case 4: return "W.04 high salt";
    case 5: return "W.05 low temperature";
    default: return "W.?? unknown warning";
  }
}

// ─── Trame M parsed fields ────────────────────────────────────────────────────
struct TrameM {
  float    ph;               // ×100, valid 3.50–9.50
  float    redox;            // mV, valid 350–1000
  float    temperature;      // ×10, valid 0.0–50.0°C
  float    salt;             // ×10, valid 0.0–10.0 g/L
  uint8_t  alarm_elx;        // byte [10]
  uint8_t  warning;          // byte [11] low nibble
  uint8_t  alarm_rdx;        // byte [11] high nibble
  bool     ph_pump_active;   // byte [12] bit 6
  bool     elx_active;       // byte [12] bit 5
  bool     pumps_forced;     // byte [13] bit 7
  bool     ph_valid;
  bool     redox_valid;
  bool     temp_valid;
  bool     salt_valid;
};

/// Parse a validated 17-byte trame M frame.
inline TrameM parse_trame_m(const uint8_t *f) {
  TrameM m{};
  float ph = ((f[2] << 8) | f[3]) / 100.0f;
  m.ph_valid = (ph >= PH_MIN && ph <= PH_MAX);
  if (m.ph_valid) m.ph = ph;

  float redox = (float)((f[4] << 8) | f[5]);
  m.redox_valid = (redox >= REDOX_MIN && redox <= REDOX_MAX);
  if (m.redox_valid) m.redox = redox;

  float temp = ((f[6] << 8) | f[7]) / 10.0f;
  m.temp_valid = (temp >= TEMP_MIN && temp <= TEMP_MAX);
  if (m.temp_valid) m.temperature = temp;

  float salt = ((f[8] << 8) | f[9]) / 10.0f;
  m.salt_valid = (salt >= SALT_MIN && salt <= SALT_MAX);
  if (m.salt_valid) m.salt = salt;

  m.alarm_elx    = f[10];
  m.warning      = f[11] & 0x0F;
  m.alarm_rdx    = (f[11] >> 4) & 0x0F;
  m.ph_pump_active  = (f[12] >> 6) & 1;
  m.elx_active      = (f[12] >> 5) & 1;
  m.pumps_forced    = (f[13] >> 7) & 1;
  return m;
}

// ─── Trame S parsed fields ────────────────────────────────────────────────────
struct TrameS {
  float ph_setpoint;
  float ph_error_max;
  float ph_error_min;
};

inline TrameS parse_trame_s(const uint8_t *f) {
  TrameS s{};
  s.ph_setpoint  = ((f[2] << 8) | f[3])   / 100.0f;
  s.ph_error_max = ((f[10] << 8) | f[11]) / 100.0f;
  s.ph_error_min = ((f[12] << 8) | f[13]) / 100.0f;
  return s;
}

// ─── Trame E parsed fields ────────────────────────────────────────────────────
struct TrameE {
  float redox_setpoint;  // mV
};

inline TrameE parse_trame_e(const uint8_t *f) {
  TrameE e{};
  e.redox_setpoint = (float)((f[2] << 8) | f[3]);
  return e;
}

// ─── Trame A parsed fields ────────────────────────────────────────────────────
// byte[2]   = ELX production % (confirmed: 70 → 70 %)
// boost_duration offset is not yet confirmed from hardware.
// bytes[2..3] as uint16 gives garbage when ELX≠0 (e.g. 17920 when prod=70%).
// Hypothesis: boost_duration is on bytes[3..4] (TBD).
// Guard: treat any value ≥ 480 min (8 h) as invalid and report 0.
static const uint16_t BOOST_DURATION_MAX = 480;  // 8 hours, sanity cap

struct TrameA {
  uint8_t  elx_production;  // byte [2], 0–100 %
  uint16_t boost_duration;  // minutes; 0 if out-of-range or not active
  bool     boost_active;    // boost_duration > 0 AND < BOOST_DURATION_MAX
  bool     cover_active;    // byte [10] bit 4
  bool     cover_forced;    // byte [10] bit 3
  bool     flow_switch;     // byte [10] bit 2
  uint8_t  alarm_elx;       // byte [12] low nibble
};

inline TrameA parse_trame_a(const uint8_t *f) {
  TrameA a{};
  a.elx_production = f[2];

  // bytes[2..3] raw — MSB is the same byte as elx_production, so this
  // will be wrong whenever production is non-zero.  Apply the sanity cap
  // until the correct offset is confirmed from hardware captures.
  uint16_t raw_boost = (uint16_t)((f[2] << 8) | f[3]);
  a.boost_duration = (raw_boost < BOOST_DURATION_MAX) ? raw_boost : 0;
  a.boost_active   = (a.boost_duration > 0);

  a.cover_active   = (f[10] >> 4) & 1;
  a.cover_forced   = (f[10] >> 3) & 1;
  a.flow_switch    = (f[10] >> 2) & 1;
  a.alarm_elx      = f[12] & 0x0F;
  return a;
}

}  // namespace akeron
}  // namespace esphome
