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
 
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/printk.h>
#include <linux/gpio/consumer.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>

#include "../codecs/jedac5.h"
#include "../codecs/pcm1792a.h"

/*****************************************************************************
* forward declarations of device-specific functions
******************************************************************************/
static int jedac_mode_init(struct snd_soc_component *codec);
static void jedac_set_attenuation( struct jedac_bcm_priv *priv, unsigned short vol_l, unsigned short vol_r);
static int jedac_i2c_set_i2s(int samplerate);

/*****************************************************************************
* Functions and structs to implement the ASoC module API
******************************************************************************/

struct jedac_bcm_priv {
	  struct gpio_desc *uisync_gpio;
		struct i2c_client *fpga;
    struct i2c_client *dac_left;
    struct i2c_client *dac_right;
    uint32_t prev_volume;
};

/* sound card init */
static int snd_rpi_jedac5_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_component *component = snd_soc_rtd_to_codec(rtd, 0)->component;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai_link *dai = rtd->dai_link;
	//struct snd_kcontrol *kctl;
	int ret;
	
	pr_info("jedac5_bcm: snd_rpi_jedac5_dai_init(rtd=%p, card=\"%s\")\n",
		(void *)rtd, card->name);
	pr_info("jedac5_bcm: snd_rpi_jedac5_dai_init: dai=\"%s\", dai_fmt=0x%x\n",
		dai->name, dai->dai_fmt);
    // init fpga chip
	ret = jedac_mode_init(component);

	pr_info("jedac5_bcm: snd_rpi_jedac5_dai_init returns %d\n", ret);
	return ret;
}

/* set hw parameters */
static int snd_rpi_jedac5_hw_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = snd_soc_rtd_to_cpu(rtd, 0);
	int samplerate = params_rate(params);
	int samplewidth = snd_pcm_format_width(params_format(params));
	int clk_ratio = 64; // fixed bclk ratio is easiest for my HW
	int err = snd_soc_dai_set_bclk_ratio(cpu_dai, clk_ratio)
	       || jedac_i2c_set_i2s(samplerate);
	
	//	snd_pcm_format_physical_width(params_format(params));
	pr_info("jedac5_bcm:snd_rpi_jedac5_hw_params(rate=%d, width=%d) err=%d\n",
		samplerate, samplewidth, err);

	return err;
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
	unsigned short vol_l, vol_r;
	pr_info("jedac5_bcm:my_snd_soc_get_volsw() private_value = %04lx\n",
		kcontrol->private_value);

	vol_l = kcontrol->private_value & 0x00ff; // lower 16 bits for left channel, 1dB units
	vol_r = (kcontrol->private_value >> 16) & 0x00ff; // lower 16 bits for left channel
	ucontrol->value.integer.value[0] = -vol_l; // alsa operates with 0.01dB units
	ucontrol->value.integer.value[1] = -vol_r;
	
	return 0;
}

static int bcm_vol_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	uint32_t new_vol;
	int changed = 0;

	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
  struct jedac_bcm_priv *priv = snd_soc_card_get_drvdata(card);
  uint16_t vol_l = ucontrol->value.integer.value[0];
  uint16_t vol_r = ucontrol->value.integer.value[1];
	
	// ALSA values are configured to 0 (mute) to 80 (0dB)
	pr_info("jedac5_bcm: vol_put() ALSA vol_l=%u, vol_r=%d\n", vol_l, vol_r);

	
	new_vol = (vol_l << 16) | vol_r;
	changed = new_vol != priv->prev_volume;

	if (!changed || !priv->dac_left || !priv->dac_right)
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

static const struct snd_kcontrol_new jedac5_controls[] = {
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
// static int snd_soc_put_volsw(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
static const struct snd_kcontrol_new jedac5_controls[] = {
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Master Playback Volume",
		.access = SNDRV_CTL_ELEM_ACCESS_TLV_READ | SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.tlv.p = dac_db_scale,
		.info = my_info_volsw,
		.get = bcm_vol_get,
		.put = bcm_vol_put,
		.private_value = 0
	}
};

