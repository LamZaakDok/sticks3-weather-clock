#include "icons.h"
#include <math.h>

// ============================================================
//  天气图标：全部用屏幕图元绘制，支持动画（p = 相位秒数）
// ============================================================

WCat wcatFromCode(int c) {
  switch (c) {
    case 0:                                    return WC_SUN;
    case 1: case 2:                            return WC_PARTLY;
    case 3:                                    return WC_OVERCAST;
    case 45: case 48:                          return WC_FOG;
    case 51: case 53: case 55: case 56: case 57: return WC_DRIZZLE;
    case 61: case 63: case 65: case 66: case 67: return WC_RAIN;
    case 71: case 73: case 75: case 77:        return WC_SNOW;
    case 80: case 81:                          return WC_SHOWER;
    case 82:                                   return WC_RAIN;
    case 85: case 86:                          return WC_SNOW;
    case 95: case 96: case 99:                 return WC_THUNDER;
    default:                                   return WC_CLOUD;
  }
}

const char* wdescFromCode(int c) {
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
}

// ---------------- 内部绘制助手 ----------------
static inline int bob(float p, float amp) { return (int)lroundf(sinf(p * 1.2f) * amp); }

static void cloud(M5Canvas& g, int cx, int cy, int w, uint16_t col) {
  int rl = (int)(w * 0.20f);
  int rm = (int)(w * 0.26f);
  int rr = (int)(w * 0.19f);
  g.fillCircle(cx - (int)(w * 0.22f), cy + (int)(w * 0.05f), rl, col);
  g.fillCircle(cx + (int)(w * 0.22f), cy + (int)(w * 0.07f), rr, col);
  g.fillCircle(cx,                    cy - (int)(w * 0.11f), rm, col);
  g.fillRoundRect(cx - (int)(w * 0.42f), cy - (int)(w * 0.02f),
                  (int)(w * 0.84f), (int)(w * 0.30f), (int)(w * 0.15f), col);
}

// 太阳：光条旋转 + 轻微呼吸
static void sun(M5Canvas& g, int cx, int cy, int s, uint16_t col, float p) {
  int   r  = (int)(s * 0.20f);
  float r1 = s * 0.32f;
  float r2 = s * 0.46f + s * 0.03f * sinf(p * 3.0f);
  float wdt = s * 0.055f;
  float rot = p * 0.5f;
  for (int i = 0; i < 8; i++) {
    float a = i * PI / 4.0f + rot;
    g.drawWideLine(cx + r1 * cosf(a), cy + r1 * sinf(a),
                   cx + r2 * cosf(a), cy + r2 * sinf(a), wdt, col);
  }
  g.fillCircle(cx, cy, r, col);
}

// 月亮：弯月 + 轻微上下浮动
static void moon(M5Canvas& g, int cx, int cy, int s, uint16_t col, uint16_t bg, float p) {
  int dy = bob(p, 1.5f);
  int r = (int)(s * 0.24f);
  g.fillCircle(cx, cy + dy, r, col);
  g.fillCircle(cx + (int)(r * 0.62f), cy + dy - (int)(r * 0.42f), (int)(r * 0.94f), bg);
}

// 雨点：持续下落
static void drops(M5Canvas& g, int cx, int cy, int s, uint16_t col, int n, float p, float speed, float span) {
  float wdt = s * 0.05f;
  if (span <= 0) span = s * 0.36f;
  for (int i = 0; i < n; i++) {
    float x = cx + (i - (n - 1) / 2.0f) * (s * 0.22f);
    float ph = fmodf(p * speed + i * (span / n), span);
    float y = cy + ph;
    g.drawWideLine(x, y, x - s * 0.05f, y + s * 0.12f, wdt, col);
  }
}

// 雪花：下落 + 左右飘
static void flakes(M5Canvas& g, int cx, int cy, int s, uint16_t col, int n, float p) {
  float wdt = s * 0.035f, d = s * 0.05f, span = s * 0.34f;
  for (int i = 0; i < n; i++) {
    float bx = cx + (i - (n - 1) / 2.0f) * (s * 0.22f);
    float ph = fmodf(p * 26.0f + i * (span / n), span);
    float y = cy + ph;
    float x = bx + sinf(p * 1.5f + i) * s * 0.04f;
    g.drawWideLine(x - d, y, x + d, y, wdt, col);
    g.drawWideLine(x, y - d, x, y + d, wdt, col);
  }
}

// 以质心为扇心填充凹多边形（闪电用）
static void polyFill(M5Canvas& g, const float* xy, int n, int cx, int cy, int s, uint16_t col) {
  float mx = 0, my = 0;
  for (int i = 0; i < n; i++) { mx += xy[2 * i]; my += xy[2 * i + 1]; }
  mx /= n; my /= n;
  auto X = [&](float v) { return cx + (int)(v * s); };
  auto Y = [&](float v) { return cy + (int)(v * s); };
  for (int i = 0; i < n; i++) {
    int j = (i + 1) % n;
    g.fillTriangle(X(mx), Y(my), X(xy[2 * i]), Y(xy[2 * i + 1]), X(xy[2 * j]), Y(xy[2 * j + 1]), col);
  }
}

