# This file is executed on every boot (including wake-boot from deepsleep)
#import esp
import uos
import time
from umqttsimple import MQTTClient
import ubinascii
import machine
import micropython
import network
import esp
esp.osdebug(None)
#uos.dupterm(None, 1) # disable REPL on UART(0)
import gc
#import webrepl
#webrepl.start()
gc.collect()

mqtt_server = '3.124.50.83'
client_id = ubinascii.hexlify(machine.unique_id())
