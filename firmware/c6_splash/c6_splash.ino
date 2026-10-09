/*
 * برد ۴ — ESP32-C6-LCD-1.47 (۱۷۲×۳۲۰)
 * صفحات: SPLASH → WEATHER → CLIMATE → SOIL → PUMP1 → PUMP2
 * BOOT (GPIO9): تک‌کلیک = صفحه بعد | در صفحات پمپ دوبار = روشن/خاموش رله
 */

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <JPEGDEC.h>
#include <LittleFS.h>
#include <SPI.h>
#include <وای فای.h>
#include <time.h>
#include <math.h>

// ---------- سخت‌افزار ----------
static const int PIN_TFT_MOSI = 6;
static const int PIN_TFT_SCLK = 7;
static const int PIN_TFT_CS   = 14;
static const int PIN_TFT_DC   = 15;
static const int PIN_TFT_RST  = 21;
static const int PIN_TFT_BL   = 22;
static const int PIN_RGB      = 8;
static const int PIN_BOOT     = 9;  // Waveshare C6-LCD-1.47

static const int SCR_W = 172;
static const int SCR_H = 320;
static const int NAV_H = 24;
static const int PAD = 8;
static const int CARD_W = SCR_W - 2 * PAD;  // 156

// ---------- شبکه ----------
static const char *وای فای_SSID = "YOUR_WIFI_SSID";
static const char *وای فای_PASS = "YOUR_WIFI_PASSWORD";
static const char *YARD_BASES[] = {
  "http://192.168.70.193",
  "http://goldon-modem.local",
  "http://goldon-yard.local",
};
static const int YARD_BASE_COUNT = 3;

static const float SIRJAN_LAT = 29.4514f;
static const float SIRJAN_LON = 55.6809f;
static const int FORECAST_DAYS = 5;
static const unsigned long POLL_MS = 8000;
static const unsigned long وای فای_RETRY_MS = 12000;
static const unsigned long BOOT_DEBOUNCE_MS = 40;
static const unsigned long BOOT_DOUBLE_MS = 450;
static const int PUMP_DUR_SEC = 30;
static const int SOIL_ADC_DRY = 3500;
static const int SOIL_ADC_WET = 1000;

static const int SPLASH_FRAME_COUNT = 61;
static const unsigned long SPLASH_DURATION_MS = 5042;
static const int JPG_BUF_MAX = 8192;

// ---------- پالت (همان panel-pages.html) ----------
static const uint16_t WX_BG = 0x1106, WX_TODAY = 0x1C9F, WX_METRICS = 0x1BDC;
static const uint16_t WX_BADGE = 0xFE8F, WX_CARD = 0x1988, WX_CARD_BD = 0x2A4C;
static const uint16_t WX_MUTE = 0x7C95, WX_WD = 0xC6BC, WX_SLASH = 0x6C12;
static const uint16_t WX_RAIN_BG = 0x1A4D, WX_RAIN_FG = 0x7E5F, WX_DRY_BG = 0x39A4;
static const uint16_t WX_LINE = 0x21AA, WX_CITY = 0xDEFB, WX_HL = 0xDEFB;
static const uint16_t WX_MLABEL = 0xCE7D, WX_MBORDER = 0x4D1E;

static const uint16_t CL_BG = 0x10C4, CL_LINE = 0x29C9;
static const uint16_t CL_TEMP_BG = 0x1947, CL_TEMP_BD = 0x3AAA, CL_TEMP_AC = 0xF449;
static const uint16_t CL_TEMP_LB = 0xF591, CL_TEMP_BB = 0x3944, CL_TEMP_VL = 0xF71A;
static const uint16_t CL_HUM_BG = 0x1966, CL_HUM_BD = 0x330E, CL_HUM_AC = 0x4E3D;
static const uint16_t CL_HUM_LB = 0x8EDE, CL_HUM_BB = 0x1DA8, CL_HUM_VL = 0xD7BF;

static const uint16_t SO_BG = 0x1104, SO_LINE = 0x21E6, SO_CARD = 0x1964, SO_CARD_BD = 0x3287;
static const uint16_t SO_AC_A = 0x3E71, SO_AC_B = 0x6E6E, SO_NAME = 0xC733;
static const uint16_t SO_OK_BG = 0x1A66, SO_OK_FG = 0x3E71, SO_DRY_BG = 0x49C3, SO_DRY_FG = 0xFE8F;
static const uint16_t SO_WET_BG = 0x1A0A, SO_WET_FG = 0x7E5F, SO_META = 0x8D73, SO_META_S = 0xD753;
static const uint16_t SO_BAR_BG = 0x08C2, SO_BAR_BD = 0x2A06, SO_BAR_LB = 0x6C6F;

