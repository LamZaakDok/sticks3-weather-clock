#include "portal.h"
#include "config.h"
#include "i18n.h"
#include "weather.h"
#include "voice.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>

extern Preferences prefs;                 // 定义在 main.cpp
extern bool g_voiceOn;                    // 语音播报开关（main.cpp）
extern int  g_voiceTrig;                  // 触发方式
extern int  g_voiceTimeMin;               // 定时时间（分钟）
extern bool g_alertTest;                  // 预警测试
extern bool g_alarmOn;                    // 闹钟
extern int  g_alarmTimeMin;               // 闹钟时间（分钟）

static WebServer s_server(80);
static DNSServer  s_dns;
static bool       s_saved = false;
static String     s_city, s_ssid, s_pass;
static const IPAddress AP_IP(192, 168, 4, 1);

// ---------------- HTML 工具 ----------------
static String esc(const String& s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if      (c == '&')  o += "&amp;";
    else if (c == '<')  o += "&lt;";
    else if (c == '>')  o += "&gt;";
    else if (c == '"')  o += "&quot;";
    else if (c == '\'') o += "&#39;";
    else                o += c;
  }
  return o;
}
static String lockSvg() {
  return "<svg width=\"12\" height=\"12\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"#94a3b8\" "
         "stroke-width=\"2\" style=\"vertical-align:-1px\"><rect x=\"4\" y=\"11\" width=\"16\" height=\"10\" rx=\"2\"/>"
         "<path d=\"M8 11V7a4 4 0 018 0v4\"/></svg>";
}
static String bars(int rssi) {
  int n = constrain((rssi + 90) / 7, 1, 4);
  String s = "<span class=\"bars\">";
  for (int i = 1; i <= 4; i++) s += String("<i class=\"b") + i + (i <= n ? " on" : "") + "\"></i>";
  s += "</span>";
  return s;
}

static const char* PAGE_CSS =
  "<style>"
  "*{box-sizing:border-box;margin:0;padding:0}"
  "body{font-family:-apple-system,'PingFang SC','Microsoft YaHei',sans-serif;background:#f6f7f9;"
  "color:#0f172a;min-height:100vh;padding:24px 18px 34px}"
  ".topbar{text-align:right;margin-bottom:8px}"
  ".brand{font-size:11px;letter-spacing:.16em;color:#98a2b3;font-weight:600}"
  ".lang{font-size:12.5px;color:#98a2b3}"
  ".lang a{color:#98a2b3;text-decoration:none}"
  ".lang a.on{color:#0f766e;font-weight:600}"
  ".hero{text-align:center;margin-bottom:18px}"
  "h1{font-size:22px;font-weight:650;letter-spacing:-.01em;margin:6px 0 4px}"
  ".sub{font-size:13px;color:#667085}"
  ".sec{font-size:11px;letter-spacing:.1em;color:#98a2b3;font-weight:600;margin:18px 0 8px}"
  ".card{background:#fff;border:1px solid #e6e9ee;border-radius:14px;padding:14px}"
  "label{display:block;font-size:12.5px;color:#475467;margin:12px 0 6px;font-weight:500}"
  "label:first-child{margin-top:0}"
  "input,select{width:100%;padding:11px 12px;border:1px solid #d9dee5;border-radius:10px;background:#fff;"
  "font-size:15px;color:#0f172a;outline:none}"
  "input:focus,select:focus{border-color:#0f766e;box-shadow:0 0 0 3px rgba(15,118,110,.10)}"
  ".row{display:flex;align-items:center;justify-content:space-between;padding:11px 4px;"
  "border-bottom:1px solid #eef1f5;border-radius:8px;cursor:pointer}"
  ".row:last-child{border-bottom:none}"
  ".row .l{font-size:14px;color:#1d2939}"
  ".bars{display:flex;gap:2px;align-items:flex-end}"
  ".bars i{width:3px;background:#cfd4dc;border-radius:1px;display:block}"
  ".bars i.on{background:#0f766e}"
  ".b1{height:5px}.b2{height:8px}.b3{height:11px}.b4{height:14px}"
  "button{width:100%;margin-top:20px;padding:14px;border:none;border-radius:10px;background:#0f766e;"
  "color:#fff;font-size:15px;font-weight:600;letter-spacing:.01em}"
  ".foot{font-size:11.5px;color:#98a2b3;text-align:center;margin-top:14px}"
  ".hint{font-size:11.5px;color:#98a2b3;margin-top:6px;text-align:center;line-height:1.6}"
  ".ok{color:#0f766e;font-size:15px;font-weight:600}"
  "#cust{display:none}"
  "</style>";

