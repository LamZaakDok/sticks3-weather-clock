# StickS3 校园天气机

基于 **M5Stack StickS3 (ESP32-S3)** 的桌面天气预报机，为住校学生设计。
带农历干支时钟、7 个天气页面、中英双语、离线语音播报、天气预警、起床闹钟、自定义配网网页和 WiFi OTA。

[English](README.md) · [简体中文]

![demo](docs/images/demo.gif)

8 个界面：

![预览](docs/images/preview.jpg)

---

## 功能

**时钟页**（横屏，唤醒时首先显示）
- 大号 `HH:MM:SS` + 公历日期、星期
- 干支纪年/月/日 + 农历，例：`丙午马年 戊戌月 丁巳日（九月初一）`

**7 个天气页**（竖屏）

| # | 页面 | 内容 |
|---|---|---|
| ① | 今日 | 城市 / 电量 / 时间、**动态天气图标**、大号温度、体感·湿度·风速、穿衣 & 带伞标签 |
| ② | 未来 5 天 | 星期 + 图标 + 高/低温 |
| ③ | 防晒指数 | 今日最高 UV、等级、强度色条、SPF 建议 |
| ④ | 生活提示 | 穿衣 & 带伞卡片，底部滚动 紫外线 / 日出 / 日落 |
| ⑤ | 24 小时趋势 | 折线绘制动画 + 降水概率柱 |
| ⑥ | 空气质量 | 美国 AQI + 等级 + PM2.5 / PM10 + 建议 |
| ⑦ | 洗晒指数 | 晾晒评分 + 依据 + 底部滚动 湿度/风速/降水概率 |

**更多**
- ⚠️ **天气预警**：高温 / 严寒 / 降雨 / 雷电 / 大雾 / 大风 / 重污染 / 极强紫外线 → 今日页红色预警条
- 🎙 **离线语音播报**：气温、天气、体感、湿度、风速、紫外线、带伞提醒；普通话 / English，女声 / 男声
- ⏰ **起床闹钟**：到点响铃 + 播报天气
- 🌏 **中英双语**：设备界面 + 配网页
- 🔌 **数据源可选**：默认 Open-Meteo，或自定义（兼容 Open-Meteo 的地址）
- 🔋 **省电**：自动息屏（白天 30 秒 / 夜间 8 秒）、主频 80MHz
- 📶 **自定义配网页**：AP 热点 + 干净的中文网页，设置 WiFi / 城市 / 语言 / 语音 / 闹钟 / 预警
- ⬆️ **WiFi OTA**：不用插线就能升级
- 天气图标全部用图形图元绘制（不用图片素材）

## 硬件

- **M5Stack StickS3 (K150)** — ESP32-S3，8MB Flash，8MB PSRAM
- 1.14" ST7789P3 彩屏（135×240），ES8311 + 喇叭 + MEMS 麦克风，BMI270 六轴，250mAh 电池

## 快速开始

### 1. 首次配网

1. 首次开机设备会开热点 **`StickS3-Weather`**（无密码）。
2. 手机连接它，打开 **`http://192.168.4.1`**。
3. 选 WiFi、输密码、填城市（中文或英文）→ **保存并连接**。
4. 设备联网后即显示天气。

> 以后改设置：**双击 KEY2** 打开设置热点（不影响已连 WiFi）。
> 彻底重新配网：**长按 KEY2 三秒**。

> ⚠️ 需要网页登录（校园网认证）的 WiFi 连不上 —— 用普通 WPA2 密码 WiFi 或手机热点。

### 2. 按键

| 操作 | 功能 |
|---|---|
| KEY1 单击 | 时钟 → 天气页 1…7 → 时钟（循环） |
| KEY1 长按 | 播报天气（触发方式选「长按 KEY1」时） |
| KEY2 单击 | 刷新天气 |
| KEY2 双击 | 打开设置热点 |
| KEY2 长按 3 秒 | 清除 WiFi 重新配网 |
| 息屏后按任意键 | 先亮屏显示时钟 |

### 3. 编译 / 烧录

```bash
cd StickS3-Weather

# 编译
pio run

# USB 烧录程序（先长按侧边电源键进下载模式，绿灯闪烁）
pio run -t upload --upload-port COM5

# 写入语音素材（文件系统，只需一次）
pio run -t uploadfs --upload-port COM5
```

OTA 空中升级（设备与电脑同一局域网）：

```powershell
powershell -ExecutionPolicy Bypass -File ota.ps1        # 按 MAC 自动找设备
```

## 语音素材

语音在电脑端生成（Windows 语音 + ffmpeg → 16kHz IMA-ADPCM），存到设备文件系统。

```bash
python tools/gen_voice.py            # 重新生成全部（中/英 × 女声/男声）到 data/audio/
pio run -t uploadfs                  # 写入设备
```

## 目录结构

```
StickS3-Weather/
├─ platformio.ini        # ESP32-S3 / 8MB / PSRAM
├─ ota.ps1 / flash.ps1   # 一键 OTA / USB 烧录
├─ src/
│  ├─ config.h           # 常量
│  ├─ main.cpp           # 主流程：配网 / 时钟 / 按键 / 省电 / OTA
│  ├─ weather.h/.cpp     # 定位 + 天气 + 空气质量（默认/自定义接口）
│  ├─ lunar.h/.cpp       # 农历 + 干支
│  ├─ i18n.h/.cpp        # 中/英双语
│  ├─ portal.h/.cpp      # 自定义配网页
│  ├─ voice.h/.cpp       # 离线语音（ADPCM 解码 + 喇叭）
│  ├─ icons.h/.cpp       # 动态天气图标
│  └─ ui.h/.cpp          # 时钟页 + 7 页界面
├─ data/audio/           # 语音素材
├─ tools/                # gen_voice.py, serial_log.py
└─ docs/                 # 预览图 + 源文件
```

## 数据来源

天气与空气质量来自 **[Open-Meteo](https://open-meteo.com)**（免费、免注册）。农历/干支由设备本地计算。

## 许可

[MIT](LICENSE)
