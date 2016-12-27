# This file is executed on every boot (including wake-boot from deepsleep)

#import esp
#import webrepl
#import gc
import network
import machine

#esp.osdebug(None)
#webrepl.start()

ap_if = network.WLAN(network.AP_IF)
ap_if.active(False)


sta_if = network.WLAN(network.STA_IF)
if not sta_if.isconnected():
    print('connecting to network...')
    sta_if.active(True)
    sta_if.connect('mcolash', '')
    while not sta_if.isconnected():
        pass
    print('network config:', sta_if.ifconfig())

gc.collect()
