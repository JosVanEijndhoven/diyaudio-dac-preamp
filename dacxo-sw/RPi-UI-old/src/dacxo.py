#!/usr/bin/python3
from oled16x2 import Oled16x2
from rotaryknob import RotaryKnob
from irrecv import IrRecv
import time
import threading
import sys
import logging

class DacVolume:
    volume = 0
    maxVolume = 29
    display = None

    def __init__(self, display):
        self.display = display
    
    def up(self):
        if self.volume >= self.maxVolume:
            return
        self.volume += 1
        self.show()
    
    def down(self):
        if self.volume <= 0:
            return
        self.volume -= 1
        self.show()
    
    def show(self):
        self.display.show_string(1, 0, "{:02d}".format(self.volume))

class DacXo:
    power = 0
    volume = None
    oled = None
    knob = None
    pressTimer = None
    irrecv = None

    def __init__(self):
        self.oled = Oled16x2(1)
        self.oled.show_string(0, 0, 'hello!')
        self.oled.show_string(1, 0, str(self.volume))
        self.oled.show_string(1, 10, '0')
        self.knob = RotaryKnob("GPIO27", "GPIO22", "GPIO23",
                               self.cbTurnR, self.cbTurnL,
                               self.cbPress, self.cbRelease)
        self.volume = DacVolume(self.oled)
        self.irrecv = IrRecv(self.volume.up, self.volume.down,
                             self.powerToggle)

    def cbIrPress(self):
        pass

    def cbTurnR(self):
        if not self.power:
            return
        if not self.pressTimer is None:
            self.pressTimer.cancel()
        self.volume.up()
    
    def cbTurnL(self):
        if not self.power:
            return
        if not self.pressTimer is None:
            self.pressTimer.cancel()
        self.volume.down()

    def cbPress(self):
        if not self.power:
            self.powerUp()
            return
        self.oled.show_string(1, 10, '1')
        self.pressTimer = threading.Timer(1.0, self.powerDown)
        self.pressTimer.start()

    def cbRelease(self):
        if not self.power:
            return
        if not self.pressTimer is None:
            self.pressTimer.cancel()
        self.oled.show_string(1, 10, '0')

    def powerUp(self):
        self.power = 1
        self.oled.clear()
        self.oled.show_string(0, 0, "Hello!")
        self.volume.show()

    def powerDown(self):
        self.power = 0
        self.oled.clear()
        self.oled.show_string(1, 15, '.')

    def powerToggle(self):
        if self.power:
            self.powerDown()
        else:
            self.powerUp()

    def run(self):
        try:
            self.powerUp()
            time.sleep(60 * 60 * 24 * 7)
        except:
            msg = str(sys.exc_info()[0])
            print(msg)
            self.oled.clear()
            self.oled.show_string(0, 0, msg[0:16])
            self.oled.show_string(1, 0, msg[16:32])
        else:
            self.powerDown()

if __name__== "__main__":
    logging.basicConfig(filename='/var/log/dacxo.log', level=logging.INFO)
    logging.info('Started')
    dac = DacXo()
    dac.run()
    logging.info('Terminating')