#include "ui.h"
#include "icons.h"
#include "i18n.h"
#include <math.h>

extern bool g_alertTest;    // 预警测试开关（main.cpp）

// ============================================================
//  界面绘制（中/英双语）
// ============================================================

// —— 背景渐变（按天气分类选色）——
static void bgColors(M5Canvas& g, WCat cat, uint16_t& top, uint16_t& bot) {
  switch (cat) {
    case WC_CLOUD: case WC_OVERCAST:
      top = g.color565(51, 72, 95);  bot = g.color565(20, 29, 41); break;
    case WC_RAIN: case WC_DRIZZLE: case WC_SHOWER: case WC_THUNDER:
      top = g.color565(39, 75, 99);  bot = g.color565(14, 26, 38); break;
    case WC_SNOW: case WC_SLEET:
      top = g.color565(58, 78, 102); bot = g.color565(24, 34, 46); break;
    case WC_FOG:
      top = g.color565(62, 80, 95);  bot = g.color565(26, 35, 46); break;
    default: // 晴 / 多云
      top = g.color565(42, 111, 176); bot = g.color565(22, 50, 79); break;
  }
}

static void vgrad(M5Canvas& g, uint16_t top, uint16_t bot) {
  int h = g.height(), w = g.width();
  int tr = (top >> 11) & 0x1F, tg = (top >> 5) & 0x3F, tb = top & 0x1F;
  int br = (bot >> 11) & 0x1F, bg = (bot >> 5) & 0x3F, bb = bot & 0x1F;
  for (int y = 0; y < h; y++) {
    int r = tr + (br - tr) * y / h;
    int gg = tg + (bg - tg) * y / h;
    int b = tb + (bb - tb) * y / h;
    g.drawFastHLine(0, y, w, (r << 11) | (gg << 5) | b);
  }
}

static uint16_t gradAt(uint16_t top, uint16_t bot, int y) {
  int tr = (top >> 11) & 0x1F, tg = (top >> 5) & 0x3F, tb = top & 0x1F;
  int br = (bot >> 11) & 0x1F, bg = (bot >> 5) & 0x3F, bb = bot & 0x1F;
  int r = tr + (br - tr) * y / SCREEN_H;
  int gg = tg + (bg - tg) * y / SCREEN_H;
  int b = tb + (bb - tb) * y / SCREEN_H;
  return (r << 11) | (gg << 5) | b;
}

// —— UTF-8 ——
static int utf8Len(unsigned char c) {
  if (c < 0x80) return 1;
  if ((c >> 5) == 0x6) return 2;
  if ((c >> 4) == 0xE) return 3;
  return 4;
}

// —— 自动换行（限制最多 maxLines 行，超出以 ... 收尾）——
static void drawWrap(M5Canvas& g, const String& s, int x, int y, int maxW, int lh, int maxLines) {
  g.setTextDatum(top_left);
  int cy = y, i = 0;
  for (int line = 0; line < maxLines; line++) {
    int cx = x;
    while (i < (int)s.length()) {
      int n = utf8Len((unsigned char)s[i]);
      String ch = s.substring(i, i + n);
      int cw = g.textWidth(ch);
      if (cx + cw > x + maxW) break;
      g.drawString(ch, cx, cy);
      cx += cw;
      i += n;
    }
    if (i >= (int)s.length()) return;
    if (line == maxLines - 1) {
      if (cx + g.textWidth("...") <= x + maxW) g.drawString("...", cx, cy);
      return;
    }
    cy += lh;
  }
}

// —— 从 ISO 日期算星期（0=周日）——
static int weekdayFromIso(const String& iso) {
  if (iso.length() < 10) return 0;
  int y = iso.substring(0, 4).toInt();
  int m = iso.substring(5, 7).toInt();
  int d = iso.substring(8, 10).toInt();
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) y -= 1;
  return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

// —— 生活建议 ——
static bool isRainCat(WCat c) {
  return c == WC_RAIN || c == WC_SHOWER || c == WC_THUNDER || c == WC_DRIZZLE || c == WC_SLEET;
}

