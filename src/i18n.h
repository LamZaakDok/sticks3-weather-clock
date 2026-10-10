#pragma once
#include <Arduino.h>

enum Lang { LANG_ZH = 0, LANG_EN = 1 };
extern Lang g_lang;

// 按当前语言取字符串
inline const char* L(const char* zh, const char* en) { return g_lang == LANG_EN ? en : zh; }

const char* i18nWeekday(int wday);      // 0=周日
const char* i18nWeatherDesc(int code);  // WMO 天气代码
const char* i18nUvText(float uv);
const char* i18nAqiCat(int aqi);
const char* i18nAqiAdvice(int aqi);
