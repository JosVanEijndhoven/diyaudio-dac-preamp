#!/usr/bin/python3

# First version inspired by https://github.com/akkana/scripts/blob/master/rpi/pyirw.py
# That one says:
# Read lirc output, in order to sense key presses on an IR remote.
# There are various Python packages that claim to do this but
# they tend to require elaborate setup and I couldn't get any to work.
# This approach requires a lircd.conf but does not require a lircrc.
# If irw works, then in theory, this should too.
# Based on irw.c, https://github.com/aldebaran/lirc/blob/master/tools/irw.c

import socket
import threading
import logging

class IrRecv:
    def __init__(self, cbVolumeUp, cbVolumeDown, cbPowerToggle):
        self.sockPath = "/var/run/lirc/lircd"
        logging.info('IrRecv start listening on socket: ' + self.sockPath)
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.connect(self.sockPath)
        self.thrd = None
        self.cbVolUp = cbVolumeUp
        self.cbVolDown = cbVolumeDown
        self.cbPwrToggle = cbPowerToggle
        self.thrd = threading.Thread(target=self.handleKeys, args=(self,))
        self.thrd.start()

    def handleKeys(self, i):
        while True:
            data = self.sock.recv(128)
            data = data.strip()
            logging.info('IrRecv data: ' + str(data))
            cmd = data.split()
            if cmd[2] == b'KEY_VOLUMEUP' or cmd[2] == b'KEY_UP':
               self.cbVolUp()
            elif cmd[2] == b'KEY_VOLUMEDOWN' or cmd[2] == b'KEY_DOWN':
                self.cbVolDown()
            elif cmd[2] == b'KEY_POWER' and cmd[1] == b'00':  # only when repeat count is 0
               self.cbPwrToggle()


