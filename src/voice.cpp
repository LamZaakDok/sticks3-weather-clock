#include "voice.h"
#include <M5Unified.h>
#include <LittleFS.h>
#include <vector>
#include <math.h>
#include "icons.h"
#include "i18n.h"

// 语言 / 音色（由 main.cpp 从配置加载）
int g_voiceLang   = 0;   // 0=普通话 1=English
int g_voiceGender = 0;   // 0=女声 1=男声
int g_voiceVolume = 180; // 0..255

static bool s_fsReady = false;

bool voice_ready() { return s_fsReady; }

bool voice_begin() {
  s_fsReady = LittleFS.begin(true);          // 失败则格式化
  if (s_fsReady) {
    Serial.printf("[voice] LittleFS ok, %u/%u bytes\n",
                  (unsigned)LittleFS.usedBytes(), (unsigned)LittleFS.totalBytes());
  } else {
    Serial.println("[voice] LittleFS mount failed");
  }
  return s_fsReady;
}

// ============================================================
//  IMA ADPCM 解码
// ============================================================
static const int IDX_TAB[16] = {-1,-1,-1,-1,2,4,6,8,-1,-1,-1,-1,2,4,6,8};
static const int STEP_TAB[89] = {
  7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,
  107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,
  796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,
  4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,
  18500,20350,22385,24623,27086,29794,32767};

static inline int adpcmNibble(int nib, int& pred, int& idx) {
  int step = STEP_TAB[idx];
  int diff = step >> 3;
  if (nib & 1) diff += step >> 2;
  if (nib & 2) diff += step >> 1;
  if (nib & 4) diff += step;
  if (nib & 8) pred -= diff; else pred += diff;
  if (pred > 32767) pred = 32767; else if (pred < -32768) pred = -32768;
  idx += IDX_TAB[nib];
  if (idx < 0) idx = 0; else if (idx > 88) idx = 88;
  return pred;
}

// 读 32 位小端
static uint32_t le32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t le16(const uint8_t* p) { return p[0] | (p[1] << 8); }

// 打开音频文件（兼容挂载点差异）
static File openClip(const String& name) {
  String p = "/audio/" + String(g_voiceLang ? "en" : "zh") + "_" +
             (g_voiceGender ? "male" : "female") + "/" + name + ".wav";
  File f = LittleFS.open(p, "r");
  if (!f) f = LittleFS.open("/littlefs" + p, "r");
  return f;
}

// 播放一个片段（阻塞到播完）
static bool playClip(const String& name) {
  File f = openClip(name);
  if (!f) { Serial.printf("[voice] missing %s\n", name.c_str()); return false; }
  size_t len = f.size();
  uint8_t* buf = (uint8_t*)ps_malloc(len);
  if (!buf) { f.close(); return false; }
  f.read(buf, len);
  f.close();

  // 解析 WAV：找 fmt 与 data
  if (len < 44 || memcmp(buf, "RIFF", 4) != 0) { free(buf); return false; }
  uint32_t sr = 16000, ba = 0, dataOff = 0, dataLen = 0;
  uint32_t p = 12;
  while (p + 8 <= len) {
    uint32_t csz = le32(buf + p + 4);
    if (memcmp(buf + p, "fmt ", 4) == 0) {
      sr = le32(buf + p + 12);
      ba = le16(buf + p + 20);
    } else if (memcmp(buf + p, "data", 4) == 0) {
      dataOff = p + 8; dataLen = csz;
      if (dataOff + dataLen > len) dataLen = len - dataOff;
      break;
    }
    p += 8 + csz + (csz & 1);
  }
  if (!dataOff || !ba) { free(buf); return false; }

  // 解码
  uint32_t blocks = dataLen / ba;
  uint32_t samples = blocks * ((ba - 4) * 2 + 1);
  int16_t* pcm = (int16_t*)ps_malloc(samples * 2);
  if (!pcm) { free(buf); return false; }
  uint32_t si = 0;
  for (uint32_t b = 0; b < blocks; b++) {
    const uint8_t* blk = buf + dataOff + b * ba;
    int pred = (int16_t)le16(blk);
    int idx  = blk[2];
    pcm[si++] = (int16_t)pred;
    for (uint32_t i = 4; i < ba; i++) {
      uint8_t byte = blk[i];
      pcm[si++] = (int16_t)adpcmNibble(byte & 0x0F, pred, idx);
      pcm[si++] = (int16_t)adpcmNibble(byte >> 4,   pred, idx);
    }
  }
  free(buf);

  M5.Speaker.setVolume(g_voiceVolume);
  M5.Speaker.playRaw(pcm, si, sr, false);
  uint32_t t0 = millis();
  while (M5.Speaker.isPlaying() && millis() - t0 < 8000) delay(5);
  delay(30);                                   // 片段间小停顿
  free(pcm);
  return true;
}

