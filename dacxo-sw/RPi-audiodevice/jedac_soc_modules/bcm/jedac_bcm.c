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

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>

#include "../codecs/jedac.h"
#include "../codecs/pcm1792a.h"

struct jedac_bcm_priv {
	  struct gpio_desc *uisync_gpio;
		struct i2c_client *fpga;
    struct i2c_client *dac_l;
    struct i2c_client *dac_r;
    uint32_t prev_volume;
};

static void jedac_set_attenuation( struct jedac_bcm_priv *priv, unsigned short vol_l, unsigned short vol_r);

static int i2c_write_retry( struct i2c_client *client, uint8_t reg, uint8_t value) {
  int err = i2c_smbus_write_byte_data( client, reg, value);
	if ((err == -EREMOTEIO) || (err == -EAGAIN)) {
		err = i2c_smbus_write_byte_data( client, reg, value);  // retry once more
	}
	if (err != 0) {
		pr_warn("jedac i2c write(%s (0x%02x), reg=%d, %d) failed with %d!\n",
			client->name, client->addr, (int)(reg), (int)(value), err);
	}
	return err;
};

/* sound card init */
static void jedac_pcm1792_init(struct i2c_client *dac, bool is_right_chan)
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
  
	for (int i = 0; i < ARRAY_SIZE(inits); i++) {
		i2c_write_retry(dac, inits[i].reg_nr, inits[i].value);
	}
}

static int jedac_bcm_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_card *card = rtd->card;
  struct jedac_bcm_priv *priv = snd_soc_card_get_drvdata(card);

	pr_info("jedac_bcm: jedac_bcm_init(card=\"%s\")\n", card->name);
  // Note that the FPGA has already done its own init during its 'probe()'
	// reg_chan = GPO0_POWERUP | GPO0_CLKMASTER;
	// i2cerr = snd_soc_component_write(codec, REGDAC_GPO0, reg_chan);

	jedac_pcm1792_init(priv->dac_l, false);
	jedac_pcm1792_init(priv->dac_r, true);
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
	
	// ALSA values are configured to 0 (mute) to 80 (0dB)
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

static const struct snd_kcontrol_new jedac_controls[] = {
	{
        .iface = SNDRV_CTL_ELEM_IFACE_MIXER,
        .name = "Master Playback Volume",
        .access = SNDRV_CTL_ELEM_ACCESS_READWRITE | SNDRV_CTL_ELEM_ACCESS_TLV_READ,
        .info = bcm_vol_info,
        .get  = bcm_vol_get,
        .put  = bcm_vol_put,
        .tlv.p = dac_db_scale,
  },
};

/* startup */
static int snd_rpi_jedac_startup(struct snd_pcm_substream *substream) {
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component = snd_soc_rtd_to_codec(rtd, 0)->component;
	// struct snd_soc_codec *codec = rtd->codec;
	// const char* name = codec ? codec->component.name : "NULL";
	// snd_soc_write(codec, REGDAC_GPO0, GPO0_POWERUP | GPO0_CLKMASTER);
	// snd_soc_component_write(component, REGDAC_GPO0, GPO0_POWERUP | GPO0_CLKMASTER);
	pr_info("jedac_bcm:snd_rpi_jedac_startup(): codec=%s powerup!\n", component->name);
	return 0;
}

/* shutdown */
static void snd_rpi_jedac_shutdown(struct snd_pcm_substream *substream) {
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component = snd_soc_rtd_to_codec(rtd, 0)->component;
	// struct snd_soc_codec *codec = rtd->codec;
	// const char* name = codec ? codec->component.name : "NULL";

	// hmm.. powerdown not a very good idea here?
	// this occurs within a minute of a song end.
	// snd_soc_write(codec, REGDAC_GPO0, 0x00); // power-down DAC into standby
	pr_info("jedac_bcm:snd_rpi_jedac_shutdown() codec=%s dummy\n", component->name);
}

/* card suspend */
static int jedac_suspend_post(struct snd_soc_card *card)
{
	pr_info("jedac_bcm: jedac_suspend_post() dummy\n");
	return 0;
}

