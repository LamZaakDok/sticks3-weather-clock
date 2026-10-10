#include "i18n.h"

Lang g_lang = LANG_ZH;

static const char* WD_ZH[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
static const char* WD_EN[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

const char* i18nWeekday(int w) {
  if (w < 0 || w > 6) w = 0;
  return (g_lang == LANG_EN) ? WD_EN[w] : WD_ZH[w];
}

const char* i18nWeatherDesc(int c) {
  if (g_lang == LANG_ZH) {
    switch (c) {
      case 0:  return "晴";
      case 1:  return "晴间多云";
      case 2:  return "多云";
      case 3:  return "阴";
      case 45: return "有雾";
      case 48: return "雾凇";
      case 51: case 53: case 55: return "毛毛雨";
      case 56: case 57: return "冻毛毛雨";
      case 61: return "小雨";
      case 63: return "中雨";
      case 65: return "大雨";
      case 66: case 67: return "冻雨";
      case 71: return "小雪";
      case 73: return "中雪";
      case 75: return "大雪";
      case 77: return "米雪";
      case 80: return "阵雨";
      case 81: return "强阵雨";
      case 82: return "暴雨";
      case 85: return "小阵雪";
      case 86: return "大阵雪";
      case 95: return "雷阵雨";
      case 96: return "雷阵雨伴冰雹";
      case 99: return "强雷雨伴冰雹";
      default: return "未知";
    }
  } else {
    switch (c) {
      case 0:  return "Clear";
      case 1:  return "Mainly clear";
      case 2:  return "Partly cloudy";
      case 3:  return "Overcast";
      case 45: return "Fog";
      case 48: return "Rime fog";
      case 51: case 53: case 55: return "Drizzle";
      case 56: case 57: return "Freezing drizzle";
      case 61: return "Light rain";
      case 63: return "Rain";
      case 65: return "Heavy rain";
      case 66: case 67: return "Freezing rain";
      case 71: return "Light snow";
      case 73: return "Snow";
      case 75: return "Heavy snow";
      case 77: return "Snow grains";
      case 80: return "Showers";
      case 81: return "Heavy showers";
      case 82: return "Violent showers";
      case 85: return "Snow showers";
      case 86: return "Heavy snow show.";
      case 95: return "Thunderstorm";
      case 96: return "Thunderstorm hail";
      case 99: return "Severe t-storm";
      default: return "Unknown";
    }
  }
}

const char* i18nUvText(float uv) {
  if (isnan(uv)) return L("--", "--");
  if (uv < 3)  return L("弱", "Low");
  if (uv < 6)  return L("中等", "Moderate");
  if (uv < 8)  return L("强", "High");
  if (uv < 11) return L("很强", "Very High");
  return L("极强", "Extreme");
}

const char* i18nAqiCat(int a) {
  if (a < 0)    return "--";
  if (a <= 50)  return L("优", "Good");
  if (a <= 100) return L("良", "Moderate");
  if (a <= 150) return L("轻度污染", "Unhealthy(S)");
  if (a <= 200) return L("中度污染", "Unhealthy");
  if (a <= 300) return L("重度污染", "Very Unhealthy");
  return L("严重污染", "Hazardous");
}

const char* i18nAqiAdvice(int a) {
  if (a < 0)    return L("暂无数据", "No data");
  if (a <= 50)  return L("空气很好，适合开窗通风、户外运动。", "Air is great, open windows & go outside.");
  if (a <= 100) return L("空气不错，可正常开窗和活动。", "Air is fine, normal activity OK.");
  if (a <= 150) return L("敏感人群减少户外剧烈运动。", "Sensitive groups: reduce outdoor exercise.");
  if (a <= 200) return L("建议关窗，减少户外活动。", "Close windows, reduce outdoor activity.");
  if (a <= 300) return L("空气较差，关窗、尽量待在室内。", "Poor air: stay indoors, keep windows shut.");
  return L("空气很差，务必关窗、避免外出。", "Very poor air: keep windows shut, stay indoors.");
}
