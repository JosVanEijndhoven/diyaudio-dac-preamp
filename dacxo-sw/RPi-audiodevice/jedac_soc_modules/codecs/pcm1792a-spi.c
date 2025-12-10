/*
 * PCM1792A ASoC codec driver
 *
 * Copyright (c) Amarula Solutions B.V. 2013
 *
 *     Michael Trimarchi <michael@amarulasolutions.com>
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
#include <linux/spi/spi.h>

#include "pcm1792a.h"


static int pcm1792a_spi_probe(struct spi_device *spi)
{
	struct regmap *regmap;
	int ret;

	regmap = devm_regmap_init_spi(spi, &pcm1792a_regmap);
	if (IS_ERR(regmap)) {
		ret = PTR_ERR(regmap);
		dev_err(&spi->dev, "Failed to register spi regmap: %d\n", ret);
		return ret;
	}

	return pcm1792a_probe(&spi->dev, regmap);
}

static int pcm1792a_spi_remove(struct spi_device *spi)
{
	pcm1792a_remove(&spi->dev);
	return 0;
}

static const struct spi_device_id pcm1792a_spi_ids[] = {
	{ "pcm1792a", 0 },
	{ },
};
MODULE_DEVICE_TABLE(spi, pcm1792a_spi_ids);

static const struct of_device_id pcm1792a_of_match[] = {
	{ .compatible = "ti,pcm1792a", },
	{ }
};
MODULE_DEVICE_TABLE(of, pcm1792a_of_match);

static struct spi_driver pcm1792a_spi_driver = {
	.driver = {
		.name = "pcm1792a",
		.owner = THIS_MODULE,
		.of_match_table = pcm1792a_of_match,
	},
	.id_table = pcm1792a_spi_ids,
	.probe = pcm1792a_spi_probe,
	.remove = pcm1792a_spi_remove,
};

module_spi_driver(pcm1792a_spi_driver);

MODULE_DESCRIPTION("ASoC PCM1792A driver - SPI");
MODULE_AUTHOR("Michael Trimarchi <michael@amarulasolutions.com>");
MODULE_LICENSE("GPL");
