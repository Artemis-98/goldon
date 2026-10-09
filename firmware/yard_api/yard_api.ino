/*
 * گلدون هوشمند — برد ۱ (حیاط)
 *
 * YARD_LINK_UART=1 → UART به برد ۳ (مودم) — بدون وای فای
 * YARD_LINK_UART=0 → وای فای STA + OTA + HTTP مستقیم
 *
 * UART به برد ۳ (مودم):
 *   Yard TX GPIO8  → Modem RX GPIO9
 *   Yard RX GPIO9  ← Modem TX GPIO8
 *   GND مشترک
 *
 * پروتکل متنی (یک خط، \n):
 *   Modem→Yard: STATUS
 *   Modem→Yard: RELAY,1,1   / RELAY,2,0
 *   Modem→Yard: AI,toggle   / AI,1 / AI,0
 *   Yard→Modem: {"ok":true,...}
 */

#define YARD_LINK_UART 1

#include <Preferences.h>
#include <DHT.h>
#include <string.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#if !YARD_LINK_UART
#include <وای فای.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#endif

#if YARD_LINK_UART
// لینک به برد ۳ (مودم)
static const int PIN_LINK_TX = 8;
static const int PIN_LINK_RX = 9;
static const uint32_t LINK_BAUD = 115200;
HardwareSerial LinkSerial(1);
#endif

#if !YARD_LINK_UART
static const char *وای فای_SSID = "YOUR_WIFI_SSID";
static const char *وای فای_PASS = "YOUR_WIFI_PASSWORD";
static const char *OTA_PASS = "YOUR_OTA_PASSWORD";
static const char *OTA_HOST = "goldon-yard";
#endif

// خاک ۱/۲ طبق سیم‌کشی کاربر — رله‌ها با پالس تأیید شد: ۱۰ و ۱۱
// DHT→4 | WATER→18 | SOIL1→1 | SOIL2→2 | RELAY1→10 | RELAY2→11 | LINK 8/9
static int PIN_SOIL1 = 1;
static int PIN_SOIL2 = 2;
static const int PIN_WATER = 18;
static int PIN_RELAY1 = 10;  // پمپ ۱ — ۱ پالس
static int PIN_RELAY2 = 11;  // پمپ ۲ — ۳ پالس

static int dhtPin = 4;
static uint8_t dhtType = DHT11;
static DHT *dht = nullptr;

static const int DHT_CANDIDATES[] = { 4 };
static const int DHT_CANDIDATE_COUNT = 1;

// تشخیص پایه رله: PULSE,<gpio> → ۲ثانیه HIGH روی همان پایه
static int pulsePin = -1;
static unsigned long pulseUntilMs = 0;

#if !YARD_LINK_UART
WebServer server(80);
#endif
Preferences prefs;

bool otaReady = false;
bool وای فایStarted = false;
bool وای فایBeginDone = false;
bool httpStarted = false;
bool otaConfigured = false;
unsigned long وای فایBeginMs = 0;
unsigned long lastوای فایDotMs = 0;

bool relay1On = false;
bool relay2On = false;
unsigned long relayOnSinceMs = 0;
static const unsigned long RELAY_MAX_ON_MS = 15000UL;
bool aiAuto = true;

float lastTemp = NAN;
float lastHum = NAN;
int lastSoil1 = 0;
int lastSoil2 = 0;
bool lastWater = false;
unsigned long lastSensorMs = 0;
unsigned long lastDhtOkMs = 0;
int dhtFailStreak = 0;
bool dhtLocked = false;
int dhtScanIdx = 0;

void disableBrownout() {
  CLEAR_PERI_REG_MASK(RTC_CNTL_BROWN_OUT_REG, RTC_CNTL_BROWN_OUT_ENA);
#ifdef RTC_CNTL_BROWN_OUT_RST_ENA
  CLEAR_PERI_REG_MASK(RTC_CNTL_BROWN_OUT_REG, RTC_CNTL_BROWN_OUT_RST_ENA);
#endif
}

