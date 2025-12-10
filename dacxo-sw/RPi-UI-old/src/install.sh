#!/bin/bash -x
cp dacxo.service /lib/systemd/system/
systemctl daemon-reload
sudo systemctl enable dacxo.service