/* startup */
static int snd_rpi_jedac5_startup(struct snd_pcm_substream *substream) {
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component = snd_soc_rtd_to_codec(rtd, 0)->component;
	// struct snd_soc_codec *codec = rtd->codec;
	// const char* name = codec ? codec->component.name : "NULL";
	// snd_soc_write(codec, REGDAC_GPO0, GPO0_POWERUP | GPO0_CLKMASTER);
	snd_soc_component_write(component, REGDAC_GPO0, GPO0_POWERUP | GPO0_CLKMASTER);
	pr_info("jedac5_bcm:snd_rpi_jedac5_startup(): codec=%s powerup!\n", component->name);
	return 0;
}

/* shutdown */
static void snd_rpi_jedac5_shutdown(struct snd_pcm_substream *substream) {
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component = snd_soc_rtd_to_codec(rtd, 0)->component;
	// struct snd_soc_codec *codec = rtd->codec;
	// const char* name = codec ? codec->component.name : "NULL";

	// hmm.. powerdown not a very good idea here?
	// this occurs within a minute of a song end.
	// snd_soc_write(codec, REGDAC_GPO0, 0x00); // power-down DAC into standby
	pr_info("jedac5_bcm:snd_rpi_jedac5_shutdown() codec=%s dummy\n", component->name);
}

/* card suspend */
static int jedac5_suspend_post(struct snd_soc_card *card)
{
	pr_info("jedac5_bcm: jedac5_suspend_post() dummy\n");
	return 0;
}

/* card resume */
static int jedac5_resume_pre(struct snd_soc_card *card)
{
	pr_info("jedac5_bcm: jedac5_resume_pre() dummy\n");
	return 0;
}

/* machine stream operations */
static struct snd_soc_ops snd_rpi_jedac5_ops = {
	.hw_params = snd_rpi_jedac5_hw_params,
	.startup = snd_rpi_jedac5_startup,
	.shutdown = snd_rpi_jedac5_shutdown,
};

// Create references to my modules.
// Note: the actual names are not important, they are later set to NULL during _probe()
SND_SOC_DAILINK_DEFS(rpi_jedac5,
	DAILINK_COMP_ARRAY(COMP_CPU("bcm2708-i2s.0")),
	DAILINK_COMP_ARRAY(COMP_CODEC("jedac5_codec.1-0020", "jedac5_codec")), //fpga is at i2c bus 1 addr 20
	DAILINK_COMP_ARRAY(COMP_PLATFORM("bcm2708-i2s.0")));

static struct snd_soc_dai_link jedac5_dai_link[] = {
{
	.name		= "JvE DAC5",
	.stream_name	= "JvE DAC",
	.dai_fmt	= JEDAC_DAIFMT,
	.ops		= &snd_rpi_jedac5_ops,
	.init		= snd_rpi_jedac5_dai_init,
	SND_SOC_DAILINK_REG(rpi_jedac5),
},
};

static const struct snd_soc_dapm_widget jedac5_dapm_widgets[] = {
SND_SOC_DAPM_OUTPUT("IOUTL"),
SND_SOC_DAPM_OUTPUT("IOUTR"),
};

static const struct snd_soc_dapm_route jedac5_dapm_routes[] = {
	{ "IOUTL", NULL, "Playback" },
	{ "IOUTR", NULL, "Playback" },
};


/* Audio card  -- machine driver */
static struct snd_soc_card jedac5_sound_card = {
	.name = "JEDAC",
	.owner = THIS_MODULE,
//	.remove = jedac5_card_remove,
	.dai_link = jedac5_dai_link,
	.num_links = ARRAY_SIZE(jedac5_dai_link),
	.aux_dev = jedac5_aux_devs,
	.num_aux_devs = ARRAY_SIZE(jedac5_aux_devs),
//	.codec_conf = rx51_codec_conf,
//	.num_configs = ARRAY_SIZE(rx51_codec_conf),

	.controls = jedac5_controls,
	.num_controls = ARRAY_SIZE(jedac5_controls),
	.dapm_widgets = jedac5_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(jedac5_dapm_widgets),
	.dapm_routes = jedac5_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(jedac5_dapm_routes),
	
