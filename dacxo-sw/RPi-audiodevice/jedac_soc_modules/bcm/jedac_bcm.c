/*
 * ASOC driver for the 5th generation DAC by Jos van Eijndhoven
 *
 * The DAC is based on a CS8416 receiver,
 * a MACH XO2-100 fpga and a CCHD-957 low-jitter clock,
 * a pair of PCM1792a dac chips in dual-mono configuration.
 * http://www.vaneijndhoven.net/jos/
 *
 * Copyright 2016 Jos van Eijndhoven
 * jos@vaneijndhoven.net
 *
 * This implementation was inspired from the 'bcm/raspidac3.c',
 * 'codecs/pcm512x-i2c.c' and the 'raspidac3-overlay.dts'.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

// Linux docs: https://www.kernel.org/doc/Documentation/sound/alsa/soc/machine.txt
 
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/printk.h>
#include <linux/i2c.h>
#include <linux/gpio/consumer.h>
#include <linux/regmap.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>

#include "../codecs/jedac.h"
#include "../codecs/pcm1792a.h"

static const struct reg_default pcm1792a_reg_defaults[] = {
	{ PCM1792A_DAC_VOL_LEFT,   PCM1792A_DAC_VOL_LEFT_DEFAULT},
  { PCM1792A_DAC_VOL_RIGHT,  PCM1792A_DAC_VOL_RIGHT_DEFAULT },
  { PCM1792A_FMT_CONTROL,    PCM1792A_FMT_CONTROL_DEFAULT },
  { PCM1792A_MODE_CONTROL,   PCM1792A_MODE_CONTROL_DEFAULT },
  { PCM1792A_STEREO_CONTROL, PCM1792A_STEREO_CONTROL_DEFAULT },
};

static bool pcm1792a_reg_writeable(struct device *dev, unsigned int reg) {
	return (reg == PCM1792A_DAC_VOL_LEFT) || (reg == PCM1792A_DAC_VOL_RIGHT) ||
         (reg == PCM1792A_FMT_CONTROL)  || (reg == PCM1792A_MODE_CONTROL) ||
				 (reg == PCM1792A_STEREO_CONTROL);
}

static bool pcm1792a_reg_readable(struct device *dev, unsigned int reg) {
	return pcm1792a_reg_writeable(dev, reg);
}

static bool pcm1792a_reg_volatile(struct device *dev, unsigned int reg) {
	return false;
}

static const struct regmap_config pcm1792_regmap_config = {
  .reg_bits         = 8,
  .val_bits          = 8,
  .max_register     = PCM1792A_REG_MAX,
	.readable_reg     = pcm1792a_reg_readable,
	.writeable_reg    = pcm1792a_reg_writeable,
	.volatile_reg     = pcm1792a_reg_volatile,
	.reg_defaults     = pcm1792a_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(pcm1792a_reg_defaults),
  .cache_type       = REGCACHE_RBTREE, // This remembers values while DAC is not powered
};

static void jedac_set_attenuation( struct jedac_bcm_priv *priv, unsigned short vol_l, unsigned short vol_r);

/* sound card init */
static int jedac_pcm1792_init(struct i2c_client *dac, bool is_powered, bool is_right_chan)
{
	struct pcm_reg_init {
		uint8_t reg_nr;
		uint8_t value;
	};
	const struct pcm_reg_init inits[] = {
    { PCM1792A_FMT_CONTROL,  0xb0}, // reg 18: audio format left justified, enable att, no mute, no demp
		{ PCM1792A_MODE_CONTROL, 0x62},  // reg 19: slow unmute, filter slow rolloff
		{ PCM1792A_STEREO_CONTROL, (is_right_chan ? 0x0c : 0x08)} // reg 20: set mono mode, choose channel
	};
  pr_info("jedac_bcm: initialize pcm1792a(%s %s) i2c registers, power=%d\n",
		dac->name, (is_right_chan ? "Right" : "Left"), is_powered);

	struct regmap *regs = dev_get_regmap(&dac->dev, NULL);
	if (!regs) {
    dev_err(&dac->dev, "jedac_bcm: initialize pcm1792a(%s) i2c registers failed: no regmap?\n", (is_right_chan ? "Right" : "Left"));
		return -ENODEV;
	}

	if (!is_powered) {
	  regcache_cache_only(regs, true);
	}

	int err = 0;
	for (int i = 0; i < ARRAY_SIZE(inits) && !err; i++) {
		err = regmap_write(regs, inits[i].reg_nr, inits[i].value);
		pr_info("jedac_bcm: init pcm1792a:  write reg=%d, val=0x%02x, err=%d\n", inits[i].reg_nr, inits[i].value, err);
	}

	if (!is_powered) {
    regcache_cache_only(regs, false);
  }

	return err;
}

