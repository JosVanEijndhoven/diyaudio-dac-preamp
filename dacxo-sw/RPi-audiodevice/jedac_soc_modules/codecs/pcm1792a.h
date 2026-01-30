/*
 * definitions for PCM1792A
 *
 * Copyright 2013 Amarula Solutions
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

#ifndef __PCM1792A_H__
#define __PCM1792A_H__

#define PCM1792A_DAC_VOL_LEFT	  0x10
#define PCM1792A_DAC_VOL_RIGHT	0x11
#define PCM1792A_FMT_CONTROL	  0x12
#define PCM1792A_MODE_CONTROL  	0x13
#define PCM1792A_STEREO_CONTROL 0x14
#define PCM1792A_REG_MAX        0x14
// for now, no reason to add the further pcm1792a regs

#define PCM1792A_DAC_VOL_LEFT_DEFAULT	  0xFF
#define PCM1792A_DAC_VOL_RIGHT_DEFAULT	0xFF
#define PCM1792A_FMT_CONTROL_DEFAULT	  0x50
#define PCM1792A_MODE_CONTROL_DEFAULT  	0x00
#define PCM1792A_STEREO_CONTROL_DEFAULT 0x00

#define PCM1792A_SOFT_MUTE	PCM1792A_FMT_CONTROL

#define PCM1792A_FMT_MASK	0x70
#define PCM1792A_FMT_SHIFT	4
#define PCM1792A_MUTE_MASK	0x01
#define PCM1792A_MUTE_SHIFT	0
#define PCM1792A_ATLD_ENABLE	(1 << 7)


#define PCM1792A_RATES (SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000 | \
			SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000 | \
			SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_192000)

#define PCM1792A_FORMATS (SNDRV_PCM_FMTBIT_S32_LE | SNDRV_PCM_FMTBIT_S24_LE | \
			  SNDRV_PCM_FMTBIT_S16_LE)

#endif