static const uint16_t PU_BG = 0x1882;
static const uint16_t PU1_HERO = 0x1D4A, PU1_HERO_B = 0x3B52, PU1_TXT = 0x9E7F, PU1_NEXT = 0x7E5F, PU1_BTN_B = 0x5CF9;
static const uint16_t PU2_HERO = 0x1DE6, PU2_HERO_B = 0x3BCB, PU2_TXT = 0x8ED6, PU2_NEXT = 0x5E73, PU2_BTN_B = 0x4CD0;
static const uint16_t PU_BLK = 0x20E3, PU_BLK_BD = 0x49E6, PU_LBL = 0x8BCF, PU_VAL = 0xF733;
static const uint16_t PU_CHIP = 0x18C2, PU_CHIP_BD = 0x3164, PU_CHIP_L = 0x6B0A, PU_CHIP_V = 0xDE76;
static const uint16_t BG_BEIGE = 0xCE79;

enum Page : uint8_t {
  PAGE_SPLASH = 0,
  PAGE_WEATHER,
  PAGE_CLIMATE,
  PAGE_SOIL,
  PAGE_PUMP1,
  PAGE_PUMP2,
  PAGE_COUNT
};

struct YardStatus {
  bool ok = false;
  int soil1 = 0, soil2 = 0;
  float temp = NAN, hum = NAN;
  bool water = false, relay1 = false, relay2 = false, aiAuto = true;
  int rssi = 0;
  String ip, err;
};

struct DayForecast {
  char date[11] = "";
  int code = -1;
  float tMax = NAN, tMin = NAN;
  int precipProb = -1;
};

struct WeatherData {
  bool ok = false;
  float temp = NAN, hum = NAN, wind = NAN, feels = NAN, uv = NAN;
  int code = -1;
  String err;
  DayForecast days[FORECAST_DAYS];
  int dayCount = 0;
};

Adafruit_ST7789 tft = Adafruit_ST7789(&SPI, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);
Adafruit_NeoPixel rgb(1, PIN_RGB, NEO_GRB + NEO_KHZ800);
JPEGDEC jpeg;

YardStatus yard;
WeatherData weather;
Page page = PAGE_SPLASH;
String yardBase = YARD_BASES[0];
bool fsOk = false;
bool timeOk = false;
unsigned long lastPollMs = 0;
unsigned long lastوای فایTryMs = 0;
unsigned long splashStartMs = 0;
int splashFrame = 0;
unsigned long pump1Until = 0, pump2Until = 0;
time_t pump1Last = 0, pump2Last = 0;

bool bootDown = false;
unsigned long bootDownMs = 0;
unsigned long bootLastEdgeMs = 0;
unsigned long bootPendingMs = 0;
bool bootPendingNext = false;
uint8_t bootClicks = 0;

static uint8_t jpgBuf[JPG_BUF_MAX];

// ---------- متن ----------
int textW(const char *s, uint8_t sz) {
  return 6 * (int)sz * (int)strlen(s);
}

void textAt(int x, int y, const char *s, uint16_t fg, uint16_t bg, uint8_t sz) {
  tft.setTextSize(sz);
  tft.setTextColor(fg, bg);
  tft.setCursor(x, y);
  tft.print(s);
}

void textCenterX(int cx, int y, const char *s, uint16_t fg, uint16_t bg, uint8_t sz) {
  textAt(cx - textW(s, sz) / 2, y, s, fg, bg, sz);
}

void textRight(int rx, int y, const char *s, uint16_t fg, uint16_t bg, uint8_t sz) {
  textAt(rx - textW(s, sz), y, s, fg, bg, sz);
}

const char *wmoLabel(int code) {
  if (code == 0) return "Clear";
  if (code == 1) return "Mainly clr";
  if (code == 2) return "Partly";
  if (code == 3) return "Overcast";
  if (code == 45 || code == 48) return "Fog";
  if (code >= 51 && code <= 57) return "Drizzle";
  if (code >= 61 && code <= 67) return "Rain";
  if (code >= 71 && code <= 77) return "Snow";
  if (code >= 80 && code <= 82) return "Showers";
  if (code >= 95 && code <= 99) return "Thunder";
  if (code < 0) return "--";
  return "Clouds";
}

const char *weekdayShort(const char *ymd) {
  if (!ymd || strlen(ymd) < 10) return "---";
  struct tm t = {};
  if (sscanf(ymd, "%d-%d-%d", &t.tm_year, &t.tm_mon, &t.tm_mday) != 3) return "---";
  t.tm_year -= 1900;
  t.tm_mon -= 1;
  t.tm_isdst = -1;
  mktime(&t);
  static const char *names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  return names[t.tm_wday];
}

bool isRainy(int code, int precipProb) {
  if (precipProb >= 50) return true;
  return (code >= 51 && code <= 67) || (code >= 80 && code <= 99);
}

int soilPct(int adc) {
  int a = constrain(adc, SOIL_ADC_WET, SOIL_ADC_DRY);
  return map(a, SOIL_ADC_DRY, SOIL_ADC_WET, 0, 100);
}

