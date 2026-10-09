/*
 * رله ۱ و ۲ به نوبت ON/OFF + OTA روی وای‌فای خانه
 */

#include <وای فای.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

static const char *وای فای_SSID = "YOUR_WIFI_SSID";
static const char *وای فای_PASS = "YOUR_WIFI_PASSWORD";
static const char *OTA_PASS = "YOUR_OTA_PASSWORD";
static const char *OTA_HOST = "goldon-yard";

static const int PIN_RELAY1 = 10;
static const int PIN_RELAY2 = 11;

bool otaReady = false;

void disableBrownout() {
  CLEAR_PERI_REG_MASK(RTC_CNTL_BROWN_OUT_REG, RTC_CNTL_BROWN_OUT_ENA);
#ifdef RTC_CNTL_BROWN_OUT_RST_ENA
  CLEAR_PERI_REG_MASK(RTC_CNTL_BROWN_OUT_REG, RTC_CNTL_BROWN_OUT_RST_ENA);
#endif
}

void setupوای فایOta() {
  disableBrownout();
  وای فای.persistent(false);
  وای فای.mode(وای فای_STA);
  delay(150);
  وای فای.setTxPower(وای فای_POWER_8_5dBm);
  وای فای.begin(وای فای_SSID, وای فای_PASS);

  Serial.printf("وای فای %s ", وای فای_SSID);
  unsigned long t0 = millis();
  while (وای فای.status() != WL_CONNECTED && millis() - t0 < 25000) {
    delay(350);
    Serial.print('.');
  }
  Serial.println();

  if (وای فای.status() != WL_CONNECTED) {
    Serial.println("وای فای FAIL");
    return;
  }

  Serial.printf("وای فای OK IP=%s RSSI=%d\n", وای فای.localIP().toString().c_str(), وای فای.RSSI());
  MDNS.begin(OTA_HOST);

  ArduinoOTA.setHostname(OTA_HOST);
  ArduinoOTA.setPassword(OTA_PASS);
  ArduinoOTA.onStart([]() {
    digitalWrite(PIN_RELAY1, LOW);
    digitalWrite(PIN_RELAY2, LOW);
    Serial.println("OTA Start");
  });
  ArduinoOTA.onEnd([]() { Serial.println("\nOTA End"); });
  ArduinoOTA.onError([](ota_error_t e) { Serial.printf("OTA Err %u\n", e); });
  ArduinoOTA.begin();
  otaReady = true;
  Serial.printf("OTA ready -> %s  pass=%s\n", وای فای.localIP().toString().c_str(), OTA_PASS);
}

void setup() {
  disableBrownout();
  Serial.begin(115200);
  delay(1200);
  Serial.println();
  Serial.println("Relay seq + وای فای OTA");

  pinMode(PIN_RELAY1, OUTPUT);
  pinMode(PIN_RELAY2, OUTPUT);
  digitalWrite(PIN_RELAY1, LOW);
  digitalWrite(PIN_RELAY2, LOW);

  delay(2500);
  setupوای فایOta();
}

void loop() {
  if (otaReady) ArduinoOTA.handle();

  // رله ۱
  Serial.println("R1 ON");
  digitalWrite(PIN_RELAY1, HIGH);
  digitalWrite(PIN_RELAY2, LOW);
  for (int i = 0; i < 10; i++) { if (otaReady) ArduinoOTA.handle(); delay(100); }

  Serial.println("R1 OFF");
  digitalWrite(PIN_RELAY1, LOW);
  for (int i = 0; i < 10; i++) { if (otaReady) ArduinoOTA.handle(); delay(100); }

  // رله ۲
  Serial.println("R2 ON");
  digitalWrite(PIN_RELAY2, HIGH);
  digitalWrite(PIN_RELAY1, LOW);
  for (int i = 0; i < 10; i++) { if (otaReady) ArduinoOTA.handle(); delay(100); }

  Serial.println("R2 OFF");
  digitalWrite(PIN_RELAY2, LOW);
  for (int i = 0; i < 10; i++) { if (otaReady) ArduinoOTA.handle(); delay(100); }
}
