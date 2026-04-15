import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client, esp32_ble_tracker
from esphome.const import CONF_ID

CODEOWNERS = ["@guix"]
DEPENDENCIES = ["ble_client", "esp32_ble_tracker"]
AUTO_LOAD = ["sensor", "binary_sensor", "text_sensor", "number", "switch"]

akeron_ns = cg.esphome_ns.namespace("akeron")
AkeronComponent = akeron_ns.class_(
    "AkeronComponent", cg.PollingComponent, ble_client.BLEClientNode
)

# Exported so sub-platforms can import and reference the component
CONF_AKERON_ID = "akeron_id"
CONF_ESP32_BLE_ID = "esp32_ble_id"
CONF_RECONNECT_DELAY = "reconnect_delay"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AkeronComponent),
            cv.GenerateID(CONF_ESP32_BLE_ID): cv.use_id(esp32_ble_tracker.ESP32BLETracker),
            cv.Optional(
                CONF_RECONNECT_DELAY, default="10s"
            ): cv.positive_time_period_milliseconds,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(ble_client.BLE_CLIENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)
    ble_tracker = await cg.get_variable(config[CONF_ESP32_BLE_ID])
    cg.add(var.set_ble_tracker(ble_tracker))
    cg.add(var.set_reconnect_delay_ms(config[CONF_RECONNECT_DELAY]))
