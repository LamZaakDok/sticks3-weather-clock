#pragma once

// ============================================================
//  StickS3 校园天气机 · 配置常量
// ============================================================

#define FW_NAME     "StickS3 校园天气机"
#define FW_VERSION  "1.0.0"

// ---- 配网热点（手机连接这个热点来设置 WiFi / 城市）----
#define AP_SSID     "StickS3-Weather"
#define AP_PASS     ""                 // 留空 = 开放热点，手机可直接连

// ---- 默认城市（首次开机若未在配网页填写，则用它）----
#define DEFAULT_CITY "北京"
#define DEFAULT_LAT   39.9075f
#define DEFAULT_LON   116.3972f

// ---- 时区（中国标准时间）----
#define TZ_INFO     "CST-8"

// ---- 自动刷新间隔（毫秒）----
#define REFRESH_INTERVAL_MS (30UL * 60UL * 1000UL)

// ---- 屏幕 ----
#define SCREEN_W 135
#define SCREEN_H 240

// ---- 显示与省电 ----
#define BRIGHT_ON        90                 // 正常亮度
#define DAY_IDLE_MS      (30UL * 1000UL)    // 白天无操作息屏
#define NIGHT_IDLE_MS    (8UL * 1000UL)     // 夜间无操作息屏
#define NIGHT_START_HOUR 23                 // 夜间开始（含）
#define NIGHT_END_HOUR   7                  // 夜间结束（不含）
#define CPU_MHZ          80                 // 主频（省电）