// ---------- JPEG اسپلش ----------
int JPEGDraw(JPEGDRAW *pDraw) {
  tft.startWrite();
  tft.setAddrWindow(pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight);
  tft.writePixels(pDraw->pPixels, (uint32_t)pDraw->iWidth * pDraw->iHeight, true, false);
  tft.endWrite();
  return 1;
}

bool drawSplashFrame(int n) {
  if (!fsOk) return false;
  char path[28];
  snprintf(path, sizeof(path), "/splash/f_%03d.jpg", n);
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  size_t sz = f.size();
  if (sz == 0 || sz > sizeof(jpgBuf)) {
    f.close();
    return false;
  }
  size_t got = f.read(jpgBuf, sz);
  f.close();
  if (got != sz) return false;
  if (!jpeg.openRAM(jpgBuf, (int)sz, JPEGDraw)) return false;
  jpeg.setPixelType(RGB565_LITTLE_ENDIAN);
  bool ok = jpeg.decode(0, 0, 0);
  jpeg.close();
  return ok;
}

void setRgb(uint8_t r, uint8_t g, uint8_t b) {
  rgb.setPixelColor(0, rgb.Color(r, g, b));
  rgb.show();
}

void updateRgbBreath(unsigned long t) {
  float phase = (t % 2000) / 2000.0f;
  float s = phase < 0.5f ? (phase * 2.0f) : (2.0f - phase * 2.0f);
  uint8_t v = (uint8_t)(20 + s * 80);
  setRgb(0, v, 18);
}

// ---------- شبکه / حیاط ----------
bool parseStatusPayload(const String &payload) {
  JsonDocument doc;
  if (deserializeJson(doc, payload)) {
    yard.err = "bad json";
    return false;
  }
  yard.ok = doc["ok"] | false;
  yard.soil1 = doc["soil1"] | 0;
  yard.soil2 = doc["soil2"] | 0;
  yard.water = doc["water"] | false;
  yard.relay1 = doc["relay1"] | false;
  yard.relay2 = doc["relay2"] | false;
  yard.aiAuto = doc["ai_auto"] | true;
  yard.rssi = doc["rssi"] | 0;
  yard.ip = doc["ip"] | "";
  yard.temp = doc["temp"].isNull() ? NAN : doc["temp"].as<float>();
  yard.hum = doc["hum"].isNull() ? NAN : doc["hum"].as<float>();
  if (!yard.ok) {
    yard.err = "ok=false";
    return false;
  }
  yard.err = "";
  return true;
}

bool httpGet(const String &url, String &outBody, int timeoutMs = 1200) {
  وای فایClient client;
  HTTPClient http;
  http.setTimeout(timeoutMs);
  http.setConnectTimeout(timeoutMs);
  http.setReuse(false);
  if (!http.begin(client, url)) return false;
  int code = http.GET();
  if (code != 200) {
    yard.err = String("HTTP ") + code;
    http.end();
    return false;
  }
  outBody = http.getString();
  http.end();
  return true;
}

bool fetchYard() {
  if (وای فای.status() != WL_CONNECTED) {
    yard.err = "no وای فای";
    yard.ok = false;
    return false;
  }
  String body;
  for (int i = 0; i < YARD_BASE_COUNT; i++) {
    if (httpGet(String(YARD_BASES[i]) + "/status", body) && parseStatusPayload(body)) {
      yardBase = YARD_BASES[i];
      return true;
    }
  }
  yard.ok = false;
  if (!yard.err.length()) yard.err = "no yard";
  return false;
}

bool setRelay(int n, bool on) {
  if (وای فای.status() != WL_CONNECTED) return false;
  String state = on ? "1" : "0";
  String body;
  auto tryUrl = [&](const String &base) -> bool {
    return httpGet(base + "/relay?n=" + String(n) + "&state=" + state, body) &&
           parseStatusPayload(body);
  };
  if (tryUrl(yardBase)) return true;
  for (int i = 0; i < YARD_BASE_COUNT; i++) {
    if (tryUrl(YARD_BASES[i])) {
      yardBase = YARD_BASES[i];
      return true;
    }
  }
  return false;
}

