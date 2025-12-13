# Analog output stage
## Introduction
This PCB provides a textbook style opamp-based analog output to accompany the digital PCB.
This PCB was designed to function as basic reference (starting point) for other
more high-end analog audio alternatives.

The main functionality of this board is:

- Implement the current-to-voltage conversion that is needed for the (current output of) the PCM1792a DAC chips.
- Create stereo balanced output on a pair of XLR connectors,
  next to a conventional (single-ended) cinch connector.

## Analog opamp output circuitry
For analog signal handling, the board currently uses LME49720 type dual operational amplifiers.
These are low-noise, low distortion, and low offset error opamps, intended for audio applications.

The first stage of opamps (U202 and U204 in the schematic) are used for current-to-voltage conversion.
The second stage of opamps (U201 and U203) are used for differential amplification
of the positive and negative signal outputs of the DAC chips.
This differential amplification is used for both:

- Filtering out common-mode distortion and power-supply artefacts, resulting in clean audio output.
- Filtering out the DC offset that is part of the PCM1792A output current.

To achieve good common-mode and DC suppression, it is needed that both of these stages
are accurately balanced. For that purpose, this implementation uses 0.1% accurate resistors
to set the opamp gains. Furthermore, the PCB provides empty posistions to mount extra resistors in
parallel to the 'main' resistors: this allows optional fine-tuning of the balancing,
if the initially obtained balance would not be satisfactory.
For now, these extra resistor positions are not mounted.

Due to accurate balancing, the analog oiutp[uts can be DC coupled to a power amplifier,
which avoids potential signal disturbance from an output series capacitor.]

For resistor type, the design uses MELF 'MMA 0204' professional grade Vishay Beyschlag resistors.
These resistors have a decent *current noise* specification in their datasheet.
I would recommend to not buy any alternative resistor types for high-end audio
that are not provided with a specification of their (current-) noise.
In other designs I have preferred their larger 'MMB 0207' versions,
as these have a significantly better noise specification.
Unfortunately, at that time, I could not easily buy those larger versions with a 0.1% accuracy specification.

## High-frequency filtering

The output of the DAC chips contains significant high-frequency content which should better be filtered away
to not disturb the power amplifier:
- audio signal frequency components around multiples of the samplerate.
- high-frequency switching artefacts

To suppress those:

- The DAC current outputs have a small capacitor to ground on the digital PCB.
- each of the audio signal wires between the digital board and this analog board has a small series-inductor,
  and a resistor in parallel with that inductor. (This is not shown in the board schematics.)
  Together with the above capacitor that forms an RLC 2nd order
  filter, preventing most high-frequency components to enter the analog board.
- The first-stage 'current-to-voltage' opamps have a feedback capacitor to filter out the high-frequency spectrum.
- Just before the analog output connector, there is another RLC 2nd order filter.

The adopted opamp type has a significant high-frequency bandwidth. That is needed to
properly filter out these high frequencies without overdriving their differential inputs.

## Attenuation relay

The analog board has a '-20dB' attenuation relay for all its output audio signals.
This relay is controlled from the digital board:
its control signal is named *ext1* in the digital pcb schematics, and originates from a status register in the FPGA.

When the volume control of the DAC board is used, together with input selection,
this DAC takes the role of 'digital pre-amplifier'.
The output of the DAC would be directly wired to a power amplifier.
In such setup, the volume setting would most of the time be rather low, mostly not playing close to max volume.
When only the DAC chip digital volume would be used, that means that most of the time quite a number of
'high' (most significant) bits of the DAC conversion would not be used. That would effectively
reduce the number of bits (the resolution) of the DA conversion.

To alleviate this effect, the '-20dB' relay reduces volume on the analog output, allowing a higher volume on the DAC.
This relay is not under explicit user control: the microcontroller automatically uses this relay
for volume settings which are further then 20dB below max volume.

## Power supply

The power supply for the audio opamps is designed for very low noise and good ripple rejection.
The 'LM317' regulators are used as constant-current-source to feed a 7.5V zener diode.
Together that results in a very high PSRR (power supply rejection ratio).
The relatively large zener diode shows a lower dynamic resistance and lower noise voltage.
The zener noise voltage is low-frequency filtered by an RC-stage, and then amplified/buffered by the same
good quality audio opamp. Note that this opamp has a far higher PSRR then the LM317.
Four seperate regulated power supply voltages are made for positive and negative voltage,
and for the left and right audio channels.

To not exceed the opamp voltage operating conditions, the positive-voltage stabilization is done with
opamps that are powered from 0 to +24V.
Correspondingly, the negativevoltage stabilization is done with opamps that are powered from 0 to -24V.

The power supply input comes from a 2x15V transformer which is not mounted on the PCB.
This transfomer is switched on/off together with the digital PCB 'analog' power transformer,
through relay 'K301' in this schematics. As with the attenuation relay, this power relay is also
controlled from the digital PCB, signal named *ext2* in the digital pcb schematics,
originating from a status register in the FPGA.
This status register is controlled by the microcontroller software through the i2c bus.

