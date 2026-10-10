#include "lunar.h"

// 农历数据表（1900–2100）。每年 1 个字：
//   低 4 位 = 闰月月份（0 表示无闰月）
//   bit4..bit15 = 12 个（含闰月共 13 个月）每月 29/30 天
//   bit16 = 闰月是大月(30)还是小月(29)
static const uint32_t LUNAR_INFO[] = {
  0x04bd8, 0x04ae0, 0x0a570, 0x054d5, 0x0d260, 0x0d950, 0x16554, 0x056a0, 0x09ad0, 0x055d2, // 1900-1909
  0x04ae0, 0x0a5b6, 0x0a4d0, 0x0d250, 0x1d255, 0x0b540, 0x0d6a0, 0x0ada2, 0x095b0, 0x14977, // 1910-1919
  0x04970, 0x0a4b0, 0x0b4b5, 0x06a50, 0x06d40, 0x1ab54, 0x02b60, 0x09570, 0x052f2, 0x04970, // 1920-1929
  0x06566, 0x0d4a0, 0x0ea50, 0x06e95, 0x05ad0, 0x02b60, 0x186e3, 0x092e0, 0x1c8d7, 0x0c950, // 1930-1939
  0x0d4a0, 0x1d8a6, 0x0b550, 0x056a0, 0x1a5b4, 0x025d0, 0x092d0, 0x0d2b2, 0x0a950, 0x0b557, // 1940-1949
  0x06ca0, 0x0b550, 0x15355, 0x04da0, 0x0a5b0, 0x14573, 0x052b0, 0x0a9a8, 0x0e950, 0x06aa0, // 1950-1959
  0x0aea6, 0x0ab50, 0x04b60, 0x0aae4, 0x0a570, 0x05260, 0x0f263, 0x0d950, 0x05b57, 0x056a0, // 1960-1969
  0x096d0, 0x04dd5, 0x04ad0, 0x0a4d0, 0x0d4d4, 0x0d250, 0x0d558, 0x0b540, 0x0b6a0, 0x195a6, // 1970-1979
  0x095b0, 0x049b0, 0x0a974, 0x0a4b0, 0x0b27a, 0x06a50, 0x06d40, 0x0af46, 0x0ab60, 0x09570, // 1980-1989
  0x04af5, 0x04970, 0x064b0, 0x074a3, 0x0ea50, 0x06b58, 0x055c0, 0x0ab60, 0x096d5, 0x092e0, // 1990-1999
  0x0c960, 0x0d954, 0x0d4a0, 0x0da50, 0x07552, 0x056a0, 0x0abb7, 0x025d0, 0x092d0, 0x0cab5, // 2000-2009
  0x0a950, 0x0b4a0, 0x0baa4, 0x0ad50, 0x055d9, 0x04ba0, 0x0a5b0, 0x15176, 0x052b0, 0x0a930, // 2010-2019
  0x07954, 0x06aa0, 0x0ad50, 0x05b52, 0x04b60, 0x0a6e6, 0x0a4e0, 0x0d260, 0x0ea65, 0x0d530, // 2020-2029
  0x05aa0, 0x076a3, 0x096d0, 0x04afb, 0x04ad0, 0x0a4d0, 0x1d0b6, 0x0d250, 0x0d520, 0x0dd45, // 2030-2039
  0x0b5a0, 0x056d0, 0x055b2, 0x049b0, 0x0a577, 0x0a4b0, 0x0aa50, 0x1b255, 0x06d20, 0x0ada0, // 2040-2049
  0x14b63, 0x09370, 0x049f8, 0x04970, 0x064b0, 0x168a6, 0x0ea50, 0x06b20, 0x1a6c4, 0x0aae0, // 2050-2059
  0x0a2e0, 0x0d2e3, 0x0c960, 0x0d557, 0x0d4a0, 0x0da50, 0x05d55, 0x056a0, 0x0a6d0, 0x055d4, // 2060-2069
  0x052d0, 0x0a9b8, 0x0a950, 0x0b4a0, 0x0b6a6, 0x0ad50, 0x055a0, 0x0aba4, 0x0a5b0, 0x052b0, // 2070-2079
  0x0b273, 0x06930, 0x07337, 0x06aa0, 0x0ad50, 0x14b55, 0x04b60, 0x0a570, 0x054e4, 0x0d160, // 2080-2089
  0x0e968, 0x0d520, 0x0daa0, 0x16aa6, 0x056d0, 0x04ae0, 0x0a9d4, 0x0a2d0, 0x0d150, 0x0f252, // 2090-2099
  0x0d520                                                                                     // 2100
};

static int leapMonth(int y) { return LUNAR_INFO[y - 1900] & 0xf; }
static int leapDays(int y)  { return leapMonth(y) ? ((LUNAR_INFO[y - 1900] & 0x10000) ? 30 : 29) : 0; }
static int monthDays(int y, int m) { return (LUNAR_INFO[y - 1900] & (0x10000 >> m)) ? 30 : 29; }