static String s_nets;   // 缓存的 WiFi 列表 HTML
static bool   s_keepWifi = false;

static void scanCache() {
  int n = WiFi.scanNetworks();
  struct Item { String ssid; int rssi; bool enc; };
  static Item it[40];
  int m = 0;
  for (int i = 0; i < n && m < 40; i++) {
    String ss = WiFi.SSID(i);
    if (!ss.length()) continue;
    it[m].ssid = ss;
    it[m].rssi = WiFi.RSSI(i);
    it[m].enc  = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    m++;
  }
  for (int i = 0; i < m; i++)
    for (int j = i + 1; j < m; j++)
      if (it[j].rssi > it[i].rssi) { Item t = it[i]; it[i] = it[j]; it[j] = t; }
  WiFi.scanDelete();

  s_nets = "";
  for (int i = 0; i < m; i++) {
    String ss = esc(it[i].ssid);
    s_nets += "<div class=\"row\" data-s=\"" + ss + "\" onclick=\"pick(this)\"><span class=\"l\">" + ss +
              (it[i].enc ? " " + lockSvg() : "") + "</span>" + bars(it[i].rssi) + "</div>";
  }
  if (!m) s_nets = "<div class=\"hint\">" + String(L("没扫到网络，请手动输入名称", "No networks found, type manually")) + "</div>";
  Serial.printf("[portal] scanned %d networks\n", m);
}

