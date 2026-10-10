#pragma once
#include <Arduino.h>

// 公历 -> 农历（1900–2100），返回中文农历描述，如 "九月初一"、"闰六月初一"
String lunarDateString(int y, int m, int d);

// 干支纪法，如 "丙午马年 戊戌月 丁巳日（九月初一）"
String chineseAlmanacString(int y, int m, int d);
