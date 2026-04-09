import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor

from . import AkeronComponent, CONF_AKERON_ID, akeron_ns

DEPENDENCIES = ["akeron"]

CONF_PH_PUMP_ACTIVE = "ph_pump_active"
CONF_ELX_ACTIVE     = "elx_active"
CONF_PUMPS_FORCED   = "pumps_forced"
CONF_COVER_ACTIVE   = "cover_active"
CONF_FLOW_SWITCH    = "flow_switch"
CONF_BOOST_ACTIVE   = "boost_active"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_AKERON_ID): cv.use_id(AkeronComponent),

        cv.Optional(CONF_PH_PUMP_ACTIVE): binary_sensor.binary_sensor_schema(
            icon="mdi:pump",
        ),
        cv.Optional(CONF_ELX_ACTIVE): binary_sensor.binary_sensor_schema(
            icon="mdi:lightning-bolt",
        ),
        cv.Optional(CONF_PUMPS_FORCED): binary_sensor.binary_sensor_schema(
            icon="mdi:pump-off",
        ),
        cv.Optional(CONF_COVER_ACTIVE): binary_sensor.binary_sensor_schema(
            icon="mdi:pool",
        ),
        cv.Optional(CONF_FLOW_SWITCH): binary_sensor.binary_sensor_schema(
            icon="mdi:water-flow",
        ),
        cv.Optional(CONF_BOOST_ACTIVE): binary_sensor.binary_sensor_schema(
            icon="mdi:rocket-launch",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_AKERON_ID])

    if CONF_PH_PUMP_ACTIVE in config:
        s = await binary_sensor.new_binary_sensor(config[CONF_PH_PUMP_ACTIVE])
        cg.add(parent.set_ph_pump_active(s))

    if CONF_ELX_ACTIVE in config:
        s = await binary_sensor.new_binary_sensor(config[CONF_ELX_ACTIVE])
        cg.add(parent.set_elx_active(s))

    if CONF_PUMPS_FORCED in config:
        s = await binary_sensor.new_binary_sensor(config[CONF_PUMPS_FORCED])
        cg.add(parent.set_pumps_forced(s))

    if CONF_COVER_ACTIVE in config:
        s = await binary_sensor.new_binary_sensor(config[CONF_COVER_ACTIVE])
        cg.add(parent.set_cover_active(s))

    if CONF_FLOW_SWITCH in config:
        s = await binary_sensor.new_binary_sensor(config[CONF_FLOW_SWITCH])
        cg.add(parent.set_flow_switch(s))

    if CONF_BOOST_ACTIVE in config:
        s = await binary_sensor.new_binary_sensor(config[CONF_BOOST_ACTIVE])
        cg.add(parent.set_boost_active(s))
