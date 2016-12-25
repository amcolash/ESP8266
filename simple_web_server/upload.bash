#!/bin/bash -x

export port=/dev/ttyS11
export baud=115200

../luatool/luatool.py --wipe --verbose --port $port --baud $baud
../luatool/luatool.py --src init.lua --verbose --port $port --baud $baud
../luatool/luatool.py --src web_server.lua --verbose --port $port --baud $baud

exit