static String clothingAdvice(float feels, String& detail) {
  float t = isnan(feels) ? 20 : feels;
  if (g_lang == LANG_EN) {
    if (t >= 30) { detail = "Hot: shorts & tee, drink water."; return "Shorts"; }
    if (t >= 24) { detail = "Warm: tee, thin jacket at night."; return "T-shirt"; }
    if (t >= 18) { detail = "Mild: long sleeve or light coat."; return "Light coat"; }
    if (t >= 12) { detail = "Cool: wear a jacket or coat."; return "Jacket"; }
    if (t >= 5)  { detail = "Cold: thick coat or sweater."; return "Thick coat"; }
    if (t >= 0)  { detail = "Freezing: down jacket needed."; return "Down coat"; }
    detail = "Severe cold: down coat + gloves."; return "Full winter";
  } else {
    if (t >= 30) { detail = "天气炎热，短袖短裤、多喝水防中暑。"; return "短袖短裤"; }
    if (t >= 24) { detail = "白天偏热，短袖为主，早晚可备薄外套。"; return "短袖即可"; }
    if (t >= 18) { detail = "气温舒适，长袖或薄外套刚好。";       return "薄外套"; }
    if (t >= 12) { detail = "有点凉，加件外套或夹克，注意保暖。"; return "加件外套"; }
    if (t >= 5)  { detail = "较冷，穿厚外套或毛衣，别着凉。";     return "厚外套"; }
    if (t >= 0)  { detail = "寒冷，棉衣或羽绒服，注意防寒。";     return "羽绒服"; }
    detail = "严寒，羽绒服+围巾手套，谨防冻伤。"; return "全套羽绒服";
  }
}

static String umbrellaAdvice(const WeatherData& w, String& detail) {
  int c0 = w.dailyCount > 0 ? w.daily[0].code : w.now.code;
  int p0 = w.dailyCount > 0 ? w.daily[0].pop : -1;
  bool rain0 = isRainCat(wcatFromCode(c0)) || p0 >= 50;
  if (rain0) {
    if (g_lang == LANG_EN) {
      detail = String("Rain likely today (") + (p0 < 0 ? String("high") : String(p0) + "%") + "). Take an umbrella.";
      return "Umbrella today";
    }
    detail = String("今天降水概率 ") + (p0 < 0 ? String("较高") : String(p0) + "%") + "，出门带上伞。";
    return "今天要带伞";
  }
  if (w.dailyCount > 1) {
    int c1 = w.daily[1].code, p1 = w.daily[1].pop;
    if (isRainCat(wcatFromCode(c1)) || p1 >= 50) {
      if (g_lang == LANG_EN) {
        detail = String("Rain likely tomorrow (") + (p1 < 0 ? String("high") : String(p1) + "%") + ").";
        return "Umbrella tomorrow";
      }
      detail = String("明天降水概率 ") + (p1 < 0 ? String("较高") : String(p1) + "%") + "，记得提前备伞。";
      return "明天要带伞";
    }
  }
  if (g_lang == LANG_EN) { detail = "Little rain expected, no umbrella needed."; return "No umbrella"; }
  detail = "未来几天降水概率不高，不用带伞。";
  return "不用带伞";
}

// —— 组件 ——
static void tag(M5Canvas& g, int x, int y, int w, int h, const String& text, uint16_t bg, uint16_t fg) {
  g.fillRoundRect(x, y, w, h, 6, bg);
  g.setFont(&fonts::efontCN_10);
  g.setTextDatum(middle_center);
  g.setTextColor(fg);
  g.drawString(text, x + w / 2, y + h / 2);
}

static void indicator(M5Canvas& g, int page, int count) {
  int cy = 233;
  int gap = 10;
  int x0 = 67 - (count - 1) * gap / 2;
  for (int i = 0; i < count; i++) {
    int x = x0 + i * gap;
    if (i == page) g.fillRoundRect(x - 5, cy - 2, 10, 4, 2, g.color565(96, 172, 255));
    else           g.fillRect(x - 2, cy - 2, 4, 4, g.color565(120, 140, 166));
  }
}

// ---- 通用混合字体：ASCII 用 Font 系列（半角），中文用 efontCN ----
enum HAlign { AL_LEFT, AL_CENTER, AL_RIGHT };

template <typename C, typename A>
static int mixedWidth(M5Canvas& g, const String& s, C cjk, A asc) {
  int w = 0, i = 0;
  while (i < (int)s.length()) {
    int n = utf8Len((unsigned char)s[i]);
    String ch = s.substring(i, i + n); i += n;
    if (n == 1) g.setFont(asc); else g.setFont(cjk);
    w += g.textWidth(ch);
  }
  return w;
}

// 从 topY 顶部起绘制，水平按 al 对齐
template <typename C, typename A>
static void drawMixed(M5Canvas& g, const String& s, int x, int topY, HAlign al,
                      uint16_t col, C cjk, A asc) {
  int w = mixedWidth(g, s, cjk, asc);
  int cx = (al == AL_CENTER) ? x - w / 2 : (al == AL_RIGHT) ? x - w : x;
  g.setTextDatum(top_left);
  g.setTextColor(col);
  int i = 0;
  while (i < (int)s.length()) {
    int n = utf8Len((unsigned char)s[i]);
    String ch = s.substring(i, i + n); i += n;
    if (n == 1) g.setFont(asc); else g.setFont(cjk);
    g.drawString(ch, cx, topY);
    cx += g.textWidth(ch);
  }
}

