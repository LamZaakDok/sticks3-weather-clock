#pragma once
#include <M5Unified.h>

// 天气图标分类
enum WCat {
  WC_SUN, WC_PARTLY, WC_CLOUD, WC_OVERCAST, WC_FOG,
  WC_DRIZZLE, WC_RAIN, WC_SLEET, WC_SNOW, WC_SHOWER, WC_THUNDER
};

WCat        wcatFromCode(int code);
const char* wdescFromCode(int code);

// 在 (cx,cy) 居中画一个宽高约 size 的天气图标
// p = 动画相位（秒），p 变化即产生动画；传 0 为静止
void drawWeatherIcon(M5Canvas& g, int cx, int cy, int size, WCat cat, bool day, uint16_t bg, float p = 0);
