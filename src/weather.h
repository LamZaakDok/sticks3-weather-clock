#pragma once
#include <Arduino.h>

// ---- 当前实况 ----
struct CurrentWx {
  float  temp     = NAN;   // 气温 °C
  float  feels    = NAN;   // 体感 °C
  int    humidity = -1;    // 湿度 %
  float  wind     = NAN;   // 风速 m/s
  int    code     = -1;    // WMO 天气代码
  bool   isDay    = true;
  String time;
};

// ---- 单日预报 ----
struct DailyWx {
  String date;             // yyyy-mm-dd
  int    code = -1;
  float  tmax = NAN;
  float  tmin = NAN;
  int    pop  = -1;        // 降水概率 %
};

// ---- 全部天气数据 ----
struct WeatherData {
  String   city;
  CurrentWx now;
  DailyWx  daily[7];
  int      dailyCount = 0;
  float    uvMax   = NAN;
  String   sunrise;
  String   sunset;
  bool     valid     = false;
  uint32_t updatedAt = 0;

  // 未来 24 小时（第 4 页趋势图）
  float  hourTemp[24] = {0};
  int    hourPop[24]  = {0};
  int    hourH[24]    = {0};     // 对应的小时（0-23）
  int    hourCount    = 0;

  // 空气质量（第 5 页）
  bool   airValid = false;
  int    aqi      = -1;
  float  pm25     = NAN;
  float  pm10     = NAN;
};

// 天气接口配置
struct ApiConfig {
  int    mode = 0;      // 0 = 默认 Open-Meteo，1 = 自定义
  String url;           // 自定义地址（需兼容 Open-Meteo 格式）
  String key;           // 可选 API Key
};
extern ApiConfig g_api;

// 城市名 -> 经纬度（Open-Meteo Geocoding，中文）
bool weather_geocode(const String& city, float& lat, float& lon, String& resolved);

// 拉取天气（Open-Meteo Forecast：实况 + 7 日 + 24 小时）
bool weather_fetch(float lat, float lon, WeatherData& out);

// 拉取空气质量（Open-Meteo Air Quality）
bool weather_fetch_air(float lat, float lon, WeatherData& out);
