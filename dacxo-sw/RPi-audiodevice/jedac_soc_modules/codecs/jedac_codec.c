/*
 * Driver for the 5th generation DAC by Jos van Eijndhoven
 *
 * The DAC is based on a CS8416 receiver,
 * a MACH XO2-100 fpga and a CCHD-957 low-jitter clock,
 * a pair of PCM1792a dac chips in dual-mono configuration.
 * http://www.vaneijndhoven.net/jos/
 *
 * Copyright 2016 Jos van Eijndhoven
 * jos@vaneijndhoven.net
 *
 * The writing of this source-code was inspired by 
 * - //www.kernel.org/doc/Documentation/sound/alsa/soc/codec.txt
 * - https://github.com/raspberrypi/linux/blob/ ... sound/soc/codecs/pcm1792a.c
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
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/printk.h>
#include <linux/i2c.h>
#include <linux/regmap.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>


#include "jedac.h"

static const struct reg_default jedac_reg_defaults[] = {
	{ REGDAC_GPO0,          0x00 },
	{ REGDAC_GPO1,          0x00 },
	{ REGDAC_GPI0,          0x00 },
	{ REGDAC_GPI1,          0x00 },
};

static bool jedac_writeable(struct device *dev, unsigned int reg) {
	return (reg == REGDAC_GPO0) || (reg == REGDAC_GPO1);
}

static bool jedac_readable(struct device *dev, unsigned int reg) {
	return (reg == REGDAC_GPI0) || (reg == REGDAC_GPI1) || jedac_writeable(dev, reg);
}

static bool jedac_volatile(struct device *dev, unsigned int reg) {
	return (reg == REGDAC_GPI0) || (reg == REGDAC_GPI1);  // run-time status
}

const struct regmap_config jedac_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = REGDAC_GPI1,
	.readable_reg = jedac_readable,
	.writeable_reg = jedac_writeable,
	.volatile_reg = jedac_volatile,
	.reg_defaults = jedac_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(jedac_reg_defaults),
	.cache_type = REGCACHE_RBTREE,
};

#if 0
struct jedac_codec_priv {
	struct regmap *regmap;
};
#endif

static int codec_set_dai_fmt(struct snd_soc_dai *codec_dai,
                             unsigned int format)
{
	pr_warn("jedac_codec set_dai_fmt(format=%u) DUMMY\n", format);
	return 0;
}

static int jedac_i2c_set_i2s(struct snd_soc_component *codec, int samplerate)
{
	int freq_base, freq_mult;
	
	if (samplerate == 48000 || samplerate == 96000 || samplerate == 192000)
		freq_base = 1; // enable other xtal oscillator
	else
		freq_base = 0; // default xtal oscillator
	
	switch (samplerate) {
		case 44100:
		case 48000: freq_mult = 1;
		break;
		case 88200:
		case 96000: freq_mult = 2;
		break;
		case 176400:
		case 192000: freq_mult = 3;
		break;
		default:
			freq_mult = 0; // illegal/unsupported samplerate
	}
	
	// send a trigger to the ui controller: denote a busy i2c and changed dac settings
	// struct jedac_bcm_priv *priv = snd_soc_card_get_drvdata(&jedac_sound_card);
	// if (0 == gpiod_get_value(priv->uisync_gpio)) {
	// 	pr_warn("jedac_bcm: jedac_i2c_set_i2s: unexpected 'low' on ui-trig gpio!");
	// }
	// gpiod_set_value(priv->uisync_gpio, 0);

	struct regmap *map = dev_get_regmap(codec->dev, NULL);
	if (!map) {
		pr_err("jedac codec: regmap not found error!");
    return -EINVAL;
	}

	unsigned int gpo_val = GPO0_POWERUP | GPO0_CLKMASTER | (freq_base << 1) | (freq_mult << 2);
	int i2cerr_w = regmap_write(map, REGDAC_GPO0, gpo_val);
	// as test, read-back fpga status byte
	unsigned int gpi_val = 0;
	int i2cerr_r = regmap_read(map, REGDAC_GPI0, &gpi_val);

	// gpiod_set_value(priv->uisync_gpio, 1);  // clear the ui trigger pulse
	if (i2cerr_w == 0 && i2cerr_r == 0)
	  pr_info("jedac_codec: i2c_set_i2s: write GPO0=0x%02x, read GPI0=0x%02x: OK!\n",
		  (int)(gpo_val), (int)(gpi_val));
	else
	  pr_warn("jedac_codec: i2c_set_i2s: write GPO0=0x%02x, read GPI0=0x%02x: i2c write error=%d, i2c read error=%d\n",
		  (int)(gpo_val), (int)(gpi_val), i2cerr_w, i2cerr_r);

	return (i2cerr_w == 0) ? i2cerr_r : i2cerr_w;
}

static int codec_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
  struct snd_soc_component *codec = dai->component;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = snd_soc_rtd_to_cpu(rtd, 0);
	int samplerate = params_rate(params);
	int samplewidth = snd_pcm_format_width(params_format(params));
	int clk_ratio = 64; // fixed bclk ratio is easiest for my HW
	int err_clk = snd_soc_dai_set_bclk_ratio(cpu_dai, clk_ratio);
	int err_rate = jedac_i2c_set_i2s(codec, samplerate);
	
	//	snd_pcm_format_physical_width(params_format(params));
	pr_info("jedac_codec: hw_params(rate=%d, width=%d) err_clk=%d err_rate=%d\n",
		samplerate, samplewidth, err_clk, err_rate);

	return err_clk;
}

static const struct snd_soc_dai_ops codec_dai_ops = {
	.set_fmt	   = codec_set_dai_fmt,
	.hw_params	 = codec_hw_params,
	// .mute_stream = codec_digital_mute
};

static struct snd_soc_dai_driver jedac_dai = {
	.name = "jedac_codec",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates = JEDAC_RATES,
		.formats = JEDAC_FORMATS
	},
	.ops = &codec_dai_ops
};

static int jedac_codec_probe(struct snd_soc_component *codec)
{
  // Called *after* the below i2c_probe()

	// Retrieve the regmap that devm_regmap_init_i2c attached to the device,
  struct regmap *regmap = dev_get_regmap(codec->dev, NULL);
  // .. and explicitly tell the component to use this regmap
  snd_soc_component_init_regmap(codec, regmap);

	// Ensure power-up, make the DAC as i2s clock master
	uint8_t reg_chan = GPO0_POWERUP | GPO0_CLKMASTER;
	int i2cerr = regmap_write(regmap, REGDAC_GPO0, reg_chan);

	pr_info("jedac_codec probe(): initialize component \"%s\": %s\n",
	  (codec && codec->name) ? codec->name : "NULL",
	  (i2cerr == 0) ? "OK" : "Fail");

	return 0;
}

static void jedac_codec_remove(struct snd_soc_component *component)
{
	pr_info("jedac_codec remove() codec\n");
}


static struct snd_soc_component_driver jedac_codec_driver = {
	.name         = "jedac codec driver",
	.probe 				= jedac_codec_probe,
	.remove 			= jedac_codec_remove,
};

static int codec_i2c_probe(struct i2c_client *i2c)
{
	struct device *dev = &i2c->dev;
	int ret = 0;
	
	// called when the os encounters this i2c device.
	// Crucial: its call to snd_soc_register_component where this i2c device announces
	// that it is part of the ASOC sound system
	pr_info("jedac_codec i2c_probe(name=\"%s\", addr=0x%02x)\n", i2c->name, (i2c->addr & 0x7f));
	
	struct regmap *regmap = devm_regmap_init_i2c(i2c, &jedac_regmap_config);
	if (IS_ERR(regmap)) {
		ret = PTR_ERR(regmap);
		dev_err(dev, "Failed to register i2c regmap: %d\n", ret);
		return ret;
	}

#if 0
 	struct jedac_codec_priv *jedac_priv = devm_kzalloc(dev, sizeof(struct jedac_codec_priv), GFP_KERNEL);
	if (!jedac_priv)
		return -ENOMEM;

	dev_set_drvdata(dev, jedac_priv);
	jedac_priv->regmap = regmap;
#endif

	ret = snd_soc_register_component(dev, &jedac_codec_driver, &jedac_dai, 1);
	if (ret && ret != -EPROBE_DEFER) {
		dev_err(dev, "jedac_codec i2c_probe: Failed to register codec component, err=%d\n", ret);
	}
	if (!ret) {
		pr_info("jedac_codec i2c_probe: registered codec component!\n");
	} else {
 	  pr_info("jedac_codec i2c_probe: register component returns %d\n", ret);
	}
	return ret;
}

static void codec_i2c_remove(struct i2c_client *i2c)
{
	const char i2c_standby[] = {REGDAC_GPO0, 0x00};
	pr_info("jedac_codec i2c_remove(), DAC power-down\n");
	
	// power-down the DAC board to stand-by
	i2c_master_send(i2c, i2c_standby, 2);
	snd_soc_unregister_component(&i2c->dev);
}

static const struct i2c_device_id codec_i2c_id[] = {
	{ "jedac_codec", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, codec_i2c_id);

static const struct of_device_id jedac_of_match[] = {
	{ .compatible = "jve,jedac_codec", },
	{ }
};
MODULE_DEVICE_TABLE(of, jedac_of_match);

static struct i2c_driver codec_i2c_driver = {
	.driver = {
		.name = "jedac codec i2c driver",
		.owner = THIS_MODULE,
//		.pm = &jedac_pm,
		.of_match_table = jedac_of_match,
	},
	.probe = codec_i2c_probe,
	.remove = codec_i2c_remove,
	.id_table = codec_i2c_id
};
module_i2c_driver(codec_i2c_driver);

MODULE_DESCRIPTION("ASoC jedac codec driver");
MODULE_AUTHOR("Jos van Eijndhoven <jos@vaneijndhoven.net>");
MODULE_LICENSE("GPL v2");