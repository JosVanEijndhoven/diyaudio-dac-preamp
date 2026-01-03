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


#include "jedac5.h"

static const struct reg_default jedac5_reg_defaults[] = {
	{ REGDAC_control0,      0x00 },
	{ REGDAC_control1,      0x00 },
	{ REGDAC_control2,      0x00 },
	{ REGDAC_control3,      0x00 },
	{ REGDAC_control4,      0x00 },
	{ REGDAC_SerAudioData,  0x00 },
	{ REGDAC_RecvErrMask,   0x00 },
	{ REGDAC_IntMask,       0x00 },
	{ REGDAC_IntModeMSB,    0x00 },
	{ REGDAC_IntModeLSB,    0x00 },
	{ REGDAC_RecvChanStat,  0x00 },
	{ REGDAC_AudioFmtDect,  0x00 },
	{ REGDAC_RecvErr,       0x00 },
	{ REGDAC_GPO0,          0x00 },
	{ REGDAC_GPO1,          0x00 },
	{ REGDAC_GPI0,          0x00 },
	{ REGDAC_GPI1,          0x00 },
};

static bool jedac5_readable(struct device *dev, unsigned int reg)
{
	return reg <= REGDAC_MAX || reg == REGDAC_GPO0 || reg == REGDAC_GPO1
		       || reg == REGDAC_GPI0 || reg == REGDAC_GPI1;
}

static bool jedac5_writeable(struct device *dev, unsigned int reg)
{
	return reg <= REGDAC_IntModeLSB || reg == REGDAC_GPO0 || reg == REGDAC_GPO1;
}

static bool jedac5_volatile(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case REGDAC_RecvChanStat: // later read might show new values
	case REGDAC_AudioFmtDect:  // later read might show new values
	case REGDAC_RecvErr:      // reading resets bits as sideeffect
	case REGDAC_GPI0:		  // run-time status sample
	case REGDAC_GPI1:		  // run-time status sample
		return true;
	default:
		return false;
	}
}

const struct regmap_config jedac5_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = REGDAC_GPI1,
	.readable_reg = jedac5_readable,
	.writeable_reg = jedac5_writeable,
	.volatile_reg = jedac5_volatile,
	.reg_defaults = jedac5_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(jedac5_reg_defaults),
	.cache_type = REGCACHE_RBTREE,
};

static int jedac5_set_dai_fmt(struct snd_soc_dai *codec_dai,
                             unsigned int format)
{
	pr_info("jedac5_set_dai_fmt(format=%u) dummy\n", format);
	return 0;
}

static int jedac5_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
	pr_info("jedac5_hw_params( rate=%d) dummy\n", params_rate(params));
	return 0;
}

static int jedac5_digital_mute(struct snd_soc_dai *dai, int mute, int )
{
	pr_info("jedac5_digital_mute( mute=%d) dummy\n", mute);
	return 0;
}

static const struct snd_soc_dai_ops jedac5_dai_ops = {
	.set_fmt	   = jedac5_set_dai_fmt,
	.hw_params	 = jedac5_hw_params,
	.mute_stream = jedac5_digital_mute
};

static struct snd_soc_dai_driver jedac5_dai = {
	.name = "jedac5_codec",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates = JEDAC5_RATES,
		.formats = JEDAC5_FORMATS },
	.ops = &jedac5_dai_ops
};

static int jedac5_probe(struct snd_soc_component *component)
{
	pr_info("jedac5_probe() codec start\n");

	return 0;
}

static void jedac5_remove(struct snd_soc_component *component)
{
	pr_info("jedac5_remove() codec\n");

	// pm_runtime_disable(dev); ??
}

static int my_info_volsw(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 2;
    uinfo->value.integer.min = -1 * DAC_max_attenuation_dB;
    uinfo->value.integer.max = 0;
    uinfo->value.integer.step = DAC_step_attenuation_dB;
	
	return 0;
}

static int my_get_volsw(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	unsigned short vol_l, vol_r;

	vol_l = kcontrol->private_value & 0x00ff; // lower 16 bits for left channel, 1dB units
	vol_r = (kcontrol->private_value >> 16) & 0x00ff; // lower 16 bits for left channel
	pr_info("jedac5_codec: my_snd_soc_get_volsw() vol_l=%d vol_r=%d\n",(int)vol_l, (int)vol_r);
	ucontrol->value.integer.value[0] = -vol_l;
	ucontrol->value.integer.value[1] = -vol_r;
	
	return 0;
}

static int my_put_volsw(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	unsigned short vol_l, vol_r;
	unsigned int new_private;
	int changed = 0;
	
	// hmm.. the line below results in some illegal pointer, avoid such call...
	//struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	
	pr_info("jedac5_codec:my_snd_soc_put_volsw() private_value = %04lx\n",
		kcontrol->private_value);
		
	vol_l = abs(ucontrol->value.integer.value[0]);
	if (vol_l > DAC_max_attenuation_dB)
		vol_l = DAC_max_attenuation_dB;
	
	vol_r = abs(ucontrol->value.integer.value[1]);
	if (vol_r > DAC_max_attenuation_dB)
		vol_r = DAC_max_attenuation_dB;
	
	new_private = (vol_r << 16) | vol_l;
	changed = new_private != kcontrol->private_value;
	
	if (changed) {
		kcontrol->private_value = new_private;
		// Dummy: done already at bcm level...
		// jedac_i2c_set_volume( vol_l, vol_r);
		// TODO: write vol_l and vol_r to i2c devices...
		// err = snd_soc_update_bits(codec, reg, val_mask, val);
	}
	
	return changed;
}

