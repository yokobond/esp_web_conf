/*
 * esp_web_conf
 *
 * This is a web service to configure ESP8266 connect WiFi network with DHCP.
 *
 * 1. Connect the module's access point SSID 'esp-????' directory.
 *  - module ID 'esp-????': '????' is determined by MAC address.
 * 2. Open 'http://192.168.4.1/wifi_conf' and set the SSID and Password of your WiFi network.
 * 3. Reboot the module to connect the WiFi network and get DHCP address.
 * 4. The module can be accessed by name 'esp-????.local' using mDNS(Bonjour).  
 *    Or, you can find the assigned IP address in 'http://192.168.4.1/' via direct connecting 'esp-????' as AP.
 *
 *
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 Koji Yokokawa <koji.yokokawa@gmail.com>
 * 
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

#define NAME_PREF "esp-"
char module_id[9] = {0};  // NAME_PREF + hex + hex + \0

String network_html;

#define WIFI_CONF_START 0

struct WifiConfStruct {
  char sta_ssid[32];
  char sta_pwd[64];
} WifiConf = {
  "ssid",
  "pwd",
};

void printWifiConf() {
  Serial.print("SSID: ");
  Serial.println(WifiConf.sta_ssid);
  Serial.print("PASS: ");
  Serial.println(WifiConf.sta_pwd);
}

void loadWifiConf() {
  Serial.println();
  Serial.println("loading WifiConf");
  for (unsigned int t = 0; t < sizeof(WifiConf); t++) {
    *((char*)&WifiConf + t) = EEPROM.read(WIFI_CONF_START + t);
  }
  printWifiConf();
}

void saveWifiConf() {
  Serial.println("writing WifiConf");
  for (unsigned int t = 0; t < sizeof(WifiConf); t++) {
    EEPROM.write(WIFI_CONF_START + t, *((char*)&WifiConf + t));
  }
  EEPROM.commit();
  printWifiConf();
}

ESP8266WebServer server(80);

void setup() {
  Serial.begin(9600);
  EEPROM.begin(512);
  delay(10);
  Serial.println();
  Serial.println();
  Serial.println("Startup");

  // scan Access Points
  scanWifi();

  // get Module ID
  uint8_t macAddr[WL_MAC_ADDR_LENGTH];
  WiFi.macAddress(macAddr);
  sprintf(module_id, "%s%02x%02x", NAME_PREF, macAddr[WL_MAC_ADDR_LENGTH - 2], macAddr[WL_MAC_ADDR_LENGTH - 1]);
  Serial.print("Module ID: ");
  Serial.println(module_id);

  // read eeprom for ssid and pass
  loadWifiConf();

  // start WiFi
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(WifiConf.sta_ssid, WifiConf.sta_pwd);
  waitConnected();
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.softAP(module_id);
  } else {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(module_id);
  }
  printIP();

  // setup Web Interface
  setupWeb();
  setupWifiConfWeb();

  // start Web Server
  server.begin();
  Serial.println();
  Serial.println("Server started");

  // start mDNS responder
  if (!MDNS.begin(module_id)) {
    Serial.println();
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println();
  Serial.println("mDNS responder started");

  // Add service to MDNS-SD
  MDNS.addService("http", "tcp", 80);
}

void scanWifi(void) {
  int founds = WiFi.scanNetworks();
  Serial.println();
  Serial.println("scan done");
  if (founds == 0) {
    Serial.println("no networks found");
  } else {
    Serial.print(founds);
    Serial.println(" networks found");
    for (int i = 0; i < founds; ++i) {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
      delay(10);
    }
  }
  Serial.println();
  network_html = "<ol>";
  for (int i = 0; i < founds; ++i)
  {
    // Print SSID and RSSI for each network found
    network_html += "<li>";
    network_html += WiFi.SSID(i);
    network_html += " (";
    network_html += WiFi.RSSI(i);
    network_html += ")";
    network_html += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*";
    network_html += "</li>";
  }
  network_html += "</ol>";
}

int waitConnected(void) {
  int wait = 0;
  Serial.println();
  Serial.println("Waiting for Wifi to connect");
  while ( wait < 20 ) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("");
      Serial.println("WiFi connected");
      return (1);
    }
    delay(500);
    Serial.print(WiFi.status());
    wait++;
  }
  Serial.println("");
  Serial.println("Connect timed out");
  return (0);
}

void printIP(void) {
  Serial.println();
  Serial.print("Name: ");
  Serial.print(module_id);
  Serial.println(".local");
  Serial.print("LAN: ");
  Serial.println(WiFi.localIP());
  Serial.print("AP: ");
  Serial.println(WiFi.softAPIP());
}

void setupWifiConfWeb(void) {
  server.on("/wifi_conf", []() {
    IPAddress ip = WiFi.localIP();
    String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
    String content = "<!DOCTYPE HTML>\r\n<html><head><title>";
    content += module_id;
    content += ".local - Configuration";
    content += "</title></head><body>";
    content += "<h1>Configuration of ESP8266</h1>";
    content += "<p>LAN: ";
    content += WifiConf.sta_ssid;
    content += "</br>IP: ";
    content += ipStr;
    content += " ( ";
    content += module_id;
    content += ".local";
    content += " )</p>";
    content += "<p>";
    content += "</p><form method='get' action='set_wifi_conf'><label for='ssid'>SSID: </label><input name='ssid'id='ssid' maxlength=32 value='";
    content += ""; // default SSID is empty to avoid careless setting
    content += "'> <label for='pwd'>PASS: </label><input name='pwd' id='pwd' maxlength=64><input type='submit'></form>";
    content += network_html;
    content += "</body></html>";
    server.send(200, "text/html", content);
  });

  server.on("/set_wifi_conf", []() {
    String new_ssid = server.arg("ssid");
    String new_pwd = server.arg("pwd");
    String content = "<!DOCTYPE HTML>\r\n<html><head><title>";
    content += module_id;
    content += ".local - set WiFi";
    content += "</title></head><body>";
    content += "<h1>Set WiFi of ESP8266</h1>";
    if (new_ssid.length() > 0) {
      new_ssid.toCharArray(WifiConf.sta_ssid, sizeof(WifiConf.sta_ssid));
      new_pwd.toCharArray(WifiConf.sta_pwd, sizeof(WifiConf.sta_pwd));
      saveWifiConf();
      content += "<p>saved '";
      content += WifiConf.sta_ssid;
      content += "'... Reset to boot into new WiFi</p>";
      content += "<body></html>";
    } else {
      content += "<p>Empty SSID is not acceptable. </p>";
      content += "<body></html>";
      Serial.println("Rejected empty SSID.");
    }
    server.send(200, "text/html", content);
  });
}

void setupWeb(void) {
  server.on("/", []() {
    IPAddress ip = WiFi.localIP();
    String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
    String content = "<!DOCTYPE HTML>\r\n<html><head><title>";
    content += module_id;
    content += ".local";
    content += "</title></head><body>";
    content += "<h1>Hello from ESP8266</h1>";
    content += "<p>LAN: ";
    content += WifiConf.sta_ssid;
    content += "</br>IP: ";
    content += ipStr;
    content += " ( ";
    content += module_id;
    content += ".local";
    content += " )</p>";
    content += "<p>";
    content += "</p>";
    content += "<ul>";
    content += "<li><a href='/wifi_conf'>WiFi Configuration</a>";
    content += "</ul>";
    content += "</body></html>";
    server.send(200, "text/html", content);
  });
}

void loop() {
  // handle web server
  server.handleClient();

  // put your main code here, to run repeatedly:

}
