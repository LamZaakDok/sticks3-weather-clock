#include "weather.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

ApiConfig g_api;

// 拼接接口地址：base + 参数 + 可选 apikey
static String buildUrl(const String& base, const String& params) {
  String url = base + (base.indexOf('?') >= 0 ? "&" : "?") + params;
  if (g_api.key.length()) url += "&apikey=" + g_api.key;
  return url;
}

// 天气接口基址（默认 / 自定义）
static String forecastBase() {
  if (g_api.mode == 1 && g_api.url.length()) return g_api.url;
  return "https://api.open-meteo.com/v1/forecast";
}
static String airBase() {
  if (g_api.mode == 1) return "";    // 自定义模式下不做空气质量
  return "https://air-quality-api.open-meteo.com/v1/air-quality";
}

// ---------------- 工具：URL 转义 ----------------
static String urlEncode(const String& s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    uint8_t c = (uint8_t)s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      o += (char)c;
    } else {
      char buf[4];
      snprintf(buf, sizeof(buf), "%%%02X", c);
      o += buf;
    }
  }
  return o;
}

// ---------------- 工具：HTTPS GET ----------------
static bool httpGet(const String& url, String& out, int timeoutMs = 12000) {
  WiFiClientSecure client;
  client.setInsecure();                 // 简化：不校验证书
  client.setTimeout(timeoutMs / 1000);

  HTTPClient https;
  https.setConnectTimeout(timeoutMs);
  https.setTimeout(timeoutMs);
  if (!https.begin(client, url)) return false;

  int code = https.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[http] GET %d (%s)\n", code, url.c_str());
    https.end();
    return false;
  }
  out = https.getString();
  https.end();
  return out.length() > 0;
}

// ---------------- 取 "T" 之后的时间 HH:MM ----------------
static String hhmmFromIso(const String& iso) {
  int t = iso.indexOf('T');
  if (t < 0 || iso.length() < (size_t)(t + 6)) return "";
  return iso.substring(t + 1, t + 6);
}

// 地名要素权重（行政级别越高越优先）
static int featureWeight(const String& code) {
  if (code == "PPLC")  return 100;
  if (code == "PPLA")  return 95;
  if (code == "PPLA2") return 90;
  if (code == "PPLA3") return 80;
  if (code == "PPLA4") return 70;
  if (code == "PPLA5") return 60;
  if (code == "ADM1")  return 55;
  if (code == "ADM2")  return 50;
  if (code == "PPL")   return 20;
  if (code == "ISL")   return 5;
  return 10;
}

static bool geocodeOne(const String& name, const char* lang, bool preferCN, float& lat, float& lon,
                       String& rname, String& radmin, int& score) {
  String url = "https://geocoding-api.open-meteo.com/v1/search?name=" + urlEncode(name) +
               "&count=5&language=" + String(lang) + "&format=json";
  String payload;
  if (!httpGet(url, payload)) return false;

  JsonDocument doc;
  if (deserializeJson(doc, payload)) return false;
  JsonArray results = doc["results"].as<JsonArray>();
  if (results.isNull() || results.size() == 0) return false;

  bool found = false;
  for (JsonObject r : results) {
    int w = featureWeight(String((const char*)(r["feature_code"] | "")));
    int pop = (int)(r["population"] | 0);
    int sc = w * 1000000 + pop;        // 行政级别优先，人口次之
    if (preferCN && String((const char*)(r["country"] | "")) == "中国") sc += 200000000;
    if (!found || sc > score) {
      found  = true;
      score  = sc;
      lat    = r["latitude"].as<float>();
      lon    = r["longitude"].as<float>();
      rname  = String((const char*)(r["name"] | ""));
      radmin = String((const char*)(r["admin1"] | ""));
    }
  }
  return found;
}

// 是否含中文字符
static bool hasCJK(const String& s) {
  for (size_t i = 0; i < s.length(); i++)
    if ((uint8_t)s[i] >= 0xE0) return true;
  return false;
}

// ---------------- 城市 -> 经纬度 ----------------
bool weather_geocode(const String& city, float& lat, float& lon, String& resolved) {
  String c = city; c.trim();
  if (!c.length()) return false;

  bool cn = hasCJK(c);
  const char* lang = cn ? "zh" : "en";    // 英文输入用 en：返回英文名，避免繁体字缺字形
  String variants[3];
  int nv = 0;
  variants[nv++] = c;
  bool suffixed = c.endsWith("市") || c.endsWith("区") || c.endsWith("县");
  if (suffixed) {
    variants[nv++] = c.substring(0, c.length() - 3);     // 去掉尾部“市/区/县”（中文 3 字节）
  } else if (c.indexOf("市") < 0 && c.indexOf("区") < 0 && c.indexOf("县") < 0 && c.indexOf("州") < 0) {
    variants[nv++] = c + "市";                            // 补上“市”再查
  }

  float  bestLat = 0, bestLon = 0;
  String bestName, bestAdmin;
  int    bestScore = -1;
  for (int i = 0; i < nv; i++) {
    float la, lo; String nm, ad;
    int sc = -1;
    if (geocodeOne(variants[i], lang, cn, la, lo, nm, ad, sc) && sc > bestScore) {
      bestScore = sc; bestLat = la; bestLon = lo; bestName = nm; bestAdmin = ad;
    }
  }
  if (bestScore < 0) return false;

  lat = bestLat; lon = bestLon;
  resolved = bestName;
  if (bestAdmin.length() && resolved.indexOf(bestAdmin) < 0 && resolved != bestAdmin)
    resolved = resolved + " " + bestAdmin;

  Serial.printf("[geo] %s -> %.4f,%.4f (%s)\n", city.c_str(), lat, lon, resolved.c_str());
  return true;
}

