import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID
from esphome.const import CONF_MODE

DEPENDENCIES = ["i2c"]
CODEOWNERS = ["@JosVanEijndhoven"]
MULTI_CONF = True

pcm1792_i2c_ns = cg.esphome_ns.namespace("pcm1792_i2c")

Pcm1792I2C = pcm1792_i2c_ns.class_(
    "Pcm1792I2C", cg.Component, i2c.I2CDevice
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(Pcm1792I2C),
        cv.Optional(CONF_MODE): cv.uint32_t,
    }
).extend(i2c.i2c_device_schema(None))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_mode(config[CONF_MODE]))
