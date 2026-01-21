/*
 * PCM1792A ASoC codec driver
 *
 * Copyright (c) Jos van Eijndhoven 2013
 *
 *     Jos van Eijndhoven <jos@vaneijndhoven.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/printk.h>
#include <sound/soc.h>

#include "pcm1792a.h"

static const struct reg_default pcm1792a_reg_defaults[] = {
	{ 0x10, 0xff },
	{ 0x11, 0xff },
	{ 0x12, 0x50 },
	{ 0x13, 0x00 },
	{ 0x14, 0x00 },
	{ 0x15, 0x01 },
	{ 0x16, 0x00 },
	{ 0x17, 0x00 },
};

static bool pcm1792a_readable_reg(struct device *, unsigned int reg)
{
	return reg >= 0x10 && reg <= 0x17;
}

static bool pcm1792a_writeable_reg(struct device *, unsigned int reg)
{
	return reg >= 0x10 && reg <= 0x15;
}

static bool pcm1792a_volatile_reg(struct device *, unsigned int reg)
{
	return reg == 0x16;
}

struct pcm1792a_private {
	struct regmap *regmap;
	unsigned int format;
	unsigned int rate;
};

static struct snd_soc_dai_driver pcm1792a_dai = {
	.name = "pcm1792a-nodai",
};

static const struct regmap_config pcm1792a_regmap = {
	.reg_bits		      = 8,
	.val_bits		      = 8,
	.max_register		  = 0x17,
	.reg_defaults		  = pcm1792a_reg_defaults,
	.num_reg_defaults	= ARRAY_SIZE(pcm1792a_reg_defaults),
	.writeable_reg		= pcm1792a_writeable_reg,
	.readable_reg		  = pcm1792a_readable_reg,
	.volatile_reg     = pcm1792a_volatile_reg,
	.cache_type       = REGCACHE_RBTREE,
};

static int pcm1792a_probe(struct snd_soc_component *component)
{
	pr_info("pcm1792a_codec probe(): component \"%s\"\n",
	  (component && component->name) ? component->name : "NULL");

	return 0;
}

static struct snd_soc_component_driver soc_codec_dev_pcm1792a = {
	.name           = "pcm1792a-i2c",
	.probe          = pcm1792a_probe,
	//.controls			= pcm1792a_controls,
	.num_controls		= 0,//ARRAY_SIZE(pcm1792a_controls),
};

static int pcm1792a_i2c_probe(struct i2c_client *i2c)
{
	struct regmap *regmap;
	struct pcm1792a_private *pcm1792a;
	struct device *dev = &i2c->dev;
	int ret = 0;
	
	pr_info("pcm1792a-i2c: probe(name=\"%s\", addr=0x%02x)\n",
	        i2c->name, (i2c->addr & 0x7f));

	// msb needs to be set to enable auto-increment of addresses
	//config.read_flag_mask = 0x80;
	//config.write_flag_mask = 0x80;

	regmap = devm_regmap_init_i2c(i2c, &pcm1792a_regmap);
	if (IS_ERR(regmap)) {
		ret = PTR_ERR(regmap);
		dev_err(dev, "Failed to register i2c regmap: %d\n", ret);
		return ret;
	}

	pcm1792a = devm_kzalloc(dev, sizeof(struct pcm1792a_private), GFP_KERNEL);
	if (!pcm1792a)
		return -ENOMEM;

	dev_set_drvdata(dev, pcm1792a);

	pcm1792a->regmap = regmap;

	ret = snd_soc_register_component(dev, &soc_codec_dev_pcm1792a, &pcm1792a_dai, 1);
	if (ret && ret != -EPROBE_DEFER) {
		dev_err(dev, "pcm1792a-i2c probe: Failed to register card \"%s\", err=%d\n", i2c->name, ret);
	}
	if (!ret) { 
		pr_info("pcm1792a-i2c probe: registered i2c card driver \"%s\", success!\n", i2c->name);
	} else {
 	  pr_info("pcm1792a-i2c probe: register i2c card driver \"%s\" returns %d\n",i2c->name, ret);
	}
	return ret;
}

static void pcm1792a_i2c_remove(struct i2c_client *i2c)
{
	pr_info("pcm1792a-i2c: pcm1792a_i2c_remove(\"%s\")\n", i2c->name);
	snd_soc_unregister_component(&i2c->dev);
}

static const struct i2c_device_id pcm1792a_i2c_ids[] = {
	{ "pcm1792a-i2c", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, pcm1792a_i2c_ids);

static const struct of_device_id pcm1792a_of_match[] = {
	// { .compatible = "ti,pcm1792a-i2c", },
	{ .compatible = "jve,pcm1792a-i2c", },
	{ }
};
MODULE_DEVICE_TABLE(of, pcm1792a_of_match);

static struct i2c_driver pcm1792a_i2c_driver = {
	.driver = {
		.name = "pcm1792a-i2c",
		.owner = THIS_MODULE,
		.of_match_table = pcm1792a_of_match,
	},
	.id_table = pcm1792a_i2c_ids,
	.probe = pcm1792a_i2c_probe,
	.remove = pcm1792a_i2c_remove,
};

module_i2c_driver(pcm1792a_i2c_driver);

MODULE_DESCRIPTION("ASoC PCM1792A driver - I2C - No DAI");
MODULE_AUTHOR("Jos van Eijndhoven <jos@vaneijndhoven.net>");
MODULE_LICENSE("GPL");