// 数值行用的中/英小号混排（中文 12、ASCII 16）
static int statsWidth(M5Canvas& g, const String& s) {
  return mixedWidth(g, s, &fonts::efontCN_12, &fonts::Font2);
}
static void drawStats(M5Canvas& g, const String& s, int cx, int topY, uint16_t col) {
  drawMixed(g, s, cx, topY, AL_CENTER, col, &fonts::efontCN_12, &fonts::Font2);
}

// 放得下就居中/靠左，放不下就横向滚动
static void drawFitOrScroll(M5Canvas& g, const String& s, int x, int y, int maxW,
                            uint16_t col, bool center) {
  int tw = g.textWidth(s);
  g.setTextColor(col);
  if (tw <= maxW) {
    g.setTextDatum(center ? middle_center : middle_left);
    g.drawString(s, center ? (x + maxW / 2) : x, y);
    return;
  }
  int span = tw + maxW + 24;
  int off  = (int)((millis() / 26) % span);
  int xpos = x + maxW - off;                 // 从右侧进入、向左滚出
  g.setClipRect(x, y - 10, maxW, 20);
  g.setTextDatum(middle_left);
  g.drawString(s, xpos, y);
  g.clearClipRect();
}

static void drawTop(M5Canvas& g, const String& city, int battery) {
  g.setFont(&fonts::efontCN_16_b);
  drawFitOrScroll(g, city, 8, 15, 84, TFT_WHITE, false);

  drawMixed(g, battery >= 0 ? String(battery) + "%" : String("--"), 127, 7, AL_RIGHT,
            g.color565(208, 220, 238), &fonts::efontCN_12, &fonts::Font2);
}

// ---------------- 天气预警 ----------------
static int buildAlerts(const WeatherData& w, String& out) {
  out = "";
  int n = 0;
  auto add = [&](const String& s) { if (n) out += "    "; out += s; n++; };

  if (g_alertTest) add(L("预警测试 高温预警 37°", "TEST Heat alert 37°"));

  WCat c = wcatFromCode(w.now.code);
  if (c == WC_THUNDER)                              add(L("雷电预警", "Thunderstorm"));
  if (w.now.code == 45 || w.now.code == 48)         add(L("大雾预警", "Dense fog"));
  float tmax = w.dailyCount > 0 ? w.daily[0].tmax : NAN;
  float tmin = w.dailyCount > 0 ? w.daily[0].tmin : NAN;
  if (!isnan(tmax) && tmax >= 35)                   add(String(L("高温预警 ", "Heat ")) + (int)lroundf(tmax) + "°");
  if (!isnan(tmin) && tmin <= -5)                   add(L("严寒预警", "Severe cold"));
  int pop = w.dailyCount > 0 ? w.daily[0].pop : -1;
  if (isRainCat(c) || pop >= 60)                    add(L("降雨预警", "Rain alert"));
  if (!isnan(w.now.wind) && w.now.wind >= 10.8f)    add(L("大风预警", "Strong wind"));
  if (w.airValid && w.aqi >= 200)                   add(L("空气重污染", "Air pollution"));
  if (!isnan(w.uvMax) && w.uvMax >= 11)             add(L("紫外线极强", "Extreme UV"));
  return n;
}

