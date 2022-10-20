import esphome.config_validation as cv
import esphome.codegen as cg
from esphome import (automation,pins)
from esphome.components import time
from esphome.const import (CONF_ID,
    CONF_DATA_PIN,
    CONF_RESET_PIN,
    CONF_CLK_PIN)
from esphome.cpp_helpers import gpio_pin_expression


CODEOWNERS = ["@badbadc0ffee"]
DEPENDENCIES = []
ds1302_ns = cg.esphome_ns.namespace("ds1302")
DS1302Component = ds1302_ns.class_("DS1302Component", time.RealTimeClock)
WriteAction = ds1302_ns.class_("WriteAction", automation.Action)
ReadAction = ds1302_ns.class_("ReadAction", automation.Action)


CONFIG_SCHEMA = time.TIME_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(DS1302Component),
        cv.Required(CONF_DATA_PIN): pins.internal_gpio_output_pin_schema,
        cv.Required(CONF_RESET_PIN): pins.internal_gpio_input_pin_schema,
        cv.Required(CONF_CLK_PIN): pins.internal_gpio_output_pin_schema
    }
)


@automation.register_action(
    "ds1302.write_time",
    WriteAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(DS1302Component),
        }
    ),
)
async def ds1302_write_time_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    # await cg.register_parented(var, config[CONF_RESET_PIN])
    # await cg.register_parented(var, config[CONF_CLK_PIN])
    # await cg.register_parented(var, config[CONF_DATA_PIN])
    return var


@automation.register_action(
    "ds1302.read_time",
    ReadAction,
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(DS1302Component),
        }
    ),
)
async def ds1302_read_time_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    # await cg.register_parented(var, config[CONF_RESET_PIN])
    # await cg.register_parented(var, config[CONF_CLK_PIN])
    # await cg.register_parented(var, config[CONF_DATA_PIN])
    return var


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    
    pinDATA = await gpio_pin_expression(config[CONF_DATA_PIN])
    pinCLK = await gpio_pin_expression(config[CONF_CLK_PIN])
    pinRESET = await gpio_pin_expression(config[CONF_RESET_PIN])
    cg.add(var.set_pinDATA(pinDATA))
    cg.add(var.set_pinCLK(pinCLK))
    cg.add(var.set_pinRESET(pinRESET))

    await cg.register_component(var, config)
    await time.register_time(var, config)