void applyRelays() {
  if (pulsePin >= 0) return;  // حین پالس، رله عادی دست نزن
  digitalWrite(PIN_RELAY1, relay1On ? HIGH : LOW);
  digitalWrite(PIN_RELAY2, relay2On ? HIGH : LOW);
}

bool pulsePinAllowed(int pin) {
  if (pin < 0 || pin > 48) return false;
  if (pin == PIN_SOIL1 || pin == PIN_SOIL2) return false;
  if (pin == PIN_WATER || pin == dhtPin) return false;
#if YARD_LINK_UART
  if (pin == PIN_LINK_TX || pin == PIN_LINK_RX) return false;
#endif
  return true;
}

void endPulse() {
  if (pulsePin < 0) return;
  digitalWrite(pulsePin, LOW);
  Serial.printf("PULSE GPIO%d OFF\n", pulsePin);
  pulsePin = -1;
  pulseUntilMs = 0;
  applyRelays();
}

void startPulse(int pin, unsigned long ms = 2000) {
  if (!pulsePinAllowed(pin)) {
    Serial.printf("PULSE deny GPIO%d\n", pin);
    return;
  }
  endPulse();
  relay1On = false;
  relay2On = false;
  pinMode(PIN_RELAY1, OUTPUT);
  pinMode(PIN_RELAY2, OUTPUT);
  digitalWrite(PIN_RELAY1, LOW);
  digitalWrite(PIN_RELAY2, LOW);
  pinMode(pin, OUTPUT);
  digitalWrite(pin, HIGH);
  pulsePin = pin;
  pulseUntilMs = millis() + ms;
  Serial.printf("PULSE GPIO%d ON %lums\n", pin, (unsigned long)ms);
}

void tickPulse() {
  if (pulsePin < 0) return;
  if ((long)(millis() - pulseUntilMs) >= 0) endPulse();
}

void bindDht(int pin, uint8_t type) {
  if (dht) {
    delete dht;
    dht = nullptr;
  }
  dhtPin = pin;
  dhtType = type;
  pinMode(pin, INPUT_PULLUP);
  dht = new DHT(pin, type);
  dht->begin();
  Serial.printf("DHT bind GPIO%d type=%s\n", pin, type == DHT22 ? "DHT22" : "DHT11");
}

void startDhtRescan() {
  dhtLocked = false;
  dhtScanIdx = 0;
  dhtFailStreak = 0;
  lastTemp = NAN;
  lastHum = NAN;
  Serial.println("DHT rescan armed");
}

void tickDhtFind() {
  if (dhtLocked) return;

  static uint8_t phase = 0;
  static uint8_t tryType = DHT11;
  static unsigned long waitUntil = 0;
  static uint8_t readTries = 0;

  if (phase == 0) {
    if (dhtScanIdx >= DHT_CANDIDATE_COUNT) {
      dhtScanIdx = 0;
      bindDht(4, DHT11);
      tryType = DHT11;
      readTries = 0;
      waitUntil = millis() + 3000;
      phase = 1;
      return;
    }
    int pin = DHT_CANDIDATES[dhtScanIdx];
    if (pin == PIN_SOIL1 || pin == PIN_SOIL2 || pin == PIN_RELAY1 || pin == PIN_RELAY2 ||
        pin == PIN_WATER) {
      dhtScanIdx++;
      return;
    }
#if YARD_LINK_UART
    if (pin == PIN_LINK_TX || pin == PIN_LINK_RX) {
      dhtScanIdx++;
      return;
    }
#endif
    Serial.printf("DHT probe GPIO%d %s\n", pin, tryType == DHT22 ? "DHT22" : "DHT11");
    bindDht(pin, tryType);
    readTries = 0;
    waitUntil = millis() + 3000;  // DHT11 به ۲ثانیه+ نیاز دارد
    phase = 1;
    return;
  }

  if (phase == 1) {
    if (millis() < waitUntil) return;
    phase = 2;
  }

  if (phase == 2) {
    if (!dht) {
      phase = 0;
      return;
    }
    // چند بار پشت‌سرهم بخوان
    float h = dht->readHumidity();
    float t = dht->readTemperature();
    if (isnan(t) || isnan(h)) {
      delay(50);
      h = dht->readHumidity();
      t = dht->readTemperature();
    }
    if (!isnan(t) && !isnan(h) && h >= 0 && h <= 100 && t > -20 && t < 80) {
      lastTemp = t;
      lastHum = h;
      lastDhtOkMs = millis();
      dhtFailStreak = 0;
      dhtLocked = true;
      phase = 0;
      Serial.printf(">>> DHT LOCK %s GPIO%d  T=%.1f H=%.1f\n",
                    tryType == DHT22 ? "DHT22" : "DHT11", dhtPin, t, h);
      return;
    }
    readTries++;
    if (readTries < 3) {
      waitUntil = millis() + 1500;
      phase = 1;
      return;
    }
    if (tryType == DHT11) {
      tryType = DHT22;
      readTries = 0;
      phase = 0;
      return;
    }
    tryType = DHT11;
    dhtScanIdx++;
    readTries = 0;
    phase = 0;
  }
}