static const float BOLT[] = {
   0.10f, 0.00f,
  -0.02f, 0.20f,
   0.05f, 0.20f,
  -0.06f, 0.42f,
   0.12f, 0.14f,
   0.05f, 0.14f,
   0.12f, 0.00f
};

void drawWeatherIcon(M5Canvas& g, int cx, int cy, int size, WCat cat, bool day, uint16_t bg, float p) {
  int s = size;
  uint16_t C_SUN   = g.color565(255, 205, 74);
  uint16_t C_CLOUD = g.color565(244, 248, 252);
  uint16_t C_DARK  = g.color565(170, 186, 204);
  uint16_t C_RAIN  = g.color565(120, 195, 255);
  uint16_t C_SNOW  = g.color565(228, 242, 255);
  uint16_t C_MOON  = g.color565(232, 238, 248);

  int cb = bob(p, 2);   // 云朵轻浮动

  switch (cat) {
    case WC_SUN:
      if (day) sun(g, cx, cy, s, C_SUN, p);
      else     moon(g, cx, cy, s, C_MOON, bg, p);
      break;

    case WC_PARTLY:
      if (day) sun(g, cx - (int)(s * 0.16f), cy - (int)(s * 0.16f), (int)(s * 0.62f), C_SUN, p);
      else     moon(g, cx - (int)(s * 0.16f), cy - (int)(s * 0.16f), (int)(s * 0.62f), C_MOON, bg, p);
      cloud(g, cx + (int)(s * 0.06f), cy + (int)(s * 0.12f) + cb, (int)(s * 0.72f), C_CLOUD);
      break;

    case WC_CLOUD:
      cloud(g, cx, cy + cb, (int)(s * 0.90f), C_CLOUD);
      break;

    case WC_OVERCAST:
      cloud(g, cx - (int)(s * 0.06f), cy - (int)(s * 0.06f) + cb, (int)(s * 0.78f), C_DARK);
      cloud(g, cx + (int)(s * 0.06f), cy + (int)(s * 0.08f) + cb, (int)(s * 0.74f), C_CLOUD);
      break;

    case WC_FOG: {
      cloud(g, cx, cy - (int)(s * 0.10f) + cb, (int)(s * 0.86f), C_CLOUD);
      float shift = sinf(p * 1.1f) * s * 0.05f;
      for (int i = 0; i < 3; i++)
        g.drawWideLine(cx - s * 0.30f + shift, cy + s * (0.20f + 0.11f * i),
                       cx + s * (0.30f - 0.06f * i) + shift, cy + s * (0.20f + 0.11f * i),
                       s * 0.045f, g.color565(205, 222, 240));
      break;
    }

    case WC_DRIZZLE:
      cloud(g, cx, cy - (int)(s * 0.12f) + cb, (int)(s * 0.84f), C_CLOUD);
      drops(g, cx, cy + (int)(s * 0.12f), s, C_RAIN, 3, p, 22.0f, s * 0.22f);
      break;

    case WC_RAIN:
      cloud(g, cx, cy - (int)(s * 0.14f) + cb, (int)(s * 0.86f), C_CLOUD);
      drops(g, cx, cy + (int)(s * 0.06f), s, C_RAIN, 3, p, 55.0f, s * 0.36f);
      break;

    case WC_SHOWER:
      sun(g, cx - (int)(s * 0.20f), cy - (int)(s * 0.20f), (int)(s * 0.52f), C_SUN, p);
      cloud(g, cx + (int)(s * 0.08f), cy - (int)(s * 0.02f) + cb, (int)(s * 0.70f), C_CLOUD);
      drops(g, cx + (int)(s * 0.06f), cy + (int)(s * 0.16f), (int)(s * 0.8f), C_RAIN, 3, p, 55.0f, s * 0.26f);
      break;

    case WC_SLEET:
      cloud(g, cx, cy - (int)(s * 0.14f) + cb, (int)(s * 0.84f), C_CLOUD);
      drops(g, cx - (int)(s * 0.14f), cy + (int)(s * 0.06f), s, C_RAIN, 1, p, 55.0f, s * 0.30f);
      flakes(g, cx + (int)(s * 0.14f), cy + (int)(s * 0.06f), s, C_SNOW, 1, p);
      break;

    case WC_SNOW:
      cloud(g, cx, cy - (int)(s * 0.14f) + cb, (int)(s * 0.86f), C_CLOUD);
      flakes(g, cx, cy + (int)(s * 0.06f), s, C_SNOW, 3, p);
      break;

    case WC_THUNDER:
      cloud(g, cx, cy - (int)(s * 0.18f) + cb, (int)(s * 0.86f), C_DARK);
      if (fmodf(p, 1.6f) < 1.05f)                    // 闪烁
        polyFill(g, BOLT, 7, cx, cy + (int)(s * 0.06f), s, C_SUN);
      drops(g, cx, cy + (int)(s * 0.10f), s, C_RAIN, 2, p, 55.0f, s * 0.24f);
      break;
  }
}
