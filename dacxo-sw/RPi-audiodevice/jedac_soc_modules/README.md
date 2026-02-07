# Audio device driver

## Introduction

The DAC design initially/traditionally focussed on using the four s/pdif inputs on its digital board.
However, that board also provides a fifth digital audio input: an i2s input intended to connect
an audio network streaming device in the form of a Raspberry Pi.
I chose a tiny 'Raspberry Pi zero 2w' for this purpose.

Connecting a Raspberry Pi through its i2s interface (in this design) keeps the DAC as
master clock, and uses the Pi as clock slave on this interface:
- This has the advantage of a clean clock with minimal jitter without any clock synchronization issues,
  in contrast with using the classic s/pdif inputs.
- It requires that -per song- the Pi informs the DAC board on the required sample-rate.
So, where the i2s interface is used to pass the audio data, the Pi also uses an i2c bus connection
to the DAC board to communicate on the sample-rate.
In practice, it also enables the Pi to control the audio volume, input selection, and power-state,
and use the Pi as full-featured audio streamer.

For the Pi to obtain such control, it needs an audio device driver.
Such audio device driver is provided in this directory.

## Contents
- Creating the development setup
- Building and installing the device driver
- Initial testing of the device driver
- Activating the device driver on boot
- Choosing the DAC as system default audio device
- Using an audio network streaming application

## Creating the development setup
To build the device driver kernel modules, I opted (and would recommend)
for the simplicity of doing that on the target Raspberry Pi itself.
This as opposed to a more elaborate setup of a Pi cross-development environment
on another desktop computer.
Building and installing the kernel modules is highly dependent on the
precise version of the Linux kernel. Local development avoids a potential mismatch.
Also, after an OS update which increments the kernel version, these modules
can be easily rebuilt. 

The start environment is the plain [64-bit Rasberry Pi OS](https://www.raspberrypi.com/software/operating-systems/).
The *Lite* version, without desktop environment, is fine: we will only use
the Linux command-line [through an 'ssh' connection](https://www.raspberrypi.com/documentation/computers/remote-access.html#ssh).
Optionally, it might also be convenient to [share your Pi home directory](https://www.raspberrypi.com/documentation/computers/remote-access.html#samba) with your desktop.


The Raspberry Pi OS comes with `gcc` and the required `include` files pre-installed, so we don't need to install that.