static void drawToday(M5Canvas& g, const WeatherData& w, const String& clock, uint16_t bgc, float p) {
  WCat cat = wcatFromCode(w.valid ? w.now.code : w.now.code);

  // 顶部：有预警就显示红色预警条（滚动），否则显示时间
  String alerts;
  int na = buildAlerts(w, alerts);
  if (na > 0) {
    int bx = 3, by = 22, bw = SCREEN_W - 6, bh = 20;
    uint16_t bg = (na >= 2) ? g.color565(206, 54, 54) : g.color565(222, 120, 40);
    g.fillRoundRect(bx, by, bw, bh, 6, bg);
    g.setFont(&fonts::efontCN_12);
    g.setTextDatum(middle_left);
    g.setTextColor(TFT_WHITE);
    int tw = g.textWidth(alerts);
    int x = (tw <= bw - 10) ? (bx + (bw - tw) / 2)
                            : (bx + 6 - (int)((millis() / 28) % (tw + 24)));
    g.setClipRect(bx, by, bw, bh);
    g.drawString(alerts, x, by + bh / 2);
    g.clearClipRect();
  } else {
    g.setFont(&fonts::efontCN_12);
    g.setTextDatum(middle_left);
    g.setTextColor(g.color565(186, 201, 222));
    g.drawString(clock, 8, 32);
  }

  drawWeatherIcon(g, 67, 70, 74, cat, w.now.isDay, bgc, p);   // 带动画

  g.setTextColor(TFT_WHITE);
  String t = w.valid && !isnan(w.now.temp) ? String((int)lroundf(w.now.temp)) : String("--");
  g.setFont(&fonts::efontCN_24_b);
  g.setTextSize(1.5f);
  int tw = g.textWidth(t);
  g.setTextSize(1.0f);
  int tcw = g.textWidth("°C");
  int x0 = (SCREEN_W - (tw + tcw + 2)) / 2;
  g.setTextDatum(top_left);
  g.setTextSize(1.5f);
  g.drawString(t, x0, 112);
  g.setTextSize(1.0f);
  g.drawString("°C", x0 + tw + 2, 124);

  g.setFont(&fonts::efontCN_16_b);
  g.setTextDatum(middle_center);
  g.setTextColor(TFT_WHITE);
  {
    String desc = w.valid ? String(i18nWeatherDesc(w.now.code)) : String("--");
    drawFitOrScroll(g, desc, 8, 160, SCREEN_W - 16, TFT_WHITE, true);
  }

  uint16_t labCol = g.color565(198, 212, 232);
  drawStats(g, L("体感", "Feels"), 22, 173, labCol);
  drawStats(g, L("湿度", "Humid"), 62, 173, labCol);
  drawStats(g, L("风速 m/s", "Wind m/s"), 104, 173, labCol);
  drawStats(g, isnan(w.now.feels) ? String("--") : String((int)lroundf(w.now.feels)) + "°", 22, 190, TFT_WHITE);
  drawStats(g, w.now.humidity < 0 ? String("--") : String(w.now.humidity) + "%", 62, 190, TFT_WHITE);
  drawStats(g, isnan(w.now.wind) ? String("--") : String(w.now.wind, 1), 104, 190, TFT_WHITE);

  String cd, ud;
  String cs = clothingAdvice(w.now.feels, cd);
  String us = umbrellaAdvice(w, ud);
  bool rain = !(us == "不用带伞" || us == "No umbrella");
  tag(g, 3, 208, 64, 20, cs, g.color565(58, 78, 44), g.color565(220, 240, 186));
  tag(g, 68, 208, 64, 20, us,
      rain ? g.color565(40, 84, 140) : g.color565(48, 62, 82),
      rain ? g.color565(190, 226, 255) : g.color565(200, 214, 232));
}

static void drawDays(M5Canvas& g, const WeatherData& w) {
  g.setFont(&fonts::efontCN_12);
  g.setTextDatum(middle_left);
  g.setTextColor(g.color565(186, 201, 222));
  g.drawString(L("未来 5 天", "Next 5 Days"), 8, 30);

  int n = w.dailyCount < 5 ? w.dailyCount : 5;
  int y0 = 54, rh = 37;
  for (int i = 0; i < n; i++) {
    int y = y0 + i * rh;
    if (i > 0) g.drawFastHLine(6, y - rh / 2, SCREEN_W - 12, g.color565(70, 92, 118));

    g.setFont(&fonts::efontCN_16_b);
    g.setTextDatum(middle_left);
    g.setTextColor(TFT_WHITE);
    String lbl = (i == 0) ? String(L("今天", "Today")) : (i == 1) ? String(L("明天", "Tmrw"))
               : String(i18nWeekday(weekdayFromIso(w.daily[i].date)));
    g.drawString(lbl, 8, y - 3);

    g.setFont(&fonts::efontCN_10);
    g.setTextColor(g.color565(176, 192, 214));
    String md = w.daily[i].date.length() >= 10 ? w.daily[i].date.substring(5) : "";
    g.drawString(md, 8, y + 13);

    drawWeatherIcon(g, 67, y, 34, wcatFromCode(w.daily[i].code), true, g.color565(30, 52, 74));

    drawMixed(g, isnan(w.daily[i].tmax) ? String("--") : String((int)lroundf(w.daily[i].tmax)) + "°",
              107, y - 10, AL_RIGHT, TFT_WHITE, &fonts::efontCN_16_b, &fonts::Font2);
    drawMixed(g, isnan(w.daily[i].tmin) ? String("") : String((int)lroundf(w.daily[i].tmin)) + "°",
              129, y - 10, AL_RIGHT, g.color565(176, 192, 214), &fonts::efontCN_16_b, &fonts::Font2);
  }
}

