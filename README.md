# ESPHome gateway for Akeron Sault DUO

An ESPHome external component that connects an ESP32 to **Akeron / CORELEC** pool
salt electrolyzers via Bluetooth Low Energy (BLE) and exposes all measurements to
Home Assistant.

---

## Supported models

| Model | Electrolysis | pH sensor + pump | Salt + Temp | Redox |
|---|:---:|:---:|:---:|:---:|
| SALT | ✓ | | | |
| DUO SALT REGUL pH (tested) | ✓ | ✓ | | |
| DUO SALT REGUL3 | ✓ | ✓ | ✓ | |
| DUO SALT REGUL4 Rx | ✓ | ✓ | ✓ | ✓ |

The component auto-detects available data: only include the sensors that your
model provides (values outside the valid range are silently ignored).

---

## Prerequisites

- An **ESP32** board (ESP32-S2/S3 also work with BLE support compiled in)
- **ESPHome** 2023.12 or later
- **Home Assistant** with the ESPHome integration
- The `esp-idf` framework (required — Arduino does not give reliable BLE client)

---

## Finding your Akeron's BLE MAC address

The Akeron continuously advertises its BLE service. Use one of these methods to
find its MAC address:

**Method 1 — nRF Connect (recommended)**
1. Install [nRF Connect for Mobile](https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-for-mobile) on your phone.
2. Scan for devices. Look for a device advertising service UUID
   `0bd51666-e7cb-469b-8e4d-2742f1ba77cc`.
3. Note the MAC address shown (e.g. `AA:BB:CC:DD:EE:FF`).

**Method 2 — ESPHome BLE scanner logs**
1. Flash a minimal ESPHome config with the `esp32_ble_tracker` component.
2. Watch the logs — look for a device with the service UUID above.
3. Copy the MAC address from the log line.

---

## Installation

```bash
git clone https://github.com/your-org/esphome-akeron-salt-duo.git
cd esphome-akeron-salt-duo
```

Copy the `components/` directory next to your ESPHome YAML, or reference it
directly via the `external_components` path.

Create your YAML based on `akeron-example.yaml`:

```yaml
external_components:
  - source:
      type: local
      path: components   # path to this repo's components/ directory

ble_client:
  - mac_address: "AA:BB:CC:DD:EE:FF"
    id: akeron_ble

akeron:
  ble_client_id: akeron_ble
  update_interval: 30s
```

Then add the sensors your model supports (see reference below).

---

## Configuration reference

### `akeron:` (main component)

| Option | Required | Default | Description |
|---|:---:|---|---|
| `ble_client_id` | yes | — | ID of the `ble_client` entry for your Akeron |
| `update_interval` | no | `30s` | How often to poll the Akeron (min ~10 s) |

---

### `sensor: platform: akeron`

All sensors are optional. Include only what your model supports.

| Key | Unit | Decimals | Valid range | Description |
|---|---|:---:|---|---|
| `ph` | pH | 2 | 3.50–9.50 | Water pH |
| `redox` | mV | 0 | 350–1000 | Redox potential |
| `temperature` | °C | 1 | 0–50 | Water temperature |
| `salt` | g/L | 1 | 0–10 | Salt concentration |
| `ph_setpoint` | pH | 2 | — | Target pH (from device config) |
| `redox_setpoint` | mV | 0 | — | Target redox (from device config) |
| `elx_production` | % | 0 | 0–100 | Electrolysis production level |
| `boost_duration` | min | 0 | — | Remaining boost time (0 = inactive) |

---

### `binary_sensor: platform: akeron`

| Key | Description |
|---|---|
| `ph_pump_active` | pH correction pump running |
| `elx_active` | Electrolysis cell active |
| `pumps_forced` | Pumps in forced mode |
| `cover_active` | Pool cover detected as active |
| `flow_switch` | Flow switch state |
| `boost_active` | Boost mode currently on |

---

### `text_sensor: platform: akeron`

| Key | Description |
|---|---|
| `alarm_elx` | Electrolysis alarm (`OK`, `E.01 electrode short/scale`, …) |
| `alarm_regulator` | pH/redox regulator alarm (`OK`, `E.10 pH probe fault`, …) |
| `warning` | Warning status (`OK`, `W.01 pH drift`, …) |

