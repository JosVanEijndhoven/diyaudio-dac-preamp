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
static int jedac_pcm1792_init(struct snd_soc_component *component, bool is_right_chan);
static int jedac_mode_init(struct snd_soc_codec *codec);
static void jedac_i2c_set_volume( unsigned short vol_l, unsigned short vol_r);
static int jedac_i2c_set_i2s(int samplerate);

/*****************************************************************************
* Functions and structs to implement the ASoC module API
******************************************************************************/

static int jedac_pcm1792_init_l(struct snd_soc_component *component) {
	return jedac_pcm1792_init(component, 0);
}
static int jedac_pcm1792_init_r(struct snd_soc_component *component) {
	return jedac_pcm1792_init(component, 1);
}

/* sound card init */
static int snd_rpi_jedac5_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dai_link *dai = rtd->dai_link;
	//struct snd_kcontrol *kctl;
	int ret;
	
	pr_info("jedac5_bcm: snd_rpi_jedac5_dai_init(rtd=%p, card=\"%s\")\n",
		(void *)rtd, card->name);
	pr_info("jedac5_bcm: snd_rpi_jedac5_dai_init: dai=\"%s\", dai_fmt=0x%x\n",
		dai->name, dai->dai_fmt);
    // init CS8416 receiver chip
	ret = jedac_mode_init(codec);

	pr_info("jedac5_bcm: snd_rpi_jedac5_dai_init returns %d\n", ret);
	return ret;
}

static struct snd_soc_aux_dev jedac5_aux_devs[] = {
	{
		.name = "pcm1792a_l",
		.codec_name = "pcm1792a.1-4d", // bus addr 9a div 2: left channel
		.init = jedac_pcm1792_init_l,
	},
	{
		.name = "pcm1792a_r",
		.codec_name = "pcm1792a.1-4c", // bus addr 98 div 2: right channel
		.init = jedac_pcm1792_init_r,
	},
};

/*
static struct snd_soc_codec_conf jedac5_codec_conf[] = {
	{
		.dev_name = "jedac-codec.2-0019",
		.name_prefix = "b",
	},
};
*/

/* set hw parameters */
static int snd_rpi_jedac5_hw_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
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
	pr_info("jedac5_bcm:my_snd_soc_get_volsw() private_value = %04lx\n",
		kcontrol->private_value);

	vol_l = kcontrol->private_value & 0x00ff; // lower 16 bits for left channel, 1dB units
	vol_r = (kcontrol->private_value >> 16) & 0x00ff; // lower 16 bits for left channel
	ucontrol->value.integer.value[0] = -vol_l; // alsa operates with 0.01dB units
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
	
	pr_info("jedac5_bcm:my_snd_soc_put_volsw() private_value = %04lx\n",
		kcontrol->private_value);
		
	// vol_x values are attenuation in dBs: 0 is max volume, 79 is min volume, 80 is mute
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
		jedac_i2c_set_volume( vol_l, vol_r);
		// TODO: write vol_l and vol_r to i2c devices...
		// err = snd_soc_update_bits(codec, reg, val_mask, val);
	}
	
	return changed;
}

// define my volume scale as -80dB to 0 dB in steps of 1 dB, where the lowest volume does mute
static DECLARE_TLV_DB_SCALE(dac_db_scale, (-100 * DAC_max_attenuation_dB),
	                        (100 * DAC_step_attenuation_dB), 1);

// static int snd_soc_put_volsw(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
static const struct snd_kcontrol_new jedac5_controls[] = {
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Master Playback Volume",
		.access = SNDRV_CTL_ELEM_ACCESS_TLV_READ | SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.tlv.p = dac_db_scale,
		.info = my_info_volsw,
		.get = my_get_volsw,
		.put = my_put_volsw,
		.private_value = 0
	}
	//SOC_DOUBLE_R_EXT_TLV("DAC Playback Volume",
	//			PCM1792A_DAC_VOL_LEFT,
	//			PCM1792A_DAC_VOL_RIGHT,
	//			0, DAC_max_attenuation_dB, 0,
	//			my_get_volsw, my_put_volsw, dac_db_scale),
};

/* startup */
static int snd_rpi_jedac5_startup(struct snd_pcm_substream *substream) {
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	
	const char* name = codec ? codec->component.name : "NULL";
	snd_soc_write(codec, REGDAC_GPO0, GPO0_POWERUP | GPO0_SPIMASTER);
	pr_info("jedac5_bcm:snd_rpi_jedac5_startup(): codec=%s powerup!\n", name);
	return 0;
}

