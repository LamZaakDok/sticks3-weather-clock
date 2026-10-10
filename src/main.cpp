#include <M5Unified.h>
#include <WiFi.h>
#include <Preferences.h>
#include <ArduinoOTA.h>
#include <time.h>

#include "config.h"
#include "weather.h"
#include "ui.h"
#include "lunar.h"
#include "portal.h"
#include "i18n.h"
#include "voice.h"

// ============================================================
//  全局状态
// ============================================================
M5Canvas  canvas(&M5.Display);      // 竖屏 135x240（天气页）
M5Canvas  canvasL(&M5.Display);     // 横屏 240x135（时钟页）
Preferences prefs;
WeatherData wx;

String g_city = DEFAULT_CITY;
String g_cityLabel;                 // 显示用（含省份）
float  g_lat = NAN, g_lon = NAN;

int      g_seq    = 0;              // 0 = 时钟页；1..6 = 天气页 0..5
int      g_battery = -1;
bool     g_sleeping = false;
uint32_t g_lastRefresh = 0;
uint32_t g_lastInput   = 0;
uint32_t g_seqEnter    = 0;      // 进入当前页的时间（用于动画）

// 语音播报配置
bool g_voiceOn      = false;
int  g_voiceTrig    = 0;         // 0=长按KEY1 1=长按KEY2 2=自动 3=定时
int  g_voiceTimeMin = 7 * 60;    // 定时时间（分钟）
bool g_announcePending = false;  // 自动播报待触发
bool g_alertTest = false;        // 预警测试（强制显示示例预警）
bool g_alarmOn   = false;        // 闹钟开关
int  g_alarmTimeMin = 6 * 60 + 30;  // 闹钟时间（分钟）

// ============================================================
//  时间辅助
// ============================================================
static bool timeOk() { return time(nullptr) > 1600000000; }

static String clockString() {
  if (!timeOk()) return L("时间同步中...", "Syncing time...");
  struct tm t; time_t now = time(nullptr); localtime_r(&now, &t);
  char buf[48];
  if (g_lang == LANG_EN) {
    snprintf(buf, sizeof(buf), "%s  %02d:%02d", i18nWeekday(t.tm_wday), t.tm_hour, t.tm_min);
  } else {
    snprintf(buf, sizeof(buf), "%d月%d日 %s  %02d:%02d",
             t.tm_mon + 1, t.tm_mday, i18nWeekday(t.tm_wday), t.tm_hour, t.tm_min);
  }
  return String(buf);
}

