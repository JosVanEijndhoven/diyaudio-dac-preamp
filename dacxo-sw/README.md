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
its description. That description is fine: although it mentions
*gstreamer1.0* in some of its package names, it actually
instals the recent versions of that.

Created *gmedia* user to run the daemon:
```
sudo adduser --system --ingroup render --no-create-home gmedia
sudo usermod -a -G audio gmedia
sudo usermod -a -G i2c gmedia
sudo usermod -a -G gpio gmedia
```

Edit the `scripts/init.d/gmediarender` file, modifying a few lines such as:
```
DAEMON_USER="gmedia:audio"
UPNP_DEVICE_NAME="MyDac"
ALSA_DEVICE="plughw:DACXO"
start-stop-daemon -x $BINARY_PATH ... --mime-filter audio
```
(On the long `start-stop-daemon` line, add the `--mime-filter audio` option
restricting this *dlna renderer* to only play audio, not video.)

Then, install it with:
```
sudo cp scripts/init.d/gmediarenderer /etc/init.d
sudo update-rc.d gmediarenderer defaults
```

## System load for playing audio streams over wifi

My small *Rapsberry Pi zero 2W* seems just fine and powerful enough
for this task. For example, a *high resolution* FLAC-encoded stream can easily be
sent to this *DLNA renderer*.
A *96K* samplerate and *24-bits* per sample stereo FLAC stream has
a variable bandwidth of roughly 400KBytes/sec. For such a stream,
the renderer uses about 7% cpu load (on 1 of the 4 available cores)
and takes about 25% (or 125MB) of the available 512MB memory.