/* shutdown */
static void snd_rpi_jedac5_shutdown(struct snd_pcm_substream *substream) {
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	const char* name = codec ? codec->component.name : "NULL";

	// hmm.. powerdown not a very good idea here?
	// this occurs within a minute of a song end.
	// snd_soc_write(codec, REGDAC_GPO0, 0x00); // power-down DAC into standby
	pr_info("jedac5_bcm:snd_rpi_jedac5_shutdown() codec=%s dummy\n", name);
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

static struct snd_soc_dai_link jedac5_dai_link[] = {
{
	.name		= "JvE DAC5",
	.stream_name	= "JvE DAC",
	.cpu_dai_name	= "bcm2708-i2s.0",
	.codec_dai_name	= "jedac5_codec",
	.platform_name	= "bcm2708-i2s.0",
	.codec_name	= "jedac5_codec.1-0020", //cs4816 is at i2c bus 1 addr 20
	.dai_fmt	= JEDAC_DAIFMT,
	.ops		= &snd_rpi_jedac5_ops,
	.init		= snd_rpi_jedac5_dai_init,
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

static struct snd_soc_codec* get_rtd_codec(void) {
	// assume my jedac device is used with only one instantiation!
	struct snd_soc_pcm_runtime *rtd = NULL;
	list_for_each_entry(rtd, &jedac5_sound_card.rtd_list, list) {
		if (rtd && rtd->codec)
			return rtd->codec;
	}
	return NULL;
}

/* sound card test */
static int snd_rpi_jedac5_probe(struct platform_device *pdev)
{
	int ret = 0;
	
	struct device_node *np = pdev->dev.of_node;
	jedac5_sound_card.dev = &pdev->dev;
	
	pr_info("jedac5_bcm: snd_rpi_jedac5_probe()\n");

	if (np) {	
		int i;
	    struct device_node *i2s_node, *pcm_node;
		struct device_node *i2c_codec_node = NULL;

		/* find my three i2c components on the dac board */
		for (i=0; i<3; i++) {
			const char *handle = i2c_node_refs[i];
			pcm_node = of_parse_phandle(np, handle, 0);
				
			if (!pcm_node) {
				dev_err(&pdev->dev, "jedac5_bcm: handle %s not found!\n", handle);
				return -EINVAL;
			} else
				pr_info("jedac5_bcm: Found handle %s for card\n", handle);
		
			if (i==0) {
				i2c_codec_node = pcm_node;
			} else {
				/* my two pcm codecs as aux dev... */
				struct snd_soc_aux_dev* aux_dev = &jedac5_aux_devs[i-1];
				aux_dev->codec_name = NULL;
				aux_dev->codec_of_node = pcm_node;
			}
		}

	    i2s_node = of_parse_phandle(np, "i2s-controller", 0);
		pr_info("jedac5_bcm: i2s_node is %s\n", (i2s_node?"OK":"NULL"));

	    if (i2s_node) {
			struct snd_soc_dai_link *dai = &jedac5_dai_link[0];
			dai->cpu_dai_name = NULL;
			dai->cpu_of_node = i2s_node;
			dai->platform_name = NULL;
			dai->platform_of_node = i2s_node;
			dai->codec_name = NULL;
			dai->codec_of_node = i2c_codec_node;
	    }
	}
	
	ret = snd_soc_register_card(&jedac5_sound_card);
	if (ret)
		dev_err(&pdev->dev,
			"snd_soc_register_card() failed: %d\n", ret);
	
	pr_info("jedac5_bcm: snd_rpi_jedac5_probe() returns %d\n", ret);

	return ret;
}

/* sound card disconnect */
static int snd_rpi_jedac5_remove(struct platform_device *pdev)
{
	struct snd_soc_codec *codec = get_rtd_codec();
	pr_info("jedac5_bcm:snd_rpi_jedac5_remove(): power-down\n");
	if (codec)
		snd_soc_write(codec, REGDAC_GPO0, 0); // power down

	// struct jedac5_private *priv = dev_get_drvdata(dev);
	// assume that will be deallocated by below calls...
	// snd_soc_unregister_codec(dev);

	return snd_soc_unregister_card(&jedac5_sound_card);
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
	struct snd_soc_codec *codec = get_rtd_codec();
	const unsigned char mode_reg = GPO0_POWERUP | GPO0_SPIMASTER;
	const signed long delay = 50 * HZ / 1000; // 50 milliseconds per iteration
	const int max_cnt = 60; // max allowed iterations until timeout error, expected is 6
	int cnt, i2cerr;
	int got_pwr = 0;
	// the codec (fpga chip) is also powered on standby, and should always work
	if (!codec)
		return -1;
	
	i2cerr = snd_soc_write(codec, REGDAC_GPO0, mode_reg);

	for (cnt = 0; !i2cerr && !got_pwr && cnt < max_cnt; cnt++)
	{
		int reg_val = snd_soc_read(codec, REGDAC_GPI1);
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

// init the cs8416 receiver chip operating mode through its i2c bus
static int jedac_mode_init(struct snd_soc_codec *codec)
{
	struct jedac5_codec_priv *priv;
    char chan = 3; // default input channel from 0 .. 3
    char reg_chan;
	int i2cerr = 0;
	
	pr_info("jedac5_bcm: jedac_mode_init(codec=%p)\n", (void *)codec);
	if (!codec)
			return -99;

#ifdef CS8416_SWMODE
    reg_chan = 0x80 | (chan << 3) | chan;
    i2cerr = snd_soc_write(codec, 0x01, 0x06) // mute on err, rmck=128Fs
          || snd_soc_write(codec, 0x02, 0x05) // set RERR (is UNLOCK) to gpo0 output
          || snd_soc_write(codec, 0x05, 0x80) // 24-bit data, left justified,
          || snd_soc_write(codec, 0x06, 0x10) // unmask the LOCK error
          || snd_soc_write(codec, 0x04, reg_chan); // select input and start RUN mode
#else
	reg_chan = GPO0_POWERUP | GPO0_SPIMASTER;
	i2cerr = snd_soc_write(codec, REGDAC_GPO0, reg_chan);
#endif
	
	priv = snd_soc_codec_get_drvdata(codec);
	if (priv) {
		priv->chan_select = chan; // Hmm... not used, rely on kcontrol structs in bcm??
	}

	return i2cerr;
}

static void jedac_i2c_set_volume( unsigned short att_l, unsigned short att_r)
{
	// vol_? values are attenuation in dBs: 0 is max volume, 79 is min volume, 80 is mute

	int id, i2cerr, reg_att;
	struct snd_soc_component* component = NULL;
	struct snd_soc_codec *codec = get_rtd_codec();
	
    int enable_20dB_att = (att_l >= 20)
                       && (att_r >= 20);
    int mute = (att_l >= DAC_max_attenuation_dB)
	        && (att_r >= DAC_max_attenuation_dB);
	
	pr_info("jedac5_bcm: jedac_i2c_set_volume(att_l=%d att_r=%d)\n",
		(int)att_l, (int)att_r);
	
	if (!codec) {
		pr_info("jedac5_bcm: jedac_i2c_set_volume: num_rtd=%d, rtd_codec=%p\n",
			jedac5_sound_card.num_rtd, (void *)codec);
		return;
	}

    // adjust the digital volume attenuation for the 20dB switch if not totally silent
    if (enable_20dB_att && !mute) {
            att_l -= 20; // raise digital (dac) volume
            att_r -= 20; // raise digital (dac) volume
    }
	
#ifdef CS8416_SWMODE
	i2cerr = snd_soc_write(codec, REGDAC_control3, enable_20dB_att?0xc0:0x00);
#else
	i2cerr = snd_soc_write(codec, REGDAC_GPO1, enable_20dB_att?GPO1_ATT20DB:0);
#endif
    if (i2cerr) {
            pr_info("jedac5_bcm: jedac_i2c_set_volume(): error in i2c codec write!\n");
            // continue further operation...
    }
	
	// the pcm1792 dacs are used in dual-mono mode:
	// write the volume to each of both codecs
	id = 0;
	list_for_each_entry(component, &jedac5_sound_card.aux_comp_list, list) {
		unsigned short att;
		if (!component)
			continue;
		if (0 == strcmp(component->name, "pcm1792a_l")) {
			att = att_l;
			id++;
		} else if (0 == strcmp(component->name, "pcm1792a_r")) {
			att = att_r;
			id++;
		} else {
			continue;
		}

		reg_att = (255 - 2*att) & 0xff;
		i2cerr = snd_soc_component_write(component, PCM1792A_DAC_VOL_LEFT, reg_att) ||
			     snd_soc_component_write(component, PCM1792A_DAC_VOL_RIGHT, reg_att);
		if (i2cerr) {
			pr_info("jedac5_bcm: jedac_i2c_set_volume(): error in i2c write!\n");
			return;
		}
	}
	if (id != 2) {
		pr_info("jedac5_bcm: jedac_i2c_set_volume(): cannot access two pcm1792 components!\n");
        if (component)
			pr_info("jedac5_bcm: jedac_i2c_set_volume(): saw component name = \"%s\"\n",
				component->name);
		return;
	}

	pr_info("jedac5_bcm: jedac_i2c_set_volume(%d, %d) OK!\n", att_l, att_r);
}

static int jedac_i2c_set_i2s(int samplerate)
{
	struct snd_soc_codec *codec = get_rtd_codec();
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
	
	gpo_val = GPO0_POWERUP | GPO0_SPIMASTER | (freq_base << 1) | (freq_mult << 2);
	i2cerr = snd_soc_write(codec, REGDAC_GPO0, gpo_val);
	// as test, read-back fpga status byte
	gpo_val = snd_soc_read(codec, REGDAC_GPI0);
	if (gpo_val < 0) // values >=0 indicate valid read, no error
		i2cerr = gpo_val;
		
	pr_info("jedac5_bcm: jedac_i2c_set_i2s: read GPI=0x%02x. i2c err=%d\n", (int)(gpo_val & 0xff), i2cerr);
	//if (i2cerr) {
	//	pr_info("jedac5_bcm: jedac_i2c_set_i2s: read GPI=0x%02x. i2c err=%d!\n", i2c_err);
	//}

	return i2cerr;
}

/*****************************************************************************/
MODULE_AUTHOR("Jos van Eijndhoven <jos@vaneijndhoven.net>");
MODULE_DESCRIPTION("ASoC driver for JvE DAC soundcard");
MODULE_LICENSE("GPL v2");