// ---------------- 拉取天气 ----------------
bool weather_fetch(float lat, float lon, WeatherData& out) {
  String params = "latitude=" + String(lat, 4) +
               "&longitude=" + String(lon, 4) +
               "&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,weather_code,wind_speed_10m"
               "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,uv_index_max,sunrise,sunset"
               "&hourly=temperature_2m,precipitation_probability"
               "&timezone=auto&forecast_days=7&forecast_hours=24";
  String url = buildUrl(forecastBase(), params);

  String payload;
  if (!httpGet(url, payload, 15000)) return false;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("[wx] json err: %s\n", err.c_str());
    return false;
  }

  JsonObject cur = doc["current"];
  if (cur.isNull()) return false;

  out.now.temp     = cur["temperature_2m"]    | NAN;
  out.now.feels    = cur["apparent_temperature"] | NAN;
  out.now.humidity = cur["relative_humidity_2m"] | -1;
  out.now.wind     = cur["wind_speed_10m"]    | NAN;
  out.now.code     = cur["weather_code"]      | -1;
  out.now.isDay    = ((int)(cur["is_day"] | 1)) == 1;
  out.now.time     = hhmmFromIso(String((const char*)(cur["time"] | "")));

  JsonObject daily = doc["daily"];
  JsonArray  dcode = daily["weather_code"];
  JsonArray  dmax  = daily["temperature_2m_max"];
  JsonArray  dmin  = daily["temperature_2m_min"];
  JsonArray  dpop  = daily["precipitation_probability_max"];
  JsonArray  dtime = daily["time"];
  JsonArray  duv   = daily["uv_index_max"];
  JsonArray  dsr   = daily["sunrise"];
  JsonArray  dss   = daily["sunset"];

  out.dailyCount = 0;
  for (size_t i = 0; i < dtime.size() && out.dailyCount < 7; i++) {
    DailyWx d;
    d.date = String((const char*)(dtime[i] | ""));
    d.code = dcode[i] | -1;
    d.tmax = dmax[i]  | NAN;
    d.tmin = dmin[i]  | NAN;
    d.pop  = dpop[i]  | -1;
    out.daily[out.dailyCount++] = d;
  }
  out.uvMax  = duv.size()  > 0 ? (float)(duv[0]  | NAN) : NAN;
  out.sunrise = dsr.size() > 0 ? hhmmFromIso(String((const char*)(dsr[0] | ""))) : "";
  out.sunset  = dss.size() > 0 ? hhmmFromIso(String((const char*)(dss[0] | ""))) : "";

  // 未来 24 小时
  JsonObject hourly = doc["hourly"];
  JsonArray  htemp  = hourly["temperature_2m"];
  JsonArray  hpop   = hourly["precipitation_probability"];
  JsonArray  htime  = hourly["time"];
  out.hourCount = 0;
  for (size_t i = 0; i < htime.size() && out.hourCount < 24; i++) {
    out.hourTemp[out.hourCount] = htemp[i] | NAN;
    out.hourPop[out.hourCount]  = hpop[i]  | 0;
    String ts = String((const char*)(htime[i] | ""));
    out.hourH[out.hourCount]    = ts.length() >= 13 ? ts.substring(11, 13).toInt() : 0;
    out.hourCount++;
  }

  out.valid     = (out.dailyCount > 0);
  out.updatedAt = millis();
  Serial.printf("[wx] ok: %.0f°C code=%d days=%d hours=%d\n",
                out.now.temp, out.now.code, out.dailyCount, out.hourCount);
  return out.valid;
}

// ---------------- 空气质量 ----------------
bool weather_fetch_air(float lat, float lon, WeatherData& out) {
  String base = airBase();
  if (!base.length()) { out.airValid = false; return false; }   // 自定义模式下不做空气质量
  String params = "latitude=" + String(lat, 4) + "&longitude=" + String(lon, 4) +
                  "&current=us_aqi,pm2_5,pm10&timezone=auto";
  String url = buildUrl(base, params);
  String payload;
  if (!httpGet(url, payload, 15000)) return false;

  JsonDocument doc;
  if (deserializeJson(doc, payload)) return false;
  JsonObject cur = doc["current"];
  if (cur.isNull()) return false;

  out.aqi  = cur["us_aqi"] | -1;
  out.pm25 = cur["pm2_5"]  | NAN;
  out.pm10 = cur["pm10"]   | NAN;
  out.airValid = (out.aqi >= 0);
  Serial.printf("[air] aqi=%d pm2.5=%.1f\n", out.aqi, out.pm25);
  return out.airValid;
}