bool fetchWeather() {
  if (وای فای.status() != WL_CONNECTED) {
    weather.err = "no وای فای";
    return false;
  }
  char url[320];
  snprintf(url, sizeof(url),
           "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,relative_humidity_2m,apparent_temperature,"
           "weather_code,wind_speed_10m,uv_index"
           "&daily=weather_code,temperature_2m_max,temperature_2m_min,"
           "precipitation_probability_max&timezone=Asia%%2FTehran&forecast_days=%d",
           SIRJAN_LAT, SIRJAN_LON, FORECAST_DAYS);

  String body;
  if (!httpGet(url, body, 8000)) {
    weather.err = yard.err.length() ? yard.err : "wx fail";
    return false;
  }
  JsonDocument doc;
  if (deserializeJson(doc, body)) {
    weather.err = "bad json";
    return false;
  }
  weather.temp = doc["current"]["temperature_2m"] | NAN;
  weather.hum = doc["current"]["relative_humidity_2m"] | NAN;
  weather.wind = doc["current"]["wind_speed_10m"] | NAN;
  weather.feels = doc["current"]["apparent_temperature"] | NAN;
  weather.uv = doc["current"]["uv_index"] | NAN;
  weather.code = doc["current"]["weather_code"] | -1;
  weather.dayCount = 0;
  JsonArray dates = doc["daily"]["time"].as<JsonArray>();
  JsonArray codes = doc["daily"]["weather_code"].as<JsonArray>();
  JsonArray tmax = doc["daily"]["temperature_2m_max"].as<JsonArray>();
  JsonArray tmin = doc["daily"]["temperature_2m_min"].as<JsonArray>();
  JsonArray precip = doc["daily"]["precipitation_probability_max"].as<JsonArray>();
  for (size_t i = 0; i < dates.size() && weather.dayCount < FORECAST_DAYS; i++) {
    DayForecast &d = weather.days[weather.dayCount++];
    const char *ds = dates[i] | "";
    strncpy(d.date, ds, sizeof(d.date) - 1);
    d.date[sizeof(d.date) - 1] = 0;
    d.code = codes[i] | -1;
    d.tMax = tmax[i] | NAN;
    d.tMin = tmin[i] | NAN;
    d.precipProb = precip[i] | -1;
  }
  weather.ok = true;
  weather.err = "";
  return true;
}

bool pumpRunning(int n) {
  unsigned long until = (n == 1) ? pump1Until : pump2Until;
  return until != 0 && millis() < until;
}

bool startPump(int n) {
  if (pumpRunning(1) || pumpRunning(2)) return false;
  if (!setRelay(n, true)) return false;
  unsigned long until = millis() + (unsigned long)PUMP_DUR_SEC * 1000UL;
  if (n == 1) {
    pump1Until = until;
    pump1Last = timeOk ? time(nullptr) : (time_t)(millis() / 1000UL);
  } else {
    pump2Until = until;
    pump2Last = timeOk ? time(nullptr) : (time_t)(millis() / 1000UL);
  }
  return true;
}

bool stopPump(int n) {
  if (!setRelay(n, false)) return false;
  if (n == 1) pump1Until = 0;
  else pump2Until = 0;
  return true;
}

void servicePumps() {
  if (pump1Until && millis() >= pump1Until) {
    setRelay(1, false);
    pump1Until = 0;
  }
  if (pump2Until && millis() >= pump2Until) {
    setRelay(2, false);
    pump2Until = 0;
  }
}

// ---------- UI نوار ----------
void drawNav(const char *title, uint16_t bg, uint16_t line, uint16_t fg) {
  tft.fillRect(0, 0, SCR_W, NAV_H, bg);
  tft.drawFastHLine(0, NAV_H - 1, SCR_W, line);
  // hint: BOOT = next
  tft.drawRoundRect(4, 3, 18, 18, 3, fg);
  textCenterX(13, 8, ">", fg, bg, 1);
  textCenterX(SCR_W / 2, 8, title, fg, bg, 1);
  tft.drawRoundRect(SCR_W - 22, 3, 18, 18, 3, fg);
  textCenterX(SCR_W - 13, 8, "B", fg, bg, 1);
}

