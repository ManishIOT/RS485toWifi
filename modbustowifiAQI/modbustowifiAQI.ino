#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WiFiClientSecure.h>
#include <SoftwareSerial.h>
#include <ModbusMaster.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>
#include <ESP8266HTTPClient.h>

#define EEPROM_SIZE 512
#define CONFIG_PIN 13 // D7

SoftwareSerial rs485Serial(D1, D2); // RS485 pins
ModbusMaster node1;
ModbusMaster node2;
WiFiClientSecure secureClient;

const char* defaultSSID = "Yavar";
const char* defaultPASS = "greatR!ng10";
const char* apSSID = "man";
const char* apPASS = "12345678";
const char* host = "esg.yavar.ai";
const char* otaPassword = "9818";

AsyncWebServer server(80);

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


int calculateAQISegment(int concentration, const int* bpLow, const int* bpHigh, const int* aqiLow, const int* aqiHigh, int count) {
  for (int i = 0; i < count; i++) {
    if (concentration <= bpHigh[i]) {
      return ((aqiHigh[i] - aqiLow[i]) * (concentration - bpLow[i])) / (bpHigh[i] - bpLow[i]) + aqiLow[i];
    }
  }
  return 500; // Max AQI
}

int computePM25AQI(uint16_t pm25) {
  const int bpLow[] =  {0, 31, 61, 91, 121, 251, 351, 431};
  const int bpHigh[] = {30, 60, 90, 120, 250, 350, 430, 500};
  const int aqiLow[] = {0, 51, 101, 201, 301, 401,  421,  451};
  const int aqiHigh[] = {50, 100, 200, 300, 400, 420, 450, 500};
  return calculateAQISegment(pm25, bpLow, bpHigh, aqiLow, aqiHigh, 8);
}

int computePM10AQI(uint16_t pm10) {
  const int bpLow[] =  {0, 51, 101, 251, 351, 431, 511, 611};
  const int bpHigh[] = {50, 100, 250, 350, 430, 510, 610, 700};
  const int aqiLow[] = {0, 51, 101, 201, 301, 401,  421,  451};
  const int aqiHigh[] = {50, 100, 200, 300, 400, 420, 450, 500};
  return calculateAQISegment(pm10, bpLow, bpHigh, aqiLow, aqiHigh, 8);
}

unsigned long lastSlave1 = 0;
unsigned long lastSlave2 = 0;

void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);
  loadSettings();

  pinMode(CONFIG_PIN, INPUT_PULLUP);

  WiFi.begin(defaultSSID, defaultPASS);
  Serial.println("Connecting to WiFi...");
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) delay(500);

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");
    Serial.println(WiFi.localIP());
    setupOTA();
  }

  rs485Serial.begin(9600);
  node1.begin(settings.slaveID1, rs485Serial);
  node2.begin(settings.slaveID2, rs485Serial);
  secureClient.setInsecure();
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) ArduinoOTA.handle();

  static unsigned long configHoldStart = 0;
  if (digitalRead(CONFIG_PIN) == LOW) {
    if (configHoldStart == 0) configHoldStart = millis();
    else if (millis() - configHoldStart > 5000) {
      startAP();
      delay(100);
      unsigned long apStart = millis();
      while (millis() - apStart < 15 * 60 * 1000UL) {
        ArduinoOTA.handle();
        delay(100);
      }
      ESP.restart();
    }
  } else {
    configHoldStart = 0;
  }

  unsigned long now = millis();
  if (now - lastSlave1 >= settings.interval * 1000UL) {
    Serial.println("LoopSlave1: Reading slave 1");
    readAndSend(node1, settings.slaveID1, settings.token1, settings.reg1a, settings.label1a, settings.reg1b, settings.label1b);
    lastSlave1 = now;
  }

  if (now - lastSlave2 >= settings.interval * 1000UL + 5000) { // Offset by 5s
    Serial.println("LoopSlave2: Reading slave 2");
    readAndSend(node2, settings.slaveID2, settings.token2, settings.reg2a, settings.label2a, settings.reg2b, settings.label2b);
    lastSlave2 = now;
  }
}

bool readTwoRegisters(ModbusMaster& node, uint16_t reg1, uint16_t& val1, uint16_t reg2, uint16_t& val2) {
  uint16_t startReg = min(reg1, reg2);
  uint16_t count = abs(reg1 - reg2) + 1;

  for (int attempt = 0; attempt < 100; attempt++) {
    if (node.readInputRegisters(startReg, count) == node.ku8MBSuccess) {
      val1 = node.getResponseBuffer(reg1 - startReg);
      val2 = node.getResponseBuffer(reg2 - startReg);
      return true;
    }
    delay(5);
  }
  return false;
}