static void fmtClockPage(String& timeStr, String& dateStr, String& lunarStr) {
  if (!timeOk()) { timeStr = "--:--:--"; dateStr = L("时间同步中...", "Syncing time..."); lunarStr = ""; return; }
  struct tm t; time_t now = time(nullptr); localtime_r(&now, &t);
  char b[40];
  snprintf(b, sizeof(b), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
  timeStr = b;
  if (g_lang == LANG_EN) {
    snprintf(b, sizeof(b), "%04d-%02d-%02d %s", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, i18nWeekday(t.tm_wday));
  } else {
    snprintf(b, sizeof(b), "%d年%d月%d日 %s", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, i18nWeekday(t.tm_wday));
  }
  dateStr = b;
  lunarStr = chineseAlmanacString(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
}

static bool isNight() {
  if (!timeOk()) return false;
  struct tm t; time_t now = time(nullptr); localtime_r(&now, &t);
  return (t.tm_hour >= NIGHT_START_HOUR || t.tm_hour < NIGHT_END_HOUR);
}

// ============================================================
//  显示辅助
// ============================================================
static void splash(const String& l1, const String& l2 = "", const String& l3 = "") {
  M5.Display.setRotation(0);
  canvas.fillSprite(TFT_BLACK);
  canvas.setFont(&fonts::efontCN_16_b);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString(l1, SCREEN_W / 2, 90);
  canvas.setFont(&fonts::efontCN_12);
  canvas.setTextColor(TFT_CYAN);
  canvas.drawString(l2, SCREEN_W / 2, 124);
  canvas.setTextColor(TFT_LIGHTGREY);
  canvas.drawString(l3, SCREEN_W / 2, 148);
  canvas.pushSprite(0, 0);
}

// 当前页的动画参数：今日=相位(秒)，趋势=进度 0..1
static float pageAnim() {
  int page = g_seq - 1;
  if (page == 0) return millis() / 1000.0f;
  if (page == 4) {
    float f = (millis() - g_seqEnter) / 1200.0f;
    return f > 1 ? 1 : f;
  }
  return 0;
}

static void drawScreen() {
  if (g_seq == 0) {
    M5.Display.setRotation(1);           // 横屏
    String ts, ds, ls;
    fmtClockPage(ts, ds, ls);
    ui_drawClock(canvasL, ts, ds, ls, g_battery);
    canvasL.pushSprite(0, 0);
  } else {
    M5.Display.setRotation(0);           // 竖屏
    ui_draw(canvas, wx, g_seq - 1, g_battery, clockString(), pageAnim());
    canvas.pushSprite(0, 0);
  }
}

static void drawNow() {
  static uint32_t lastBatt = 0;
  if (g_battery < 0 || millis() - lastBatt > 10000) {
    g_battery = M5.Power.getBatteryLevel();
    lastBatt = millis();
  }
  drawScreen();
}

// ============================================================
//  配网（自定义 AP 热点网页）
// ============================================================
static void wifiConnect() {
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  bool forcePortal = prefs.getBool("portal", false);   // 用户主动要求重新配网
  bool connected = false;

  if (!forcePortal && ssid.length()) {                 // 有存档，先尝试直连
    splash(L("正在连接 WiFi", "Connecting WiFi"), ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) delay(500);
    connected = (WiFi.status() == WL_CONNECTED);
  }
  if (!forcePortal && !connected) {                    // 回退：内核里保存的上次 WiFi（兼容旧版本）
    splash(L("正在连接 WiFi", "Connecting WiFi"), L("使用上次网络...", "using last network..."));
    WiFi.mode(WIFI_STA);
    WiFi.begin();
    for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) delay(500);
    connected = (WiFi.status() == WL_CONNECTED);
  }
  if (connected) {
    prefs.putBool("portal", false);
    Serial.printf("[wifi] connected, ip=%s\n", WiFi.localIP().toString().c_str());
    return;
  }

  // 启动配网热点
  splash(L("手机配网", "WiFi Setup"), String(L("连热点 ", "AP: ")) + AP_SSID, "192.168.4.1");
  String city;
  if (portalRun(city)) {
    prefs.putBool("portal", false);
    if (city.length()) {
      g_city = city;
      g_cityLabel = city;
      g_lat = NAN; g_lon = NAN;              // 城市变了，重新定位
      prefs.putString("city", g_city);
      prefs.putString("cityLabel", city);
      prefs.remove("lat3");
      prefs.remove("lon3");
    }
  } else {
    splash(L("配网超时", "Setup timeout"), L("即将重启...", "Restarting..."));
    delay(2000);
    ESP.restart();
  }
}

static void syncNtp() {
  splash(L("联网成功", "Connected"), L("正在同步时间...", "Syncing time..."), WiFi.localIP().toString());
  configTzTime(TZ_INFO, "ntp.aliyun.com", "ntp1.aliyun.com", "pool.ntp.org");
  for (int i = 0; i < 20 && !timeOk(); i++) delay(200);
}

static void setupOta() {
  ArduinoOTA.setHostname("sticks3-weather");
  ArduinoOTA.onStart([]() { splash(L("OTA 升级中...", "OTA updating..."), L("请不要断电", "Do not power off")); });
  ArduinoOTA.onEnd([]()   { splash(L("升级完成", "Update done"), L("正在重启...", "Restarting...")); });
  ArduinoOTA.onError([](ota_error_t e) { splash(L("OTA 出错", "OTA error"), String((int)e)); });
  ArduinoOTA.begin();
  Serial.printf("[ota] ready: sticks3-weather.local  IP %s\n", WiFi.localIP().toString().c_str());
}

// ============================================================
//  定位 / 天气
// ============================================================
static void resolveLocation() {
  if (!isnan(g_lat) && !isnan(g_lon)) return;
  splash(L("查询城市", "Locating"), g_city);
  float lat, lon;
  String resolved;
  if (weather_geocode(g_city, lat, lon, resolved)) {
    g_lat = lat; g_lon = lon;
    if (resolved.length()) g_cityLabel = resolved;
    prefs.putFloat("lat3", g_lat);
    prefs.putFloat("lon3", g_lon);
    prefs.putString("cityLabel", g_cityLabel);
  } else {
    g_lat = DEFAULT_LAT; g_lon = DEFAULT_LON;
  }
}

static void refreshWeather(bool showMsg) {
  bool visible = !g_sleeping;
  if (showMsg && visible) splash(L("正在更新天气...", "Updating weather..."), g_city);

  if (WiFi.status() != WL_CONNECTED) {
    if (visible) splash(L("网络已断开", "Network lost"), L("正在重连...", "Reconnecting..."));
    WiFi.reconnect();
    for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) delay(300);
  }
  if (WiFi.status() == WL_CONNECTED) {
    resolveLocation();
    if (weather_fetch(g_lat, g_lon, wx)) {
      wx.city = g_cityLabel.length() ? g_cityLabel : g_city;
      prefs.putString("city", g_city);
      weather_fetch_air(g_lat, g_lon, wx);     // 空气质量
      if (g_voiceOn && g_voiceTrig == 2) g_announcePending = true;   // 自动播报
    }
  }
  g_lastRefresh = millis();
  if (visible) drawNow();
}

