#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <ArduinoJson.h>
#include <time.h>

// ====== Configuration ======
#define FIRMWARE_VERSION "1.0.0"
const char* firmware_url = "https://raw.githubusercontent.com/ManishIOT/bintest/refs/heads/main/version.json";

const char* WIFI_SSID = "Yavar";
const char* WIFI_PASSWORD = "greatR!ng10";

const char* MQTT_BROKER = "esg.yavar.ai";
const int MQTT_PORT = 8883;
const char* DEVICE_TOKEN = "gNGPKm2AB8xJAZ2aBKxy";
const char* MQTT_TOPIC = "v1/devices/me/telemetry";

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

// Self-signed CA certificate (PEM format)
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

// Static BearSSL certificates to avoid memory leaks
static BearSSL::X509List certGitHub(root_ca1);
static BearSSL::X509List certMQTT(root_ca);

// Separate clients for MQTT and HTTP Update
WiFiClientSecure mqttClientSecure;
WiFiClientSecure httpClientSecure;
PubSubClient mqttClient(mqttClientSecure);

void printWakeupReason() {
  Serial.print("Reset reason: ");
  Serial.println(ESP.getResetReason());
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

void connectWiFi() {
  Serial.print("Connecting to WiFi ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" WiFi connected");
  } else {
    Serial.println(" Failed to connect to WiFi");
  }
}

void setupTime() {
  Serial.println("Setting up time using NTP...");
  const long gmtOffset_sec = 5 * 3600 + 30 * 60;
  const int daylightOffset_sec = 0;

  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println();
  Serial.print("Current time: ");
  Serial.println(ctime(&now));
}

void connectMQTT() {
  mqttClientSecure.setTrustAnchors(&certMQTT);
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);

  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT... ");
    if (mqttClient.connect(DEVICE_TOKEN, DEVICE_TOKEN, NULL)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(". Retrying in 3 seconds");
      delay(3000);
    }
  }
}



// ====== Main Setup ======
void setup() {
  Serial.begin(115200);
  delay(1000);

  printWakeupReason();
  connectWiFi();
  setupTime();

  connectMQTT();
  mqttClient.publish(MQTT_TOPIC, "{\"temperature\":27,\"humidity\":58}");
  mqttClient.disconnect();
  Serial.println("MQTT published.");

  checkAndUpdateFirmware();

  Serial.println("Turning off Wi-Fi...");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();
  delay(1);
}

// ====== Main Loop ======
void loop() {
  Serial.println("Sleeping with Wi-Fi OFF for 30 seconds...");
  delay(30000);

  Serial.println("Waking up Wi-Fi...");
  WiFi.forceSleepWake();
  delay(1);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Failed to connect to WiFi after wake.");
    return;
  }

  Serial.println("\nWi-Fi connected again!");

  connectMQTT();
  mqttClient.publish(MQTT_TOPIC, "{\"temperature\":30,\"humidity\":60}");
  mqttClient.disconnect();
  Serial.println("Published again after wakeup.");

  checkAndUpdateFirmware();

  Serial.println("Turning off Wi-Fi again...");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();
  delay(1);
}