// ---------- صفحات ----------
void drawWeather() {
  tft.fillScreen(WX_BG);
  drawNav("SIRJAN", WX_BG, WX_LINE, ST77XX_WHITE);

  if (!weather.ok) {
    textCenterX(SCR_W / 2, 150, weather.err.length() ? weather.err.c_str() : "Loading...",
                WX_MUTE, WX_BG, 1);
    return;
  }

  const int todayH = 108;
  const int todayY = NAV_H;
  tft.fillRect(0, todayY, SCR_W, todayH, WX_TODAY);

  bool rain = weather.dayCount > 0
                  ? isRainy(weather.days[0].code, weather.days[0].precipProb)
                  : isRainy(weather.code, -1);

  textAt(PAD, todayY + 6, "TODAY", WX_CITY, WX_TODAY, 1);
  int bw = rain ? 28 : 24;
  tft.fillRoundRect(SCR_W - PAD - bw, todayY + 4, bw, 12, 4, rain ? WX_RAIN_BG : WX_DRY_BG);
  textCenterX(SCR_W - PAD - bw / 2, todayY + 6, rain ? "RAIN" : "DRY",
              rain ? WX_RAIN_FG : WX_BADGE, rain ? WX_RAIN_BG : WX_DRY_BG, 1);

  char tempBuf[8];
  bool miss = isnan(weather.temp);
  if (miss) strcpy(tempBuf, "--");
  else snprintf(tempBuf, sizeof(tempBuf), "%d", (int)lroundf(weather.temp));
  textAt(PAD, todayY + 28, tempBuf, ST77XX_WHITE, WX_TODAY, miss ? 2 : 3);
  int tw = textW(tempBuf, miss ? 2 : 3);
  if (!miss) textAt(PAD + tw + 2, todayY + 32, "C", WX_CITY, WX_TODAY, 1);
  textRight(SCR_W - PAD, todayY + 34, miss ? "--" : wmoLabel(weather.code), ST77XX_WHITE, WX_TODAY, 1);

  char hl[20];
  float tMax = weather.dayCount ? weather.days[0].tMax : NAN;
  float tMin = weather.dayCount ? weather.days[0].tMin : NAN;
  if (!isnan(tMax) && !isnan(tMin))
    snprintf(hl, sizeof(hl), "H%d L%d", (int)lroundf(tMax), (int)lroundf(tMin));
  else
    strcpy(hl, "H-- L--");
  textRight(SCR_W - PAD, todayY + 50, hl, WX_HL, WX_TODAY, 1);

  const int metY = todayY + todayH - 40;
  tft.fillRoundRect(PAD, metY, CARD_W, 34, 5, WX_METRICS);
  const char *labs[4] = {"HUM", "WND", "FLS", "UV"};
  char vals[4][8];
  if (isnan(weather.hum)) strcpy(vals[0], "--");
  else snprintf(vals[0], 8, "%d%%", (int)lroundf(weather.hum));
  if (isnan(weather.wind)) strcpy(vals[1], "--");
  else snprintf(vals[1], 8, "%d", (int)lroundf(weather.wind));
  if (isnan(weather.feels)) strcpy(vals[2], "--");
  else snprintf(vals[2], 8, "%d", (int)lroundf(weather.feels));
  if (isnan(weather.uv)) strcpy(vals[3], "--");
  else snprintf(vals[3], 8, "%d", (int)lroundf(weather.uv));
  int cell = CARD_W / 4;
  for (int i = 0; i < 4; i++) {
    int cx = PAD + i * cell;
    if (i) tft.drawFastVLine(cx, metY + 4, 26, WX_MBORDER);
    textCenterX(cx + cell / 2, metY + 4, labs[i], WX_MLABEL, WX_METRICS, 1);
    textCenterX(cx + cell / 2, metY + 16, vals[i], ST77XX_WHITE, WX_METRICS, 1);
  }

  const int daysY = NAV_H + todayH;
  tft.fillRect(0, daysY, SCR_W, SCR_H - daysY, WX_BG);
  textAt(PAD, daysY + 4, "NEXT DAYS", WX_MUTE, WX_BG, 1);

  const int cardW = 74;
  const int cardH = 42;
  const int gap = 6;
  int shown = 0;
  for (int i = 1; i < weather.dayCount && shown < 4; i++, shown++) {
    int col = shown % 2;
    int row = shown / 2;
    int x = PAD + col * (cardW + gap);
    int y = daysY + 18 + row * (cardH + gap);
    DayForecast &d = weather.days[i];
    bool r = isRainy(d.code, d.precipProb);
    tft.fillRoundRect(x, y, cardW, cardH, 5, WX_CARD);
    tft.drawRoundRect(x, y, cardW, cardH, 5, WX_CARD_BD);
    textAt(x + 4, y + 4, weekdayShort(d.date), WX_WD, WX_CARD, 1);
    tft.fillRoundRect(x + cardW - 28, y + 3, 24, 10, 3, r ? WX_RAIN_BG : WX_DRY_BG);
    textCenterX(x + cardW - 16, y + 4, r ? "R" : "D", r ? WX_RAIN_FG : WX_BADGE,
                r ? WX_RAIN_BG : WX_DRY_BG, 1);
    char tbuf[16];
    if (!isnan(d.tMax) && !isnan(d.tMin))
      snprintf(tbuf, sizeof(tbuf), "%d/%d", (int)lroundf(d.tMax), (int)lroundf(d.tMin));
    else
      strcpy(tbuf, "--/--");
    textAt(x + 4, y + 22, tbuf, ST77XX_WHITE, WX_CARD, 1);
  }
}

void drawClimateCard(int y, bool isTemp, float value) {
  const int h = 136;
  uint16_t bg = isTemp ? CL_TEMP_BG : CL_HUM_BG;
  uint16_t bd = isTemp ? CL_TEMP_BD : CL_HUM_BD;
  uint16_t ac = isTemp ? CL_TEMP_AC : CL_HUM_AC;
  uint16_t lb = isTemp ? CL_TEMP_LB : CL_HUM_LB;
  uint16_t bb = isTemp ? CL_TEMP_BB : CL_HUM_BB;
  uint16_t vl = isTemp ? CL_TEMP_VL : CL_HUM_VL;

  tft.fillRoundRect(PAD, y, CARD_W, h, 7, bg);
  tft.drawRoundRect(PAD, y, CARD_W, h, 7, bd);
  tft.fillRect(PAD + 1, y + 1, CARD_W - 2, 3, ac);

  textAt(PAD + 10, y + 12, isTemp ? "TEMP" : "HUMIDITY", lb, bg, 1);
  tft.fillRoundRect(PAD + CARD_W - 46, y + 10, 36, 12, 5, bb);
  textCenterX(PAD + CARD_W - 28, y + 12, "DHT11", lb, bb, 1);

  char num[8];
  bool miss = isnan(value);
  if (miss) strcpy(num, "--");
  else snprintf(num, sizeof(num), "%d", (int)lroundf(value));
  textAt(PAD + 10, y + 48, num, vl, bg, miss ? 2 : 3);
  int nw = textW(num, miss ? 2 : 3);
  textAt(PAD + 10 + nw + 4, y + 58, isTemp ? "C" : "%", lb, bg, 1);

  textAt(PAD + 10, y + h - 20, "AIR", lb, bg, 1);
  const char *foot = !yard.ok ? "OFFLINE" : (miss ? "NO DHT" : (yard.water ? "WATER OK" : "NO WATER"));
  textRight(PAD + CARD_W - 10, y + h - 20, foot, lb, bg, 1);
}

