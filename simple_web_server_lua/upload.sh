#!/bin/bash

export port=/dev/ttyUSB0
export baud=115200

if [[ $EUID -ne 0  ]]; then
  echo "This script must be run as root" 1>&2
  exit 1
fi

mv init.lua init.lua.bak
mv web_server.lua web_server.lua.bak

LuaSrcDiet init.lua.bak -o init.lua
LuaSrcDiet web_server.lua.bak -o web_server.lua

../luatool/luatool/luatool.py --wipe --verbose --port $port --baud $baud
../luatool/luatool/luatool.py --src init.lua --verbose --port $port --baud $baud
../luatool/luatool/luatool.py --src web_server.lua --verbose --port $port --baud $baud --restart

rm init.lua
rm web_server.lua

mv init.lua.bak init.lua
mv web_server.lua.bak web_server.lua
