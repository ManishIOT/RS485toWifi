#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WiFiClientSecure.h>
#include <SoftwareSerial.h>
#include <ModbusMaster.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>
#include <time.h>
#include <ArduinoJson.h>
#include <ESP8266httpUpdate.h>

// ====== Configuration ======
#define FIRMWARE_VERSION "1.0.0"
const char* firmware_url = "https://raw.githubusercontent.com/ManishIOT/bintest/refs/heads/main/version.json";

#define EEPROM_SIZE 512
#define CONFIG_PIN 13 // D7

SoftwareSerial rs485Serial(D1, D2); // RS485 pins
ModbusMaster node1;
ModbusMaster node2;
WiFiClientSecure secureClient;
PubSubClient mqttClient(secureClient);  // use secure client

const char* defaultSSID = "Yavar";
const char* defaultPASS = "greatR!ng10";
const char* apSSID = "man";
const char* apPASS = "12345678";
const char* host = "esg.yavar.ai";
const char* otaPassword = "9818";
const char* root_ca = R"EOF(
-----BEGIN CERTIFICATE-----
MIICbDCCAhOgAwIBAgIUBJ78EaiFpVA8EVF9ix+hW5J4rvUwCgYIKoZIzj0EAwIw
gYsxCzAJBgNVBAYTAklOMQswCQYDVQQIDAJUTjETMBEGA1UEBwwKY29pbWJhdG9y
ZTEOMAwGA1UECgwFWWF2YXIxDDAKBgNVBAsMA0lPVDEVMBMGA1UEAwwMZXNnLnlh
dmFyLmFpMSUwIwYJKoZIhvcNAQkBFhZtYW5pc2hrdW1hci5tQHlhdmFyLmFpMB4X
DTI1MDYwNjE0MjU0MVoXDTI5MDMwMjE0MjU0MVowgYsxCzAJBgNVBAYTAklOMQsw
CQYDVQQIDAJUTjETMBEGA1UEBwwKY29pbWJhdG9yZTEOMAwGA1UECgwFWWF2YXIx
DDAKBgNVBAsMA0lPVDEVMBMGA1UEAwwMZXNnLnlhdmFyLmFpMSUwIwYJKoZIhvcN
AQkBFhZtYW5pc2hrdW1hci5tQHlhdmFyLmFpMFkwEwYHKoZIzj0CAQYIKoZIzj0D
AQcDQgAEO3YSpHol0mzSOXn6o82/1HGcmUvXUOwLsT6RbB8oMoAuF7D+gM9ALFUo
xPJKVc51twHi22/2OIoklURbsXY2CaNTMFEwHQYDVR0OBBYEFCywiR7VG6BxZhRo
HzZQD7AwOacIMB8GA1UdIwQYMBaAFCywiR7VG6BxZhRoHzZQD7AwOacIMA8GA1Ud
EwEB/wQFMAMBAf8wCgYIKoZIzj0EAwIDRwAwRAIgJdedE+1KHbYXiy/j1TzoOWZ6
nKuMVBILc3SeaYNOjLECIBIRYzdGuNZzzmK3/mbvQymxBbcdq1xHaZiMRgJKV1Y8
-----END CERTIFICATE-----
)EOF";


