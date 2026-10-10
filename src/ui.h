#pragma once
#include <M5Unified.h>
#include "config.h"
#include "weather.h"

// 天气页（page: 0=今日 1=未来 2=防晒 3=生活提示 4=24小时趋势 5=空气质量 6=洗晒指数）
// anim: 今日页=动画相位(秒)；趋势页=绘制进度 0..1；其它页忽略
void ui_draw(M5Canvas& g, const WeatherData& w, int page, int battery, const String& clockStr, float anim);

// 横屏时钟页（唤醒时首先显示）
void ui_drawClock(M5Canvas& g, const String& timeStr, const String& dateStr,
                  const String& lunarStr, int battery);
