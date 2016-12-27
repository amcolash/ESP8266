#!/bin/bash

if [[ $EUID -ne 0  ]]; then
  echo "This script must be run as root" 1>&2
  exit 1
fi

for f in *.py
do
  echo Removing old version of $f
  sudo ampy --port /dev/ttyUSB0 rm $f
  echo Sending $f
  sudo ampy --port /dev/ttyUSB0 put $f
done

echo Rebooting
sudo ampy --port /dev/ttyUSB0 run reboot.py
