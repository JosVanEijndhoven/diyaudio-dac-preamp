#!/usr/bin/python3
import smbus
import time

class Oled16x2:
    # Glyphs used to build big chars:
    # Each of these 8 glyphs will be stored in the display as custom char
    bigCharGlyphs = [
        [0x07, 0x0f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f],  # 0: GlyphLT
        [0x1f, 0x1f, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00],  # 1:   1
        [0x1c, 0x1e, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f],  # 2: GlyphRT
        [0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x0f, 0x07],  # 3:   3
        [0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x0f, 0x07],  # 4:   4
        [0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1e, 0x1c],  # 5: GlyphLR
        [0x1f, 0x1f, 0x1f, 0x00, 0x00, 0x00, 0x1f, 0x1f],  # 6: GlyphUM
        [0x1f, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x1f, 0x1f]]  # 7: GlyphLM
    # Big char defines:
    # 10 chars for digits, each defined out of 2 rows and 3 columns of glyphs or white/black chars:
    blk = ord(' ')
    wht = 0x1f
    bigChars = [
        [   0,   1,   2,   3,   4,   5],  # 0
        [ blk, wht, blk, blk, wht, blk],  # 1
        [   6,   6,   2,   3,   7,   7],  # 2
        [   6,   6,   2,   7,   7,   5],  # 3
        [   3,   4,   2, blk, blk, wht],  # 4
        [ wht,   6,   6,   7,   7,   5],  # 5
        [   0,   6,   6,   3,   7,   5],  # 6
        [   1,   1,   2, blk,   0, blk],  # 7
        [   0,   6,   2,   3,   7,   5],  # 8
        [   0,   6,   2, blk, blk, wht],  # 9
    ]

    busAddr   = 0x3c  # find devices from: i2cdetect -y 1
    cmdMode   = 0x80
    dataMode  = 0x40
    CGRamAddr = 0x40
    DDRamAddr = 0x80

    def __init__(self, i2cBusNum):
        self.bus = smbus.SMBus(i2cBusNum)  # 0 = /dev/i2c-0 (port I2C0), 1 = /dev/i2c-1 (port I2C1)
        self.send_cmd(0x2a)  # **** Set "RE"=1	00101010B
        self.send_cmd(0x71) and self.send_data([0x5c])  # Enable internal Vdd regulator
        self.send_cmd(0x28)
        self.send_cmd(0x08)  # **** Set Sleep Mode On
        self.send_cmd(0x2A)  # **** Set "RE"=1	00101010B
        self.send_cmd(0x79)  # **** Set "SD"=1	01111001B
        self.send_cmd(0xD5)
        self.send_cmd(0x70)
        self.send_cmd(0x78)  # **** Set "SD"=0  01111000B
        self.send_cmd(0x08)  # **** Set 5-dot, 3 or 4 line(0x09), 1 or 2 line(0x08)
        self.send_cmd(0x06)  # **** Set Com31-->Com0  Seg0-->Seg99
        self.send_cmd(0x72) and self.send_data([0])  # **** Set ROM A and 8 CGRAM
        self.send_cmd(0x2A)  # **** Set "RE"=1
        self.send_cmd(0x79)  # **** Set "SD"=1
        self.send_cmd(0xDA)  # **** Set Seg Pins HW Config
        self.send_cmd(0x10)
        self.send_cmd(0x81)  # **** Set Contrast
        self.send_cmd(0xFF)
        self.send_cmd(0xDB)  # **** Set VCOM deselect level
        self.send_cmd(0x30)  # **** VCC x 0.83
        self.send_cmd(0xDC)  # **** Set gpio - turn EN for 15V generator on.
        self.send_cmd(0x03)
        self.send_cmd(0x78)  # **** Exiting Set OLED Characterization
        self.send_cmd(0x28)
        self.send_cmd(0x2A)
        self.send_cmd(0x06)  # **** Set Entry Mode
        self.send_cmd(0x08)
        self.send_cmd(0x28)  # **** Set "IS"=0 , "RE" =0 #28
        self.send_cmd(0x01)  # **** Clear display
        time.sleep(0.020)
        # install the glyphs as 8 new chars
        for inx in range(8):
            self.create_char(inx, self.bigCharGlyphs[inx])
        self.send_cmd(0x0c)  # **** Turn on Display
        return

    def create_char(self, inx, pixels):
        self.send_cmd(self.CGRamAddr + inx * 8)
        self.send_data(pixels)

    def show_data(self, row, col, data):
        inx = col + row * 0x40  # row is 0 or 1
        self.send_cmd(self.DDRamAddr + inx)
        self.send_data(data)
        return

    def show_string(self, row, col, word):
        self.show_data(row, col, [ord(char) for char in word])

    def show_bigdigit(self, col, digit):  # 0 <= digit <= 9 
        self.show_data(0, col, self.bigChars[digit][0:3])
        self.show_data(1, col, self.bigChars[digit][3:6])

    def clear(self):
        self.send_cmd(0x01)  # **** Clear display

    def send_cmd(self, cmd):  # single byte cmd
        self.bus.write_byte_data(self.busAddr, self.cmdMode, cmd)

    def send_data(self, data):  # list of data
        self.bus.write_i2c_block_data(self.busAddr, self.dataMode, data)

    def test(self):
        print('Hello from oled.test!')
        self.show_bigdigit(0,2)
        self.show_bigdigit(3,8)
        self.show_string(0, 7, 'hello!')
        self.show_string(1, 6, '0123456789')
        print('Done')

if __name__== "__main__":
    oled = Oled16x2(1)
    oled.test()
