#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WiFiClientSecure.h>
#include <SoftwareSerial.h>
#include <ModbusMaster.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>

#define EEPROM_SIZE 512
#define CONFIG_PIN 13 // D7

SoftwareSerial rs485Serial(D1, D2);
ModbusMaster node1, node2;
WiFiClientSecure client;

const char* defaultSSID = "Yavar";
const char* defaultPASS = "greatR!ng10";
const char* apSSID = "man";
const char* apPASS = "12345678";
const char* host = "esg.yavar.ai";
const int httpsPort = 443;

AsyncWebServer server(80);
const char* otaPassword = "9818";

struct Settings {
  uint8_t slaveID1;
  char token1[64];
  uint16_t reg1a;
  char label1a[16];
  uint16_t reg1b;
  char label1b[16];
  uint8_t slaveID2;
  char token2[64];
  uint16_t reg2a;
  char label2a[16];
  uint16_t reg2b;
  char label2b[16];
  uint16_t interval;
} settings;

unsigned long lastSent = 0;
unsigned long apStartTime = 0;

void startAP();
void setupWebServer();
void loadSettings();
void saveSettings();
void setupOTA();

int calculateNAQI(float concentration, bool isPM25) {
  struct Breakpoint {
    float low, high;
    int indexLow, indexHigh;
  };

  const Breakpoint pm25Breakpoints[] = {
    {0.0, 30.0, 0, 50}, {31.0, 60.0, 51, 100}, {61.0, 90.0, 101, 200},
    {91.0, 120.0, 201, 300}, {121.0, 250.0, 301, 400}, {251.0, 500.0, 401, 500}
  };

  const Breakpoint pm10Breakpoints[] = {
    {0.0, 50.0, 0, 50}, {51.0, 100.0, 51, 100}, {101.0, 250.0, 101, 200},
    {251.0, 350.0, 201, 300}, {351.0, 430.0, 301, 400}, {431.0, 600.0, 401, 500}
  };

  const Breakpoint* bp = isPM25 ? pm25Breakpoints : pm10Breakpoints;
  int len = isPM25 ? sizeof(pm25Breakpoints)/sizeof(Breakpoint) : sizeof(pm10Breakpoints)/sizeof(Breakpoint);

  for (int i = 0; i < len; i++) {
    if (concentration >= bp[i].low && concentration <= bp[i].high) {
      float Clow = bp[i].low;
      float Chigh = bp[i].high;
      int Ilow = bp[i].indexLow;
      int Ihigh = bp[i].indexHigh;
      return round(((Ihigh - Ilow) / (Chigh - Clow)) * (concentration - Clow) + Ilow);
    }
  }
  return -1;
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);
  loadSettings();

  pinMode(CONFIG_PIN, INPUT_PULLUP);
  WiFi.begin(defaultSSID, defaultPASS);
  Serial.println("Connecting to WiFi...");
  unsigned long connectStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - connectStart < 10000) delay(500);

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected!");
    Serial.println(WiFi.localIP());
    setupOTA();
  }

  rs485Serial.begin(9600);
  node1.begin(settings.slaveID1, rs485Serial);
  node2.begin(settings.slaveID2, rs485Serial);
  client.setInsecure();
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) ArduinoOTA.handle();

  unsigned long start = millis();
  while (millis() - start < 5000) {
    if (digitalRead(CONFIG_PIN) != LOW) break;
    delay(100);
  }

  if (digitalRead(CONFIG_PIN) == LOW) {
    startAP();
    apStartTime = millis();
    while (millis() - apStartTime < 15 * 60 * 1000UL) {
      ArduinoOTA.handle();
      delay(100);
    }
    ESP.restart();
  }

  if (WiFi.status() != WL_CONNECTED || millis() - lastSent < settings.interval * 1000UL) return;
  lastSent = millis();

  for (int dev = 1; dev <= 2; dev++) {
    ModbusMaster* node = (dev == 1) ? &node1 : &node2;
    uint8_t sid = (dev == 1) ? settings.slaveID1 : settings.slaveID2;
    const char* token = (dev == 1) ? settings.token1 : settings.token2;
    uint16_t r1 = (dev == 1) ? settings.reg1a : settings.reg2a;
    const char* l1 = (dev == 1) ? settings.label1a : settings.label2a;
    uint16_t r2 = (dev == 1) ? settings.reg1b : settings.reg2b;
    const char* l2 = (dev == 1) ? settings.label1b : settings.label2b;

    node->begin(sid, rs485Serial);
    delay(200);

    String payload = "{";
    bool ok = false;
    float v1 = NAN, v2 = NAN;

    if (node->readInputRegisters(r1, 1) == node->ku8MBSuccess) {
      v1 = node->getResponseBuffer(0) / 10.0;
      payload += "\"" + String(l1) + "\":" + String(v1);
      ok = true;
    }

    delay(300);

    if (node->readInputRegisters(r2, 1) == node->ku8MBSuccess) {
      v2 = node->getResponseBuffer(0) / 10.0;
      if (ok) payload += ",";
      payload += "\"" + String(l2) + "\":" + String(v2);
      ok = true;
    }

    // Only calculate AQI for device 1
    if (dev == 1 && ok) {
      bool hasPM25 = (String(l1).indexOf("pm2") >= 0 || String(l2).indexOf("pm2") >= 0);
      bool hasPM10 = (String(l1).indexOf("pm10") >= 0 || String(l2).indexOf("pm10") >= 0);
      int aqi1 = hasPM25 ? calculateNAQI((String(l1).indexOf("pm2") >= 0 ? v1 : v2), true) : -1;
      int aqi2 = hasPM10 ? calculateNAQI((String(l1).indexOf("pm10") >= 0 ? v1 : v2), false) : -1;
      int finalAQI = max(aqi1, aqi2);
      payload += ",\"aqi\":" + String(finalAQI);
    }

    payload += "}";

    if (ok && client.connect(host, httpsPort)) {
      String url = "/api/v1/" + String(token) + "/telemetry";
      client.print(String("POST ") + url + " HTTP/1.1\r\n");
      client.print(String("Host: ") + host + "\r\n");
      client.print("Content-Type: application/json\r\n");
      client.print("Content-Length: " + String(payload.length()) + "\r\n");
      client.print("Connection: close\r\n\r\n");
      client.print(payload);
      client.stop();
      Serial.println("Sent: " + payload);
    } else {
      Serial.println("Failed to send for slave " + String(sid));
    }
  }
}