void drawClimate() {
  tft.fillScreen(CL_BG);
  drawNav("CLIMATE", CL_BG, CL_LINE, ST77XX_WHITE);
  drawClimateCard(NAV_H + 6, true, yard.temp);
  drawClimateCard(NAV_H + 6 + 136 + 6, false, yard.hum);
}

void drawSoilCard(int y, int index, int adc) {
  const int h = 136;
  bool isA = index == 0;
  bool linked = yard.ok;
  int pct = linked ? soilPct(adc) : -1;
  bool dry = linked && pct < 35;
  bool wet = linked && pct > 70;
  uint16_t accent = dry ? SO_DRY_FG : (isA ? SO_AC_A : SO_AC_B);

  tft.fillRoundRect(PAD, y, CARD_W, h, 7, SO_CARD);
  tft.drawRoundRect(PAD, y, CARD_W, h, 7, SO_CARD_BD);
  tft.fillRect(PAD + 1, y + 1, CARD_W - 2, 3, accent);

  textAt(PAD + 10, y + 10, isA ? "SOIL 1" : "SOIL 2", SO_NAME, SO_CARD, 1);
  const char *st = !linked ? "OFF" : (dry ? "DRY" : (wet ? "WET" : "OK"));
  uint16_t stBg = !linked || dry ? SO_DRY_BG : (wet ? SO_WET_BG : SO_OK_BG);
  uint16_t stFg = !linked || dry ? SO_DRY_FG : (wet ? SO_WET_FG : SO_OK_FG);
  tft.fillRoundRect(PAD + CARD_W - 34, y + 8, 26, 12, 5, stBg);
  textCenterX(PAD + CARD_W - 21, y + 10, st, stFg, stBg, 1);

  char num[8];
  if (!linked) strcpy(num, "--");
  else snprintf(num, sizeof(num), "%d", pct);
  textAt(PAD + 10, y + 36, num, ST77XX_WHITE, SO_CARD, linked ? 3 : 2);
  int nw = textW(num, linked ? 3 : 2);
  if (linked) textAt(PAD + 10 + nw + 2, y + 48, "%", SO_META, SO_CARD, 1);
  textRight(PAD + CARD_W - 10, y + 42, isA ? "Plant A" : "Plant B", SO_META_S, SO_CARD, 1);
  char adcBuf[16];
  if (!linked) strcpy(adcBuf, "NO LINK");
  else snprintf(adcBuf, sizeof(adcBuf), "ADC %d", adc);
  textRight(PAD + CARD_W - 10, y + 56, adcBuf, SO_META, SO_CARD, 1);

  textAt(PAD + 10, y + h - 32, "MOISTURE", SO_BAR_LB, SO_CARD, 1);
  textRight(PAD + CARD_W - 10, y + h - 32, "DRY-WET", SO_BAR_LB, SO_CARD, 1);
  int barX = PAD + 10, barY = y + h - 18, barW = CARD_W - 20;
  tft.fillRoundRect(barX, barY, barW, 8, 3, SO_BAR_BG);
  tft.drawRoundRect(barX, barY, barW, 8, 3, SO_BAR_BD);
  if (linked) {
    int fw = (barW * constrain(pct, 0, 100)) / 100;
    if (fw > 2) tft.fillRoundRect(barX + 1, barY + 1, fw - 2, 6, 2, accent);
  }
}

void drawSoil() {
  tft.fillScreen(SO_BG);
  drawNav("SOIL", SO_BG, SO_LINE, ST77XX_WHITE);
  drawSoilCard(NAV_H + 6, 0, yard.soil1);
  drawSoilCard(NAV_H + 6 + 136 + 6, 1, yard.soil2);
}

