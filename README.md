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

## Which entities to enable per model

| Entity | SALT | REGUL pH | REGUL3 | REGUL4 Rx |
|---|:---:|:---:|:---:|:---:|
| `ph` | | ✓ | ✓ | ✓ |
| `ph_setpoint` | | ✓ | ✓ | ✓ |
| `ph_pump_active` | | ✓ | ✓ | ✓ |
| `alarm_regulator` | | ✓ | ✓ | ✓ |
| `temperature` | | | ✓ | ✓ |
| `salt` | | | ✓ | ✓ |
| `redox` | | | | ✓ |
| `redox_setpoint` | | | | ✓ |
| `elx_production` | ✓ | ✓ | ✓ | ✓ |
| `boost_duration` | ✓ | ✓ | ✓ | ✓ |
| `boost_active` | ✓ | ✓ | ✓ | ✓ |
| `elx_active` | ✓ | ✓ | ✓ | ✓ |
| `flow_switch` | ✓ | ✓ | ✓ | ✓ |
| `alarm_elx` | ✓ | ✓ | ✓ | ✓ |
| `warning` | ✓ | ✓ | ✓ | ✓ |

---

## Troubleshooting

**Device not found during BLE scan**
- The ESP32 must be within ~5–10 m of the Akeron (BLE range). Walls and water
  reduce range significantly.
- The Akeron must be powered on and not already connected to another BLE client
  (it supports only one connection at a time).
- Try restarting both the ESP32 and the Akeron.

**Connected but no data**
- Check that the MAC address in your YAML matches your device exactly.
- Enable `DEBUG` log level and look for frame parsing messages.
- Verify the Akeron firmware version supports the protocol described here
  (tested on CORELEC firmware v1.x).

**Values jump or seem wrong**
- The component filters out-of-range values (e.g. pH outside 3.50–9.50),
  so spurious spikes are discarded automatically.
- If `boost_duration` and `elx_production` seem inconsistent, see the note in
  the protocol spec — they share byte[2] of trame A.

**BLE connection drops frequently**
- Increase `update_interval` (e.g. `60s`) to reduce BLE traffic.
- Ensure no other BLE client (phone app, etc.) is trying to connect
  simultaneously.

---

## Credits

Protocol reverse-engineering inspired by
[sylvaing's original Arduino/MQTT project](https://github.com/sylvaing19/hackeron)
(also known as "hackeron"). This ESPHome component is a clean-room rewrite for
native Home Assistant integration without MQTT.

---

## License

MIT — see [LICENSE](LICENSE).