static void cardBox(M5Canvas& g, int y, int h, const char* title, const String& value,
                    const String& desc, uint16_t bg, uint16_t accent) {
  int x = 6, w = SCREEN_W - 12;
  g.fillRoundRect(x, y, w, h, 10, bg);
  g.fillRoundRect(x, y + 6, 3, h - 12, 1, accent);

  g.setFont(&fonts::efontCN_12);
  g.setTextDatum(top_left);
  g.setTextColor(g.color565(234, 242, 253));
  g.drawString(title, x + 11, y + 5);

  int ty = y + 23;
  if (value.length()) {
    g.setFont(&fonts::efontCN_16_b);
    g.setTextColor(accent);
    g.drawString(value, x + 11, ty);
    ty += 22;
  }
  if (desc.length()) {
    g.setFont(&fonts::efontCN_12);
    g.setTextColor(g.color565(226, 235, 248));
    drawWrap(g, desc, x + 11, ty, w - 20, 15, 2);
  }
}

static void marquee(M5Canvas& g, const String& s, int y, int h, uint16_t col) {
  int tw = mixedWidth(g, s, &fonts::efontCN_12, &fonts::Font2);
  int span = tw + SCREEN_W;
  int x = SCREEN_W - (int)((millis() / 24) % span);
  int topY = y + (h - 16) / 2;
  g.setClipRect(0, y, SCREEN_W, h);
  g.setTextDatum(top_left);
  g.setTextColor(col);
  int i = 0, cx = x;
  while (i < (int)s.length()) {
    int nn = utf8Len((unsigned char)s[i]);
    String ch = s.substring(i, i + nn); i += nn;
    if (nn == 1) g.setFont(&fonts::Font2); else g.setFont(&fonts::efontCN_12);
    g.drawString(ch, cx, topY);
    cx += g.textWidth(ch);
  }
  g.clearClipRect();
}

static void drawTips(M5Canvas& g, const WeatherData& w) {
  g.setFont(&fonts::efontCN_12);
  g.setTextDatum(middle_left);
  g.setTextColor(g.color565(186, 201, 222));
  g.drawString(L("生活提示", "Daily Tips"), 8, 30);

  String cd, ud;
  String cs = clothingAdvice(w.now.feels, cd);
  String us = umbrellaAdvice(w, ud);
  bool rain = !(us == "不用带伞" || us == "No umbrella");

  cardBox(g, 38, 78, L("穿衣建议", "Clothing"), cs, cd, g.color565(36, 60, 88), g.color565(162, 216, 142));
  cardBox(g, 120, 78, L("要带伞吗", "Umbrella?"), us, ud,
          rain ? g.color565(40, 88, 146) : g.color565(36, 60, 88),
          rain ? g.color565(150, 200, 255) : g.color565(178, 194, 212));

  String marq = String(L("紫外线 ", "UV ")) + i18nUvText(w.uvMax) +
                String(L("          日出 ", "          Sunrise ")) + (w.sunrise.length() ? w.sunrise : String("--")) +
                String(L("          日落 ", "          Sunset ")) + (w.sunset.length() ? w.sunset : String("--")) +
                "          ";
  marquee(g, marq, 204, 22, g.color565(216, 228, 246));
}

// ---------------- 第 3 页：防晒指数 ----------------
static uint16_t uvColor(M5Canvas& g, float uv) {
  if (isnan(uv)) return g.color565(150, 160, 175);
  if (uv < 3)  return g.color565(110, 215, 130);
  if (uv < 6)  return g.color565(238, 208, 92);
  if (uv < 8)  return g.color565(242, 162, 72);
  if (uv < 11) return g.color565(232, 104, 104);
  return g.color565(172, 112, 222);
}
static String uvAdvice(float uv) {
  if (isnan(uv)) return L("暂无数据", "No data");
  if (uv < 3)  return L("紫外线弱，无需特别防晒。", "UV is low, no special protection.");
  if (uv < 6)  return L("建议 SPF15+ 防晒霜，戴帽子。", "Use SPF15+, wear a hat.");
  if (uv < 8)  return L("建议 SPF30+，遮阳帽 + 墨镜。", "Use SPF30+, hat + sunglasses.");
  if (uv < 11) return L("紫外线很强，SPF30+/50，尽量避开正午。", "Very high: SPF30+/50, avoid midday.");
  return L("紫外线极强，务必 SPF50+，避免外出。", "Extreme: SPF50+, avoid going out.");
}

