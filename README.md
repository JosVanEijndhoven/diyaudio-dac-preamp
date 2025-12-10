# Audio DAC control and UI

## Introduction

This repository provides the control and user-interface software for my home-brewn audio DAC v5.
This DAC can work as digital preamplifier, by providing 4 digital inputs and volume control.
The digital inputs are optical and coax(spdif),
with support for 44.1kHz and 48kHz sample rates and the x2 and x4 multiples thereof.

This 'control and UI' is provided to run on a [Lilygo T-Display-s3](https://lilygo.cc/en-ca/products/t-display-s3)
board, with an additional [rotary encoder](https://esphome.io/components/sensor/rotary_encoder.html)
knob for volume control and press-click support for input channel selection and power on/off.
As this implementation is based on [esphome](https://esphome.io/),
it also provides its UI remotely (through wifi)
in a [home assistant](https://www.home-assistant.io/) context on -for instance- a mobile phone or tablet.
By slightly modifying the provided yaml configuration file, other control/display boards are easily allowed.

## Contents

The `dac.yaml` provided in the top directory is the main file,
providing the configuration from which `esphome` creates the binary image to be downloaded
in the Lilygo board.
To allow a somewhat more concise configuration, new esphome 'components' are provided for the pcm1792 dac chips.
They reside in the `components/pcm1792_i2c` subdirectory in this repo. Their C++ files are included
in the code build process, through the `external_components` directive in the yaml file.

## How to build

Building the controller binary and uploading the binary image in the controller board
can be done with the standard mechanisms as provided by [esphome](https://esphome.io/).
Personally, I use a local installation of `esphome` and its command-line interface from a Linux shell.
In that environment, the build and upload works like:
```
cd <top level directory of this repo>
source <esphome install dir>/venv/bin/activate
esphome run --device /dev/ttyACM0 dac.yaml
```

## License
All source and configuration files provided in this repository are provided without any warrenty,
and under copyright and license:

Copyright 2025 Jos van Eijndhoven

[GNU General Public License v3](https://www.gnu.org/licenses/gpl-3.0.nl.html)

