import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    ICON_THERMOMETER,
    ICON_EMPTY,
)

from . import AkeronComponent, CONF_AKERON_ID, akeron_ns

DEPENDENCIES = ["akeron"]

CONF_PH             = "ph"
CONF_REDOX          = "redox"
CONF_TEMPERATURE    = "temperature"
CONF_SALT           = "salt"
CONF_PH_SETPOINT    = "ph_setpoint"
CONF_REDOX_SETPOINT = "redox_setpoint"
CONF_ELX_PRODUCTION = "elx_production"
CONF_BOOST_DURATION = "boost_duration"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_AKERON_ID): cv.use_id(AkeronComponent),

        cv.Optional(CONF_PH): sensor.sensor_schema(
            unit_of_measurement="pH",
            accuracy_decimals=2,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:ph",
        ),
        cv.Optional(CONF_REDOX): sensor.sensor_schema(
            unit_of_measurement="mV",
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:flash",
        ),
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
            icon=ICON_THERMOMETER,
        ),
        cv.Optional(CONF_SALT): sensor.sensor_schema(
            unit_of_measurement="g/L",
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:shaker-outline",
        ),
        cv.Optional(CONF_PH_SETPOINT): sensor.sensor_schema(
            unit_of_measurement="pH",
            accuracy_decimals=2,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:ph",
        ),
        cv.Optional(CONF_REDOX_SETPOINT): sensor.sensor_schema(
            unit_of_measurement="mV",
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:flash",
        ),
        cv.Optional(CONF_ELX_PRODUCTION): sensor.sensor_schema(
            unit_of_measurement="%",
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:percent",
        ),
        cv.Optional(CONF_BOOST_DURATION): sensor.sensor_schema(
            unit_of_measurement="min",
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:timer-outline",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_AKERON_ID])

    if CONF_PH in config:
        s = await sensor.new_sensor(config[CONF_PH])
        cg.add(parent.set_ph(s))

    if CONF_REDOX in config:
        s = await sensor.new_sensor(config[CONF_REDOX])
        cg.add(parent.set_redox(s))

    if CONF_TEMPERATURE in config:
        s = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(parent.set_temperature(s))

    if CONF_SALT in config:
        s = await sensor.new_sensor(config[CONF_SALT])
        cg.add(parent.set_salt(s))

    if CONF_PH_SETPOINT in config:
        s = await sensor.new_sensor(config[CONF_PH_SETPOINT])
        cg.add(parent.set_ph_setpoint(s))

    if CONF_REDOX_SETPOINT in config:
        s = await sensor.new_sensor(config[CONF_REDOX_SETPOINT])
        cg.add(parent.set_redox_setpoint(s))

    if CONF_ELX_PRODUCTION in config:
        s = await sensor.new_sensor(config[CONF_ELX_PRODUCTION])
        cg.add(parent.set_elx_production(s))

    if CONF_BOOST_DURATION in config:
        s = await sensor.new_sensor(config[CONF_BOOST_DURATION])
        cg.add(parent.set_boost_duration(s))