static int jedac_bcm_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_card *card = rtd->card;
  struct jedac_bcm_priv *priv = snd_soc_card_get_drvdata(card);

	pr_info("jedac_bcm: init(card=\"%s\", priv=%s)\n", card->name, ((priv == NULL) ? "NULL!" : "OK"));
	if (!priv)
	  return -EINVAL;

  // Note that the FPGA has already done its own init during its 'probe()'
  bool is_powered = false;
	unsigned int gpi1_val = 0;
  int err = regmap_read(priv->fpga_regs, REGDAC_GPI1, &gpi1_val);
	if (!err) {
		is_powered = (gpi1_val & GPI1_ANAPWR) != 0;
	}

	if (is_powered) {
		gpiod_set_value(priv->uisync_gpio, 0);  // pull-down 'uisync' pin: signal UI controller on change and stay silent
	}

	// the pcm1792 dac chip registers get initial assignment.
	// When they are not yet powered-up, this initialization remains in the regmap cache,
  jedac_pcm1792_init(priv->dac_l, is_powered, false);
	jedac_pcm1792_init(priv->dac_r, is_powered, true);

	if (is_powered) {
		gpiod_set_value(priv->uisync_gpio, 1);
	}

	return 0;
}

// replace the volume control from soc-ops.c
// which is inserted through e.g. #define SOC_DOUBLE_R in soc.h
static int bcm_vol_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
  uinfo->count = 2;       // Stereo
  uinfo->value.integer.min = 0;
  uinfo->value.integer.max = DAC_max_attenuation_dB;

	return 0;
}

static int bcm_vol_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_info("jedac_bcm:my_snd_soc_get_volsw() private_value = %04lx\n",
		kcontrol->private_value);

	unsigned short vol_l = kcontrol->private_value & 0x00ff; // lower 16 bits for left channel, 1dB units
	unsigned short vol_r = (kcontrol->private_value >> 16) & 0x00ff; // lower 16 bits for left channel
	ucontrol->value.integer.value[0] = -vol_l; // alsa operates with 0.01dB units
	ucontrol->value.integer.value[1] = -vol_r;
	
	return 0;
}

static int bcm_vol_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
  struct jedac_bcm_priv *priv = snd_soc_card_get_drvdata(card);
  uint16_t vol_l = ucontrol->value.integer.value[0];
  uint16_t vol_r = ucontrol->value.integer.value[1];
	
	// ALSA values are configured to 0 (mute) to 80 (0dB, max volume)
	pr_info("jedac_bcm: vol_put() ALSA vol_l=%u, vol_r=%d\n", vol_l, vol_r);

	uint32_t new_vol = (vol_l << 16) | vol_r;
	int changed = new_vol != priv->prev_volume;

	if (!changed || !priv->dac_l || !priv->dac_r)
	  return 1;
	
	priv->prev_volume = new_vol;
	jedac_set_attenuation( priv, DAC_max_attenuation_dB - vol_l, DAC_max_attenuation_dB - vol_r);

	return 1;
}

// define my volume scale as -80dB to 0 dB in steps of 1 dB, where the lowest volume does mute
/* 
 * min_db: -8000 (centibels)
 * step: 100 (1 dB in centibels)
 * mute: 1 (the minimum value 0 will be displayed as "Mute" in UI)
 */
static DECLARE_TLV_DB_SCALE(dac_db_scale, (-100 * DAC_max_attenuation_dB),
	                        (100 * DAC_step_attenuation_dB), 1);