static void drawSunscreen(M5Canvas& g, const WeatherData& w) {
  g.setFont(&fonts::efontCN_12);
  g.setTextDatum(middle_left);
  g.setTextColor(g.color565(186, 201, 222));
  g.drawString(L("防晒指数", "UV Index"), 8, 30);

  if (isnan(w.uvMax)) {
    g.setFont(&fonts::efontCN_16_b);
    g.setTextDatum(middle_center);
    g.setTextColor(g.color565(200, 210, 226));
    g.drawString(L("暂无数据", "No data"), 67, 120);
    return;
  }

  uint16_t col = uvColor(g, w.uvMax);

  drawMixed(g, L("今日最高 UV", "Today's max UV"), 67, 44, AL_CENTER,
            g.color565(190, 205, 226), &fonts::efontCN_10, &fonts::Font2);

  g.setFont(&fonts::Font6);
  g.setTextDatum(middle_center);
  g.setTextColor(col);
  g.drawString(String((int)lroundf(w.uvMax)), 67, 86);

  drawMixed(g, String(L("紫外线 ", "UV ")) + i18nUvText(w.uvMax), 67, 118, AL_CENTER,
            col, &fonts::efontCN_16_b, &fonts::Font2);

  int bx = 14, bw = SCREEN_W - 28, seg = bw / 4, by = 156;
  uint16_t cols[4] = {g.color565(110, 215, 130), g.color565(238, 208, 92),
                      g.color565(242, 162, 72),  g.color565(232, 104, 104)};
  for (int i = 0; i < 4; i++) g.fillRect(bx + i * seg, by, seg - 2, 8, cols[i]);
  float up = w.uvMax; if (up < 0) up = 0; if (up > 11) up = 11;
  int px = bx + (int)(up / 11.0f * (bw - 2));
  g.fillTriangle(px, by - 5, px - 5, by - 12, px + 5, by - 12, TFT_WHITE);

  g.setFont(&fonts::efontCN_12);
  g.setTextDatum(top_left);
  g.setTextColor(g.color565(228, 237, 250));
  drawWrap(g, uvAdvice(w.uvMax), 12, 172, SCREEN_W - 24, 14, 2);

  marquee(g, String(L("10:00-16:00 紫外线最强    ", "UV peaks 10:00-16:00    ")),
          204, 20, g.color565(180, 196, 218));
}

// ---------------- 第 5 页：未来 24 小时趋势 ----------------
static void drawTrend(M5Canvas& g, const WeatherData& w, float prog) {
  g.setFont(&fonts::efontCN_12);
  g.setTextDatum(middle_left);
  g.setTextColor(g.color565(186, 201, 222));
  g.drawString(L("未来 24 小时", "Next 24 Hours"), 8, 30);

  int n = w.hourCount;
  if (n < 2) {
    g.setFont(&fonts::efontCN_16_b);
    g.setTextDatum(middle_center);
    g.setTextColor(g.color565(200, 210, 226));
    g.drawString(L("暂无数据", "No data"), 67, 120);
    return;
  }

  float mn = 1e9f, mx = -1e9f;
  for (int i = 0; i < n; i++) { float t = w.hourTemp[i]; if (t < mn) mn = t; if (t > mx) mx = t; }
  if (mx - mn < 1) { mn -= 0.5f; mx += 0.5f; }

  drawMixed(g, String(L("最高 ", "Max ")) + (int)lroundf(mx) + String(L("°  最低 ", "°  Min ")) +
               (int)lroundf(mn) + "°", 12, 40, AL_LEFT,
            g.color565(200, 214, 234), &fonts::efontCN_10, &fonts::Font2);

  int x0 = 12, x1 = 127;
  int yTop = 64, yBot = 134;
  int popTop = 142, popBase = 176;
  auto X = [&](int i) { return x0 + (x1 - x0) * i / (n - 1); };
  auto Y = [&](float t) { return yBot - (int)((t - mn) / (mx - mn) * (yBot - yTop)); };

  if (prog < 0) prog = 0; if (prog > 1) prog = 1;
  float shownF = prog * (n - 1);
  int shown = (int)shownF;

  for (int i = 0; i < n && i <= shown; i++) {
    int p = w.hourPop[i]; if (p < 0) p = 0; if (p > 100) p = 100;
    int bh = (popBase - popTop) * p / 100;
    if (bh > 0) g.fillRect(X(i) - 2, popBase - bh, 4, bh,
                           p >= 50 ? g.color565(96, 172, 240) : g.color565(70, 118, 168));
  }
  g.drawFastHLine(x0 - 2, popBase, x1 - x0 + 4, g.color565(96, 116, 140));

  uint16_t lc = g.color565(255, 190, 90);
  for (int i = 1; i <= shown && i < n; i++)
    g.drawWideLine(X(i - 1), Y(w.hourTemp[i - 1]), X(i), Y(w.hourTemp[i]), 3, lc);
  if (shown < n - 1) {                          // 正在生长的那一段
    float f = shownF - shown;
    int xa = X(shown), xb = X(shown + 1);
    float ya = Y(w.hourTemp[shown]), yb = Y(w.hourTemp[shown + 1]);
    g.drawWideLine(xa, (int)ya, xa + (int)((xb - xa) * f), (int)(ya + (yb - ya) * f), 3, lc);
  }

  for (int i = 0; i < n; i += 6) {
    char b[8]; snprintf(b, sizeof(b), "%02d", w.hourH[i]);
    drawMixed(g, String(b), X(i), popBase + 4, AL_CENTER, g.color565(180, 196, 218),
              &fonts::efontCN_10, &fonts::Font2);
  }

  g.drawWideLine(24, 206, 40, 206, 3, g.color565(255, 190, 90));
  drawMixed(g, L("气温", "Temp"), 44, 200, AL_LEFT, g.color565(200, 214, 234), &fonts::efontCN_10, &fonts::Font2);
  g.fillRect(82, 203, 8, 6, g.color565(96, 172, 240));
  drawMixed(g, L("降水", "Rain"), 94, 200, AL_LEFT, g.color565(200, 214, 234), &fonts::efontCN_10, &fonts::Font2);
}