---

### `number: platform: akeron`

These entities are **read-write**: they display the current value confirmed by
the device and allow changing it from Home Assistant.

| Key | Unit | Range | Step | Description |
|---|---|---|---|---|
| `ph_setpoint` | pH | 6.80–7.80 | 0.05 | Target pH setpoint |
| `elx_production` | % | 0–100 | 10 | Electrolysis production level |

> **Tip:** if you configure a `number` entity, the corresponding read-only
> `sensor` entity (`ph_setpoint` sensor / `elx_production` sensor) is redundant
> — both reflect the confirmed value from the device. Use one or the other.

---

### `switch: platform: akeron`

| Key | Description |
|---|---|
| `cover_force` | Force the Akeron to treat the pool cover as closed (ON) or open (OFF) |

The `cover_force` switch is useful for pools **without a physical cover cable**
connected to the Akeron. When the cover is physically closed but the Akeron
doesn't know it (because there is no cable), you can switch this ON to stop
electrolysis. Switch it OFF when the cover is open and you want electrolysis
to resume. The state is read back from the device after every poll cycle.

---

## Which entities to enable per model

| Entity | SALT | REGUL pH | REGUL3 | REGUL4 Rx |
|---|:---:|:---:|:---:|:---:|
| `ph` sensor/number | | ✓ | ✓ | ✓ |
| `ph_setpoint` sensor | | ✓ | ✓ | ✓ |
| `ph_pump_active` | | ✓ | ✓ | ✓ |
| `alarm_regulator` | | ✓ | ✓ | ✓ |
| `temperature` | | | ✓ | ✓ |
| `salt` | | | ✓ | ✓ |
| `redox` sensor | | | | ✓ |
| `redox_setpoint` sensor | | | | ✓ |
| `elx_production` sensor/number | ✓ | ✓ | ✓ | ✓ |
| `boost_duration` | ✓ | ✓ | ✓ | ✓ |
| `boost_active` | ✓ | ✓ | ✓ | ✓ |
| `elx_active` | ✓ | ✓ | ✓ | ✓ |
| `flow_switch` | ✓ | ✓ | ✓ | ✓ |
| `cover_force` switch | ✓ | ✓ | ✓ | ✓ |
| `alarm_elx` | ✓ | ✓ | ✓ | ✓ |
| `warning` | ✓ | ✓ | ✓ | ✓ |
| `connection_status` (diag) | ✓ | ✓ | ✓ | ✓ |
| `last_update` (diag) | ✓ | ✓ | ✓ | ✓ |

---

## Alarm code reference

### Electrolysis alarms (`alarm_elx`)

| Code | Message |
|---|---|
| OK | No alarm |
| E.01 | Electrode short-circuit or scaled |
| E.02 | Salt or water temperature fault |
| E.03 | Electrode worn or disconnected |
| E.04 | Electrical short-circuit |
| E.06 | Device overtemperature |
| E.07 | No water flow |

### Regulator alarms (`alarm_regulator`) — REGUL pH / REGUL3 / REGUL4 only

| Code | Message |
|---|---|
| OK | No alarm |
| E.10 | pH probe read error (< 5.2 or > 9.5) |
| E.11 | pH stagnant despite injections |
| E.13 | pH below alarm threshold (< 6.0) |
| E.14 | pH above alarm threshold (> 9.0) |
| E.15 | pH correction inverted |
| E.18 | Water temperature too low (< 12°C) |
| E.19 | Salt level too low (< 2.0 g/L) |
| E.20 | Redox too high (> 950 mV) |
| E.21 | Redox low (< 350 mV) |
| E.22 | Redox too low (< 250 mV) |

> **Note:** the mapping from raw nibble to E.xx code for regulator alarms is
> estimated from CORELEC documentation. If you observe incorrect decoding,
> open an issue with the raw frame bytes from DEBUG logs.

---

## Known limitations

### Boost duration (byte overlap)

Byte[2] of trame A encodes **ELX production %** (confirmed correct). The same
byte is also the MSB of a uint16 that was supposed to encode **boost duration**
in minutes. This creates a conflict: when production is non-zero, the derived
uint16 is garbage (e.g. 17920 when production = 70%).