// Define the input names for the mixer UI
static const char *const jedac_input_texts[] = {
    "Raspberry Pi", "HDMI/Spdif Coax 1", "Spdif Coax 2", "Spdif Optical 1", "Spdif Optical 2"
};
/* * SOC_ENUM_SINGLE(reg, shift, max_items, texts)
 * reg:   The FPGA register (e.g., REGDAC_SOURCE)
 * shift: The bit position where the selection starts
 * 5:     Number of items in the list
 */
static const struct soc_enum jedac_input_enum =
    SOC_ENUM_SINGLE(REGDAC_GPO0, 2, 5, jedac_input_texts);

static int jedac_input_put(struct snd_kcontrol *kcontrol,
                           struct snd_ctl_elem_value *ucontrol)
{
  struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
  struct jedac_bcm_priv *priv = snd_soc_card_get_drvdata(card);
  unsigned int sel = ucontrol->value.enumerated.item[0];

  if (sel >= 5) return -EINVAL; // Safety check

	// 1. Read current state to see if we actually need to do anything
	unsigned int gpo0;
  int err = regmap_read(priv->fpga_regs, REGDAC_GPO0, &gpo0);
  if (err) return err;

	unsigned int curr_input_sel = ((gpo0 & GPO0_CLKMASTER) == 0) ? (((gpo0 & GPO0_SLVINPUT) >> 2) + 1) : 0;
	if (sel == curr_input_sel)
		return 0;  // no change on input select

  dev_info(card->dev, "jedac_bcm: Switching input to %d\n", sel);
  gpiod_set_value(priv->uisync_gpio, 0);  // pull-down 'uisync' pin: signal UI controller on change and stay silent
  
  // 2. Perform the I2C write to the FPGA
	if (sel == 0) {
		// I2S input: enough to put DAC in clock master mode:
    err = regmap_update_bits(priv->fpga_regs, REGDAC_GPO0,
			                       GPO0_CLKMASTER, GPO0_CLKMASTER);
	} else {
		// Put DAC in clock slave mode with input select
		unsigned int spdif_input = sel - 1;
    err = regmap_update_bits(priv->fpga_regs, REGDAC_GPO0,
			                       GPO0_CLKMASTER | GPO0_SLVINPUT, (spdif_input << 2));
	}
	gpiod_set_value(priv->uisync_gpio, 1);
  if (err) return err;

  return 1; // Return 1 to inform ALSA the value actually changed
}

static int jedac_input_get(struct snd_kcontrol *kcontrol,
                           struct snd_ctl_elem_value *ucontrol)
{
  struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
  struct jedac_bcm_priv *priv = snd_soc_card_get_drvdata(card);
  unsigned int val;

  // Read directly from the FPGA to see what the hardware is actually doing
  int err = regmap_read(priv->fpga_regs, REGDAC_GPO0, &val);
  if (err)
      return err;

	unsigned int input_nr = ((val & GPO0_CLKMASTER) == GPO0_CLKMASTER)
	                      ? 0
												: ((val & GPO0_SLVINPUT) >> 2) + 1;
  // ALSA expects the index to be placed in this specific union member
  // Range: 0 to 4
  ucontrol->value.enumerated.item[0] = input_nr;

  return 0;
}

static const struct snd_kcontrol_new jedac_controls[] = {
	{
        .iface = SNDRV_CTL_ELEM_IFACE_MIXER,
        .name = "Master",  // or: Master Playback Volume
        .access = SNDRV_CTL_ELEM_ACCESS_READWRITE | SNDRV_CTL_ELEM_ACCESS_TLV_READ,
        .info = bcm_vol_info,
        .get  = bcm_vol_get,
        .put  = bcm_vol_put,
        .tlv.p = dac_db_scale,
  },
	SOC_ENUM_EXT("Input Source",
		           jedac_input_enum, 
               jedac_input_get,
               jedac_input_put)
};

/* startup */
static int snd_rpi_jedac_startup(struct snd_pcm_substream *substream) {
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component = snd_soc_rtd_to_codec(rtd, 0)->component;
	pr_info("jedac_bcm:snd_rpi_jedac_startup(): codec=%s Dummy!\n", component->name);
	return 0;
}

/* shutdown */
static void snd_rpi_jedac_shutdown(struct snd_pcm_substream *substream) {
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component = snd_soc_rtd_to_codec(rtd, 0)->component;
	pr_info("jedac_bcm:snd_rpi_jedac_shutdown() codec=%s Dummy\n", component->name);
}