void setRelay(int n, bool on) {
  if (on) {
    if (n == 1) {
      relay1On = true;
      relay2On = false;
    } else if (n == 2) {
      relay2On = true;
      relay1On = false;
    }
  } else {
    if (n == 1) relay1On = false;
    else if (n == 2) relay2On = false;
  }
  applyRelays();
  if (on) relayOnSinceMs = millis();
  else if (!relay1On && !relay2On) relayOnSinceMs = 0;
}

int readSoilAvg(int pin) {
  pinMode(pin, INPUT);
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) {
    sum += analogRead(pin);
    delay(2);
  }
  return (int)(sum / 8);
}

void readSoilWater() {
  lastSoil1 = readSoilAvg(PIN_SOIL1);
  lastSoil2 = readSoilAvg(PIN_SOIL2);
  lastWater = digitalRead(PIN_WATER) == HIGH;
}

// اسکن ADC برای پیدا کردن پایهٔ واقعی سنسور خاک
int buildAdcScanJson(char *body, size_t bodySize) {
  static const int pins[] = {
    1, 2, 3, 5, 6, 7, 12, 13, 14, 15, 16, 17, 19, 20, 21
  };
  const int n = sizeof(pins) / sizeof(pins[0]);
  int off = snprintf(body, bodySize, "{\"ok\":true,\"adc\":[");
  for (int i = 0; i < n && off > 0 && (size_t)off < bodySize - 40; i++) {
    int p = pins[i];
    if (p == PIN_RELAY1 || p == PIN_RELAY2 || p == dhtPin) continue;
#if YARD_LINK_UART
    if (p == PIN_LINK_TX || p == PIN_LINK_RX) continue;
#endif
    int v = readSoilAvg(p);
    off += snprintf(body + off, bodySize - off,
                    "%s{\"pin\":%d,\"v\":%d}",
                    (off > 20 && body[off - 1] != '[') ? "," : "",
                    p, v);
  }
  snprintf(body + off, bodySize - off,
           "],\"soil1_pin\":%d,\"soil2_pin\":%d}",
           PIN_SOIL1, PIN_SOIL2);
  return (int)strlen(body);
}

void readDht() {
  if (!dht || !dhtLocked) return;
  float t = dht->readTemperature();
  float h = dht->readHumidity();
  if (!isnan(t) && !isnan(h)) {
    lastTemp = t;
    lastHum = h;
    lastDhtOkMs = millis();
    dhtFailStreak = 0;
  } else {
    dhtFailStreak++;
    if (dhtFailStreak >= 15) {
      Serial.println("DHT lost — rescan");
      startDhtRescan();
    }
  }
}

