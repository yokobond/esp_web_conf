# esp_web_conf

This is a web service to configure ESP8266 connect WiFi network with DHCP.

This program is based on [esp8266/Arduino](https://github.com/esp8266/Arduino) and inspired by these projects
- [chriscook8/esp-arduino-apboot](https://github.com/chriscook8/esp-arduino-apboot)
- [ESP8266 Access Point using Arduino IDE - Hackster.io](https://www.hackster.io/rayburne/esp8266-access-point-using-arduino-ide)


## Instructions

First of all, you should upload this sketch using [esp8266/Arduino](https://github.com/esp8266/Arduino) (*updated Sep 09, 2015 or later*).

Then follow this instructions.  

1. Connect the module's access point SSID 'esp-????' directly.
 - module ID 'esp-????': '????' is determined by MAC address.
2. Open 'http://192.168.4.1/wifi_conf' and set the SSID and Password of your WiFi network.
3. Reboot the module to connect the WiFi network and get DHCP address.
4. The module can be accessed by name 'esp-????.local' using mDNS(Bonjour).  
   Or, you can find the assigned IP address in 'http://192.168.4.1/' via direct connecting 'esp-????' as AP.


## Monitoring

It can be monitored its process via serial port (9600 baud).
You can watch the debug-prints like followings.

```
Startup

scan done
2 networks found
1: y-home-g (-52)*
2: y-home-gw (-52)*

Module ID: esp-ecd4

loading WifiConf
SSID: ssid
PASS: password

Waiting for Wifi to connect
66666666666666666666
Connect timed out

Name: esp-ecd4.local
LAN: 0.0.0.0
AP: 192.168.4.1

Server started

mDNS responder started
```