/* card suspend */
static int jedac_suspend_post(struct snd_soc_card *card)
{
	pr_info("jedac_bcm: jedac_suspend_post() Dummy\n");
	return 0;
}

/* card resume */
static int jedac_resume_pre(struct snd_soc_card *card)
{
	pr_info("jedac_bcm: jedac_resume_pre() Dummy\n");
	return 0;
}

/* machine stream operations */
static struct snd_soc_ops snd_rpi_jedac_ops = {
	.startup = snd_rpi_jedac_startup,
	.shutdown = snd_rpi_jedac_shutdown,
};

// Create references to my modules.
// Note: the actual names are not important, they are later set to NULL during _probe()
SND_SOC_DAILINK_DEFS(rpi_jedac,
	DAILINK_COMP_ARRAY(COMP_CPU("bcm2708-i2s.0")),
	DAILINK_COMP_ARRAY(COMP_CODEC("jedac_codec.1-0020", "jedac_codec")), //fpga is at i2c bus 1 addr 20
	DAILINK_COMP_ARRAY(COMP_PLATFORM("bcm2708-i2s.0")));

static struct snd_soc_dai_link jedac_dai_link[] = {
{
	.name		= "JvE DAC5",
	.stream_name	= "JvE DAC",
	.dai_fmt	= JEDAC_DAIFMT,
	.ops		= &snd_rpi_jedac_ops,
	.init		= jedac_bcm_init,
	SND_SOC_DAILINK_REG(rpi_jedac),
},
};

static int jedac_bcm_power_event(struct snd_soc_dapm_widget *w,
                                 struct snd_kcontrol *kcontrol, int event)
{
  struct snd_soc_card *card = w->dapm->card;
  struct jedac_bcm_priv *priv = snd_soc_card_get_drvdata(card);

	if (!priv || !priv->fpga_regs) {
		pr_err("jedac_bcm power_event: No access to fpga regmap error!\n");
		return -EINVAL;
	}

	// Check current power relay status: Maybe got set manually outside scope of the DAPM framework.
	unsigned int gpo0_val = 0;
  int err = regmap_read(priv->fpga_regs, REGDAC_GPI0, &gpo0_val);
	if (err) {
		pr_err("jedac_bcm power_event: i2c access error %d!\n", err);
		return err;
	}
	uint8_t power_is_on = (gpo0_val & GPO0_POWERUP) != 0;

  if (SND_SOC_DAPM_EVENT_ON(event)) {
    dev_info(card->dev, "JEDAC: Powering up DAC rails, (power switch state is %d)\n", power_is_on);

    /* A. Tell FPGA to power ON the DACs */
		if (!power_is_on) {
		  err = regmap_update_bits(priv->fpga_regs, REGDAC_GPO0, GPO0_POWERUP, GPO0_POWERUP);
		}

    /* B. Wait for analog power to come up slowly */
		bool is_powered = false;
		for (int i = 0; i < 5; i++) {
	    unsigned int gpi1_val = 0;
      err = regmap_read(priv->fpga_regs, REGDAC_GPI1, &gpi1_val);
		  if (!err) {
			  is_powered = (gpi1_val & GPI1_ANAPWR) != 0;
		  }
			pr_info("jedac_bcm: power_event: DAC rails: regmap_err=%d, gpi1=0x%02x, Vana confirmed=%d\n",
				err, gpi1_val, is_powered);
			if (is_powered)
			  break;

			msleep(200);  // milliseconds: wait and retry..
		}

    /* C. Now that DACs have power, initialize them via I2C */
		if (is_powered) {
			pr_info("jedac_bcm: flush regmap cache to pcm1792 dacs");
      // Mark the register cache as "Dirty", then "Sync" to write cached values
			struct regmap *regs = dev_get_regmap(&priv->dac_l->dev, NULL);
      regcache_mark_dirty(regs);
      int err_l = regcache_sync(regs);
			unsigned int val;
			int err_rd = regmap_read(regs, 18, &val);
			pr_info("jedac_bcm: flush&sync err=%d, read back 18: val=0x%02x, err=%d\n", err_l, val, err_rd);
			int phys_val = i2c_smbus_read_byte_data(priv->dac_l, 18); // Read register 18 (0x12)
			if (phys_val < 0)
        pr_info("jedac_bcm: bypass read of reg 18: err=%d\n", phys_val);
			else
        pr_info("jedac_bcm: bypass read of reg 18: val=0x%02x\n", phys_val);

			regs = dev_get_regmap(&priv->dac_r->dev, NULL);
      regcache_mark_dirty(regs);
      regcache_sync(regs);
		} else {
			pr_err("jedac_pcm: power_event: power-up DAC rails failed (err=%d)!", err);
		}
  }
  return err;
}