void readAndSend(ModbusMaster &node, uint8_t id, const char* token, uint16_t regA, const char* labelA, uint16_t regB, const char* labelB) {
  Serial.printf("Reading slave %d registers %d and %d\n", id, regA, regB);

  uint16_t rawA = 0, rawB = 0;
  bool success = readTwoRegisters(node, regA, rawA, regB, rawB);

  if (!success) {
    Serial.printf("Failed to read both registers from slave %d\n", id);
    return;
  }

  String payload = "{";
  payload += "\"" + String(labelA) + "\":" + String(rawA) + ",";
  payload += "\"" + String(labelB) + "\":" + String(rawB);
    if (id == settings.slaveID1 &&
      (String(labelA).startsWith("pm") || String(labelB).startsWith("pm"))) {
    int pm25 = 0, pm10 = 0;
    if (String(labelA) == "pm2.5") pm25 = rawA;
    if (String(labelB) == "pm2.5") pm25 = rawB;
    if (String(labelA) == "pm10") pm10 = rawA;
    if (String(labelB) == "pm10") pm10 = rawB;

    int aqiPM25 = computePM25AQI(pm25);
    int aqiPM10 = computePM10AQI(pm10);
    int finalAQI = max(aqiPM25, aqiPM10);

    payload += ",\"AQI\":" + String(finalAQI);
  }
  payload += "}";

  sendData(token, payload);
}


// ... (no changes to includes, setup, loop, etc. — skipping to sendData())

void sendData(const char* token, const String& payload) {
  HTTPClient http;
  String url = "https://" + String(host) + "/api/v1/" + String(token) + "/telemetry";

  http.begin(secureClient, url);  // <== FIXED: using WiFiClientSecure
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST(payload);
  if (httpCode > 0) {
    Serial.printf("Sent to %s: %s\n", token, payload.c_str());
    Serial.printf("HTTP code: %d\n", httpCode);
    String response = http.getString();
    Serial.println("Response: " + response);
  } else {
    Serial.printf("HTTPS send failed: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
}


void startAP() {
  WiFi.softAP(apSSID, apPASS);
  Serial.print("AP started at ");
  Serial.println(WiFi.softAPIP());
  setupWebServer();
  setupOTA();
}

void setupOTA() {
  ArduinoOTA.setHostname("modbus-config");
  ArduinoOTA.setPassword(otaPassword);
  ArduinoOTA.begin();
}

void setupWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = R"=====(
    <html><head><style>
    body { font-family: Arial; max-width: 500px; margin: auto; padding: 10px; }
    input[type=text], input[type=number] { width: 100%; padding: 8px; margin: 4px 0; }
    input[type=submit] { width: 100%; padding: 10px; background: #0078D7; color: white; border: none; }
    </style></head><body><h2>Configure Device</h2><form action='/save' method='POST'>
    )=====";
    html += "Slave ID 1: <input name='slave1' type='number' value='" + String(settings.slaveID1) + "'><br>";
    html += "Token 1: <input name='token1' type='text' value='" + String(settings.token1) + "'><br>";
    html += "Reg1 A: <input name='reg1a' type='number' value='" + String(settings.reg1a) + "'><br>";
    html += "Label A: <input name='label1a' type='text' value='" + String(settings.label1a) + "'><br>";
    html += "Reg1 B: <input name='reg1b' type='number' value='" + String(settings.reg1b) + "'><br>";
    html += "Label B: <input name='label1b' type='text' value='" + String(settings.label1b) + "'><br><hr>";

    html += "Slave ID 2: <input name='slave2' type='number' value='" + String(settings.slaveID2) + "'><br>";
    html += "Token 2: <input name='token2' type='text' value='" + String(settings.token2) + "'><br>";
    html += "Reg2 A: <input name='reg2a' type='number' value='" + String(settings.reg2a) + "'><br>";
    html += "Label A: <input name='label2a' type='text' value='" + String(settings.label2a) + "'><br>";
    html += "Reg2 B: <input name='reg2b' type='number' value='" + String(settings.reg2b) + "'><br>";
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
    request->send(200, "text/html", "<h3>Saved. Rebooting...</h3>");
    delay(1000);
    ESP.restart();
  });

  server.begin();
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

    settings.slaveID2 = 10;
    strcpy(settings.token2, "gNGPKm2AB8xJAZ2aBKxy");
    settings.reg2a = 1;
    strcpy(settings.label2a, "temperature");
    settings.reg2b = 2;
    strcpy(settings.label2b, "humidity");

    settings.interval = 20;
    saveSettings();
  }
}

void saveSettings() {
  EEPROM.put(0, settings);
  EEPROM.commit();
}