// ============================================================
//  语音播报
// ============================================================
static void wakeToClock() {
  g_sleeping = false;
  M5.Display.setBrightness(BRIGHT_ON);
  g_seq = 0;
  g_seqEnter = millis();
  drawNow();
}

static void announceNow() {
  if (!g_voiceOn || !voice_ready()) return;
  if (g_sleeping) wakeToClock();
  g_lastInput = millis();
  voice_announce(wx);
  g_lastInput = millis();
}

// 打开设置热点（不丢 WiFi）
static void openSettings() {
  splash(L("打开设置热点", "Setup hotspot"),
         String(L("连热点 ", "AP ")) + AP_SSID, "192.168.4.1");
  String city;
  if (portalRun(city, true)) {
    if (city.length()) {
      g_city = city; g_cityLabel = city;
      g_lat = NAN; g_lon = NAN;
      prefs.putString("city", g_city);
      prefs.putString("cityLabel", city);
      prefs.remove("lat3"); prefs.remove("lon3");
    }
  }
  g_lastInput = millis();
  drawNow();
}

// 起床闹钟：响铃 + 播报天气
static void runAlarm() {
  if (g_sleeping) wakeToClock();
  else { g_seq = 0; g_seqEnter = millis(); drawNow(); }
  Serial.println("[alarm] ringing");
  M5.Speaker.setVolume(200);
  bool stopped = false;
  for (int i = 0; i < 6 && !stopped; i++) {
    M5.Speaker.tone(880, 200);  delay(260);
    M5.Speaker.tone(1320, 200); delay(360);
    M5.update();
    if (M5.BtnA.wasClicked() || M5.BtnB.wasClicked()) stopped = true;   // 按键停铃
  }
  if (!stopped && g_voiceOn && voice_ready()) voice_announce(wx);        // 顺带播报天气
  g_lastInput = millis();
}

// ============================================================
//  setup / loop
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(200);

  auto cfg = M5.config();
  M5.begin(cfg);
  setCpuFrequencyMhz(CPU_MHZ);          // 省电：主频降到 80MHz
  M5.Display.setRotation(0);
  M5.Display.setBrightness(BRIGHT_ON);

  canvas.setColorDepth(16);
  canvas.createSprite(SCREEN_W, SCREEN_H);
  canvas.setTextWrap(false);
  canvasL.setColorDepth(16);
  canvasL.createSprite(SCREEN_H, SCREEN_W);
  canvasL.setTextWrap(false);

  splash(FW_NAME, String("v") + FW_VERSION, L("开机启动中...", "Starting up..."));
  Serial.printf("\n=== %s v%s ===\n", FW_NAME, FW_VERSION);

  prefs.begin("weather", false);
  g_city = prefs.getString("city", DEFAULT_CITY);
  g_cityLabel = prefs.getString("cityLabel", g_city);
  g_lat  = prefs.getFloat("lat3", NAN);      // 新键：强制用新定位逻辑重新解析一次
  g_lon  = prefs.getFloat("lon3", NAN);
  g_lang = (Lang)prefs.getInt("lang", LANG_ZH);
  g_api.mode = prefs.getInt("apiMode", 0);
  g_api.url  = prefs.getString("apiUrl", "");
  g_api.key  = prefs.getString("apiKey", "");

  // 语音播报配置
  g_voiceOn      = prefs.getBool("voiceOn", false);
  g_voiceTrig    = prefs.getInt("voiceTrig", 0);
  g_voiceTimeMin = prefs.getInt("voiceTime", 7 * 60);
  g_voiceLang    = prefs.getInt("voiceLang", 0);
  g_voiceGender  = prefs.getInt("voiceGen", 0);
  g_alertTest    = prefs.getBool("alertTest", false);
  g_alarmOn      = prefs.getBool("alarmOn", false);
  g_alarmTimeMin = prefs.getInt("alarmTime", 6 * 60 + 30);

  voice_begin();                     // 挂载语音素材

  wifiConnect();
  syncNtp();
  setupOta();

  g_lastRefresh = 0;
  g_lastInput   = millis();
  refreshWeather(true);
}