void readSensors() {
  readSoilWater();
  readDht();
  lastSensorMs = millis();
}

void saveAiAuto() {
  prefs.begin("goldon", false);
  prefs.putBool("ai_auto", aiAuto);
  prefs.end();
}

void loadAiAuto() {
  prefs.begin("goldon", true);
  aiAuto = prefs.getBool("ai_auto", true);
  prefs.end();
}

// ساخت JSON وضعیت (مشترک UART / HTTP)
int buildStatusJson(char *body, size_t bodySize) {
  lastSoil1 = analogRead(PIN_SOIL1);
  lastSoil2 = analogRead(PIN_SOIL2);
  lastWater = digitalRead(PIN_WATER) == HIGH;

  char tempBuf[16];
  char humBuf[16];
  if (isnan(lastTemp)) strcpy(tempBuf, "null");
  else snprintf(tempBuf, sizeof(tempBuf), "%.1f", lastTemp);
  if (isnan(lastHum)) strcpy(humBuf, "null");
  else snprintf(humBuf, sizeof(humBuf), "%.1f", lastHum);

#if YARD_LINK_UART
  const char *ip = "uart";
  int rssi = 0;
#else
  String ipStr = وای فای.localIP().toString();
  const char *ip = ipStr.c_str();
  int rssi = وای فای.RSSI();
#endif

  return snprintf(body, bodySize,
                  "{\"ok\":true,"
                  "\"soil1\":%d,\"soil2\":%d,"
                  "\"soil1_pin\":%d,\"soil2_pin\":%d,"
                  "\"temp\":%s,\"hum\":%s,"
                  "\"dht_pin\":%d,\"dht_type\":\"%s\",\"dht_ok\":%s,\"dht_scan\":%d,"
                  "\"water\":%s,"
                  "\"relay1\":%s,\"relay2\":%s,"
                  "\"relay1_pin\":%d,\"relay2_pin\":%d,"
                  "\"pulse_pin\":%d,"
                  "\"ai_auto\":%s,"
                  "\"rssi\":%d,\"ip\":\"%s\","
                  "\"link\":\"%s\","
                  "\"uptime_ms\":%lu}",
                  lastSoil1, lastSoil2,
                  PIN_SOIL1, PIN_SOIL2,
                  tempBuf, humBuf,
                  dhtPin, dhtType == DHT22 ? "DHT22" : "DHT11",
                  dhtLocked ? "true" : "false",
                  dhtScanIdx,
                  lastWater ? "true" : "false",
                  relay1On ? "true" : "false",
                  relay2On ? "true" : "false",
                  PIN_RELAY1, PIN_RELAY2,
                  pulsePin,
                  aiAuto ? "true" : "false",
                  rssi, ip,
#if YARD_LINK_UART
                  "uart",
#else
                  "وای فای",
#endif
                  (unsigned long)millis());
}

void tickRelaySafety() {
  if ((relay1On || relay2On) && relayOnSinceMs > 0 &&
      (millis() - relayOnSinceMs > RELAY_MAX_ON_MS)) {
    Serial.println("SAFETY: relay max-on — forcing OFF");
    relay1On = false;
    relay2On = false;
    relayOnSinceMs = 0;
    applyRelays();
  }
}

void tickSensors() {
  if (!dhtLocked) tickDhtFind();
  if (dhtLocked && millis() - lastSensorMs > 2500) {
    readSensors();
  } else if (!dhtLocked && millis() - lastSensorMs > 2500) {
    readSoilWater();
    lastSensorMs = millis();
  }
}

#if YARD_LINK_UART
Stream *cmdOut = &LinkSerial;

void replyStatusUart() {
  char body[512];
  buildStatusJson(body, sizeof(body));
  if (cmdOut) cmdOut->println(body);
  if (cmdOut != &Serial) Serial.println(body);
}