static String buildPage(int lang, const String& curCity) {
  bool en = (lang == 1);
  auto T = [&](const char* zh, const char* en2) { return String(en ? en2 : zh); };
  auto sel = [&](int cur, int val) { return String(cur == val ? " selected" : ""); };

  bool vOn   = prefs.getBool("voiceOn", false);
  int  vTrig = prefs.getInt("voiceTrig", 0);
  int  vTime = prefs.getInt("voiceTime", 7 * 60);
  int  vLang = prefs.getInt("voiceLang", 0);
  int  vGen  = prefs.getInt("voiceGen", 0);
  int  vAlert = prefs.getBool("alertTest", false) ? 1 : 0;
  int  vAlarm = prefs.getBool("alarmOn", false) ? 1 : 0;
  int  vAlarmTime = prefs.getInt("alarmTime", 6 * 60 + 30);
  char abuf[8];
  snprintf(abuf, sizeof(abuf), "%02d:%02d", vAlarmTime / 60, vAlarmTime % 60);
  String alarmStr = abuf;
  char tbuf[8];
  snprintf(tbuf, sizeof(tbuf), "%02d:%02d", vTime / 60, vTime % 60);
  String timeStr = tbuf;

  String curSsid = s_keepWifi ? prefs.getString("ssid", "") : "";
  String curPass = s_keepWifi ? prefs.getString("pass", "") : "";

  String langLinks = "<a href=\"/?lang=0\"" + String(en ? "" : " class=\"on\"") + ">" +
                     T("中文", "中文") + "</a> · <a href=\"/?lang=1\"" +
                     String(en ? " class=\"on\"" : "") + ">EN</a>";

  return String("<!DOCTYPE html><html lang=\"") + (en ? "en" : "zh-CN") + "\"><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>" + T("StickS3 天气机 · 配网", "StickS3 Weather · Setup") + "</title>" + PAGE_CSS + "</head><body>"
    "<div class=\"topbar\"><span class=\"lang\">" + langLinks + "</span></div>"
    "<div class=\"hero\">"
    "<div class=\"brand\">STICKS3 · WEATHER</div>"
    "<h1>" + T("设备配网", "Device Setup") + "</h1>"
    "<div class=\"sub\">" + T("连接 WiFi 并设置城市", "Connect WiFi & set city") + "</div>"
    "</div>"
    "<form method=\"POST\" action=\"/save\">"
    "<input type=\"hidden\" name=\"lang\" value=\"" + String(lang) + "\">"
    "<div class=\"sec\">" + T("WiFi", "WiFi") + "</div>"
    "<div class=\"card\">"
      "<label>" + T("网络名称", "Network") + "</label>"
      "<input name=\"ssid\" id=\"ssid\" value=\"" + esc(curSsid) + "\" placeholder=\"" + T("选择下方网络或手动输入", "Pick below or type") + "\" required>"
      "<label>" + T("密码", "Password") + "</label>"
      "<input name=\"pwd\" type=\"password\" value=\"" + esc(curPass) + "\" placeholder=\"" + T("没有密码就留空", "Blank if open") + "\">"
      "<label>" + T("城市", "City") + "</label>"
      "<input name=\"city\" placeholder=\"" + String(DEFAULT_CITY) + "\" value=\"" + esc(curCity) + "\">"
    "</div>"
    "<div class=\"sec\">" + T("附近网络", "Nearby Networks") + "</div>"
    "<div class=\"card\">" + s_nets + "</div>"
    "<div class=\"sec\">" + T("显示与数据", "Display & Data") + "</div>"
    "<div class=\"card\">"
      "<label>" + T("天气数据源", "Weather source") + "</label>"
      "<select name=\"apiMode\" id=\"apiMode\" onchange=\"tog()\">"
        "<option value=\"0\">" + T("默认（Open-Meteo）", "Default (Open-Meteo)") + "</option>"
        "<option value=\"1\">" + T("自定义接口", "Custom API") + "</option>"
      "</select>"
      "<div id=\"cust\">"
        "<label>" + T("接口地址（兼容 Open-Meteo）", "API URL (Open-Meteo compatible)") + "</label>"
        "<input name=\"apiUrl\" placeholder=\"https://api.open-meteo.com/v1/forecast\">"
        "<label>" + T("API Key（可选）", "API Key (optional)") + "</label>"
        "<input name=\"apiKey\" placeholder=\"" + T("可选", "optional") + "\">"
      "</div>"
      "<label>" + T("预警测试", "Alert test") + "</label>"
      "<select name=\"alertTest\">"
        "<option value=\"0\"" + sel(vAlert, 0) + ">" + T("关闭", "Off") + "</option>"
        "<option value=\"1\"" + sel(vAlert, 1) + ">" + T("开启（首页显示示例预警）", "On (sample alert)") + "</option>"
      "</select>"
    "</div>"

    "<div class=\"sec\">" + T("起床闹钟", "Alarm") + "</div>"
    "<div class=\"card\">"
      "<label>" + T("是否开启", "Enable") + "</label>"
      "<select name=\"alarmOn\">"
        "<option value=\"0\"" + sel(vAlarm, 0) + ">" + T("关闭", "Off") + "</option>"
        "<option value=\"1\"" + sel(vAlarm, 1) + ">" + T("开启", "On") + "</option>"
      "</select>"
      "<label>" + T("响铃时间", "Time") + "</label>"
      "<input name=\"alarmTime\" type=\"time\" value=\"" + alarmStr + "\">"
    "</div>"

    "<div class=\"sec\">" + T("语音播报", "Voice Announcement") + "</div>"
    "<div class=\"card\">"
      "<label>" + T("是否播报", "Enable") + "</label>"
      "<select name=\"voiceOn\" id=\"vOn\" onchange=\"vtog()\">"
        "<option value=\"0\"" + sel(vOn ? 1 : 0, 0) + ">" + T("关闭", "Off") + "</option>"
        "<option value=\"1\"" + sel(vOn ? 1 : 0, 1) + ">" + T("开启", "On") + "</option>"
      "</select>"
      "<div id=\"vcfg\">"
        "<label>" + T("触发方式", "Trigger") + "</label>"
        "<select name=\"voiceTrig\" id=\"vTrig\" onchange=\"vtog()\">"
          "<option value=\"0\"" + sel(vTrig, 0) + ">" + T("长按 KEY1", "Hold KEY1") + "</option>"
          "<option value=\"1\"" + sel(vTrig, 1) + ">" + T("长按 KEY2", "Hold KEY2") + "</option>"
          "<option value=\"2\"" + sel(vTrig, 2) + ">" + T("自动播报（每次更新后）", "Auto (after update)") + "</option>"
          "<option value=\"3\"" + sel(vTrig, 3) + ">" + T("定时播报", "Scheduled") + "</option>"
        "</select>"
        "<div id=\"vtimewrap\">"
          "<label>" + T("定时时间", "Time") + "</label>"
          "<input name=\"voiceTime\" type=\"time\" value=\"" + timeStr + "\">"
        "</div>"
        "<label>" + T("播报语言", "Language") + "</label>"
        "<select name=\"voiceLang\">"
          "<option value=\"0\"" + sel(vLang, 0) + ">普通话</option>"
          "<option value=\"1\"" + sel(vLang, 1) + ">English</option>"
        "</select>"
        "<label>" + T("音色", "Voice") + "</label>"
        "<select name=\"voiceGen\">"
          "<option value=\"0\"" + sel(vGen, 0) + ">" + T("女声", "Female") + "</option>"
          "<option value=\"1\"" + sel(vGen, 1) + ">" + T("男声", "Male") + "</option>"
        "</select>"
      "</div>"
    "</div>"

    "<button type=\"submit\">" + T("保存并连接", "Save & Connect") + "</button>"
    "</form>"
    "<div class=\"foot\">" + String(FW_NAME) + " · v" + FW_VERSION + "</div>"
    "<script>function pick(e){document.getElementById('ssid').value=e.dataset.s;"
      "document.querySelectorAll('.row').forEach(function(x){x.style.background=''});"
      "e.style.background='#f0fdfa';}"
      "function tog(){document.getElementById('cust').style.display="
      "document.getElementById('apiMode').value=='1'?'block':'none';}"
      "function vtog(){var on=document.getElementById('vOn').value=='1';"
      "document.getElementById('vcfg').style.display=on?'block':'none';"
      "document.getElementById('vtimewrap').style.display="
      "document.getElementById('vTrig').value=='3'?'block':'none';}"
      "tog();vtog();</script>"
    "</body></html>";
}