void loop() {
  M5.update();
  ArduinoOTA.handle();

  // 按键
  if (M5.BtnA.wasClicked()) {
    g_lastInput = millis();
    if (g_sleeping) wakeToClock();
    else { g_seq = (g_seq + 1) % 8; g_seqEnter = millis(); drawNow(); }
  }
  // KEY2：单击=刷新（延迟判定），双击=打开设置热点
  static uint32_t bClickMs = 0;
  static bool bPending = false;
  if (M5.BtnB.wasClicked()) {
    g_lastInput = millis();
    if (g_sleeping) wakeToClock();
    else { bClickMs = millis(); bPending = true; }
  }
  if (bPending) {
    if (M5.BtnB.wasDoubleClicked()) { bPending = false; openSettings(); }
    else if (millis() - bClickMs > 350) { bPending = false; refreshWeather(true); }
  }

  // 语音播报：长按触发
  static bool latchA = false, latchB = false;
  if (!M5.BtnA.isPressed()) latchA = false;
  if (!M5.BtnB.isPressed()) latchB = false;
  if (g_voiceOn && voice_ready()) {
    if (g_voiceTrig == 0 && M5.BtnA.pressedFor(1200) && !latchA) { latchA = true; announceNow(); }
    if (g_voiceTrig == 1 && M5.BtnB.pressedFor(1200) && !latchB) { latchB = true; announceNow(); }
  }

  // 自动播报（天气更新后）
  if (g_announcePending) { g_announcePending = false; announceNow(); }

  // 起床闹钟
  if (g_alarmOn && timeOk()) {
    struct tm t; time_t now = time(nullptr); localtime_r(&now, &t);
    static int lastAlarmDay = -1;
    if ((t.tm_hour * 60 + t.tm_min) == g_alarmTimeMin && t.tm_yday != lastAlarmDay) {
      lastAlarmDay = t.tm_yday;
      runAlarm();
    }
  }

  // 定时播报
  if (g_voiceOn && g_voiceTrig == 3 && timeOk()) {
    struct tm t; time_t now = time(nullptr); localtime_r(&now, &t);
    static int lastDay = -1;
    if ((t.tm_hour * 60 + t.tm_min) == g_voiceTimeMin && t.tm_yday != lastDay) {
      lastDay = t.tm_yday;
      announceNow();
    }
  }

  // 重新配网：默认长按 KEY2 3 秒；若语音占用了 KEY2 长按，则改用长按 KEY1
  bool reconf = M5.BtnB.pressedFor(3000);
  if (g_voiceOn && g_voiceTrig == 1) reconf = M5.BtnA.pressedFor(3000);
  if (reconf) {
    splash(L("重新配网", "Reconfigure WiFi"), L("清除 WiFi 后重启...", "Clearing WiFi & restart..."));
    prefs.putBool("portal", true);      // 强制进入配网
    prefs.remove("ssid");
    prefs.remove("pass");
    WiFi.disconnect(true, true);        // 连内核里保存的 WiFi 一起清除
    delay(1500);
    ESP.restart();
  }

  // 自动更新
  if (millis() - g_lastRefresh > REFRESH_INTERVAL_MS) {
    refreshWeather(false);
  }

  // 省电：无操作息屏（夜间更快息屏）
  uint32_t limit = isNight() ? NIGHT_IDLE_MS : DAY_IDLE_MS;
  if (!g_sleeping && (millis() - g_lastInput > limit)) {
    g_sleeping = true;
    M5.Display.setBrightness(0);
  }

  // 刷新节奏（息屏时完全不重绘）
  if (!g_sleeping) {
    if (g_seq == 0) {                    // 时钟页：每秒
      static uint32_t lastSec = 0;
      uint32_t s = millis() / 1000;
      if (s != lastSec) { lastSec = s; drawNow(); }
    } else if (g_seq == 1) {             // 今日页：图标动画 ~11fps
      static uint32_t lastA = 0;
      if (millis() - lastA > 90) { lastA = millis(); drawNow(); }
    } else if (g_seq == 3) {             // 防晒页：底部滚动条
      static uint32_t lastUv = 0;
      if (millis() - lastUv > 90) { lastUv = millis(); drawNow(); }
    } else if (g_seq == 7) {             // 洗晒页：底部滚动条
      static uint32_t lastLd = 0;
      if (millis() - lastLd > 90) { lastLd = millis(); drawNow(); }
    } else if (g_seq == 4) {             // 生活提示页：底部滚动条
      static uint32_t lastFast = 0;
      if (millis() - lastFast > 90) { lastFast = millis(); drawNow(); }
    } else if (g_seq == 5) {             // 趋势页：折线绘制动画
      float f = (millis() - g_seqEnter) / 1200.0f;
      if (f < 1.0f) {
        static uint32_t lastT = 0;
        if (millis() - lastT > 30) { lastT = millis(); drawNow(); }
      }
    }
  }

  delay(20);
}
