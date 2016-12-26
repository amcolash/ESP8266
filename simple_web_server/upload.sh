#!/bin/sh

export port=/dev/ttyUSB0
export baud=115200

cp init.lua init.lua.bak
cp web_server.lua web_server.lua.bak

lstrip init.lua
lstrip web_server.lua

../luatool/luatool/luatool.py --wipe --verbose --port $port --baud $baud
../luatool/luatool/luatool.py --src init.lua --verbose --port $port --baud $baud
../luatool/luatool/luatool.py --src web_server.lua --verbose --port $port --baud $baud

rm *.lua
mv init.lua.bak init.lua
mv web_server.lua.bak web_server.lua

exit