// Let's Encrypt ISRG Root X1 (used by GitHub)
const char* root_ca1 PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFgTCCBGmgAwIBAgIQOXJEOvkit1HX02wQ3TE1lTANBgkqhkiG9w0BAQwFADB7
MQswCQYDVQQGEwJHQjEbMBkGA1UECAwSR3JlYXRlciBNYW5jaGVzdGVyMRAwDgYD
VQQHDAdTYWxmb3JkMRowGAYDVQQKDBFDb21vZG8gQ0EgTGltaXRlZDEhMB8GA1UE
AwwYQUFBIENlcnRpZmljYXRlIFNlcnZpY2VzMB4XDTE5MDMxMjAwMDAwMFoXDTI4
MTIzMTIzNTk1OVowgYgxCzAJBgNVBAYTAlVTMRMwEQYDVQQIEwpOZXcgSmVyc2V5
MRQwEgYDVQQHEwtKZXJzZXkgQ2l0eTEeMBwGA1UEChMVVGhlIFVTRVJUUlVTVCBO
ZXR3b3JrMS4wLAYDVQQDEyVVU0VSVHJ1c3QgUlNBIENlcnRpZmljYXRpb24gQXV0
aG9yaXR5MIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEAgBJlFzYOw9sI
s9CsVw127c0n00ytUINh4qogTQktZAnczomfzD2p7PbPwdzx07HWezcoEStH2jnG
vDoZtF+mvX2do2NCtnbyqTsrkfjib9DsFiCQCT7i6HTJGLSR1GJk23+jBvGIGGqQ
Ijy8/hPwhxR79uQfjtTkUcYRZ0YIUcuGFFQ/vDP+fmyc/xadGL1RjjWmp2bIcmfb
IWax1Jt4A8BQOujM8Ny8nkz+rwWWNR9XWrf/zvk9tyy29lTdyOcSOk2uTIq3XJq0
tyA9yn8iNK5+O2hmAUTnAU5GU5szYPeUvlM3kHND8zLDU+/bqv50TmnHa4xgk97E
xwzf4TKuzJM7UXiVZ4vuPVb+DNBpDxsP8yUmazNt925H+nND5X4OpWaxKXwyhGNV
icQNwZNUMBkTrNN9N6frXTpsNVzbQdcS2qlJC9/YgIoJk2KOtWbPJYjNhLixP6Q5
D9kCnusSTJV882sFqV4Wg8y4Z+LoE53MW4LTTLPtW//e5XOsIzstAL81VXQJSdhJ
WBp/kjbmUZIO8yZ9HE0XvMnsQybQv0FfQKlERPSZ51eHnlAfV1SoPv10Yy+xUGUJ
5lhCLkMaTLTwJUdZ+gQek9QmRkpQgbLevni3/GcV4clXhB4PY9bpYrrWX1Uu6lzG
KAgEJTm4Diup8kyXHAc/DVL17e8vgg8CAwEAAaOB8jCB7zAfBgNVHSMEGDAWgBSg
EQojPpbxB+zirynvgqV/0DCktDAdBgNVHQ4EFgQUU3m/WqorSs9UgOHYm8Cd8rID
ZsswDgYDVR0PAQH/BAQDAgGGMA8GA1UdEwEB/wQFMAMBAf8wEQYDVR0gBAowCDAG
BgRVHSAAMEMGA1UdHwQ8MDowOKA2oDSGMmh0dHA6Ly9jcmwuY29tb2RvY2EuY29t
L0FBQUNlcnRpZmljYXRlU2VydmljZXMuY3JsMDQGCCsGAQUFBwEBBCgwJjAkBggr
BgEFBQcwAYYYaHR0cDovL29jc3AuY29tb2RvY2EuY29tMA0GCSqGSIb3DQEBDAUA
A4IBAQAYh1HcdCE9nIrgJ7cz0C7M7PDmy14R3iJvm3WOnnL+5Nb+qh+cli3vA0p+
rvSNb3I8QzvAP+u431yqqcau8vzY7qN7Q/aGNnwU4M309z/+3ri0ivCRlv79Q2R+
/czSAaF9ffgZGclCKxO/WIu6pKJmBHaIkU4MiRTOok3JMrO66BQavHHxW/BBC5gA
CiIDEOUMsfnNkjcZ7Tvx5Dq2+UUTJnWvu6rvP3t3O9LEApE9GQDTF1w52z97GA1F
zZOFli9d31kWTz9RvdVFGD/tSo7oBmF0Ixa1DVBzJ0RHfxBdiSprhTEUxOipakyA
vGp4z7h/jnZymQyd/teRCBaho1+V
-----END CERTIFICATE-----
)EOF";