static int yearDays(int y) {
  int sum = 348;
  for (int i = 0x8000; i > 0x8; i >>= 1) sum += (LUNAR_INFO[y - 1900] & i) ? 1 : 0;
  return sum + leapDays(y);
}

// 公历 -> 距 1970-01-01 的天数（Howard Hinnant 算法）
static long daysFromCivil(int y, unsigned m, unsigned d) {
  y -= (m <= 2);
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (long)era * 146097L + (long)doe - 719468L;
}

static const char* CN_MON[] = {"正", "二", "三", "四", "五", "六", "七", "八", "九", "十", "冬", "腊"};
static const char* CN_NUM[] = {"一", "二", "三", "四", "五", "六", "七", "八", "九", "十"};

static String dayName(int d) {
  if (d == 10) return "初十";
  if (d == 20) return "二十";
  if (d == 30) return "三十";
  if (d < 10)  return String("初") + CN_NUM[d - 1];
  if (d < 20)  return String("十") + CN_NUM[d - 11];
  return String("廿") + CN_NUM[d - 21];
}

String lunarDateString(int y, int m, int d) {
  if (y < 1900 || y > 2100) return "";
  long offset = daysFromCivil(y, m, d) - daysFromCivil(1900, 1, 31);

  int i = 1900, temp = 0;
  while (i < 2101 && offset > 0) { temp = yearDays(i); offset -= temp; i++; }
  if (offset < 0) { offset += temp; i--; }
  int ly = i;

  int leap = leapMonth(ly);
  bool isLeap = false;
  i = 1;
  while (i < 13 && offset > 0) {
    if (leap > 0 && i == leap + 1 && !isLeap) { i--; isLeap = true; temp = leapDays(ly); }
    else { temp = monthDays(ly, i); }
    if (isLeap && i == leap + 1) isLeap = false;
    offset -= temp;
    i++;
  }
  if (offset == 0 && leap > 0 && i == leap + 1) {
    if (isLeap) { isLeap = false; }
    else { isLeap = true; i--; }
  }
  if (offset < 0) { offset += temp; i--; }

  int lm = i;
  int ld = (int)offset + 1;
  if (lm < 1 || lm > 12) return "";

  return String(isLeap ? "闰" : "") + CN_MON[lm - 1] + "月" + dayName(ld);
}

// ============================================================
//  干支纪年 / 纪月 / 纪日（21 世纪范围内）
// ============================================================
static const char* GAN[]    = {"甲", "乙", "丙", "丁", "戊", "己", "庚", "辛", "壬", "癸"};
static const char* ZHI[]    = {"子", "丑", "寅", "卯", "辰", "巳", "午", "未", "申", "酉", "戌", "亥"};
static const char* ZODIAC[] = {"鼠", "牛", "虎", "兔", "龙", "蛇", "马", "羊", "猴", "鸡", "狗", "猪"};

// 21 世纪节气推算常数（小寒…冬至，共 24 个）
static const double TERM_C[24] = {
  5.4055, 20.12, 3.87, 18.73, 5.63, 20.646, 4.81, 20.1, 5.52, 21.04, 5.678, 21.37,
  7.108, 22.83, 7.5, 23.13, 7.646, 23.042, 8.318, 23.438, 7.438, 22.36, 7.18, 21.94};

static int solarTermDay(int year, int n) {
  int yy = year - 2000;
  int d = (int)(yy * 0.2422 + TERM_C[n]) - yy / 4;
  return d;
}

String chineseAlmanacString(int y, int m, int d) {
  if (y < 2000 || y > 2099) return lunarDateString(y, m, d);   // 超出范围退回农历

  // 年柱（以立春为界）
  int lichun = solarTermDay(y, 2);
  int yy = (m < 2 || (m == 2 && d < lichun)) ? y - 1 : y;
  int yidx = ((yy - 4) % 60 + 60) % 60;
  int ygan = yidx % 10;

  // 月柱（以节为界）
  static const int JIE[12] = {0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22};
  static const int MZ[12]  = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0};
  int zhi = (d >= solarTermDay(y, JIE[m - 1])) ? MZ[m - 1] : MZ[(m - 2 + 12) % 12];
  int mgan = (((ygan % 5) * 2 + 2) + ((zhi - 2 + 12) % 12)) % 10;

  // 日柱
  long off = daysFromCivil(y, m, d) - daysFromCivil(1900, 1, 31);
  int didx = (int)(((off + 40) % 60 + 60) % 60);

  String s = String(GAN[yidx % 10]) + ZHI[yidx % 12] + ZODIAC[yidx % 12] + "年 ";
  s += String(GAN[mgan]) + ZHI[zhi] + "月 ";
  s += String(GAN[didx % 10]) + ZHI[didx % 12] + "日";

  String lu = lunarDateString(y, m, d);
  if (lu.length()) s += String("（") + lu + "）";
  return s;
}