/* 2. Define the Widget and Route */
static const struct snd_soc_dapm_widget jedac_bcm_widgets[] = {
    SND_SOC_DAPM_SUPPLY("DAC_Rails", SND_SOC_NOPM, 0, 0, jedac_bcm_power_event,
                        SND_SOC_DAPM_POST_PMU),
		SND_SOC_DAPM_HP("Main Output", NULL), // A "Sink" for the audio
};

static const struct snd_soc_dapm_route jedac_bcm_routes[] = {
    /* Connect the Codec's output to our Power Supply widget */
    { "Playback", NULL, "DAC_Rails" }, 
    /* "Main Output" is fed by the FPGA's playback stream */
    { "Main Output", NULL, "Playback" }, 
};

/* Audio card  -- machine driver */
static struct snd_soc_card jedac_sound_card = {
	.name = "JEDAC",
	.owner = THIS_MODULE,
//	.remove = jedac_card_remove,
	.dai_link = jedac_dai_link,
	.num_links = ARRAY_SIZE(jedac_dai_link),
	.controls = jedac_controls,
	.num_controls = ARRAY_SIZE(jedac_controls),
	.dapm_widgets = jedac_bcm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(jedac_bcm_widgets),
	.dapm_routes = jedac_bcm_routes,
	.num_dapm_routes = ARRAY_SIZE(jedac_bcm_routes),
	
	.suspend_post = jedac_suspend_post,
	.resume_pre = jedac_resume_pre,
};

// Helper to clear the i2c device reference count
static void jedac_put_i2c_device_action(void *dev)
{
    put_device((struct device *)dev);
}

/* node names of the i2c devices, to match with the jedac-overlay.dts */
static const char* i2c_node_refs[] = {
	"jve,jedac_codec",
	"jve,dac_l",
	"jve,dac_r"
};

