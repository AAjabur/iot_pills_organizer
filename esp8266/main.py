# Complete project details at https://RandomNerdTutorials.com

import wifimgr
from time import sleep
import machine

try:
  import usocket as socket
except:
  import socket

led = machine.Pin(2, machine.Pin.OUT)

wlan = wifimgr.get_connection()
if wlan is None:
    print("Could not initialize the network connection.")
    while True:
        pass  # you shall not pass :D

# Main Code goes here, wlan is a working network.WLAN(STA_IF) instance.
print(wlan.ifconfig())

def sub_cb(topic, msg):
    def dates_cb(msg):
        print('Received a message in topic dates')
        print('msg = ' + str(msg))
        return 0
    if(topic == b'dates'):
        dates_cb(msg)
    else:
        pass

def set_subscribers(client):
    client.subscribe(b'dates')
    return client

def restart_and_reconnect():
  print('Failed to connect to MQTT broker. Reconnecting...')
  time.sleep(10)
  machine.reset()

try:
  client = MQTTClient(client_id, mqtt_server)
  client.set_callback(sub_cb)
  client.connect()
  print('Conected to MQTT broker with client_id %s' % (client_id))
except OSError as e:
  restart_and_reconnect()
  
client = set_subscribers(client)

while True:
    try:
        client.check_msg()
    except OSError as e:
        restart_and_reconnect()
    
  

