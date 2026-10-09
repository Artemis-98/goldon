/*
 * گلدون هوشمند — برد ۳ (مودم / آنتن وای فای)
 *
 * نقش: پل بین پنل (برد ۲، وای فای) و حیاط (برد ۱، UART)
 *
 * سیم UART به برد ۱:
 *   Modem TX GPIO8  → Yard RX GPIO9
 *   Modem RX GPIO9  ← Yard TX GPIO8
 *   GND مشترک
 *
 * HTTP (همان API حیاط):
 *   GET /status
 *   GET /relay?n=1|2&state=0|1|toggle
 *   GET /ai?state=0|1|toggle
 *   GET /dhtscan
 */

#include <وای فای.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

static const char *وای فای_SSID = "YOUR_WIFI_SSID";
static const char *وای فای_PASS = "YOUR_WIFI_PASSWORD";
static const char *OTA_PASS = "YOUR_OTA_PASSWORD";
static const char *OTA_HOST = "goldon-modem";

static const int PIN_LINK_TX = 8;
static const int PIN_LINK_RX = 9;
static const uint32_t LINK_BAUD = 115200;
HardwareSerial LinkSerial(1);

WebServer server(80);

String lastYardJson = "{\"ok\":false,\"err\":\"no yard yet\",\"link\":\"modem\"}";
unsigned long lastYardMs = 0;
static const unsigned long UART_TIMEOUT_MS = 2500;

void disableBrownout() {
  CLEAR_PERI_REG_MASK(RTC_CNTL_BROWN_OUT_REG, RTC_CNTL_BROWN_OUT_ENA);
#ifdef RTC_CNTL_BROWN_OUT_RST_ENA
  CLEAR_PERI_REG_MASK(RTC_CNTL_BROWN_OUT_REG, RTC_CNTL_BROWN_OUT_RST_ENA);
#endif
}

void drainUart() {
  while (LinkSerial.available()) (void)LinkSerial.read();
}

// یک فرمان بفرست، یک خط JSON برگردان
bool uartExchange(const String &cmd, String &reply) {
  drainUart();
  LinkSerial.println(cmd);
  LinkSerial.flush();

  reply = "";
  unsigned long t0 = millis();
  while (millis() - t0 < UART_TIMEOUT_MS) {
    while (LinkSerial.available()) {
      char c = (char)LinkSerial.read();
      if (c == '\n' || c == '\r') {
        reply.trim();
        if (reply.length() > 0) {
          lastYardJson = reply;
          lastYardMs = millis();
          return true;
        }
      } else if (reply.length() < 600) {
        reply += c;
      } else {
        reply = "";
      }
    }
    delay(1);
    yield();
  }
  reply = "{\"ok\":false,\"err\":\"uart timeout\",\"link\":\"modem\"}";
  return false;
}

void sendCorsJson(const String &body) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", body);
}

void handleRoot() {
  server.send(200, "text/plain",
              "goldon-modem #3 — proxies to yard #1 via UART\n"
              "/status /relay /ai /dhtscan");
}

void handleStatus() {
  String reply;
  if (!uartExchange("STATUS", reply)) {
    sendCorsJson(reply);
    return;
  }
  // نشان بده از مودم آمده
  if (reply.indexOf("\"link\"") >= 0) {
    reply.replace("\"link\":\"uart\"", "\"link\":\"modem\"");
    reply.replace("\"link\":\"وای فای\"", "\"link\":\"modem\"");
  }
  sendCorsJson(reply);
}

void handleRelay() {
  if (!server.hasArg("n")) {
    sendCorsJson("{\"ok\":false,\"err\":\"missing n\"}");
    return;
  }
  int n = server.arg("n").toInt();
  if (n != 1 && n != 2) {
    sendCorsJson("{\"ok\":false,\"err\":\"n must be 1|2\"}");
    return;
  }

  int state = 0;
  if (server.hasArg("state")) {
    String s = server.arg("state");
    if (s == "toggle") {
      // وضعیت فعلی را بپرس
      String cur;
      if (uartExchange("STATUS", cur)) {
        String key = (n == 1) ? "\"relay1\":true" : "\"relay2\":true";
        state = cur.indexOf(key) >= 0 ? 0 : 1;
      } else {
        sendCorsJson(cur);
        return;
      }
    } else {
      state = s.toInt() != 0 ? 1 : 0;
    }
  }

  String cmd = String("RELAY,") + n + "," + state;
  String reply;
  uartExchange(cmd, reply);
  if (reply.indexOf("\"link\"") >= 0) {
    reply.replace("\"link\":\"uart\"", "\"link\":\"modem\"");
    reply.replace("\"link\":\"وای فای\"", "\"link\":\"modem\"");
  }
  sendCorsJson(reply);
}

void handleAi() {
  String arg = "toggle";
  if (server.hasArg("state")) arg = server.arg("state");
  String cmd = String("AI,") + arg;
  String reply;
  uartExchange(cmd, reply);
  if (reply.indexOf("\"link\"") >= 0) {
    reply.replace("\"link\":\"uart\"", "\"link\":\"modem\"");
    reply.replace("\"link\":\"وای فای\"", "\"link\":\"modem\"");
  }
  sendCorsJson(reply);
}