// ---------------- 第 6 页：空气质量 ----------------
static uint16_t aqiColor(M5Canvas& g, int a) {
  if (a < 0)    return g.color565(150, 160, 175);
  if (a <= 50)  return g.color565(110, 215, 130);
  if (a <= 100) return g.color565(238, 208, 92);
  if (a <= 150) return g.color565(242, 162, 72);
  if (a <= 200) return g.color565(232, 104, 104);
  if (a <= 300) return g.color565(172, 112, 222);
  return g.color565(158, 66, 78);
}

static void drawAir(M5Canvas& g, const WeatherData& w) {
  g.setFont(&fonts::efontCN_12);
  g.setTextDatum(middle_left);
  g.setTextColor(g.color565(186, 201, 222));
  g.drawString(L("空气质量", "Air Quality"), 8, 30);

  if (!w.airValid) {
    g.setFont(&fonts::efontCN_16_b);
    g.setTextDatum(middle_center);
    g.setTextColor(g.color565(200, 210, 226));
    g.drawString(L("暂无数据", "No data"), 67, 120);
    return;
  }

  uint16_t col = aqiColor(g, w.aqi);

  g.setFont(&fonts::Font6);                        // 半角大号数字
  g.setTextDatum(middle_center);
  g.setTextColor(col);
  g.drawString(String(w.aqi), 67, 84);

  g.setFont(&fonts::efontCN_16_b);
  g.setTextColor(col);
  g.drawString(i18nAqiCat(w.aqi), 67, 122);

  drawMixed(g, L("美国 AQI 指数", "US AQI"), 67, 140, AL_CENTER,
            g.color565(190, 205, 226), &fonts::efontCN_10, &fonts::Font2);

  uint16_t pc = g.color565(200, 214, 234);
  drawMixed(g, "PM2.5", 40, 164, AL_LEFT, pc, &fonts::efontCN_12, &fonts::Font2);
  drawMixed(g, "PM10",  40, 184, AL_LEFT, pc, &fonts::efontCN_12, &fonts::Font2);
  drawMixed(g, isnan(w.pm25) ? String("--") : String(w.pm25, 0), 108, 164, AL_RIGHT, TFT_WHITE, &fonts::efontCN_12, &fonts::Font2);
  drawMixed(g, isnan(w.pm10) ? String("--") : String(w.pm10, 0), 108, 184, AL_RIGHT, TFT_WHITE, &fonts::efontCN_12, &fonts::Font2);

  g.setFont(&fonts::efontCN_12);
  g.setTextColor(g.color565(228, 237, 250));
  g.setTextDatum(top_left);
  drawWrap(g, i18nAqiAdvice(w.aqi), 12, 204, SCREEN_W - 24, 14, 2);
}

// ---------------- 第 7 页：洗晒指数 ----------------
static int laundryScore(const WeatherData& w, String& reasons) {
  int score = 100;
  reasons = "";
  bool en = (g_lang == LANG_EN);
  int pop  = w.dailyCount > 0 ? w.daily[0].pop  : -1;
  int code = w.dailyCount > 0 ? w.daily[0].code : w.now.code;
  if (isRainCat(wcatFromCode(code))) { score -= 45; reasons += en ? "Rainy " : "今天有雨 "; }
  if (pop >= 70)      { score -= 50; reasons += en ? "High rain chance " : "降水概率高 "; }
  else if (pop >= 40) { score -= 30; reasons += en ? "Some rain " : "可能有雨 "; }
  else if (pop >= 20) { score -= 12; }
  int hum = w.now.humidity;
  if (hum >= 88)      { score -= 25; reasons += en ? "Very humid " : "湿度很大 "; }
  else if (hum >= 75) { score -= 12; reasons += en ? "Humid " : "湿度偏大 "; }
  if (!isnan(w.now.wind) && w.now.wind < 1.0f) { score -= 8; reasons += en ? "No breeze " : "几乎无风 "; }
  float tmax = w.dailyCount > 0 ? w.daily[0].tmax : w.now.temp;
  if (!isnan(tmax) && tmax < 8)       { score -= 18; reasons += en ? "Cold " : "气温低 "; }
  else if (!isnan(tmax) && tmax > 34) { score -= 8; }
  if (score < 0) score = 0;
  if (score > 100) score = 100;
  return score;
}

