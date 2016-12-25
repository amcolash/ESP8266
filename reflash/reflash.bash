#!/bin/bash -x

export port=/dev/ttyS11
export baud=115200

#esptool.py --port $port erase_flash
esptool.py --port $port write_flash 0x000000 nodemcu-master-18-modules-2016-12-05-04-43-57-float.bin 

exit
