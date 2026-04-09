import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor

from . import AkeronComponent, CONF_AKERON_ID, akeron_ns

DEPENDENCIES = ["akeron"]

CONF_ALARM_ELX       = "alarm_elx"
CONF_ALARM_REGULATOR = "alarm_regulator"
CONF_WARNING         = "warning"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_AKERON_ID): cv.use_id(AkeronComponent),

        cv.Optional(CONF_ALARM_ELX): text_sensor.text_sensor_schema(
            icon="mdi:alert-circle",
        ),
        cv.Optional(CONF_ALARM_REGULATOR): text_sensor.text_sensor_schema(
            icon="mdi:alert",
        ),
        cv.Optional(CONF_WARNING): text_sensor.text_sensor_schema(
            icon="mdi:alert-outline",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_AKERON_ID])

    if CONF_ALARM_ELX in config:
        s = await text_sensor.new_text_sensor(config[CONF_ALARM_ELX])
        cg.add(parent.set_alarm_elx(s))

    if CONF_ALARM_REGULATOR in config:
        s = await text_sensor.new_text_sensor(config[CONF_ALARM_REGULATOR])
        cg.add(parent.set_alarm_regulator(s))

    if CONF_WARNING in config:
        s = await text_sensor.new_text_sensor(config[CONF_WARNING])
        cg.add(parent.set_warning(s))