/* sound card detect */
// see for instance examples like:
// https://github.com/raspberrypi/linux/blob/rpi-6.12.y/sound/soc/bcm/hifiberry_dacplus.c
// or, for a card with two codes like me:
// https://github.com/raspberrypi/linux/blob/rpi-6.12.y/sound/soc/bcm/pifi-40.c
static int snd_jedac_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np = pdev->dev.of_node;
	jedac_sound_card.dev = &pdev->dev;
	
	if (np) {
    pr_info("jedac_bcm: start probe(), device node \"%s\"\n", np->name);
	} else {
    pr_err("jedac_bcm: probe(): device node error!\n");
		return -EINVAL;
	}


	// Allocate private memory managed by the device
	struct jedac_bcm_priv* priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
  if (!priv) {
		pr_err("jedac_bcm: probe() failed to alloc priv struct!\n");
    return -ENOMEM;
	}
	snd_soc_card_set_drvdata(&jedac_sound_card, priv);

	// Obtain access to the gpio pin "uisync" to send signals to the UI controller
	// The "uisync" name and its gpio pin are defined in the DTS overlay file
	// ... and check if we already acquired the GPIO in a previous (deferred) probe attempt
  if (!priv->uisync_gpio) {
	  priv->uisync_gpio = devm_gpiod_get(&pdev->dev, "uisync", GPIOD_OUT_HIGH_OPEN_DRAIN);
	  if (IS_ERR(priv->uisync_gpio)) {
			int gpio_err = PTR_ERR(priv->uisync_gpio);
		  pr_err("jedac_bcm: failed to access the 'uisync' gpio pin!, err=%d\n", gpio_err);
			priv->uisync_gpio = NULL;
		  return gpio_err;
	  } else {
		  pr_info("jedac_bcm: successfully acquired 'uisync' gpio pin!\n");
	  }
	}

	struct snd_soc_dai_link *dai = &jedac_dai_link[0];
	/* find my three i2c components on the dac board: an FPGA and two PCM1792 */
	struct device_node *nodes[3];
	struct i2c_client *clients[3];
	for (int i = 0; i < 3; i++) {
		const char *name = i2c_node_refs[i];
		nodes[i] = of_parse_phandle(np, name, 0);

		if (!nodes[i]) {
			dev_err(&pdev->dev, "jedac_bcm: handle %s not found!\n", name);
		  clients[i] = NULL;
			ret = -ENOENT;
			continue;
		}

    clients[i] = of_find_i2c_device_by_node(nodes[i]);
		if (clients[i]) {
			devm_add_action_or_reset(&pdev->dev, jedac_put_i2c_device_action, &clients[i]->dev);
		} else {
		  pr_info("jedac_bcm: For handle %s i2c device NOT found\n", name);
			if (ret == 0)
			  ret = -EPROBE_DEFER;  // maybe the i2c subsystem is not ready yet. try again later
			continue;
		}
	}
  priv->fpga  = clients[0];
  priv->dac_l = clients[1];
  priv->dac_r = clients[2];
	priv->prev_volume = 0;
  priv->fpga_regs = NULL;

	// Obtain access to the FPGA i2c registers.
	// This might need a further 'DEFER': need to wait until the codec 'probe' finishes,
	// so it has allocated its regmap:
	if (priv->fpga && !priv->fpga_regs) {
	  priv->fpga_regs = dev_get_regmap(&priv->fpga->dev, NULL);
		if (!priv->fpga_regs && (ret == 0))
		  ret = -EPROBE_DEFER;
	}

	// The two pcm1792 regmaps are created here:
	if (priv->dac_l && !dev_get_regmap(&priv->dac_l->dev, NULL)) {
    struct regmap *regs = devm_regmap_init_i2c(priv->dac_l, &pcm1792_regmap_config);
	  if (!regs) {
		  dev_err(&priv->dac_l->dev, "jedac_codec: Failed to register i2c regmap for Left Dac!\n");
		  return -ENODEV;
	  }
  }

	if (priv->dac_r && !dev_get_regmap(&priv->dac_r->dev, NULL)) {
    struct regmap *regs = devm_regmap_init_i2c(priv->dac_r, &pcm1792_regmap_config);
	  if (!regs) {
		  dev_err(&priv->dac_r->dev, "jedac_codec: Failed to register i2c regmap for Right Dac\n");
		  return -ENODEV;
	  }
  }

	// Find the i2s (dai) interface from the card to the codec:
	struct device_node *i2s_node = of_parse_phandle(np, "i2s-controller", 0);
	if (!i2s_node) {
		dev_err(&pdev->dev, "jedac_bcm: i2s_node not found!\n");
		ret = -ENOENT;
	} else {
		pr_info("jedac_bcm: Found i2s handle for card\n");
	}

	// We have one i2s 'digital audio interface' towards the board FPGA
  dai->cpus[0].name = NULL;
	dai->cpus[0].dai_name = NULL;
	dai->cpus[0].of_node = i2s_node;
	dai->platforms[0].name = NULL;
	dai->platforms[0].of_node = i2s_node;
	dai->codecs[0].name = NULL;
	dai->codecs[0].of_node = nodes[0];  // our fpga acts as dai codec

	if (ret == 0) {
		// Good, all device tree nodes found!
	  ret = devm_snd_soc_register_card(&pdev->dev, &jedac_sound_card);
	}
	const char *msg = (ret == 0) ? "Success" :
	                  (ret == -EINVAL) ? "Incomplete snd_soc_card struct?" :
										(ret == -ENODEV) ? "Linked component not found?" :
										(ret == -ENOENT) ? "DT node or property missing?" :
										(ret == -EIO) ? "Communication failure" :
										(ret == -EPROBE_DEFER) ? "Deferred" : "Failure";
	
	if (ret && (ret != -EPROBE_DEFER)) {
    dev_err(&pdev->dev, "jedac_bcm: probe: register_card error: \"%s\", return %d\n", msg, ret);
	} else if (ret) {
    dev_warn(&pdev->dev, "jedac_bcm: probe: register_card: \"%s\", return %d\n", msg, ret);
	} else {
		pr_info("jedac_bcm: probe: Register_card: Success!\n");
	}

	// fix refcount of_node_get()/of_node_put()
  of_node_put(i2s_node);
  of_node_put(nodes[0]);
	of_node_put(nodes[1]);
	of_node_put(nodes[2]);
	return ret;
}

