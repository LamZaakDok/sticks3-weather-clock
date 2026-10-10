#pragma once
#include <Arduino.h>
#include "weather.h"

// 挂载语音素材文件系统（LittleFS）
bool voice_begin();
bool voice_ready();

// 播报当前天气（语言 / 音色 由 g_voiceLang/g_voiceGender 决定）
void voice_announce(const WeatherData& w);

extern int g_voiceLang;    // 0=普通话 1=English
extern int g_voiceGender;  // 0=女声 1=男声
extern int g_voiceVolume;  // 0..255
