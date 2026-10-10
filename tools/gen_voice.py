#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
生成 StickS3 天气播报语音素材。
用 Windows 中文语音(Microsoft Huihui / Zira)合成 -> ffmpeg 去静音+重采样。
用法:
  python tools/gen_voice.py            # 生成全部（zh/en × female/male）
  python tools/gen_voice.py zh female  # 只生成一种
"""
import os, sys, subprocess, tempfile, wave, shutil

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT  = os.path.join(ROOT, "data", "audio")
TMP  = tempfile.gettempdir()
FFMPEG = shutil.which("ffmpeg")

# 语音名（本机已安装）
VOICE = {
    ("zh", "female"): "Microsoft Huihui Desktop",
    ("en", "female"): "Microsoft Zira Desktop",
    # 男声：用女声降调合成（本机无男声语音）
    ("zh", "male"):   "Microsoft Huihui Desktop",
    ("en", "male"):   "Microsoft Zira Desktop",
}

# 中文片段
ZH = {
    "intro": "天气播报",
    "now":   "当前",
    "temp":  "气温",
    "feels": "体感",
    "hum":   "湿度",
    "wind":  "风速",
    "uv":    "紫外线",
    "deg":   "度",
    "pct":   "百分之",
    "mps":   "米每秒",
    "minus": "零下",
    "point": "点",
    "n0": "零", "n1": "一", "n2": "二", "n3": "三", "n4": "四",
    "n5": "五", "n6": "六", "n7": "七", "n8": "八", "n9": "九",
    "n10": "十", "n100": "百",
    "w0": "晴", "w1": "晴间多云", "w2": "多云", "w3": "阴", "w4": "有雾",
    "w5": "毛毛雨", "w6": "雨", "w7": "雨夹雪", "w8": "雪", "w9": "阵雨", "w10": "雷阵雨",
    "uv0": "弱", "uv1": "中等", "uv2": "强", "uv3": "很强", "uv4": "极强",
    "umb1": "记得带伞", "umb0": "不用带伞",
}

# 英文片段
EN = {
    "intro": "Weather report",
    "now":   "currently",
    "temp":  "temperature",
    "feels": "feels like",
    "hum":   "humidity",
    "wind":  "wind",
    "uv":    "U V index",
    "deg":   "degrees",
    "pct":   "percent",
    "mps":   "meters per second",
    "minus": "minus",
    "point": "point",
    "n0": "zero", "n1": "one", "n2": "two", "n3": "three", "n4": "four",
    "n5": "five", "n6": "six", "n7": "seven", "n8": "eight", "n9": "nine",
    "n10": "ten", "n11": "eleven", "n12": "twelve", "n13": "thirteen", "n14": "fourteen",
    "n15": "fifteen", "n16": "sixteen", "n17": "seventeen", "n18": "eighteen", "n19": "nineteen",
    "n20": "twenty", "n30": "thirty", "n40": "forty", "n50": "fifty",
    "n60": "sixty", "n70": "seventy", "n80": "eighty", "n90": "ninety", "n100": "hundred",
    "w0": "clear", "w1": "partly cloudy", "w2": "cloudy", "w3": "overcast", "w4": "foggy",
    "w5": "drizzle", "w6": "rain", "w7": "sleet", "w8": "snow", "w9": "showers", "w10": "thunderstorm",
    "uv0": "low", "uv1": "moderate", "uv2": "high", "uv3": "very high", "uv4": "extreme",
    "umb1": "remember your umbrella", "umb0": "no umbrella needed",
}


def synth(text, voice, out_wav):
    """用 SAPI 合成到临时 wav"""
    txt = os.path.join(TMP, "tts_in.txt")
    with open(txt, "w", encoding="utf-8") as f:
        f.write(text)
    ps = (
        "Add-Type -AssemblyName System.Speech;"
        f"$s=New-Object System.Speech.Synthesis.SpeechSynthesizer;"
        f"$s.SelectVoice('{voice}');"
        f"$s.SetOutputToWaveFile('{out_wav}');"
        f"$t=Get-Content -Raw -Encoding UTF8 '{txt}';"
        "$s.Speak($t);$s.Dispose()"
    )
    subprocess.run(["powershell", "-NoProfile", "-Command", ps],
                   check=True, capture_output=True)


def convert(src, dst, male):
    """去静音 + 重采样到 16k 单声道 + ADPCM 压缩；male=降调"""
    af = ("silenceremove=start_periods=1:start_threshold=-45dB:start_silence=0.02,"
          "areverse,silenceremove=start_periods=1:start_threshold=-45dB:start_silence=0.02,areverse")
    if male:
        af += ",asetrate=22050*0.84,aresample=16000,atempo=1.19"
    cmd = [FFMPEG, "-y", "-i", src, "-af", af, "-ar", "16000", "-ac", "1",
           "-c:a", "adpcm_ima_wav", dst]
    subprocess.run(cmd, check=True, capture_output=True)


def main():
    which = sys.argv[1:] or None
    targets = []
    for lang in ("zh", "en"):
        for gender in ("female", "male"):
            if not which or (lang in which and gender in which):
                targets.append((lang, gender))
    os.makedirs(OUT, exist_ok=True)

    for lang, gender in targets:
        clips = ZH if lang == "zh" else EN
        voice = VOICE[(lang, gender)]
        sub = os.path.join(OUT, f"{lang}_{gender}")
        os.makedirs(sub, exist_ok=True)
        total = 0
        for cid, text in clips.items():
            tmp_wav = os.path.join(TMP, "tts_raw.wav")
            out_wav = os.path.join(sub, cid + ".wav")
            synth(text, voice, tmp_wav)
            convert(tmp_wav, out_wav, male=(gender == "male"))
            total += os.path.getsize(out_wav)
        print(f"{lang}/{gender}: {len(clips)} 个片段, {total/1024:.0f} KB -> {sub}")


if __name__ == "__main__":
    main()
