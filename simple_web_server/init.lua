wifi.setmode (wifi.STATION)
print ('set mode=STATION (mode=' .. wifi.getmode () .. ')')
print ('MAC: ', wifi.sta.getmac ())
print ('chip: ', node.chipid ())
print ('heap: ', node.heap ())

wifi.sta.sethostname ("huzzah")
wifi.sta.config ("mcolash", "")

tmr.alarm (0, 10000, 0, function () dofile ("web_server.lua") end)