static void handleRoot() {
  Serial.println("[portal] GET /");
  int lang = s_server.hasArg("lang") ? s_server.arg("lang").toInt() : (int)g_lang;
  s_server.send(200, "text/html; charset=utf-8", buildPage(lang, s_server.hasArg("city") ? s_server.arg("city") : ""));
}

static void handleSave() {
  s_ssid = s_server.arg("ssid"); s_ssid.trim();
  s_pass = s_server.arg("pwd");
  s_city = s_server.arg("city"); s_city.trim();
  int  lang    = s_server.hasArg("lang") ? s_server.arg("lang").toInt() : (int)g_lang;
  int  apiMode = s_server.hasArg("apiMode") ? s_server.arg("apiMode").toInt() : 0;
  String apiUrl = s_server.arg("apiUrl"); apiUrl.trim();
  String apiKey = s_server.arg("apiKey"); apiKey.trim();

  if (!s_ssid.length()) {
    s_server.send(400, "text/html; charset=utf-8", buildPage(lang, s_city));
    return;
  }

  prefs.putString("ssid", s_ssid);
  prefs.putString("pass", s_pass);
  if (s_city.length()) prefs.putString("city", s_city);
  prefs.putInt("lang", lang);
  prefs.putInt("apiMode", (apiMode == 1) ? 1 : 0);
  prefs.putString("apiUrl", apiUrl);
  prefs.putString("apiKey", apiKey);

  g_lang = (Lang)((lang == 1) ? LANG_EN : LANG_ZH);
  g_api.mode = (apiMode == 1) ? 1 : 0;
  g_api.url  = apiUrl;
  g_api.key  = apiKey;

  // 语音播报
  bool voiceOn = (s_server.arg("voiceOn") == "1");
  int  vTrig = s_server.arg("voiceTrig").toInt();
  String vt = s_server.arg("voiceTime");
  int vTime = 7 * 60;
  int colon = vt.indexOf(':');
  if (colon > 0) vTime = vt.substring(0, colon).toInt() * 60 + vt.substring(colon + 1).toInt();
  int vLang = s_server.arg("voiceLang").toInt();
  int vGen  = s_server.arg("voiceGen").toInt();
  prefs.putBool("voiceOn", voiceOn);
  prefs.putInt("voiceTrig", vTrig);
  prefs.putInt("voiceTime", vTime);
  prefs.putInt("voiceLang", vLang);
  prefs.putInt("voiceGen", vGen);
  g_voiceOn = voiceOn; g_voiceTrig = vTrig; g_voiceTimeMin = vTime;
  g_voiceLang = vLang; g_voiceGender = vGen;

  bool alertTest = (s_server.arg("alertTest") == "1");
  prefs.putBool("alertTest", alertTest);
  g_alertTest = alertTest;

  bool alarmOn = (s_server.arg("alarmOn") == "1");
  String at = s_server.arg("alarmTime");
  int alarmTime = 6 * 60 + 30;
  int ac = at.indexOf(':');
  if (ac > 0) alarmTime = at.substring(0, ac).toInt() * 60 + at.substring(ac + 1).toInt();
  prefs.putBool("alarmOn", alarmOn);
  prefs.putInt("alarmTime", alarmTime);
  g_alarmOn = alarmOn; g_alarmTimeMin = alarmTime;

  bool en = (lang == 1);
  s_server.send(200, "text/html; charset=utf-8",
    String("<!DOCTYPE html><html lang=\"") + (en ? "en" : "zh-CN") + "\"><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">" + PAGE_CSS +
    "</head><body><div class=\"hero\"><div class=\"brand\">STICKS3 · WEATHER</div></div>"
    "<h1 style=\"text-align:center\">" + (en ? "Saved" : "已保存") + "</h1><div class=\"card\"><div class=\"ok\">" +
    (en ? "Connecting to WiFi…" : "正在连接 WiFi…") + "</div>"
    "<div class=\"hint\">" + (en ? "Weather shows shortly. You can close this page.<br>If it keeps failing, the hotspot restarts."
                                 : "稍后屏幕会显示天气，即可关闭本页面。<br>若一直连不上，设备会重新开启热点。") +
    "</div></div></body></html>");
  s_saved = true;
}

