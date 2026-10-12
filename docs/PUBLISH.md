# 发布指南（GitHub）

## 1. 建议的仓库信息

- **仓库名**：`sticks3-weather-clock`（或 `m5stick-s3-weather`）
- **描述（About）**：
  > Desk weather clock for M5Stack StickS3 (ESP32-S3): Chinese lunar calendar, 7 weather pages, bilingual, offline voice, weather alerts, custom WiFi setup page & OTA.
- **Topics**（标签）：
  `m5stack` `sticks3` `esp32-s3` `weather` `weather-station` `arduino` `platformio` `iot` `chinese-calendar` `open-meteo` `ota` `tts`

## 2. 发布步骤

```bash
cd StickS3-Weather
git init
git add .
git commit -m "StickS3 weather clock: initial release"
git branch -M main
git remote add origin https://github.com/<你的用户名>/sticks3-weather-clock.git
git push -u origin main
```

然后建 **Release**（`v1.0.0`），把 `release/sticks3-weather-full.bin` 传上去当附件。

## 3. 别人怎么刷（写进 README 或 Release 说明）

整包镜像已含程序 + 语音素材，一条命令刷完：

```bash
esptool.py --chip esp32s3 -p COM5 write_flash 0x0 sticks3-weather-full.bin
```

> StickS3 进下载模式：长按机身右侧面靠下的电源键，直到绿灯闪烁。

## 4. 发帖文案

### M5Stack 社区 / Hackster / Reddit（英文）

> **StickS3 Weather Clock — a dorm-desk weather gadget**
>
> I built a full weather clock for the new **M5Stack StickS3 (ESP32-S3)**:
> • Chinese lunar + sexagenary (干支) clock (landscape on wake)
> • 7 weather pages: today, 5-day, UV index, daily tips, 24h trend (animated), air quality, laundry-drying index
> • Animated weather icons drawn with primitives
> • Bilingual (中文/English), offline voice announcements (普通话/English, male/female)
> • Weather alerts, wake-up alarm, power saving
> • Custom phone setup page (AP + web UI) and WiFi OTA
>
> Weather data from Open-Meteo (free, no key). PlatformIO project, MIT licensed.
> Feedback welcome!

### B 站 / 知乎 / 小红书（中文）

> **我用 M5Stack StickS3 做了个「宿舍天气机」**
>
> 给住校学生用的桌面天气预报机：
> • 横屏时钟：时间 + 农历 + 干支（丙午马年 戊戌月 丁巳日…）
> • 7 个天气页：今日 / 未来5天 / 防晒 / 生活提示 / 24小时趋势 / 空气质量 / 洗晒指数
> • 天气图标带动画，数字全部半角排版
> • 中英双语、离线语音播报（普通话/English，男声/女声）
> • 天气预警、起床闹钟、省电息屏
> • 手机配网网页 + WiFi 空中升级（OTA）
>
> 数据用 Open-Meteo（免费免注册），代码开源（MIT），PlatformIO 一键编译。

## 5. 发布前检查清单

- [ ] `README.md`（英文）与 `README.zh-CN.md`（中文）互相有链接
- [ ] `docs/images/preview.jpg` 显示正常
- [ ] `LICENSE` 里的版权署名改一下（现在写的是占位）
- [ ] 删掉个人信息：`ota.ps1` 里的 MAC 地址 `7c-e8-b1-e5-49-cc` 换成占位或改成自动探测
- [ ] `data/audio/` 已包含（否则别人克隆后没有语音）
- [ ] 仓库配好 topics