**Current workaround:** any boost_duration value ≥ 480 min (8 h) is silently
replaced with 0. Boost_active is only true when 0 < boost_duration < 480.

To help resolve this, enable DEBUG logs, activate boost from the physical Akeron
panel, and look for the `Trame A raw:` log line. Share the raw bytes in an
issue so the correct offset can be identified.

---

## Troubleshooting

**ESP32 overheating / brownout**

The ESP32 at 240 MHz with simultaneous WiFi + BLE can draw enough current to
cause a brownout on boards with a weak USB cable or power supply.

Mitigations (apply all for best results):
1. Use a quality USB cable (thick wire, rated ≥ 1 A) or a dedicated 5 V power supply.
2. Reduce CPU frequency to 160 MHz in your YAML:
   ```yaml
   esp32:
     framework:
       type: esp-idf
       sdkconfig_options:
         CONFIG_ESP32_DEFAULT_CPU_FREQ_160: "y"
         CONFIG_ESP32_DEFAULT_CPU_FREQ_240: "n"
   ```
3. Disable continuous BLE scanning (scanner stops once connected):
   ```yaml
   esp32_ble_tracker:
     scan_parameters:
       continuous: false
       active: false
   ```
4. Increase `update_interval` to `60s` or more.
5. Delay the first BLE poll — the component already does this automatically
   (10 s after boot) to let WiFi stabilise first.

**Device not found during BLE scan**
- The ESP32 must be within ~5–10 m of the Akeron (BLE range). Walls, a full
  pool, and other RF sources reduce range significantly.
- The Akeron only supports **one simultaneous BLE connection**. If the nRF
  Connect app (or another device) is already connected, the ESP32 cannot connect.
- Try restarting both the ESP32 and the Akeron.

**Connected but no data / `connection_status` stuck at "Connecting"**
- Check the MAC address in your YAML exactly matches your Akeron.
- Enable `DEBUG` log level: `logger: level: DEBUG`. Look for `Trame A raw:` lines.
- If no raw frames appear but the device is connected, check that the Akeron
  firmware supports the protocol described here (tested on CORELEC v1.x).
- The component has a 5-minute watchdog: if no valid frame is received, it
  forces a BLE reconnect automatically.

**Values stuck / sensor showing "Unavailable" in HA**
- The `last_update` diagnostic counter stops incrementing → BLE data stopped.
- On disconnect all numeric sensors publish NaN (→ "Unavailable" in HA) and
  text sensors show "Unavailable". This is intentional — stale data is hidden.
- Check `connection_status`: if "Disconnected", the ble_client will reconnect
  automatically within a few seconds.

**Values jump or seem wrong**
- The component filters out-of-range values (pH outside 3.50–9.50, etc.),
  so spurious spikes are discarded automatically.
- See the Boost Duration section above for the known byte[2] overlap issue.

**BLE connection drops frequently**
- Use `update_interval: 60s` — polling every 30 s generates more BLE traffic.
- Ensure no other BLE client (phone app, etc.) tries to connect simultaneously.
- Add `esp32_ble_tracker: scan_parameters: continuous: false`.

---

## Changelog

### Phase 3 (current)
- Full alarm code decoding with CORELEC E.xx messages
- Diagnostic sensors: `connection_status`, `last_update` (frame counter)
- All sensors publish NaN (→ "Unavailable" in HA) on BLE disconnect
- 5-minute watchdog: forces BLE reconnect if no frame received while connected
- CPU frequency 160 MHz recommendation to reduce heat
- Raw trame A logging at DEBUG level to help debug boost_duration

### Phase 2
- Write commands: pH setpoint, ELX production %, cover force switch
- Number entities for writable controls
- Post-write poll (1 s delay) to confirm written values

### Phase 1
- Initial BLE read-only component
- Sensors: pH, redox, temperature, salt, setpoints, ELX production, boost
- Binary sensors: pumps, cover, flow switch, boost active
- Text sensors: alarm ELX, alarm regulator, warning

---

## Credits

Protocol reverse-engineering inspired by
[sylvaing's original Arduino/MQTT project](https://github.com/sylvaing19/hackeron)
(also known as "hackeron"). This ESPHome component is a clean-room rewrite for
native Home Assistant integration without MQTT.

---

## License

MIT — see [LICENSE](LICENSE).
