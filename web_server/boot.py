# This file is executed on every boot (including wake-boot from deepsleep)

#import esp
#import webrepl
#import gc
import network
import time

#esp.osdebug(None)
#webrepl.start()

SSID='mcolash'
PASS=''

AP_ENABLED = True
CONNECT_ENABLED = False

ap_if = network.WLAN(network.AP_IF)
ap_if.active(AP_ENABLED)


sta_if = network.WLAN(network.STA_IF)
sta_if.active(CONNECT_ENABLED)
if CONNECT_ENABLED and not sta_if.isconnected():
    print('connecting to network...')
    sta_if.connect(SSID, PASS)
    while not sta_if.isconnected():
        pass

    print('network config:', sta_if.ifconfig())

gc.collect()
