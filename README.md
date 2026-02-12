# DACXO: Audio DAC and digital pre-amp with low jitter

![overview photo](images/dac5-overview-850.jpg)

## Introduction

This [github repository](https://github.com/JosVanEijndhoven/diyaudio-dac-preamp) 
provides all HW and SW sources of my DIY audio DAC design.
The 'audio Digital-to-Analog Conversion' is the heart and main aim of the design.
However, more audio device functionality was built around that:

- Functionality for a digital pre-amp is provided, with input selection
  and volume control. Inputs are optical and coax s/pdif, HDMI-arc, and a network DLNA audio player.
- The control UI (user interface) is created with [esphome](https://esphome.io/).
  Besides the front panel control knob, this also provides remote control from a mobile phone
  and/or integration in [home assistant](https://www.home-assistant.io/) automation environments.
- The core DAC design aims for high-end audio, with a focus on low jitter, not
  aiming to reach low component cost.

The intention of this repository is to show some solutions across a wide range of design choices,
potentially of interest to other DIY audio enthousiasts.
Supporting purely cloning the full hw/sw design is not the intention,
as some design files might be outdated for the various tools and components that were used:
the design start was around 2015. (But its development still continous in 2026...)
This DAC design builds on previous experience: it is already my 5th design.

Only in december 2025 I started to upload my design in github, and thereby make it public visible.
I still have to spend further effort in documenting the design and its choices.

The name **DACXO** is related to:
- Its good **X**tal **O**scillator and low-jitter clocking.
- Its **X**tra **O**riginal clock synchronization scheme on the s/pdif inputs.
- The now **X** [years](https://en.wikipedia.org/wiki/Roman_numerals) **O**ld circuit design when creating this open-source repository.
- As bonus, the domain name [dacxo](http://www.dacxo.com) was available.

## Contents

This repository contains all hardware and software design aspects:

- The [front control and remote UI software](./esphome-ui/) based on a commercial *esp32-s3* board with display.
- The [digital signal processing board](./dacxo-hw/PCBs/PCB-dacxo/), schematics and PCB design.
- The [logic configuration of the FPGA](./dacxo-hw/FPGA-content/) on that board,
- The [analog output board](./dacxo-hw/PCBs/PCB-opampout/), schematics and PCB design,
- Addition of a Raspberry Pi as [network media player](./dacxo-sw/), with i2s output to the DAC,
  including a dedicated [Linux kernel audio device driver](./dacxo-sw/RPi-audiodevice/).

## Focus on Jitter

The main focus on this design is having good behavior on jitter (that is, achieve very low jitter).
Already for decades now, jitter is known to badly affect the sound quality of digital audio reproduction.
Unfortunately, achieving low jitter makes a device more expensive, and measuring jitter behavior requires expensive specialised equipment.
As result, many digital audio consumer products still show bad jitter behavoir.

The audible effects of jitter in a digital audio player are:

- Weak, uncontrolled, bass
- Unfocussed and unstable stereo imaging
- Unpleasantly sharp (metal-sounding) highs

Because many (cheaper) audio consumer devices behave badly in this respect,
the old vinyl record playing has risen again in popularity.
Luckily, good digital audio players don't show these artifacts,
and can sound really natural and convincing!

Presumably in large part due to jitter effects, this DAC design sounds significantly better than for instance the
[Cambridge Audio EXN100 player](https://www.cambridgeaudio.com/row/en/products/ex/exn100) which I bought early 2025, 
which costs around 2000 US$. (Just in my opinion and some of my audio-loving friends.)

For further background information on jitter in digital audio, you can easily find many sources on internet.

As a final note, jitter artefacts do not only occur in audio DAC (digital-to-analog conversion),
but similarly in analog-to-digital conversion (ADC).
So, a poor-quality studio recording (or home recording)
will introduce jitter artefacts in the digital audio stream.
Once in there, these jitter artefacts cannot be removed anymore:
a bad recording will sound bad forever. 
A good low-jitter player cannot improve a bad recording.
Unfortunately, it seems to me that there are many bad-quality recordings around...

## License
All source and configuration files provided in this repository are provided without any warrenty,
and under copyright and license:

Copyright 2025 Jos van Eijndhoven

[GNU General Public License v3](https://www.gnu.org/licenses/gpl-3.0.nl.html)

