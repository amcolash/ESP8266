print("ESP8266 Server")
wifi.setmode(wifi.STATIONAP);
wifi.ap.config({ssid="power-remote",pwd=nil});
print("Server IP Address:",wifi.ap.getip())

dofile("server.lua")
