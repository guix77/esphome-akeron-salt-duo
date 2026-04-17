import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch

from . import AkeronComponent, CONF_AKERON_ID, akeron_ns

DEPENDENCIES = ["akeron"]

CONF_COVER_FORCE = "cover_force"

AkeronCoverForceSwitch = akeron_ns.class_("AkeronCoverForceSwitch", switch.Switch)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_AKERON_ID): cv.use_id(AkeronComponent),

        cv.Optional(CONF_COVER_FORCE): switch.switch_schema(
            AkeronCoverForceSwitch,
            icon="mdi:window-shutter",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_AKERON_ID])

    if CONF_COVER_FORCE in config:
        s = await switch.new_switch(config[CONF_COVER_FORCE])
        cg.add(s.set_parent(parent))
        cg.add(parent.set_cover_force_switch(s))