	.suspend_post = jedac5_suspend_post,
	.resume_pre = jedac5_resume_pre,
};

/* node names of the i2c devices, to match with the jedac5-overlay.dts */
static const char* i2c_node_refs[] = {
	"jve,dac_core",
	"jve,dac_l",
	"jve,dac_r"
};

static struct snd_soc_component* get_rtd_component(void) {
	// assume my jedac device is used with only one instantiation!
	struct snd_soc_pcm_runtime *rtd = NULL;
	list_for_each_entry(rtd, &jedac5_sound_card.rtd_list, list) {
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
static int snd_rpi_jedac5_probe(struct platform_device *pdev)
{
	int ret = -EINVAL;
	int i;
	struct device_node *np = pdev->dev.of_node;
	jedac5_sound_card.dev = &pdev->dev;
	
	if (np) {
    pr_info("jedac5_bcm: start probe(), device node OK!\n");
	} else {
    pr_err("jedac5_bcm: probe(): device node error!\n");
		return -EINVAL;
	}


	// Allocate private memory managed by the device
	struct jedac_bcm_priv* priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
  if (!priv) {
		pr_err("jedac5_bcm: probe() failed to alloc priv struct!\n");
    return -ENOMEM;
	}
	snd_soc_card_set_drvdata(&jedac5_sound_card, priv);

	// Obtain access to the gpio pin "uisync" to send signals to the UI controller
	// The "uisync" name and its gpio pin are defined in the DTS overlay file
	priv->uisync_gpio = devm_gpiod_get(&pdev->dev, "uisync", GPIOD_OUT_HIGH_OPEN_DRAIN);
	if (IS_ERR(priv->uisync_gpio)) {
		pr_err("jedac5_bcm: failed to access the 'uisync' gpio pin!\n");
		return -EINVAL;
	} else {
		pr_err("jedac5_bcm: successfully acquired 'uisync' gpio pin!\n");
	}

	struct snd_soc_dai_link *dai = &jedac5_dai_link[0];
	/* find my three i2c components on the dac board: an FPGA and two PCM1792 */
  int found_nodes = 0;
	struct device_node *nodes[3];
	for (i = 0; i < 3; i++) {
		const char *handle = i2c_node_refs[i];
		nodes[i] = of_parse_phandle(np, handle, 0);
			
		if (!nodes[i]) {
			dev_err(&pdev->dev, "jedac5_bcm: handle %s not found!\n", handle);
			continue;
		}
		pr_info("jedac5_bcm: Found handle %s for card\n", handle);
	  found_nodes++;
	}
	priv->fpga = of_find_i2c_device_by_node(nodes[0]);
	priv->dac_left = of_find_i2c_device_by_node(nodes[1]);
	priv->dac_right = of_find_i2c_device_by_node(nodes[2]);
	priv->current_volume = 0;

	struct device_node i2s_node = of_parse_phandle(np, "i2s-controller", 0);
	if (!i2s_node) {
		dev_err(&pdev->dev, "jedac5_bcm: i2s_node not found!\n");
	} else {
		pr_info("jedac5_bcm: Found i2s handle for card\n");
		found_nodes++;
	}

	// We have one i2s 'digital audio interface' towards the board FPGA
  pr_info("jedac5_bcm: dai num_cpus=%u, num_platforms=%u, num_codecs=%u\n",
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
	  ret = devm_snd_soc_register_card(&pdev->dev, &jedac5_sound_card);
	  const char *msg = (ret == 0) ? "Success" :
	                  (ret == -EINVAL) ? "Incomplete snd_soc_card struct?" :
										(ret == -ENODEV) ? "Linked component not found?" :
										(ret == -ENOENT) ? "DT node or property missing?" :
										(ret == -EIO) ? "Communication failure" :
										(ret == -EPROBE_DEFER) ? "Deferred" : "Failure";
	
	  if (ret && (ret != -EPROBE_DEFER)) {
      dev_err(&pdev->dev, "jedac5_bcm: probe: register_card error: \"%s\", return %d\n", msg, ret);
	  } else {
		  pr_info("jedac5_bcm: probe: register_card: \"%s\", return %d\n", msg, ret);
	  }
	} else {
		pr_info("jedac5_bcm: probe: No register_card: nodes found=%d, return %d\n", found_nodes, ret);
	}

	// fix refcount of_node_get()/of_node_put()
  of_node_put(i2s_node);
  of_node_put(nodes[0]);
	of_node_put(nodes[1]);
	of_node_put(nodes[2]);
	return ret;
}

/* sound card disconnect */
static void snd_rpi_jedac5_remove(struct platform_device *pdev)
{
	pr_info("jedac5_bcm:snd_rpi_jedac5_remove(): power-down\n");
	struct snd_soc_component* component = get_rtd_component();
	if (component) {
    snd_soc_component_write(component, REGDAC_GPO0, 0); // power down
	}
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	if (card) {
		struct jedac5_bcm_priv *priv = snd_soc_card_get_drvdata(card);
		gpiod_set_value(priv->uisync_gpio, 1);  // just for safety: deactivate
	}
}

static const struct of_device_id jedac5_of_match[] = {
	{ .compatible = "jve,jedac5_bcm", },
	{},
};
MODULE_DEVICE_TABLE(of, jedac5_of_match);

/* sound card platform driver */
static struct platform_driver snd_rpi_jedac5_driver = {
	.driver = {
		.name   = "snd-rpi-jedac5_bcm",
		.owner  = THIS_MODULE,
		.of_match_table = jedac5_of_match,
	},
	.probe          = snd_rpi_jedac5_probe,
	.remove         = snd_rpi_jedac5_remove,
};

module_platform_driver(snd_rpi_jedac5_driver);


/*****************************************************************************
* Above code contains the ASoC API structs and functions
* Below code adds the more specific device functionality
******************************************************************************/

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
		pr_info("jedac5_bcm: jedac_await_powerup: i2c error!\n");
	else if (!got_pwr)
		pr_info("jedac5_bcm: jedac_await_powerup: powerup timeout error!\n");
	else
		pr_info("jedac5_bcm: jedac_await_powerup: power OK after %d iterations\n", cnt);

	return !got_pwr;
}

static int jedac_pcm1792_init(struct snd_soc_component *component, bool is_right_chan)
{
	int err, i;
	const signed long delay = 50 * HZ / 1000; // 50 milliseconds per iteration
	
	if (component)
		pr_info("jedac5_bcm: jedac_pcm1792_init(name=%s, id=%d, is_right=%d)\n",
			component->name, component->id, is_right_chan);
	else {
		pr_info("jedac5_bcm: jedac_pcm1792_init(NULL)??\n");
		return -1;
	}

	err = jedac_await_powerup();
    // reg 18: audio format left justified, enable att, no mute, no demp
    // reg 19: slow unmute, filter slow rolloff
    // reg 20: set mono mode, choose left
	if (!err)
	{
		for (i=0, err=-1; i<10 && err<0; i++) // make 3 tries..
		{
			err = snd_soc_component_write(component, 18, 0xb0);
			if (err < 0)
			{
				set_current_state(TASK_INTERRUPTIBLE);
				schedule_timeout(delay);
			} else
			{
				err = snd_soc_component_write(component, 19, 0x62);
				if (err >= 0) err = snd_soc_component_write(component, 20, (is_right_chan?0x0c:0x08));
			}
		}
	}

	pr_info("jedac5_bcm: jedac_pcm1792_init(\"%s\") returns %d\n", component->name, err);
	return err;
}

// init the FPGA (or cs8416 receiver chip) operating mode through its i2c bus
static int jedac_mode_init(struct snd_soc_component *codec)
{
	struct jedac5_codec_priv *priv;
  char chan = 3; // default input channel from 0 .. 3
  char reg_chan;
	int i2cerr = 0;
	
	pr_info("jedac5_bcm: jedac_mode_init(codec=%p) set power & master\n", (void *)codec);
	if (!codec)
			return -99;

	reg_chan = GPO0_POWERUP | GPO0_CLKMASTER;
	i2cerr = snd_soc_component_write(codec, REGDAC_GPO0, reg_chan);
	
	priv = snd_soc_component_get_drvdata(codec);
	if (priv) {
		priv->chan_select = chan; // Hmm... not used, rely on kcontrol structs in bcm??
	}

	return i2cerr;
}

static void jedac_set_attenuation_pcm1792(struct i2c_client *dac, uint16_t att)
{
	uint8_t chip_att = (att == 0) ? 0 : (255 - ((80 - att) * 2));
	// write att value to both left and right on-chip channel, as we use mono mode
	int err = i2c_smbus_write_byte_data(dac, PCM1792A_DAC_VOL_LEFT, chip_att);
	if (!err)
	  err = i2c_smbus_write_byte_data(dac, PCM1792A_DAC_VOL_RIGHT, chip_att);

  if (err) {
    pr_warn("jedac5_bcm: set_attenuation_pcn1792(): error=%d in i2c dac write!\n", err);
}

static void jedac_set_attenuation( struct jedac_bcm_priv *priv, uint16_t att_l, uint16_t att_r)
{
	// att_? values are attenuation in dBs: 0 is max volume, 79 is min volume, 80 is mute
  int enable_20dB_att = (att_l >= 20) && (att_r >= 20);
  int mute = (att_l >= DAC_max_attenuation_dB) && (att_r >= DAC_max_attenuation_dB);
	
	pr_info("jedac5_bcm: set_attenuation(att_l=%u att_r=%u)\n", att_l, att_r);
	
  // adjust the analog volume attenuation -20dB relay if not totally silent
  if (enable_20dB_att && !mute) {
    att_l -= 20; // raise digital (dac) volume
    att_r -= 20; // raise digital (dac) volume
  }
	
	// write the board 20dB_attenuation to the fpga:
	int err = i2c_smbus_write_byte_data(priv->fpga, REGDAC_GPO1, (enable_20dB_att ? GPO1_ATT20DB : 0));
  if (err) {
    pr_warn("jedac5_bcm: set_attenuation(): error=%d in i2c codec write!\n", err);
    // continue further operation...
  } else {
		pr_info("jedac5_bcm: set_attenuation(): wrote enable_20db_att=%d\n", enable_20dB_att);
	}

	// the pcm1792 dacs are used in dual-mono mode:
	// write the volume to each of both codecs
  jedac_set_attenuation_pcm1792(priv->dac_l, att_l);
  jedac_set_attenuation_pcm1792(priv->dac_r, att_r);
}

static int jedac_i2c_set_i2s(int samplerate)
{
	struct snd_soc_component *codec = get_rtd_component();
	int i2cerr, freq_base, freq_mult, gpo_val;

	if (!codec) {
		pr_info("jedac5_bcm: jedac_i2c_set_i2s: num_rtd=%d, rtd_codec=%p\n",
			jedac5_sound_card.num_rtd, (void *)codec);
		return -1;
	}
	
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
	struct jedac5_bcm_priv *priv = snd_soc_card_get_drvdata(&jedac5_sound_card);
	if (0 == gpiod_get_value(priv->uisync_gpio)) {
		pr_warn("jedac5_bcm: jedac_i2c_set_i2s: unexpected 'low' on ui-trig gpio!");
	}
	gpiod_set_value(priv->uisync_gpio, 0);

	gpo_val = GPO0_POWERUP | GPO0_CLKMASTER | (freq_base << 1) | (freq_mult << 2);
	i2cerr = snd_soc_component_write(codec, REGDAC_GPO0, gpo_val);
	// as test, read-back fpga status byte
	gpo_val = snd_soc_component_read(codec, REGDAC_GPI0);
	if (gpo_val < 0) // values >=0 indicate valid read, no error
		i2cerr = gpo_val;

	gpiod_set_value(priv->uisync_gpio, 1);  // clear the ui trigger pulse
	pr_info("jedac5_bcm: jedac_i2c_set_i2s: read GPI=0x%02x. i2c err=%d\n", (int)(gpo_val & 0xff), i2cerr);

	return i2cerr;
}

/*****************************************************************************/
MODULE_AUTHOR("Jos van Eijndhoven <jos@vaneijndhoven.net>");
MODULE_DESCRIPTION("ASoC driver for JvE DAC soundcard");
MODULE_LICENSE("GPL v2");