void handleUartLine(String line) {
  line.trim();
  if (!line.length()) return;
  Serial.printf("CMD: %s\n", line.c_str());

  if (line.equalsIgnoreCase("HELP") || line.equalsIgnoreCase("?")) {
    const char *help =
        "cmds: STATUS|PING|RELAY,n,0|1|PULSE,gpio|SETRELAY,n,gpio|"
        "AI,toggle|DHTSCAN|ADCSCAN|SETSOIL,n,gpio";
    if (cmdOut) cmdOut->println(help);
    if (cmdOut != &Serial) Serial.println(help);
    return;
  }

  if (line.equalsIgnoreCase("STATUS") || line.equalsIgnoreCase("PING")) {
    replyStatusUart();
    return;
  }

  if (line.startsWith("RELAY,")) {
    int c1 = line.indexOf(',');
    int c2 = line.indexOf(',', c1 + 1);
    if (c1 > 0 && c2 > c1) {
      int n = line.substring(c1 + 1, c2).toInt();
      int st = line.substring(c2 + 1).toInt();
      if (n == 1 || n == 2) {
        endPulse();
        setRelay(n, st != 0);
        Serial.printf("Relay %d -> %s\n", n, st ? "ON" : "OFF");
      }
    }
    replyStatusUart();
    return;
  }

  if (line.startsWith("PULSE,")) {
    int pin = line.substring(6).toInt();
    startPulse(pin, 2000);
    replyStatusUart();
    return;
  }

  if (line.startsWith("SETRELAY,")) {
    int c1 = line.indexOf(',');
    int c2 = line.indexOf(',', c1 + 1);
    if (c1 > 0 && c2 > c1) {
      int n = line.substring(c1 + 1, c2).toInt();
      int pin = line.substring(c2 + 1).toInt();
      if (pulsePinAllowed(pin) && (n == 1 || n == 2)) {
        endPulse();
        if (n == 1) {
          pinMode(PIN_RELAY1, OUTPUT);
          digitalWrite(PIN_RELAY1, LOW);
          PIN_RELAY1 = pin;
        } else {
          pinMode(PIN_RELAY2, OUTPUT);
          digitalWrite(PIN_RELAY2, LOW);
          PIN_RELAY2 = pin;
        }
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
        applyRelays();
        Serial.printf("SET RELAY%d = GPIO%d\n", n, pin);
      }
    }
    replyStatusUart();
    return;
  }

  if (line.startsWith("AI,")) {
    String a = line.substring(3);
    a.trim();
    if (a.equalsIgnoreCase("toggle")) aiAuto = !aiAuto;
    else if (a == "1" || a.equalsIgnoreCase("on")) aiAuto = true;
    else if (a == "0" || a.equalsIgnoreCase("off")) aiAuto = false;
    saveAiAuto();
    Serial.printf("AI auto -> %s\n", aiAuto ? "ON" : "OFF");
    replyStatusUart();
    return;
  }

  if (line.equalsIgnoreCase("DHTSCAN")) {
    startDhtRescan();
    replyStatusUart();
    return;
  }

  if (line.equalsIgnoreCase("ADCSCAN")) {
    char body[700];
    buildAdcScanJson(body, sizeof(body));
    if (cmdOut) cmdOut->println(body);
    if (cmdOut != &Serial) Serial.println(body);
    return;
  }

  if (line.startsWith("SETSOIL,")) {
    int c1 = line.indexOf(',');
    int c2 = line.indexOf(',', c1 + 1);
    if (c1 > 0 && c2 > c1) {
      int n = line.substring(c1 + 1, c2).toInt();
      int pin = line.substring(c2 + 1).toInt();
      if (pin >= 0 && pin <= 48 && (n == 1 || n == 2)) {
        if (n == 1) PIN_SOIL1 = pin;
        else PIN_SOIL2 = pin;
        pinMode(pin, INPUT);
        readSoilWater();
        Serial.printf("SET SOIL%d = GPIO%d  v=%d\n",
                      n, pin, n == 1 ? lastSoil1 : lastSoil2);
      }
    }
    replyStatusUart();
    return;
  }

  const char *err = "{\"ok\":false,\"err\":\"bad cmd\"}";
  if (cmdOut) cmdOut->println(err);
  if (cmdOut != &Serial) Serial.println(err);
}