// MQTT topic for ThingsBoard telemetry
const char* MQTT_TOPIC = "v1/devices/me/telemetry";
// MQTT Broker info
const char* MQTT_BROKER = "esg.yavar.ai";
const int MQTT_PORT = 8883;
// Static BearSSL certificates to avoid memory leaks
static BearSSL::X509List certGitHub(root_ca1);
WiFiClientSecure httpClientSecure;
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
  checkAndUpdateFirmware();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");
    Serial.println(WiFi.localIP());
    setupOTA();
    // configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    // time_t now = time(nullptr);
    // while (now < 8 * 3600 * 2) {
    // delay(500);
    // Serial.print(".");
    // now = time(nullptr);

    // }
  }
  // Use setTrustAnchors for ESP8266 BearSSL
  // secureClient.setTrustAnchors(new BearSSL::X509List(root_ca));
  // mqttClient.setServer(host, MQTT_PORT);
  // connectMQTT();

  rs485Serial.begin(9600);
  node1.begin(settings.slaveID1, rs485Serial);
  node2.begin(settings.slaveID2, rs485Serial);
  secureClient.setInsecure();
}
void setupTime() {
  Serial.println("Setting up time using NTP...");
  const long gmtOffset_sec = 5 * 3600 + 30 * 60;
  const int daylightOffset_sec = 0;
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) { // wait until time is synced (epoch > some threshold)
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println();
  Serial.print("Current time: ");
  Serial.println(ctime(&now));
}

void checkAndUpdateFirmware() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected! Skipping firmware update.");
    return;
  }

  Serial.println("Checking for firmware update...");

  httpClientSecure.setTrustAnchors(&certGitHub);
  HTTPClient https;
  if (https.begin(httpClientSecure, firmware_url)) {
    int httpCode = https.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = https.getString();
      Serial.println("Received JSON:");
      Serial.println(payload);

      StaticJsonDocument<512> doc;
      DeserializationError error = deserializeJson(doc, payload);
      if (error) {
        Serial.print("JSON parse failed: ");
        Serial.println(error.c_str());
        https.end();
        return;
      }

      const char* newVersion = doc["version"];
      const char* binURL = doc["bin_url"];  // FIXED key here

      Serial.print("New version: ");
      Serial.println(newVersion);
      Serial.print("Current version: ");
      Serial.println(FIRMWARE_VERSION);

      if (String(newVersion) != FIRMWARE_VERSION) {
        Serial.println("New firmware available, updating...");
        https.end();

        t_httpUpdate_return ret = ESPhttpUpdate.update(httpClientSecure, binURL);
        switch (ret) {
          case HTTP_UPDATE_FAILED:
            Serial.printf("Update failed. Error (%d): %s\n",
                          ESPhttpUpdate.getLastError(),
                          ESPhttpUpdate.getLastErrorString().c_str());
            break;
          case HTTP_UPDATE_NO_UPDATES:
            Serial.println("No update available.");
            break;
          case HTTP_UPDATE_OK:
            Serial.println("Update successful. Rebooting...");
            break;
        }
      } else {
        Serial.println("Firmware is up to date.");
        https.end();
      }
    } else {
      Serial.printf("Failed to fetch version JSON, HTTP code: %d\n", httpCode);
      https.end();
    }
  } else {
    Serial.println("Failed to begin HTTPS connection for JSON.");
  }
}

void loop() {

  checkAndUpdateFirmware();
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


void connectMQTT(const char* token) {
  // while (!mqttClient.connected()) {



      // Use setTrustAnchors for ESP8266 BearSSL
    setupTime();
    secureClient.setTrustAnchors(new BearSSL::X509List(root_ca));
    mqttClient.setServer(host, MQTT_PORT);
    Serial.print("Connecting to MQTT... ");
    if (mqttClient.connect(token, token, NULL)) {
      Serial.println("connected");
      mqttClient.publish(MQTT_TOPIC, "{\"test\":25,\"test1\":60}");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(". Retrying in 30 seconds");
      delay(30000);
    }
  // }
}


void sendData(const char* token, const String& payload) {
  
  
  // if (!mqttClient.connected()) {
  connectMQTT(token);
  // }
  // else{
  mqttClient.publish(MQTT_TOPIC, payload.c_str());
  // }
  mqttClient.loop();




  // HTTPClient http;
  // String url = "https://" + String(host) + "/api/v1/" + String(token) + "/telemetry";

  // http.begin(secureClient, url);  // <== FIXED: using WiFiClientSecure
  // http.addHeader("Content-Type", "application/json");
  // int httpCode = http.POST(payload);
  // if (httpCode > 0) {
  //   Serial.printf("Sent to %s: %s\n", token, payload.c_str());
  //   Serial.printf("HTTP code: %d\n", httpCode);
  //   String response = http.getString();
  //   Serial.println("Response: " + response);
  // } else {
  //   Serial.printf("HTTPS send failed: %s\n", http.errorToString(httpCode).c_str());
  // }
  // http.end();
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