void startAP() {
  WiFi.softAP(apSSID, apPASS);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  setupWebServer();
  setupOTA();
}

void setupWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = R"=====(<html><head><style>
      body { font-family: Arial; max-width: 500px; margin: auto; padding: 10px; }
      input[type=text], input[type=number] {
        width: 100%; padding: 8px; margin: 4px 0; box-sizing: border-box;
      }
      input[type=submit] {
        width: 100%; padding: 10px; background: #0078D7; color: white; border: none;
      }
    </style></head><body><h2>Configure Device</h2><form action='/save' method='POST'>)=====";
    html += "Slave ID 1: <input name='slave1' type='number' value='" + String(settings.slaveID1) + "'><br>";
    html += "Token 1: <input name='token1' type='text' value='" + String(settings.token1) + "'><br>";
    html += "Reg1 Addr A: <input name='reg1a' type='number' value='" + String(settings.reg1a) + "'><br>";
    html += "Label A: <input name='label1a' type='text' value='" + String(settings.label1a) + "'><br>";
    html += "Reg1 Addr B: <input name='reg1b' type='number' value='" + String(settings.reg1b) + "'><br>";
    html += "Label B: <input name='label1b' type='text' value='" + String(settings.label1b) + "'><br><hr>";
    html += "Slave ID 2: <input name='slave2' type='number' value='" + String(settings.slaveID2) + "'><br>";
    html += "Token 2: <input name='token2' type='text' value='" + String(settings.token2) + "'><br>";
    html += "Reg2 Addr A: <input name='reg2a' type='number' value='" + String(settings.reg2a) + "'><br>";
    html += "Label A: <input name='label2a' type='text' value='" + String(settings.label2a) + "'><br>";
    html += "Reg2 Addr B: <input name='reg2b' type='number' value='" + String(settings.reg2b) + "'><br>";
    html += "Label B: <input name='label2b' type='text' value='" + String(settings.label2b) + "'><br><hr>";
    html += "Interval (s): <input name='interval' type='number' value='" + String(settings.interval) + "'><br>";
    html += "<input type='submit' value='Save & Reboot'></form></body></html>";
    request->send(200, "text/html", html);
  });

  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request){
    settings.slaveID1 = request->getParam("slave1", true)->value().toInt();
    strncpy(settings.token1, request->getParam("token1", true)->value().c_str(), sizeof(settings.token1));
    settings.reg1a = request->getParam("reg1a", true)->value().toInt();
    strncpy(settings.label1a, request->getParam("label1a", true)->value().c_str(), sizeof(settings.label1a));
    settings.reg1b = request->getParam("reg1b", true)->value().toInt();
    strncpy(settings.label1b, request->getParam("label1b", true)->value().c_str(), sizeof(settings.label1b));
    settings.slaveID2 = request->getParam("slave2", true)->value().toInt();
    strncpy(settings.token2, request->getParam("token2", true)->value().c_str(), sizeof(settings.token2));
    settings.reg2a = request->getParam("reg2a", true)->value().toInt();
    strncpy(settings.label2a, request->getParam("label2a", true)->value().c_str(), sizeof(settings.label2a));
    settings.reg2b = request->getParam("reg2b", true)->value().toInt();
    strncpy(settings.label2b, request->getParam("label2b", true)->value().c_str(), sizeof(settings.label2b));
    settings.interval = request->getParam("interval", true)->value().toInt();
    saveSettings();
    request->send(200, "text/html", "<h3>Settings saved. Rebooting...</h3>");
    delay(1000);
    ESP.restart();
  });

  server.begin();
}

void saveSettings() {
  EEPROM.put(0, settings);
  EEPROM.commit();
}

void loadSettings() {
  EEPROM.get(0, settings);
  if (settings.interval == 0 || settings.interval > 3600) {
    settings.slaveID1 = 1;
    strcpy(settings.token1, "O2WnnllXp3xOTjjkYx62");
    settings.reg1a = 0;
    strcpy(settings.label1a, "pm2.5");
    settings.reg1b = 1;
    strcpy(settings.label1b, "pm10");
    settings.slaveID2 = 2;
    strcpy(settings.token2, "gNGPKm2AB8xJAZ2aBKxy");
    settings.reg2a = 1;
    strcpy(settings.label2a, "temp");
    settings.reg2b = 2;
    strcpy(settings.label2b, "humidity");
    settings.interval = 10;
    saveSettings();
  }
}

void setupOTA() {
  ArduinoOTA.setHostname("modbus-config");
  ArduinoOTA.setPassword(otaPassword);
  ArduinoOTA.begin();
}
