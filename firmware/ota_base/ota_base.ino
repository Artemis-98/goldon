/*
 * گلدون هوشمند — حیاط
 * OTA روی همان وای‌فای لپ‌تاپ (Station mode)
 */

#include <وای فای.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <DHT.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

static const char *وای فای_SSID = "YOUR_WIFI_SSID";
static const char *وای فای_PASS = "YOUR_WIFI_PASSWORD";
static const char *OTA_PASS = "YOUR_OTA_PASSWORD";
static const char *OTA_HOST = "goldon-yard";

static const int PIN_SOIL1 = 1;
static const int PIN_SOIL2 = 2;
static const int PIN_DHT = 4;
static const int PIN_RELAY1 = 10;
static const int PIN_RELAY2 = 11;

DHT dht(PIN_DHT, DHT11);
bool otaReady = false;
bool وای فایStarted = false;

void disableBrownout() {
  CLEAR_PERI_REG_MASK(RTC_CNTL_BROWN_OUT_REG, RTC_CNTL_BROWN_OUT_ENA);
#ifdef RTC_CNTL_BROWN_OUT_RST_ENA
  CLEAR_PERI_REG_MASK(RTC_CNTL_BROWN_OUT_REG, RTC_CNTL_BROWN_OUT_RST_ENA);
#endif
}

void startوای فایOta() {
  disableBrownout();
  Serial.printf("Connecting وای فای SSID=%s ...\n", وای فای_SSID);

  وای فای.persistent(false);
  وای فای.mode(وای فای_STA);
  delay(200);
  وای فای.setTxPower(وای فای_POWER_8_5dBm);
  وای فای.begin(وای فای_SSID, وای فای_PASS);

  unsigned long start = millis();
  while (وای فای.status() != WL_CONNECTED && millis() - start < 30000) {
    delay(400);
    Serial.print('.');
  }
  Serial.println();

  if (وای فای.status() != WL_CONNECTED) {
    Serial.println("وای فای FAILED — check SSID/PASS or 2.4GHz");
    return;
  }

  Serial.printf("وای فای OK  IP=%s  RSSI=%d\n",
                وای فای.localIP().toString().c_str(), وای فای.RSSI());

  if (MDNS.begin(OTA_HOST)) {
    Serial.printf("mDNS: http://%s.local\n", OTA_HOST);
  }

  ArduinoOTA.setHostname(OTA_HOST);
  ArduinoOTA.setPassword(OTA_PASS);
  ArduinoOTA.onStart([]() {
    Serial.println("OTA Start");
    digitalWrite(PIN_RELAY1, LOW);
    digitalWrite(PIN_RELAY2, LOW);
  });
  ArduinoOTA.onEnd([]() { Serial.println("\nOTA End"); });
  ArduinoOTA.onProgress([](unsigned int p, unsigned int t) {
    Serial.printf("OTA %u%%\r", (p * 100) / t);
  });
  ArduinoOTA.onError([](ota_error_t e) { Serial.printf("OTA Error %u\n", e); });
  ArduinoOTA.begin();
  otaReady = true;
  Serial.printf("OTA ready — upload to %s  password=%s\n",
                وای فای.localIP().toString().c_str(), OTA_PASS);
}

void setup() {
  disableBrownout();
  Serial.begin(115200);
  delay(1500);
  Serial.println();
  Serial.println("================================");
  Serial.println(" Goldon Yard — وای فای STA OTA");
  Serial.println("================================");

  pinMode(PIN_RELAY1, OUTPUT);
  pinMode(PIN_RELAY2, OUTPUT);
  digitalWrite(PIN_RELAY1, LOW);
  digitalWrite(PIN_RELAY2, LOW);
  analogReadResolution(12);
  dht.begin();
}

unsigned long lastPrint = 0;

void loop() {
  if (!وای فایStarted && millis() > 4000) {
    وای فایStarted = true;
    startوای فایOta();
  }

  if (otaReady) ArduinoOTA.handle();

  // اگر قطع شد دوباره وصل شو
  if (وای فایStarted && otaReady && وای فای.status() != WL_CONNECTED) {
    Serial.println("وای فای lost — reconnect");
    otaReady = false;
    وای فای.reconnect();
    unsigned long t0 = millis();
    while (وای فای.status() != WL_CONNECTED && millis() - t0 < 15000) delay(300);
    if (وای فای.status() == WL_CONNECTED) {
      ArduinoOTA.begin();
      otaReady = true;
      Serial.printf("Reconnected IP=%s\n", وای فای.localIP().toString().c_str());
    }
  }

  if (millis() - lastPrint > 5000) {
    lastPrint = millis();
    int s1 = analogRead(PIN_SOIL1);
    int s2 = analogRead(PIN_SOIL2);
    Serial.printf("وای فای=%d ip=%s soil1=%d soil2=%d\n",
                  وای فای.status() == WL_CONNECTED,
                  وای فای.status() == WL_CONNECTED ? وای فای.localIP().toString().c_str() : "-",
                  s1, s2);
  }
}
