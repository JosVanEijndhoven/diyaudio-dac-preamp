#!/usr/bin/python3
from gpiozero import Button

class RotaryKnob:
    def __init__(self, pinVolA, pinVolB, pinSwitch, cbTurnR, cbTurnL, cbPress, cbRelease):
        # arg has 3 pin names
        self.volA = Button(pinVolA)
        self.volB = Button(pinVolB)
        self.switch = Button(pinSwitch)
        self.cbTurnR = cbTurnR
        self.cbTurnL = cbTurnL
        self.cbPress = cbPress
        self.cbRelease = cbRelease
        self.volA.when_pressed = self.volANegedge
        self.switch.when_pressed = self.cbPress
        self.switch.when_released = self.cbRelease
    
    def volANegedge(self):
        if self.volB.is_pressed:
            self.cbTurnL()
        else:
            self.cbTurnR()

