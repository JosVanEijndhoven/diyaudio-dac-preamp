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

#include <linux/regmap.h>

#define PCM1792A_DAC_VOL_LEFT	0x10
#define PCM1792A_DAC_VOL_RIGHT	0x11
#define PCM1792A_FMT_CONTROL	0x12
#define PCM1792A_MODE_CONTROL	0x13
#define PCM1792A_SOFT_MUTE	PCM1792A_FMT_CONTROL

#define PCM1792A_FMT_MASK	0x70
#define PCM1792A_FMT_SHIFT	4
#define PCM1792A_MUTE_MASK	0x01
#define PCM1792A_MUTE_SHIFT	0
#define PCM1792A_ATLD_ENABLE	(1 << 7)


#define PCM1792A_RATES (SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_8000_48000 | \
			SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000 | \
			SNDRV_PCM_RATE_192000)

#define PCM1792A_FORMATS (SNDRV_PCM_FMTBIT_S32_LE | SNDRV_PCM_FMTBIT_S24_LE | \
			  SNDRV_PCM_FMTBIT_S16_LE)

//extern const struct regmap_config pcm1792a_regmap;

//int pcm1792a_probe(struct device *dev, struct regmap *regmap);
//void pcm1792a_remove(struct device *dev);

#endif
