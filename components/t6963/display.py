import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display
from esphome.const import (
    CONF_ID,
    CONF_LAMBDA,
    CONF_PAGES,
    CONF_WIDTH,
    CONF_HEIGHT,
    CONF_UPDATE_INTERVAL,
)

DEPENDENCIES = []
AUTO_LOAD = ["display"]

t6963_ns = cg.esphome_ns.namespace("t6963")
T6963Display = t6963_ns.class_(
    "T6963Display", cg.PollingComponent, display.DisplayBuffer
)

CONF_CS_PIN   = "cs_pin"
CONF_WR_PIN   = "wr_pin"
CONF_RD_PIN   = "rd_pin"
CONF_CD_PIN   = "cd_pin"
CONF_RST_PIN  = "rst_pin"
CONF_DATA_PINS = "data_pins"

# Validate exactly 8 data pins
def validate_data_pins(value):
    if len(value) != 8:
        raise cv.Invalid("Exactly 8 data pins required (DB0-DB7)")
    return value

CONFIG_SCHEMA = display.FULL_DISPLAY_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(T6963Display),
        cv.Required(CONF_WIDTH):      cv.int_range(min=1, max=640),
        cv.Required(CONF_HEIGHT):     cv.int_range(min=1, max=480),
        cv.Required(CONF_CS_PIN):     pins.gpio_output_pin_schema,
        cv.Required(CONF_WR_PIN):     pins.gpio_output_pin_schema,
        cv.Required(CONF_RD_PIN):     pins.gpio_output_pin_schema,
        cv.Required(CONF_CD_PIN):     pins.gpio_output_pin_schema,
        cv.Required(CONF_RST_PIN):    pins.gpio_output_pin_schema,
        cv.Required(CONF_DATA_PINS):  cv.All(
            cv.ensure_list(pins.gpio_pin_schema({cv.Optional("mode"): cv.one_of("OUTPUT", upper=True)})),
            validate_data_pins,
        ),
    }
).extend(cv.polling_component_schema("1s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await display.register_display(var, config)

    cg.add(var.set_width(config[CONF_WIDTH]))
    cg.add(var.set_height(config[CONF_HEIGHT]))

    cs  = await cg.gpio_pin_expression(config[CONF_CS_PIN])
    wr  = await cg.gpio_pin_expression(config[CONF_WR_PIN])
    rd  = await cg.gpio_pin_expression(config[CONF_RD_PIN])
    cd  = await cg.gpio_pin_expression(config[CONF_CD_PIN])
    rst = await cg.gpio_pin_expression(config[CONF_RST_PIN])
    cg.add(var.set_cs_pin(cs))
    cg.add(var.set_wr_pin(wr))
    cg.add(var.set_rd_pin(rd))
    cg.add(var.set_cd_pin(cd))
    cg.add(var.set_rst_pin(rst))

    for i, pin_conf in enumerate(config[CONF_DATA_PINS]):
        pin = await cg.gpio_pin_expression(pin_conf)
        cg.add(var.set_data_pin(i, pin))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA],
            [(display.DisplayBufferRef, "it")],
            return_type=cg.void,
        )
        cg.add(var.set_writer(lambda_))