void drawPump(int n) {
  bool p1 = (n == 1);
  uint16_t heroBg = p1 ? PU1_HERO : PU2_HERO;
  uint16_t heroBd = p1 ? PU1_HERO_B : PU2_HERO_B;
  uint16_t heroTx = p1 ? PU1_TXT : PU2_TXT;
  uint16_t nextCol = p1 ? PU1_NEXT : PU2_NEXT;
  uint16_t btnBd = p1 ? PU1_BTN_B : PU2_BTN_B;
  bool running = pumpRunning(n);
  time_t last = p1 ? pump1Last : pump2Last;

  tft.fillScreen(PU_BG);
  drawNav(p1 ? "PUMP 1" : "PUMP 2", PU_BG, 0x3985, ST77XX_WHITE);

  int y = NAV_H + 8;
  tft.fillRoundRect(PAD, y, CARD_W, 48, 7, heroBg);
  tft.drawRoundRect(PAD, y, CARD_W, 48, 7, heroBd);
  textAt(PAD + 10, y + 8, p1 ? "PUMP 1" : "PUMP 2", heroTx, heroBg, 1);
  textAt(PAD + 10, y + 24, p1 ? "Plant A" : "Plant B", heroTx, heroBg, 2);
  y += 56;

  tft.fillRoundRect(PAD, y, CARD_W, 40, 6, PU_BLK);
  tft.drawRoundRect(PAD, y, CARD_W, 40, 6, PU_BLK_BD);
  textAt(PAD + 10, y + 6, "LAST RUN", PU_LBL, PU_BLK, 1);
  char lastBuf[24];
  if (last <= 0) strcpy(lastBuf, "--");
  else if (!timeOk) strcpy(lastBuf, "Recent");
  else {
    struct tm tmRun = {};
    localtime_r(&last, &tmRun);
    snprintf(lastBuf, sizeof(lastBuf), "%02d:%02d", tmRun.tm_hour, tmRun.tm_min);
  }
  textAt(PAD + 10, y + 20, lastBuf, PU_VAL, PU_BLK, 1);
  y += 48;

  tft.fillRoundRect(PAD, y, CARD_W, 40, 6, PU_BLK);
  tft.drawRoundRect(PAD, y, CARD_W, 40, 6, PU_BLK_BD);
  textAt(PAD + 10, y + 6, "NEXT RUN", PU_LBL, PU_BLK, 1);
  textAt(PAD + 10, y + 20, "--", nextCol, PU_BLK, 1);
  y += 48;

  int chipW = (CARD_W - 6) / 2;
  tft.fillRoundRect(PAD, y, chipW, 36, 5, PU_CHIP);
  tft.drawRoundRect(PAD, y, chipW, 36, 5, PU_CHIP_BD);
  textAt(PAD + 6, y + 6, "DURATION", PU_CHIP_L, PU_CHIP, 1);
  char dur[12];
  snprintf(dur, sizeof(dur), "%ds", PUMP_DUR_SEC);
  textAt(PAD + 6, y + 18, dur, PU_CHIP_V, PU_CHIP, 1);

  tft.fillRoundRect(PAD + chipW + 6, y, chipW, 36, 5, PU_CHIP);
  tft.drawRoundRect(PAD + chipW + 6, y, chipW, 36, 5, PU_CHIP_BD);
  textAt(PAD + chipW + 12, y + 6, "STATUS", PU_CHIP_L, PU_CHIP, 1);
  textAt(PAD + chipW + 12, y + 18, running ? "Run" : "Idle",
         running ? nextCol : PU_CHIP_V, PU_CHIP, 1);
  y += 44;

  tft.fillRoundRect(PAD, y, CARD_W, 36, 7, heroBg);
  tft.drawRoundRect(PAD, y, CARD_W, 36, 7, btnBd);
  textCenterX(PAD + CARD_W / 2, y + 12, running ? "STOP (2x BOOT)" : "WATER (2x BOOT)",
              heroTx, heroBg, 1);

  textCenterX(SCR_W / 2, SCR_H - 14, "BOOT: next page", PU_CHIP_L, PU_BG, 1);
}

void enterPage(Page p) {
  page = p;
  if (p == PAGE_SPLASH) {
    splashStartMs = millis();
    splashFrame = 0;
    tft.fillScreen(BG_BEIGE);
    if (!drawSplashFrame(1)) {
      textAt(20, 150, "splash missing", ST77XX_BLACK, BG_BEIGE, 1);
      splashFrame = 1;
    } else {
      splashFrame = 1;
    }
  } else if (p == PAGE_WEATHER) {
    if (!weather.ok && وای فای.status() == WL_CONNECTED) fetchWeather();
    drawWeather();
  } else if (p == PAGE_CLIMATE) {
    fetchYard();
    drawClimate();
  } else if (p == PAGE_SOIL) {
    fetchYard();
    drawSoil();
  } else if (p == PAGE_PUMP1) {
    fetchYard();
    drawPump(1);
  } else if (p == PAGE_PUMP2) {
    fetchYard();
    drawPump(2);
  }
}

void goNextPage() {
  Page next = (Page)((page + 1) % PAGE_COUNT);
  enterPage(next);
}

