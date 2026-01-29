/*
 * Driver for the 5th generation DAC by Jos van Eijndhoven
 *
 * Copyright 2016 Jos van Eijndhoven
 * jos@vaneijndhoven.net
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
 
#ifndef _JEDAC_H
#define _JEDAC_H

#define DAC_IS_CLK_MASTER 1

#ifdef DAC_IS_CLK_MASTER
#define JEDAC_RATES (SNDRV_PCM_RATE_44100  | SNDRV_PCM_RATE_48000 |\
			          SNDRV_PCM_RATE_88200  | SNDRV_PCM_RATE_96000 |\
			          SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_192000)
					  // i2s left justified output, rpi is clock slave
// Hmm... I prefer SND_SOC_DAIFMT_LEFT_J however,
//        only SND_SOC_DAIFMT_I2S is now implemented in soc/bcm/bcm2708-i2s.c :-(
// see for repair: https://github.com/humppe/spdif-encoder/blob/master/spdif-hack.patch
#define JEDAC_DAIFMT (SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CONT |\
					  SND_SOC_DAIFMT_NB_NF  | SND_SOC_DAIFMT_CBP_CFP)
#else
#define JEDAC_RATES (SNDRV_PCM_RATE_44100  | SNDRV_PCM_RATE_48000 |\
			          SNDRV_PCM_RATE_88200  | SNDRV_PCM_RATE_96000)
#define JEDAC_DAIFMT (SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CONT |\
					  SND_SOC_DAIFMT_NB_NF  | SND_SOC_DAIFMT_CBS_CFS)
#endif
					  
#define JEDAC_FORMATS (SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S16_LE)
//#define JEDAC_FORMATS (SNDRV_PCM_FMTBIT_S32_LE)

#define DAC_max_attenuation_dB 80
#define DAC_step_attenuation_dB 1

// Registers in FPGA i2c interface, same device adress as the cs8416
// 'GPO*' registyers are read/write
// 'GPI*' registers are read-only
#define REGDAC_GPO0			0x30
#define REGDAC_GPO1			0x31
#define REGDAC_GPI0			0x34
#define REGDAC_GPI1			0x35
#define REGDAC_MAX			0x35

// *** bitfields in GPO0 ***
// If GPO0_CLKMASTER set, use i2s dac input, else one of the s/pdif inputs
#define GPO0_CLKMASTER		0x01
#define GPO0_BASE48KHZ		0x02
// CLKRATE: in master mode: 1:44.1 or 48, 2: 88.2 or 96, 3: 176.4 or 192kHz
//          in slave mode: input channel select 0..3
#define GPO0_CLKRATE		0x0c
// gather the above three fields in a mask that concerns clock config:
#define GPO0_CLKMASK    0x0f
#define GPO0_POWERUP		0x80   // output to Vana power relay: 1: power switched on, 0: off

// *** bifields in GPO1 ***
#define GPO1_ATT20DB		0x01

// *** bifields in GPI0 ***

// *** bifields in GPI1 ***
#define GPI1_ANAPWR			0x01   // measured Vana: 1 is 'on' (with 0.1s delay), 0 is 'off'

// GPIO pin number on RPi Zero to interact with EspHome UI controller
#define GPIO_UI_TRIG    27
		  
#endif /* _JEDAC_H */
