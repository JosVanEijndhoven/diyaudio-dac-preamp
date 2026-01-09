# Audio stream DLNA renderer to the DAC

## Introduction

To stream audio to the DAC over wifi, a 'DLNA' network
interface is created.
This is realised with a tiny
[Raspberry Pi zero 2W](https://www.raspberrypi.com/products/raspberry-pi-zero-2-w/).

For streaming audio,
(DLNA renderer)[https://github.com/hzeller/gmrender-resurrect] software is installed. This allows other application to send audio,
in particular [BubbleUpnp](https://play.google.com/store/apps/details?id=com.bubblesoft.android.bubbleupnp),
or maybe [VLC](https://github.com/videolan/vlc).

The received audio is passed to the DAC board through an *i2s* interface.
[i2s](https://en.wikipedia.org/wiki/I2S) in general allows a configuration choice whether the
sender or the receiver provides the clock signal.
Here the DAC provides its 'master' clock,
which means that there is no synchronization issue.
That is a major advantage over using s/pdiff interfaces.

## Installing the DLNA renderer

The (gmrender-resurrect)[https://github.com/hzeller/gmrender-resurrect]
software is installed on the *raspberry Pi zero 2w* according to
its description. That desfription is fine: although it mentions
*gstreamer1.0* in some of its package names, it actually
instals the recent versions of that.

Created *gmedia* user to run the daemon:
```
sudo adduser --system --ingroup render --no-create-home gmedia
sudo usermod -a -G audio gmedia
sudo usermod -a -G i2c gmedia
sudo usermod -a -G gpio gmedia
```

Edit the `scripts/init.d/gmediarender` file:

Then, install it with:
```
sudo cp scripts/init.d/gmediarenderer /etc/init.d
sudo update-rc.d gmediarenderer defaults
```

## The Pi Linux ALSA device driver

A dedicated Linux kernel-level device driver is developed, so that the DAC board
becomes available as a true ALSA audio device.

- It configures the Pi *i2s* interface as clock slave, for both the
bit-clock and the word-clock (frame). This setting avoids clock synchronization issues with
the DAC high-quality crystal oscillators.
- Per audio stream, it writes its sample rate to the DAC
fpga, so that suitable clock frequencies are generated.

### Building and installing the ALSA device driver on Raspberry Pi

### ALSA driver source code

The device driver sources consists of various files:

1. `bcm/jedac5_bcm.c`: This is 'board control module', it specifies that there is an *i2c* connection to the board, to control the FPGA and the two pcm1792 dac chips.
It also specifies the *i2s* audio data interface.
2. `codecs/jedac5_codec.c`: It implements the dac control
3. `codecs/jedac5.h`: constants regarding the codec, also passed to the `jedac5_bcm.c`.
4. `codecs/pcm1792a.h`: constants to drive the pcm1792a on-chip registers.
   This should better have been part of the `pcm179x` kernel driver source code.

Some notes on this software:

Around [jedac5_bcm.c:266](jedac_soc_modules/bcm/jedac5_bcm.c#L266) the *i2s* interface mode
is set to `JEDAC_DAIFMT`. This is defined in `codecs/jedac5.h` with (among others)
the value of `SND_SOC_DAIFMT_CBP_CFP`. This configures the Pi *i2s* interface
as clock slave, for both the bit-clock and the word-clock.