void onPumpDoubleBoot() {
  int n = (page == PAGE_PUMP1) ? 1 : 2;
  bool ok;
  if (pumpRunning(n)) ok = stopPump(n);
  else ok = startPump(n);
  Serial.printf("pump %d %s -> %s\n", n, pumpRunning(n) ? "run" : "idle", ok ? "ok" : "fail");
  setRgb(ok ? 0 : 80, ok ? 60 : 0, 0);
  drawPump(n);
}

void handleBootClick() {
  if (page == PAGE_PUMP1 || page == PAGE_PUMP2) {
    unsigned long now = millis();
    if (bootClicks == 1 && (now - bootLastEdgeMs) <= BOOT_DOUBLE_MS) {
      bootClicks = 0;
      bootPendingNext = false;
      onPumpDoubleBoot();
      return;
    }
    bootClicks = 1;
    bootLastEdgeMs = now;
    bootPendingNext = true;
    bootPendingMs = now;
  } else {
    bootClicks = 0;
    bootPendingNext = false;
    goNextPage();
  }
}

void pollBoot() {
  bool down = digitalRead(PIN_BOOT) == LOW;
  unsigned long now = millis();

  if (down && !bootDown && (now - bootDownMs) > BOOT_DEBOUNCE_MS) {
    bootDown = true;
    bootDownMs = now;
  } else if (!down && bootDown && (now - bootDownMs) > BOOT_DEBOUNCE_MS) {
    bootDown = false;
    bootDownMs = now;
    handleBootClick();
  }

  if (bootPendingNext && bootClicks == 1 && (now - bootPendingMs) > BOOT_DOUBLE_MS) {
    bootPendingNext = false;
    bootClicks = 0;
    goNextPage();
  }
}

void ensureوای فای() {
  if (وای فای.status() == WL_CONNECTED) return;
  if (millis() - lastوای فایTryMs < وای فای_RETRY_MS) return;
  lastوای فایTryMs = millis();
  وای فای.mode(وای فای_STA);
  وای فای.begin(وای فای_SSID, وای فای_PASS);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== goldon C6 panel 172x320 ===");

  pinMode(PIN_TFT_BL, OUTPUT);
  digitalWrite(PIN_TFT_BL, HIGH);
  pinMode(PIN_BOOT, INPUT_PULLUP);

  rgb.begin();
  rgb.setBrightness(40);
  setRgb(0, 40, 20);

  SPI.begin(PIN_TFT_SCLK, -1, PIN_TFT_MOSI, PIN_TFT_CS);
  tft.init(172, 320);
  tft.setRotation(0);
  tft.setSPISpeed(40000000);

  fsOk = LittleFS.begin(true);
  Serial.printf("LittleFS %s\n", fsOk ? "OK" : "FAIL");

  وای فای.mode(وای فای_STA);
  وای فای.begin(وای فای_SSID, وای فای_PASS);
  lastوای فایTryMs = millis();

  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  enterPage(PAGE_SPLASH);
}

void loop() {
  unsigned long now = millis();
  pollBoot();
  servicePumps();
  ensureوای فای();

  if (!timeOk && وای فای.status() == WL_CONNECTED) {
    time_t t = time(nullptr);
    if (t > 1700000000) timeOk = true;
  }

  if (page == PAGE_SPLASH) {
    updateRgbBreath(now);
    unsigned long elapsed = now - splashStartMs;
    int target = (int)((elapsed % SPLASH_DURATION_MS) * SPLASH_FRAME_COUNT / SPLASH_DURATION_MS) + 1;
    if (target < 1) target = 1;
    if (target > SPLASH_FRAME_COUNT) target = SPLASH_FRAME_COUNT;
    if (target != splashFrame) {
      splashFrame = target;
      drawSplashFrame(splashFrame);
    }
    return;
  }

  if (now - lastPollMs >= POLL_MS) {
    lastPollMs = now;
    bool yardChanged = fetchYard();
    if (page == PAGE_WEATHER && !weather.ok) fetchWeather();
    if (page == PAGE_CLIMATE && yardChanged) drawClimate();
    else if (page == PAGE_SOIL && yardChanged) drawSoil();
    else if (page == PAGE_PUMP1 && yardChanged) drawPump(1);
    else if (page == PAGE_PUMP2 && yardChanged) drawPump(2);
    else if (page == PAGE_WEATHER && weather.ok) {
      // leave as-is unless first load
    }
  }

  if (page == PAGE_WEATHER && weather.ok == false && وای فای.status() == WL_CONNECTED) {
    static unsigned long lastWx = 0;
    if (now - lastWx > 15000) {
      lastWx = now;
      if (fetchWeather()) drawWeather();
    }
  }

  if ((page == PAGE_PUMP1 || page == PAGE_PUMP2) &&
      (pumpRunning(1) || pumpRunning(2))) {
    static unsigned long lastPumpDraw = 0;
    if (now - lastPumpDraw > 1000) {
      lastPumpDraw = now;
      drawPump(page == PAGE_PUMP1 ? 1 : 2);
    }
  }
}