/* sound card disconnect */
static void snd_jedac_remove(struct platform_device *pdev)
{
	pr_info("jedac_bcm:snd_rpi_jedac_remove(): power-down DUMMY\n");
}

static const struct of_device_id jedac_of_match[] = {
	{ .compatible = "jve,jedac_bcm", },
	{},
};
MODULE_DEVICE_TABLE(of, jedac_of_match);

/* sound card platform driver */
static struct platform_driver snd_rpi_jedac_driver = {
	.driver = {
		.name   = "snd-rpi-jedac_bcm",
		.owner  = THIS_MODULE,
		.of_match_table = jedac_of_match,
	},
	.probe          = snd_jedac_probe,
	.remove         = snd_jedac_remove,
};
module_platform_driver(snd_rpi_jedac_driver);

static void jedac_set_attenuation_pcm1792(struct i2c_client *dac, uint16_t att)
{
	unsigned int chip_att = (att >= DAC_max_attenuation_dB) ? 0 : (255 - 2 * att);
	// For the chip register: 255 is 0dB attenuation, full volume. Lower values give 0.5dB per step
	// write att value to both left and right on-chip channel, as we use mono mode
	struct regmap *regs = dev_get_regmap(&dac->dev, NULL);
	int err = regmap_write(regs, PCM1792A_DAC_VOL_LEFT, chip_att);
	if (!err)
	  err = regmap_write(regs, PCM1792A_DAC_VOL_RIGHT, chip_att);

  if (err) {
		dev_warn(&dac->dev, "jedac_bcm: set_attenuation_pcn1792(): write err=%d\n", err);
	}
}

static void jedac_set_attenuation( struct jedac_bcm_priv *priv, uint16_t att_l, uint16_t att_r)
{
	// att_? values are attenuation in dBs: 0 is max volume, 79 is min volume, 80 is mute
  int enable_20dB_att = (att_l >= 20) && (att_r >= 20);
  int mute = (att_l >= DAC_max_attenuation_dB) && (att_r >= DAC_max_attenuation_dB);
	
	pr_info("jedac_bcm: set_attenuation(att_l=%u att_r=%u)\n", att_l, att_r);
	
  // adjust the analog volume attenuation -20dB relay if not totally silent
  if (enable_20dB_att && !mute) {
    att_l -= 20; // raise digital (dac) volume
    att_r -= 20; // raise digital (dac) volume
  }
	
	gpiod_set_value(priv->uisync_gpio, 0);  // pull-down 'uisync' pin: signal UI controller on change and stay silent
	// write the board 20dB_attenuation to the fpga:
	int err = regmap_write(priv->fpga_regs, REGDAC_GPO1, (enable_20dB_att ? GPO1_ATT20DB : 0));
  if (err) {
    pr_warn("jedac_bcm: set_attenuation(): in \"%s\" i2c write: err=%d!\n", priv->fpga->name, err);
    // continue further operation...
  } else {
		pr_info("jedac_bcm: set_attenuation(): wrote enable_20db_att=%d\n", enable_20dB_att);
	}

	// the pcm1792 dacs are used in dual-mono mode:
	// write the volume to each of both codecs
  jedac_set_attenuation_pcm1792(priv->dac_l, att_l);
  jedac_set_attenuation_pcm1792(priv->dac_r, att_r);
	gpiod_set_value(priv->uisync_gpio, 0);
}

/*****************************************************************************/
MODULE_AUTHOR("Jos van Eijndhoven <jos@vaneijndhoven.net>");
MODULE_DESCRIPTION("ASoC driver for JvE DAC soundcard");
MODULE_LICENSE("GPL v2");
MODULE_SOFTDEP("pre: regmap_i2c"); // Hints to the kernel to load this first