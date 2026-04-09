import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number

from . import AkeronComponent, CONF_AKERON_ID, akeron_ns

DEPENDENCIES = ["akeron"]

CONF_PH_SETPOINT    = "ph_setpoint"
CONF_ELX_PRODUCTION = "elx_production"

AkeronPhSetpointNumber    = akeron_ns.class_("AkeronPhSetpointNumber", number.Number)
AkeronElxProductionNumber = akeron_ns.class_("AkeronElxProductionNumber", number.Number)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_AKERON_ID): cv.use_id(AkeronComponent),

        cv.Optional(CONF_PH_SETPOINT): number.number_schema(
            AkeronPhSetpointNumber,
            unit_of_measurement="pH",
            icon="mdi:ph",
        ),
        cv.Optional(CONF_ELX_PRODUCTION): number.number_schema(
            AkeronElxProductionNumber,
            unit_of_measurement="%",
            icon="mdi:percent",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_AKERON_ID])

    if CONF_PH_SETPOINT in config:
        n = await number.new_number(
            config[CONF_PH_SETPOINT],
            min_value=6.80,
            max_value=7.80,
            step=0.05,
        )
        cg.add(n.set_parent(parent))
        cg.add(parent.set_ph_setpoint_number(n))

    if CONF_ELX_PRODUCTION in config:
        n = await number.new_number(
            config[CONF_ELX_PRODUCTION],
            min_value=0,
            max_value=100,
            step=10,
        )
        cg.add(n.set_parent(parent))
        cg.add(parent.set_elx_production_number(n))