// ============================================================
//  数字 -> 片段序列
// ============================================================
static void numZh(std::vector<String>& v, int n) {
  if (n < 0) { v.push_back("minus"); n = -n; }
  if (n == 0) { v.push_back("n0"); return; }
  if (n >= 100) { v.push_back("n1"); v.push_back("n100"); n -= 100; }
  int tens = n / 10, ones = n % 10;
  if (tens == 1) v.push_back("n10");
  else if (tens > 1) { v.push_back(String("n") + tens); v.push_back("n10"); }
  if (ones > 0) v.push_back(String("n") + ones);
}

static const char* EN_TEEN[10] = {"n10","n11","n12","n13","n14","n15","n16","n17","n18","n19"};
static const char* EN_TENS[10] = {"", "", "n20","n30","n40","n50","n60","n70","n80","n90"};
static void numEn(std::vector<String>& v, int n) {
  if (n < 0) { v.push_back("minus"); n = -n; }
  if (n < 10) { v.push_back(String("n") + n); return; }
  if (n < 20) { v.push_back(EN_TEEN[n - 10]); return; }
  if (n >= 100) { v.push_back("n1"); v.push_back("n100"); n -= 100; }
  v.push_back(EN_TENS[n / 10]);
  if (n % 10) v.push_back(String("n") + (n % 10));
}

static void num(std::vector<String>& v, float f, bool oneDecimal) {
  int ip = (int)floorf(f);
  if (g_voiceLang) numEn(v, ip); else numZh(v, ip);
  if (oneDecimal) {
    int dec = (int)lroundf((f - ip) * 10.0f);
    if (dec < 0) dec = -dec;
    if (dec > 0) {
      v.push_back("point");
      if (g_voiceLang) numEn(v, dec); else numZh(v, dec);
    }
  }
}

// ============================================================
//  拼装 + 播放整段播报
// ============================================================
void voice_announce(const WeatherData& w) {
  if (!s_fsReady) { Serial.println("[voice] fs not ready"); return; }

  std::vector<String> seq;
  bool zh = (g_voiceLang == 0);

  seq.push_back("intro");
  seq.push_back("now");
  if (!isnan(w.now.temp)) {
    seq.push_back("temp");
    num(seq, w.now.temp, false);
    seq.push_back("deg");
  }
  seq.push_back(String("w") + (int)wcatFromCode(w.now.code));
  if (!isnan(w.now.feels)) {
    seq.push_back("feels");
    num(seq, w.now.feels, false);
    seq.push_back("deg");
  }
  if (w.now.humidity >= 0) {
    seq.push_back("hum");
    num(seq, (float)w.now.humidity, false);
    seq.push_back("pct");
  }
  if (!isnan(w.now.wind)) {
    seq.push_back("wind");
    num(seq, w.now.wind, true);
    seq.push_back("mps");
  }

  if (!isnan(w.uvMax)) {
    seq.push_back("uv");
    int lv = 0; float u = w.uvMax;
    if (u >= 11) lv = 4; else if (u >= 8) lv = 3; else if (u >= 6) lv = 2; else if (u >= 3) lv = 1;
    seq.push_back(String("uv") + lv);
  }

  // 带伞提醒
  bool rain = false;
  if (w.dailyCount > 0) {
    WCat c = wcatFromCode(w.daily[0].code);
    rain = (c == WC_RAIN || c == WC_SHOWER || c == WC_THUNDER || c == WC_DRIZZLE || c == WC_SLEET)
           || w.daily[0].pop >= 50;
  }
  seq.push_back(rain ? "umb1" : "umb0");

  Serial.printf("[voice] announce %s/%s, %u clips\n",
                zh ? "zh" : "en", g_voiceGender ? "male" : "female", (unsigned)seq.size());
  for (auto& c : seq) playClip(c);
}