// define my volume scale as -80dB to 0 dB in steps of 1 dB, where the lowest volume does mute
static DECLARE_TLV_DB_SCALE(dac_db_scale, (-100 * DAC_max_attenuation_dB),
	                        (100 * DAC_step_attenuation_dB), 1);

// static int snd_soc_put_volsw(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
static const struct snd_kcontrol_new jedac5_codec_controls[] = {
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "CODEC Playback Volume",
		.access = SNDRV_CTL_ELEM_ACCESS_TLV_READ | SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.tlv.p = dac_db_scale,
		.info = my_info_volsw,
		.get = my_get_volsw,
		.put = my_put_volsw,
		.private_value = 0
	}
};

static const struct snd_soc_dapm_widget jedac5_dapm_widgets[] = {
SND_SOC_DAPM_OUTPUT("IOUTL"),
SND_SOC_DAPM_OUTPUT("IOUTR"),
};

static const struct snd_soc_dapm_route jedac5_dapm_routes[] = {
	{ "IOUTL", NULL, "Playback" },
	{ "IOUTR", NULL, "Playback" },
};

static struct snd_soc_component_driver jedac5_codec_driver = {
	.name         = "snd_jve_dac",
	.probe 				= jedac5_probe,
	.remove 			= jedac5_remove,
	.controls		    = jedac5_codec_controls,
	.num_controls		= ARRAY_SIZE(jedac5_codec_controls),
	.dapm_widgets		= jedac5_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(jedac5_dapm_widgets),
	.dapm_routes		= jedac5_dapm_routes,
	.num_dapm_routes	= ARRAY_SIZE(jedac5_dapm_routes)
};

static int jedac5_i2c_probe(struct i2c_client *i2c)
{
	struct device *dev = &i2c->dev;
	struct jedac5_codec_priv *jedac5_priv;
 	struct regmap *regmap;
	int ret = 0;
	
	pr_info("jedac5_i2c_probe(name=\"%s\", addr=0x%02x)\n", i2c->name, (i2c->addr & 0x7f));

	regmap = devm_regmap_init_i2c(i2c, &jedac5_regmap_config);
	if (IS_ERR(regmap)) {
		ret = PTR_ERR(regmap);
		dev_err(dev, "Failed to register i2c regmap: %d\n", ret);
		return ret;
	}

 	jedac5_priv = devm_kzalloc(dev, sizeof(struct jedac5_codec_priv), GFP_KERNEL);
	if (!jedac5_priv)
		return -ENOMEM;

	dev_set_drvdata(dev, jedac5_priv);
	jedac5_priv->regmap = regmap;

	ret = snd_soc_register_component(dev, &jedac5_codec_driver, &jedac5_dai, 1);
	if (ret && ret != -EPROBE_DEFER) {
		dev_err(dev, "jedac5_i2c_probe: Failed to register card, err=%d\n", ret);
	} else {
		pr_info("jedac5_i2c_probe: registered card driver!\n");
	}

 	pr_info("jedac5_i2c_probe: returns %d\n", ret);
	return ret;
}

static void jedac5_i2c_remove(struct i2c_client *i2c)
{
	const char i2c_standby[] = {REGDAC_GPO0, 0x00};
	pr_info("jedac5_i2c_remove(), DAC power-down\n");
	
	// power-down the DAC board to stand-by
	i2c_master_send(i2c, i2c_standby, 2);
	snd_soc_unregister_component(&i2c->dev);
}

static const struct i2c_device_id jedac5_i2c_id[] = {
	{ "jedac5_codec", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, jedac5_i2c_id);

static const struct of_device_id jedac5_of_match[] = {
	{ .compatible = "jve,jedac5_codec", },
	{ }
};
MODULE_DEVICE_TABLE(of, jedac5_of_match);

static struct i2c_driver jedac5_i2c_driver = {
	.driver = {
		.name = "jedac5_codec",
		.owner = THIS_MODULE,
//		.pm = &jedac5_pm,
		.of_match_table = jedac5_of_match,
	},
	.probe = jedac5_i2c_probe,
	.remove = jedac5_i2c_remove,
	.id_table = jedac5_i2c_id
};

static int __init jedac5_modinit(void)
{
	int ret = 0;
	ret = i2c_add_driver(&jedac5_i2c_driver);
	if (ret != 0) {
		printk(KERN_ERR "Failed to register jedac5 I2C driver: %d\n",
		       ret);
	}

	return ret;
}
module_init(jedac5_modinit);

static void __exit jedac5_exit(void)
{
	i2c_del_driver(&jedac5_i2c_driver);
}
module_exit(jedac5_exit);

MODULE_DESCRIPTION("ASoC jedac5 codec driver");
MODULE_AUTHOR("Jos van Eijndhoven <jos@vaneijndhoven.net>");
MODULE_LICENSE("GPL v2");