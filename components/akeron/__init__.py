import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client
from esphome.const import CONF_ID

CODEOWNERS = ["@guix"]
DEPENDENCIES = ["ble_client"]
AUTO_LOAD = ["sensor", "binary_sensor", "text_sensor"]

akeron_ns = cg.esphome_ns.namespace("akeron")
AkeronComponent = akeron_ns.class_(
    "AkeronComponent", cg.PollingComponent, ble_client.BLEClientNode
)

# Exported so sub-platforms can import and reference the component
CONF_AKERON_ID = "akeron_id"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AkeronComponent),
        }
    )
    .extend(cv.polling_component_schema("30s"))
    .extend(ble_client.BLE_CLIENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)
