# StickS3 Weather Clock — 发布长文 / Showcase post

> 适合发到：M5Stack Community / Hackster / Reddit (r/esp32, r/M5Stack) / 知乎 / B 站简介 / 小红书
> 用法：英文段发国际平台，中文段发国内平台，按需删减。

---

## 🇬🇧 English

### I turned the M5Stack StickS3 into a dorm-desk weather clock

My little brother lives in a school dorm. Every morning he asks the same two questions: *"do I need a jacket?"* and *"do I need an umbrella?"* So I built him a weather clock — and it turned into a full project on the **brand-new M5Stack StickS3 (ESP32-S3)**.

**What it does**

- **Landscape clock on wake** — big time, plus the Chinese lunar date and the sexagenary cycle: `丙午马年 戊戌月 丁巳日（九月初一）`.
- **7 weather pages**: Today · Next 5 days · UV index · Daily tips · 24-hour trend · Air quality · Laundry-drying index.
- **Animated weather icons** — the sun's rays rotate, rain falls, snow drifts, lightning flickers. All drawn with graphics primitives (no image assets).
- **Bilingual** 简体中文 / English for the whole UI.
- **Offline voice announcements** — it speaks today's temperature, conditions, feels-like, humidity, wind, UV level and an umbrella reminder. 普通话/English, female/male voice. Runs entirely offline.
- **Weather alerts** — a red banner on the Today page when it's about to be very hot, very cold, rainy, foggy, windy, polluted or extremely sunny.
- **Wake-up alarm**, **power saving**, and a **custom phone setup page**.

**How the tricky parts work**

- **Lunar + sexagenary calendar** — computed on-device with a 1900–2100 table; the year/month pillars use 立春 and the solar terms (verified against real Spring Festival dates).
- **Voice** — clips are pre-generated on a PC (Windows SAPI + ffmpeg → 16 kHz IMA-ADPCM), stored in the device file system, decoded and played through the ES8311 codec. Concatenative, so no cloud needed.
- **Setup page** — instead of the stock captive portal, a custom AP + web UI: it scans nearby WiFi, and lets you set WiFi, city, language, voice and alerts. Open `192.168.4.1` from your phone.
- **OTA** — firmware updates over WiFi.

**Data**: weather and air quality from **Open-Meteo** (free, no API key).

**Stack**: Arduino / PlatformIO, M5Unified, ~2.6 MB firmware on the 8 MB StickS3. MIT licensed.

It's my first "real" hardware+software project — happy to hear feedback, and PRs are welcome. 👇 screenshots in the repo.

---

## 🇨🇳 中文

### 我把 M5Stack StickS3 做成了宿舍桌面天气机

我弟在寄宿学校，每天早上就问两件事："要不要加外套？""要不要带伞？" 于是我给他做了个天气机 —— 结果做成了 **全新 M5Stack StickS3 (ESP32-S3)** 上的一个完整项目。

**它能做什么**

- **唤醒时钟（横屏）**：大号时间 + 农历 + 干支（`丙午马年 戊戌月 丁巳日（九月初一）`）
- **7 个天气页**：今日 · 未来 5 天 · 防晒 · 生活提示 · 24 小时趋势 · 空气质量 · 洗晒指数
- **动态天气图标**：太阳光线旋转、雨点下落、雪花飘、闪电闪烁 —— 全部用图元绘制，不用图片素材
- **中英双语**：整个界面可切中文 / English
- **离线语音播报**：念出气温、天气、体感、湿度、风速、紫外线等级和带伞提醒；普通话/English、男声/女声；**完全离线**
- **天气预警**：高温/严寒/降雨/雷电/大雾/大风/重污染/极强紫外线时，今日页弹红色预警条
- **起床闹钟**、**省电息屏**、**自定义手机配网页**

**几个难点怎么解决的**

- **农历 + 干支**：用 1900–2100 的农历表在设备端算；年柱以立春为界、月柱以节气为界（拿真实春节日期校验过）
- **语音**：电脑端预生成片段（Windows 语音 + ffmpeg → 16kHz IMA-ADPCM）存进设备文件系统，解码后用 ES8311 播放 —— 拼接式，**不需要联网**
- **配网页**：没用默认门户，自己写了 AP + 网页：扫描附近 WiFi，可设置 WiFi / 城市 / 语言 / 语音 / 闹钟 / 预警；手机打开 `192.168.4.1` 即可
- **OTA**：WiFi 空中升级

**数据**：天气与空气质量来自 **Open-Meteo**（免费、免注册）。

**技术**：Arduino / PlatformIO + M5Unified，8MB StickS3 上约 2.6MB 固件，MIT 开源。

第一次做"软硬结合"的完整项目，欢迎提意见，也欢迎 PR 🙌