static void handleNotFound() {
  s_server.sendHeader("Location", "http://" + AP_IP.toString() + "/", true);
  s_server.send(302, "text/plain", "");
}

static bool tryConnect(const String& ssid, const String& pass, uint32_t ms) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < ms) delay(300);
  return WiFi.status() == WL_CONNECTED;
}

bool portalRun(String& cityOut, bool keepWifi) {
  s_keepWifi = keepWifi;
  s_saved = false; s_city = ""; s_ssid = ""; s_pass = "";
  String oldSsid = prefs.getString("ssid", "");
  String oldPass = prefs.getString("pass", "");

  WiFi.mode(WIFI_AP_STA);
  if (AP_PASS[0] == '\0') WiFi.softAP(AP_SSID);
  else                    WiFi.softAP(AP_SSID, AP_PASS);
  delay(300);
  Serial.printf("[portal] AP=%s  ip=%s\n", AP_SSID, WiFi.softAPIP().toString().c_str());

  scanCache();

  s_dns.start(53, "*", AP_IP);
  s_server.on("/", HTTP_GET, handleRoot);
  s_server.on("/save", HTTP_POST, handleSave);
  s_server.onNotFound(handleNotFound);
  s_server.begin();
  Serial.println("[portal] web server started");

  uint32_t t0 = millis();
  while (millis() - t0 < 5UL * 60UL * 1000UL) {
    s_dns.processNextRequest();
    s_server.handleClient();
    if (s_saved) break;
    delay(2);
  }
  s_server.stop();
  s_dns.stop();

  if (!s_saved) { WiFi.softAPdisconnect(true); return false; }

  WiFi.softAPdisconnect(true);
  // 设置模式且 WiFi 没变、且仍然连着 -> 不打断连接
  bool needReconnect = !keepWifi || s_ssid != oldSsid || s_pass != oldPass ||
                       WiFi.status() != WL_CONNECTED;
  bool ok = true;
  if (needReconnect) ok = tryConnect(s_ssid, s_pass, 25000);
  if (ok) {
    cityOut = s_city;
    Serial.printf("[portal] %s ip=%s city=%s\n",
                  needReconnect ? "connected" : "kept",
                  WiFi.localIP().toString().c_str(), s_city.c_str());
  } else {
    Serial.println("[portal] connect failed");
  }
  return ok;
}