/* card resume */
static int jedac_resume_pre(struct snd_soc_card *card)
{
	pr_info("jedac_bcm: jedac_resume_pre() dummy\n");
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

static const struct snd_soc_dapm_widget jedac_bcm_widgets[] = {
    SND_SOC_DAPM_HP("Main Output", NULL), // A "Sink" for the audio
};

static const struct snd_soc_dapm_route jedac_bcm_routes[] = {
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

/* node names of the i2c devices, to match with the jedac-overlay.dts */
static const char* i2c_node_refs[] = {
	"jve,dac_core",
	"jve,dac_l",
	"jve,dac_r"
};

static struct snd_soc_component* get_rtd_component(void) {
	// assume my jedac device is used with only one instantiation!
	struct snd_soc_pcm_runtime *rtd = NULL;
	list_for_each_entry(rtd, &jedac_sound_card.rtd_list, list) {
		struct snd_soc_dai* dai = NULL;
		if (rtd && (dai = snd_soc_rtd_to_codec(rtd, 0)))
			return dai->component;
	}
	return NULL;
}

/* sound card detect */
// see for instance examples like:
// https://github.com/raspberrypi/linux/blob/rpi-6.12.y/sound/soc/bcm/hifiberry_dacplus.c
// or, for a card with two codes like me:
// https://github.com/raspberrypi/linux/blob/rpi-6.12.y/sound/soc/bcm/pifi-40.c
static int snd_jedac_probe(struct platform_device *pdev)
{
	int ret = -EINVAL;
	struct device_node *np = pdev->dev.of_node;
	jedac_sound_card.dev = &pdev->dev;
	
	if (np) {
    pr_info("jedac_bcm: start probe(), device node OK!\n");
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
	priv->uisync_gpio = devm_gpiod_get(&pdev->dev, "uisync", GPIOD_OUT_HIGH_OPEN_DRAIN);
	if (IS_ERR(priv->uisync_gpio)) {
		pr_err("jedac_bcm: failed to access the 'uisync' gpio pin!\n");
		return -EINVAL;
	} else {
		pr_err("jedac_bcm: successfully acquired 'uisync' gpio pin!\n");
	}

	struct snd_soc_dai_link *dai = &jedac_dai_link[0];
	/* find my three i2c components on the dac board: an FPGA and two PCM1792 */
  int found_nodes = 0;
	struct device_node *nodes[3];
	for (int i = 0; i < 3; i++) {
		const char *handle = i2c_node_refs[i];
		nodes[i] = of_parse_phandle(np, handle, 0);
			
		if (!nodes[i]) {
			dev_err(&pdev->dev, "jedac_bcm: handle %s not found!\n", handle);
			continue;
		}
		pr_info("jedac_bcm: Found handle %s for card\n", handle);
	  found_nodes++;
	}
	priv->fpga = of_find_i2c_device_by_node(nodes[0]);
	priv->dac_l = of_find_i2c_device_by_node(nodes[1]);
	priv->dac_r = of_find_i2c_device_by_node(nodes[2]);
	priv->prev_volume = 0;

	if (!priv->fpga) {
		dev_warn(&pdev->dev, "jedac_bcm: fpga chip not found on i2c bus addr=0x%02x!\n",
			priv->fpga->addr);
	}
	if (!priv->dac_l) {
		dev_warn(&pdev->dev, "jedac_bcm: left dac chip not found on i2c bus addr=0x%02x!\n",
			priv->dac_l->addr);
	}
	if (!priv->dac_r) {
		dev_warn(&pdev->dev, "jedac_bcm: right dac chip not found on i2c bus addr=0x%02x!\n",
		  priv->dac_r->addr);
	}

	struct device_node *i2s_node = of_parse_phandle(np, "i2s-controller", 0);
	if (!i2s_node) {
		dev_err(&pdev->dev, "jedac_bcm: i2s_node not found!\n");
	} else {
		pr_info("jedac_bcm: Found i2s handle for card\n");
		found_nodes++;
	}

	// We have one i2s 'digital audio interface' towards the board FPGA
  pr_info("jedac_bcm: dai num_cpus=%u, num_platforms=%u, num_codecs=%u\n",
    dai->num_cpus, dai->num_platforms, dai->num_codecs);
  dai->cpus[0].name = NULL;
	dai->cpus[0].dai_name = NULL;
	dai->cpus[0].of_node = i2s_node;
	dai->platforms[0].name = NULL;
	dai->platforms[0].of_node = i2s_node;
	dai->codecs[0].name = NULL;
	dai->codecs[0].of_node = nodes[0];  // our fpga acts as dai codec

	if (found_nodes == 4) {
		// Good, all device tree nodes found!
	  ret = devm_snd_soc_register_card(&pdev->dev, &jedac_sound_card);
	  const char *msg = (ret == 0) ? "Success" :
	                  (ret == -EINVAL) ? "Incomplete snd_soc_card struct?" :
										(ret == -ENODEV) ? "Linked component not found?" :
										(ret == -ENOENT) ? "DT node or property missing?" :
										(ret == -EIO) ? "Communication failure" :
										(ret == -EPROBE_DEFER) ? "Deferred" : "Failure";
	
	  if (ret && (ret != -EPROBE_DEFER)) {
      dev_err(&pdev->dev, "jedac_bcm: probe: register_card error: \"%s\", return %d\n", msg, ret);
	  } else {
		  pr_info("jedac_bcm: probe: register_card: \"%s\", return %d\n", msg, ret);
	  }
	} else {
		pr_info("jedac_bcm: probe: No register_card: nodes found=%d, return %d\n", found_nodes, ret);
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
	pr_info("jedac_bcm:snd_rpi_jedac_remove(): power-down\n");
	struct snd_soc_component* component = get_rtd_component();
	if (component) {
    snd_soc_component_write(component, REGDAC_GPO0, 0); // power down
	}
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	if (card) {
		struct jedac_bcm_priv *priv = snd_soc_card_get_drvdata(card);
		gpiod_set_value(priv->uisync_gpio, 1);  // just for safety: deactivate
	}
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


/*****************************************************************************
* Above code contains the ASoC API structs and functions
* Below code adds the more specific device functionality
******************************************************************************/

#if 0
// power-up the board from standby, and await the presence of the
// powersupply: we need that to access the pcm1792 devices
// return 0 on success, !=0 on timeout or i2c or codec failure
static int jedac_await_powerup(void)
{
	struct snd_soc_component *comp = get_rtd_component();
	const unsigned char mode_reg = GPO0_POWERUP | GPO0_CLKMASTER;
	const signed long delay = 50 * HZ / 1000; // 50 milliseconds per iteration
	const int max_cnt = 60; // max allowed iterations until timeout error, expected is 6
	int cnt, i2cerr;
	int got_pwr = 0;
	// the codec (fpga chip) is also powered on standby, and should always work
	if (!comp)
		return -1;
	
	i2cerr = snd_soc_component_write(comp, REGDAC_GPO0, mode_reg);

	for (cnt = 0; !i2cerr && !got_pwr && cnt < max_cnt; cnt++)
	{
		int reg_val = snd_soc_component_read(comp, REGDAC_GPI1);
		if (reg_val < 0) // i2c read error
			i2cerr = 1;
		else if (reg_val & GPI1_ANAPWR)
			got_pwr = 1; // Yes, success :-)
		else
		{
			// timer wait for 50 millisecnds
			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(delay);
		}
	}
	
	// Maybe, we have just hit the moment of power-up.
	// Then we need to allow some extra time for:
	// - the xtal oscillator to ramp-up (2 msec startup time) and
	// - the pcm1792 to do its internal reset sequence (1024 clocks is 0.2msec)
	if (got_pwr && cnt > 1)
	{
		signed long short_delay = delay/2;
		if (short_delay < 1)
			short_delay = 1;
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(short_delay);
	}
	
	if (i2cerr)
		pr_info("jedac_bcm: jedac_await_powerup: i2c error!\n");
	else if (!got_pwr)
		pr_info("jedac_bcm: jedac_await_powerup: powerup timeout error!\n");
	else
		pr_info("jedac_bcm: jedac_await_powerup: power OK after %d iterations\n", cnt);

	return !got_pwr;
}
#endif

static void jedac_set_attenuation_pcm1792(struct i2c_client *dac, uint16_t att)
{
	uint8_t chip_att = (att == 0) ? 0 : (255 - ((80 - att) * 2));
	// write att value to both left and right on-chip channel, as we use mono mode
	int err = i2c_write_retry(dac, PCM1792A_DAC_VOL_LEFT, chip_att);
	if (!err)
	  err = i2c_write_retry(dac, PCM1792A_DAC_VOL_RIGHT, chip_att);

  if (err)
    pr_warn("jedac_bcm: set_attenuation_pcn1792(): error=%d in i2c dac write!\n", err);
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
	
	// write the board 20dB_attenuation to the fpga:
	int err = i2c_write_retry(priv->fpga, REGDAC_GPO1, (enable_20dB_att ? GPO1_ATT20DB : 0));
  if (err) {
    pr_warn("jedac_bcm: set_attenuation(): error=%d in i2c codec write!\n", err);
    // continue further operation...
  } else {
		pr_info("jedac_bcm: set_attenuation(): wrote enable_20db_att=%d\n", enable_20dB_att);
	}

	// the pcm1792 dacs are used in dual-mono mode:
	// write the volume to each of both codecs
  jedac_set_attenuation_pcm1792(priv->dac_l, att_l);
  jedac_set_attenuation_pcm1792(priv->dac_r, att_r);
}

/*****************************************************************************/
MODULE_AUTHOR("Jos van Eijndhoven <jos@vaneijndhoven.net>");
MODULE_DESCRIPTION("ASoC driver for JvE DAC soundcard");
MODULE_LICENSE("GPL v2");