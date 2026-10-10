#pragma once
#include <Arduino.h>

// 启动自定义配网（AP 热点 + 中文网页）。
// 成功保存并连上 WiFi 返回 true，并在 cityOut 里输出所选城市。
// keepWifi=true 时用于「设置」：不清 WiFi、保留当前连接（STA 不中断）。
bool portalRun(String& cityOut, bool keepWifi = false);