static void drawLaundry(M5Canvas& g, const WeatherData& w) {
  g.setFont(&fonts::efontCN_12);
  g.setTextDatum(middle_left);
  g.setTextColor(g.color565(186, 201, 222));
  g.drawString(L("洗晒指数", "Drying Index"), 8, 30);

  String reasons;
  int sc = laundryScore(w, reasons);
  const char* lbl;
  uint16_t col;
  if (sc >= 80)      { lbl = L("非常适合", "Excellent"); col = g.color565(120, 216, 140); }
  else if (sc >= 60) { lbl = L("适合", "Good");          col = g.color565(182, 222, 130); }
  else if (sc >= 40) { lbl = L("一般", "Fair");          col = g.color565(238, 204, 100); }
  else if (sc >= 20) { lbl = L("不太适合", "Poor");      col = g.color565(238, 152, 92); }
  else               { lbl = L("不适合", "Bad");         col = g.color565(226, 112, 112); }

  g.setFont(&fonts::Font6);
  g.setTextDatum(middle_center);
  g.setTextColor(col);
  g.drawString(String(sc), 67, 86);

  g.setFont(&fonts::efontCN_16_b);
  g.setTextColor(col);
  g.drawString(lbl, 67, 126);

  g.setFont(&fonts::efontCN_12);
  g.setTextDatum(top_left);
  g.setTextColor(g.color565(228, 237, 250));
  reasons.trim();
  if (!reasons.length()) reasons = L("天气不错，适合洗晒。", "Nice weather, good for drying.");
  drawWrap(g, reasons, 12, 156, SCREEN_W - 24, 15, 3);

  String info = String(L("湿度 ", "Humidity ")) +
                (w.now.humidity < 0 ? String("--") : String(w.now.humidity) + "%") +
                String(L("    风速 ", "    Wind ")) +
                (isnan(w.now.wind) ? String("--") : String(w.now.wind, 1) + " m/s") +
                String(L("    降水概率 ", "    Rain ")) +
                ((w.dailyCount > 0 && w.daily[0].pop >= 0) ? String(w.daily[0].pop) + "%" : String("--")) +
                "        ";
  marquee(g, info, 204, 20, g.color565(180, 196, 218));
}

// ---------------- 对外入口 ----------------
void ui_draw(M5Canvas& g, const WeatherData& w, int page, int battery, const String& clockStr, float anim) {
  WCat cat = wcatFromCode(w.valid ? w.now.code : -1);
  uint16_t top, bot;
  bgColors(g, cat, top, bot);
  vgrad(g, top, bot);

  drawTop(g, w.city.length() ? w.city : String("--"), battery);

  switch (page) {
    case 0:  drawToday(g, w, clockStr, gradAt(top, bot, 74), anim); break;
    case 1:  drawDays(g, w);           break;
    case 2:  drawSunscreen(g, w);      break;
    case 3:  drawTips(g, w);           break;
    case 4:  drawTrend(g, w, anim);    break;
    case 5:  drawAir(g, w);            break;
    default: drawLaundry(g, w);        break;
  }

  indicator(g, page, 7);
}

// ---------------- 横屏时钟页 ----------------
void ui_drawClock(M5Canvas& g, const String& timeStr, const String& dateStr,
                  const String& lunarStr, int battery) {
  uint16_t top = g.color565(20, 34, 52), bot = g.color565(8, 14, 22);
  vgrad(g, top, bot);

  g.setFont(&fonts::Font7);                      // 半角大号时间
  g.setTextDatum(middle_center);
  g.setTextColor(TFT_WHITE);
  g.drawString(timeStr, g.width() / 2, 44);

  drawMixed(g, dateStr, g.width() / 2, 78, AL_CENTER,
            g.color565(186, 208, 236), &fonts::efontCN_16_b, &fonts::Font2);

  if (lunarStr.length()) {
    g.setFont(&fonts::efontCN_12);
    if (g.textWidth(lunarStr) > g.width() - 8) g.setFont(&fonts::efontCN_10);
    g.setTextDatum(middle_center);
    g.setTextColor(g.color565(150, 200, 235));
    g.drawString(lunarStr, g.width() / 2, 112);
  }

  if (battery >= 0) {
    drawMixed(g, String(battery) + "%", g.width() - 8, 6, AL_RIGHT,
              g.color565(150, 175, 205), &fonts::efontCN_10, &fonts::Font2);
  }
}
