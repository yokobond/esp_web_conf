/*
 * esp_web_conf
 *
 * This is a web service to configure ESP8266 connect WiFi network with DHCP.
 *
 * ### Setup WiFi
 * Then follow this instructions to connect WiFi network.  
 * 
 * 1. Connect the module's access point SSID 'esp-????' directly.
 *  - module ID 'esp-????': '????' is determined by MAC address.
 * 2. Open 'http://192.168.4.1/wifi_conf' and set the SSID and Password of your WiFi network.
 * 3. Reboot the module to connect the WiFi network and get DHCP address.
 * 4. The module can be accessed by name 'esp-????.local' using mDNS(Bonjour).
 *    Or, you can find the assigned IP address in 'http://192.168.4.1/' via direct connecting 'esp-????' as AP.
 *
 * ### Change Module ID
 * When you want to change the Module name, open '/module_id' and change the module ID. 
 * If you submit blank ID, the default ID 'esp-????' will be used. 
 *
 * ### Update This Sketch
 * When you want to update this sketch via Web, open '/update' and upload a compiled sketch file named '*.bin' by a FORM in the page.
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

#define WIFI_CONF_FORMAT {0, 0, 0, 1}
const uint8_t wifi_conf_format[] = WIFI_CONF_FORMAT;
#define NAME_PREF "esp-"

String network_html;

#define WIFI_CONF_START 0

struct WiFiConfStruct {
  uint8_t format[4];
  char sta_ssid[32];
  char sta_pwd[64];
  char module_id[32];
} WiFiConf = {
  WIFI_CONF_FORMAT,
  "ssid",
  "password",
  ""
};

void printWiFiConf(void) {
  Serial.print("SSID: ");
  Serial.println(WiFiConf.sta_ssid);
  Serial.print("Password: ");
  Serial.println(WiFiConf.sta_pwd);
  Serial.print("Module ID: ");
  Serial.println(WiFiConf.module_id);
}

bool loadWiFiConf() {
  Serial.println();
  Serial.println("loading WiFiConf");
  if (EEPROM.read(WIFI_CONF_START + 0) == wifi_conf_format[0] &&
      EEPROM.read(WIFI_CONF_START + 1) == wifi_conf_format[1] &&
      EEPROM.read(WIFI_CONF_START + 2) == wifi_conf_format[2] &&
      EEPROM.read(WIFI_CONF_START + 3) == wifi_conf_format[3])
  {
    for (unsigned int t = 0; t < sizeof(WiFiConf); t++) {
      *((char*)&WiFiConf + t) = EEPROM.read(WIFI_CONF_START + t);
    }
    printWiFiConf();
    return true;
  } else {
    Serial.println("WiFiConf was not saved on EEPROM.");
    return false;
  }
}

void saveWiFiConf(void) {
  Serial.println("writing WiFiConf");
  for (unsigned int t = 0; t < sizeof(WiFiConf); t++) {
    EEPROM.write(WIFI_CONF_START + t, *((char*)&WiFiConf + t));
  }
  EEPROM.commit();
  printWiFiConf();
}

void setDefaultModuleId(char* dst) {
  uint8_t macAddr[WL_MAC_ADDR_LENGTH];
  WiFi.macAddress(macAddr);
  sprintf(dst, "%s%02x%02x", NAME_PREF, macAddr[WL_MAC_ADDR_LENGTH - 2], macAddr[WL_MAC_ADDR_LENGTH - 1]);
}

void resetModuleId(void) {
  uint8_t macAddr[WL_MAC_ADDR_LENGTH];
  WiFi.macAddress(macAddr);
  setDefaultModuleId(WiFiConf.module_id);
  Serial.print("Reset Module ID to default: ");
  Serial.println(WiFiConf.module_id);
}

ESP8266WebServer server(80);

void setup() {
  Serial.begin(9600);
  EEPROM.begin(512);
  delay(10);
  Serial.println();
  Serial.println();
  Serial.println("Startup");

  // read eeprom for ssid and pass
  if (!loadWiFiConf()) {
    // EEPROM was not initialized.
    resetModuleId();
    saveWiFiConf();
  }

  // scan Access Points
  scanWiFi();

  // start WiFi
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(WiFiConf.sta_ssid, WiFiConf.sta_pwd);
  waitConnected();
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.softAP(WiFiConf.module_id);
  } else {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WiFiConf.module_id);
  }
  printIP();

  // setup Web Interface
  setupWeb();
  setupWiFiConf();
  setupWebUpdate();

  // start Web Server
  server.begin();
  Serial.println();
  Serial.println("Server started");

  // start mDNS responder
  if (!MDNS.begin(WiFiConf.module_id)) {
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

void scanWiFi(void) {
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
  Serial.println("Waiting for WiFi to connect");
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
  Serial.print(WiFiConf.module_id);
  Serial.println(".local");
  Serial.print("LAN: ");
  Serial.println(WiFi.localIP());
  Serial.print("AP: ");
  Serial.println(WiFi.softAPIP());
}

void setupWiFiConf(void) {
  server.on("/wifi_conf", []() {
    IPAddress ip = WiFi.localIP();
    String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
    String content = "<!DOCTYPE HTML>\r\n<html><head><title>";
    content += WiFiConf.module_id;
    content += ".local - Configuration";
    content += "</title></head><body>";
    content += "<h1>Configuration of ESP8266</h1>";
    content += "<p>LAN: ";
    content += WiFiConf.sta_ssid;
    content += "</br>IP: ";
    content += ipStr;
    content += " ( ";
    content += WiFiConf.module_id;
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
    content += WiFiConf.module_id;
    content += ".local - set WiFi";
    content += "</title></head><body>";
    content += "<h1>Set WiFi of ESP8266</h1>";
    if (new_ssid.length() > 0) {
      new_ssid.toCharArray(WiFiConf.sta_ssid, sizeof(WiFiConf.sta_ssid));
      new_pwd.toCharArray(WiFiConf.sta_pwd, sizeof(WiFiConf.sta_pwd));
      saveWiFiConf();
      content += "<p>saved '";
      content += WiFiConf.sta_ssid;
      content += "'... Reset to boot into new WiFi</p>";
      content += "<body></html>";
    } else {
      content += "<p>Empty SSID is not acceptable. </p>";
      content += "<body></html>";
      Serial.println("Rejected empty SSID.");
    }
    server.send(200, "text/html", content);
  });

  server.on("/module_id", []() {
    IPAddress ip = WiFi.localIP();
    String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
    char defaultId[sizeof(WiFiConf.module_id)];
    setDefaultModuleId(defaultId);
    String content = "<!DOCTYPE HTML>\r\n<html><head><title>";
    content += WiFiConf.module_id;
    content += ".local - Module ID";
    content += "</title></head><body>";
    content += "<h1>Module ID of ESP8266</h1>";
    content += "<p>Module ID: ";
    content += WiFiConf.module_id;
    content += "</br>IP: ";
    content += ipStr;
    content += "</p>";
    content += "<p>";
    content += "<form method='get' action='set_module_id'><label for='module_id'>New Module ID: </label><input name='module_id' id='module_id' maxlength=32 value='";
    content += WiFiConf.module_id;
    content += "'><input type='submit'></form>";
    content += " Empty will reset to default ID '";
    content += defaultId;
    content += "'</p>";
    content += "</body></html>";
    server.send(200, "text/html", content);
  });

  server.on("/set_module_id", []() {
    String new_id = server.arg("module_id");
    String content = "<!DOCTYPE HTML>\r\n<html><head>";
    content += "<title>";
    content += WiFiConf.module_id;
    content += ".local - set WiFi";
    content += "</title>";
    content += "</head><body>";
    if (new_id.length() > 0) {
      new_id.toCharArray(WiFiConf.module_id, sizeof(WiFiConf.module_id));
    } else {
      resetModuleId();
    }
    saveWiFiConf();
    content += "<h1>Set WiFi of ESP8266</h1>";
    content += "<p>Set Module ID to '";
    content += WiFiConf.module_id;
    content += "' ... Restart to applay it. </p>";
    content += "</body></html>";
    server.send(200, "text/html", content);
  });
}

const char* sketchUploadForm = "<form method='POST' action='/upload_sketch' enctype='multipart/form-data'><input type='file' name='sketch'><input type='submit' value='Upload'></form>";

void setupWebUpdate(void) {
  server.on("/update", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/html", sketchUploadForm);
  });
  server.onFileUpload([]() {
    if (server.uri() != "/upload_sketch") return;
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.setDebugOutput(true);
      WiFiUDP::stopAll();
      Serial.printf("Sketch: %s\n", upload.filename.c_str());
      uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
      if (!Update.begin(maxSketchSpace)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
      Serial.setDebugOutput(false);
    }
    yield();
  });
  server.on("/upload_sketch", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  });
}

void setupWeb(void) {
  server.on("/", []() {
    IPAddress ip = WiFi.localIP();
    String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
    String content = "<!DOCTYPE HTML>\r\n<html><head><title>";
    content += WiFiConf.module_id;
    content += ".local";
    content += "</title></head><body>";
    content += "<h1>Hello from ESP8266</h1>";
    content += "<p>LAN: ";
    content += WiFiConf.sta_ssid;
    content += "</br>IP: ";
    content += ipStr;
    content += " ( ";
    content += WiFiConf.module_id;
    content += ".local";
    content += " )</p>";
    content += "<p>";
    content += "</p>";
    content += "<ul>";
    content += "<li><a href='/wifi_conf'>Setup WiFi</a>";
    content += "<li><a href='/module_id'>Setup Module ID</a>";
    content += "<li><a href='/update'>Update Sketch</a>";
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
