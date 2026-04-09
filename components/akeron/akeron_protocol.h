#pragma once

#include <cstdint>
#include <cstring>
#include <cmath>

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

// ─── Alarm text maps ──────────────────────────────────────────────────────────
// Alarm codes are raw nibble values (0–15) from the GATT frame.
// Their mapping to CORELEC E.xx display codes is based on documentation;
// nibble-to-E.xx correspondence is confirmed for ELX, estimated for regulator.

/// ELX alarm codes — trame M byte[10] (full byte) and trame A byte[12] low nibble.
inline const char *alarm_elx_text(uint8_t code) {
  switch (code) {
    case 0: return "OK";
    case 1: return "E.01 Electrode short-circuit or scaled";
    case 2: return "E.02 Salt or water temperature fault";
    case 3: return "E.03 Electrode worn or disconnected";
    case 4: return "E.04 Electrical short-circuit";
    case 6: return "E.06 Device overtemperature";
    case 7: return "E.07 No water flow";
    default: {
      // Return a static buffer with the raw code for unknown values
      static char buf[24];
      snprintf(buf, sizeof(buf), "E.?? Unknown alarm (%u)", code);
      return buf;
    }
  }
}

/// Regulator alarm codes — trame M byte[11] high nibble (4 bits, 0–15).
// Mapping nibble → CORELEC E.xx is estimated from documentation; verify on hardware.
// Not all E.xx codes are present (E.12, E.16, E.17 are absent from the spec list).
inline const char *alarm_rdx_text(uint8_t code) {
  switch (code) {
    case 0:  return "OK";
    case 1:  return "E.10 pH probe read error (< 5.2 or > 9.5)";
    case 2:  return "E.11 pH stagnant despite injections";
    case 3:  return "E.13 pH below alarm threshold (< 6.0)";
    case 4:  return "E.14 pH above alarm threshold (> 9.0)";
    case 5:  return "E.15 pH correction inverted";
    case 6:  return "E.18 Water temperature too low (< 12°C)";
    case 7:  return "E.19 Salt level too low (< 2.0 g/L)";
    case 8:  return "E.20 Redox too high (> 950 mV)";
    case 9:  return "E.21 Redox low (< 350 mV)";
    case 10: return "E.22 Redox too low (< 250 mV)";
    default: {
      static char buf[28];
      snprintf(buf, sizeof(buf), "E.?? Unknown alarm (%u)", code);
      return buf;
    }
  }
}

/// Warning status — trame M byte[11] low nibble (4 bits, 0–15).
// Warning codes are informational; they do not stop operation.
inline const char *warning_text(uint8_t code) {
  switch (code) {
    case 0: return "OK";
    case 1: return "W.01 pH drift detected";
    case 2: return "W.02 Redox drift detected";
    case 3: return "W.03 Salt level low";
    case 4: return "W.04 Salt level high";
    case 5: return "W.05 Water temperature low";
    default: {
      static char buf[28];
      snprintf(buf, sizeof(buf), "W.?? Unknown warning (%u)", code);
      return buf;
    }
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
  uint8_t  raw_byte10;      // byte [10] verbatim — needed by cover_force command
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

  a.raw_byte10     = f[10];
  a.cover_active   = (f[10] >> 4) & 1;
  a.cover_forced   = (f[10] >> 3) & 1;
  a.flow_switch    = (f[10] >> 2) & 1;
  a.alarm_elx      = f[12] & 0x0F;
  return a;
}

// ─── Write command builders ───────────────────────────────────────────────────
// All commands are 17-byte frames. Unused payload bytes = 0xFF ("don't change").

static const uint8_t CMD_FRAME_LEN = 17;

/// pH setpoint command (trame S, mnemo=83).
/// consigne: e.g. 7.35 → bytes[2..3] = 735 big-endian.
inline void build_command_ph_setpoint(float consigne, uint8_t out[CMD_FRAME_LEN]) {
  memset(out, 0xFF, CMD_FRAME_LEN);
  out[0] = FRAME_MARKER;
  out[1] = MNEMO_S;
  uint16_t val = (uint16_t) roundf(consigne * 100.0f);
  out[2] = (val >> 8) & 0xFF;
  out[3] = val & 0xFF;
  out[15] = calc_crc(out, 15);
  out[16] = FRAME_MARKER;
}

/// ELX production command (trame A, mnemo=65).
/// percent: 0–100, step 10.
inline void build_command_elx_production(uint8_t percent, uint8_t out[CMD_FRAME_LEN]) {
  memset(out, 0xFF, CMD_FRAME_LEN);
  out[0] = FRAME_MARKER;
  out[1] = MNEMO_A;
  out[2] = percent;
  out[15] = calc_crc(out, 15);
  out[16] = FRAME_MARKER;
}

/// Cover force command (trame A, mnemo=65).
/// Flips bit 3 of byte[10]; all other bits are preserved from current_a10
/// (the last raw byte[10] received from a trame A indication).
inline void build_command_cover_force(bool state, uint8_t current_a10,
                                      uint8_t out[CMD_FRAME_LEN]) {
  memset(out, 0xFF, CMD_FRAME_LEN);
  out[0] = FRAME_MARKER;
  out[1] = MNEMO_A;
  const uint8_t mask = (1 << 3);
  out[10] = state ? (current_a10 | mask) : (current_a10 & ~mask);
  out[15] = calc_crc(out, 15);
  out[16] = FRAME_MARKER;
}

}  // namespace akeron
}  // namespace esphome
