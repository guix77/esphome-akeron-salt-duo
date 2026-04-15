import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch

from . import AkeronComponent, CONF_AKERON_ID, akeron_ns

DEPENDENCIES = ["akeron"]

CONF_COVER_FORCE = "cover_force"
CONF_DEBUG_LOGS = "debug_logs"

AkeronCoverForceSwitch = akeron_ns.class_("AkeronCoverForceSwitch", switch.Switch)
AkeronDebugSwitch = akeron_ns.class_("AkeronDebugSwitch", switch.Switch)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_AKERON_ID): cv.use_id(AkeronComponent),

        cv.Optional(CONF_COVER_FORCE): switch.switch_schema(
            AkeronCoverForceSwitch,
            icon="mdi:window-shutter",
        ),
        cv.Optional(CONF_DEBUG_LOGS): switch.switch_schema(
            AkeronDebugSwitch,
            icon="mdi:bug",
            entity_category="diagnostic",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_AKERON_ID])

    if CONF_COVER_FORCE in config:
        s = await switch.new_switch(config[CONF_COVER_FORCE])
        cg.add(s.set_parent(parent))
        cg.add(parent.set_cover_force_switch(s))

    if CONF_DEBUG_LOGS in config:
        s = await switch.new_switch(config[CONF_DEBUG_LOGS])
        cg.add(s.set_parent(parent))
        cg.add(parent.set_debug_switch(s))