void serviceStream(Stream &s, String &buf, Stream &reply) {
  while (s.available()) {
    char c = (char)s.read();
    if (c == '\n' || c == '\r') {
      if (buf.length()) {
        cmdOut = &reply;
        handleUartLine(buf);
        buf = "";
      }
    } else if (buf.length() < 120) {
      buf += c;
    } else {
      buf = "";
    }
  }
}

void serviceUart() {
  static String linkBuf;
  static String usbBuf;
  serviceStream(LinkSerial, linkBuf, LinkSerial);
  serviceStream(Serial, usbBuf, Serial);
}

void tickUartHeartbeat() {
  static unsigned long lastHb = 0;
  if (millis() - lastHb < 2000) return;
  lastHb = millis();
}
#endif

#if !YARD_LINK_UART
void sendStatusHttp() {
  char body[512];
  buildStatusJson(body, sizeof(body));
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", body);
}

void handleStatus() { sendStatusHttp(); }

void handleRelay() {
  if (!server.hasArg("n")) {
    server.send(400, "application/json", "{\"ok\":false,\"err\":\"missing n\"}");
    return;
  }
  int n = server.arg("n").toInt();
  if (n != 1 && n != 2) {
    server.send(400, "application/json", "{\"ok\":false,\"err\":\"n must be 1|2\"}");
    return;
  }
  bool on = false;
  if (server.hasArg("state")) {
    String s = server.arg("state");
    if (s == "toggle") on = (n == 1) ? !relay1On : !relay2On;
    else on = s.toInt() != 0;
  }
  setRelay(n, on);
  sendStatusHttp();
}

void handleAi() {
  if (server.hasArg("state")) {
    String s = server.arg("state");
    if (s == "toggle") aiAuto = !aiAuto;
    else aiAuto = s.toInt() != 0;
    saveAiAuto();
  }
  sendStatusHttp();
}

void handleDhtScan() {
  startDhtRescan();
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json",
              "{\"ok\":true,\"msg\":\"rescan started — poll /status\"}");
}

void handleRoot() {
  server.send(200, "text/plain",
              "goldon-yard — /status /relay /ai /dhtscan");
}

void startHttp() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/relay", HTTP_GET, handleRelay);
  server.on("/ai", HTTP_GET, handleAi);
  server.on("/dhtscan", HTTP_GET, handleDhtScan);
  server.begin();
  Serial.println("HTTP :80");
}

void finishوای فایServices() {
  if (otaReady) return;
  if (وای فای.status() != WL_CONNECTED) return;

  وای فای.setSleep(true);
  وای فای.setTxPower(وای فای_POWER_MINUS_1dBm);
  {
    Preferences p;
    p.begin("goldon", false);
    p.putUInt("boot_fail", 0);
    p.end();
  }

  Serial.printf("\nوای فای OK  IP=%s  RSSI=%d\n",
                وای فای.localIP().toString().c_str(), وای فای.RSSI());

  if (!otaConfigured) {
    if (MDNS.begin(OTA_HOST)) {
      MDNS.addService("http", "tcp", 80);
    }
    ArduinoOTA.setHostname(OTA_HOST);
    ArduinoOTA.setPassword(OTA_PASS);
    ArduinoOTA.onStart([]() {
      relay1On = false;
      relay2On = false;
      applyRelays();
    });
    ArduinoOTA.begin();
    otaConfigured = true;
  }
  ArduinoOTA.begin();
  otaReady = true;
  if (!httpStarted) {
    startHttp();
    httpStarted = true;
  }
  if (!dhtLocked) {
    bindDht(4, DHT11);
    startDhtRescan();
  }
  Serial.printf("OTA ready password=%s\n", OTA_PASS);
}