void handleDhtScan() {
  String reply;
  uartExchange("DHTSCAN", reply);
  sendCorsJson("{\"ok\":true,\"msg\":\"dhtscan forwarded to yard\"}");
}

void handlePulse() {
  if (!server.hasArg("pin")) {
    sendCorsJson("{\"ok\":false,\"err\":\"missing pin\"}");
    return;
  }
  int pin = server.arg("pin").toInt();
  String cmd = String("PULSE,") + pin;
  String reply;
  uartExchange(cmd, reply);
  if (reply.indexOf("\"link\"") >= 0) {
    reply.replace("\"link\":\"uart\"", "\"link\":\"modem\"");
    reply.replace("\"link\":\"وای فای\"", "\"link\":\"modem\"");
  }
  sendCorsJson(reply);
}

void handleSetRelay() {
  if (!server.hasArg("n") || !server.hasArg("pin")) {
    sendCorsJson("{\"ok\":false,\"err\":\"need n and pin\"}");
    return;
  }
  String cmd = String("SETRELAY,") + server.arg("n") + "," + server.arg("pin");
  String reply;
  uartExchange(cmd, reply);
  if (reply.indexOf("\"link\"") >= 0) {
    reply.replace("\"link\":\"uart\"", "\"link\":\"modem\"");
    reply.replace("\"link\":\"وای فای\"", "\"link\":\"modem\"");
  }
  sendCorsJson(reply);
}

void handleAdcScan() {
  String reply;
  if (!uartExchange("ADCSCAN", reply)) {
    // اسکن ممکن است بیشتر از ۸۰۰ms طول بکشد
    sendCorsJson(reply);
    return;
  }
  sendCorsJson(reply);
}

void handleSetSoil() {
  if (!server.hasArg("n") || !server.hasArg("pin")) {
    sendCorsJson("{\"ok\":false,\"err\":\"need n and pin\"}");
    return;
  }
  String cmd = String("SETSOIL,") + server.arg("n") + "," + server.arg("pin");
  String reply;
  uartExchange(cmd, reply);
  if (reply.indexOf("\"link\"") >= 0) {
    reply.replace("\"link\":\"uart\"", "\"link\":\"modem\"");
    reply.replace("\"link\":\"وای فای\"", "\"link\":\"modem\"");
  }
  sendCorsJson(reply);
}

void startHttp() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/relay", HTTP_GET, handleRelay);
  server.on("/ai", HTTP_GET, handleAi);
  server.on("/dhtscan", HTTP_GET, handleDhtScan);
  server.on("/pulse", HTTP_GET, handlePulse);
  server.on("/setrelay", HTTP_GET, handleSetRelay);
  server.on("/adcscan", HTTP_GET, handleAdcScan);
  server.on("/setsoil", HTTP_GET, handleSetSoil);
  server.begin();
  Serial.println("HTTP :80 proxy ready");
}

void setup() {
  disableBrownout();
  setCpuFrequencyMhz(160);
  Serial.begin(115200);
  delay(600);
  Serial.println();
  Serial.println("================================");
  Serial.println(" Goldon Modem #3 — وای فای + UART");
  Serial.println("================================");

  LinkSerial.begin(LINK_BAUD, SERIAL_8N1, PIN_LINK_RX, PIN_LINK_TX);
  Serial.printf("UART %lu baud  TX=GPIO%d RX=GPIO%d\n",
                (unsigned long)LINK_BAUD, PIN_LINK_TX, PIN_LINK_RX);
  Serial.println("Wire: Modem8->Yard9 , Modem9<-Yard8 , GND");

  وای فای.persistent(false);
  وای فای.mode(وای فای_STA);
  وای فای.setHostname(OTA_HOST);
  وای فای.begin(وای فای_SSID, وای فای_PASS);
  Serial.printf("وای فای %s ", وای فای_SSID);
  unsigned long t0 = millis();
  while (وای فای.status() != WL_CONNECTED && millis() - t0 < 40000) {
    delay(300);
    Serial.print('.');
  }
  Serial.println();
  if (وای فای.status() != WL_CONNECTED) {
    Serial.println("وای فای FAILED");
  } else {
    Serial.printf("وای فای OK IP=%s RSSI=%d\n",
                  وای فای.localIP().toString().c_str(), وای فای.RSSI());
  }

  if (MDNS.begin(OTA_HOST)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("mDNS http://%s.local/status\n", OTA_HOST);
  }

  ArduinoOTA.setHostname(OTA_HOST);
  ArduinoOTA.setPassword(OTA_PASS);
  ArduinoOTA.begin();
  startHttp();

  String probe;
  if (uartExchange("STATUS", probe)) {
    Serial.println("Yard UART OK");
    Serial.println(probe);
  } else {
    Serial.println("Yard UART no reply yet — check wires GND/8/9");
  }

  Serial.printf("OTA password=%s\n", OTA_PASS);
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
}