void beginوای فایAsync() {
  disableBrownout();
  setCpuFrequencyMhz(80);
  Serial.printf("Connecting وای فای SSID=%s ...\n", وای فای_SSID);
  Serial.println("Need stable 5V (main PSU). USB-alone usually brownouts.");
  وای فای.persistent(false);
  وای فای.disconnect(true);
  delay(100);
  وای فای.mode(وای فای_STA);
  delay(500);
  وای فای.setSleep(وای فای_PS_MIN_MODEM);
  وای فای.setTxPower(وای فای_POWER_MINUS_1dBm);
  delay(200);
  وای فای.begin(وای فای_SSID, وای فای_PASS);
  وای فایBeginDone = true;
  وای فایBeginMs = millis();
  lastوای فایDotMs = millis();
}
#endif

void setup() {
  disableBrownout();
  Serial.begin(115200);
  delay(800);
  Serial.println();
  Serial.println("================================");
#if YARD_LINK_UART
  Serial.println(" Goldon Yard #1 — UART link");
  Serial.println(" (وای فای disabled)");
#else
  Serial.println(" Goldon Yard #1 — وای فای OTA");
#endif
  Serial.println("================================");

  pinMode(PIN_RELAY1, OUTPUT);
  pinMode(PIN_RELAY2, OUTPUT);
  pinMode(PIN_SOIL1, INPUT);
  pinMode(PIN_SOIL2, INPUT);
  pinMode(PIN_WATER, INPUT);
  relay1On = false;
  relay2On = false;
  applyRelays();
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  loadAiAuto();
  Serial.printf("AI auto = %s\n", aiAuto ? "ON" : "OFF");
  readSoilWater();

#if YARD_LINK_UART
  bindDht(4, DHT11);
  startDhtRescan();
  LinkSerial.begin(LINK_BAUD, SERIAL_8N1, PIN_LINK_RX, PIN_LINK_TX);
  Serial.printf("UART link %lu baud  TX=GPIO%d RX=GPIO%d\n",
                (unsigned long)LINK_BAUD, PIN_LINK_TX, PIN_LINK_RX);
  Serial.println("Wire: Yard8->Modem9 , Yard9<-Modem8 , GND");
  Serial.println("USB terminal cmds: STATUS | RELAY,1,1 | HELP");
  replyStatusUart();
#else
  // DHT را بعد از وای فای شروع کن — روی USB باعث WDT/brownout می‌شود
  setCpuFrequencyMhz(80);
  Serial.println("وای فای mode — sensors after connect");
  Serial.println("وای فای in 2s...");
#endif
}

void loop() {
  tickPulse();
  tickRelaySafety();
#if YARD_LINK_UART
  tickSensors();
  serviceUart();
#else
  if (otaReady) tickSensors();

  if (!وای فایStarted && millis() > 5000) {
    وای فایStarted = true;
    beginوای فایAsync();
  }
  if (وای فایBeginDone && !otaReady) {
    if (وای فای.status() == WL_CONNECTED) finishوای فایServices();
    else if (millis() - lastوای فایDotMs > 500) {
      lastوای فایDotMs = millis();
      Serial.print('.');
      yield();
      if (millis() - وای فایBeginMs > 40000) {
        وای فایBeginMs = millis();
        وای فای.disconnect(false);
        delay(300);
        وای فای.begin(وای فای_SSID, وای فای_PASS);
      }
    }
  }
  if (!otaReady && millis() - lastSensorMs > 3000) {
    readSoilWater();
    lastSensorMs = millis();
  }
  if (otaReady) {
    ArduinoOTA.handle();
    server.handleClient();
  }
  if (otaReady && وای فای.status() != WL_CONNECTED) {
    otaReady = false;
    وای فایBeginMs = millis();
    وای فای.reconnect();
  }
#endif
}
