#!/usr/bin/env python
"""读取 StickS3 串口日志（可先等待串口出现，用于抓开机日志）。
用法: python tools/serial_log.py [wait秒] [记录秒] [COM口]
  python tools/serial_log.py 45 30          # 等待串口最多45秒，出现后记录30秒
  python tools/serial_log.py 0 12 COM6      # 立即读 COM6 12 秒
"""
import sys, time
import serial
from serial.tools import list_ports


def detect():
    for p in list_ports.comports():
        hw = (p.hwid or "").upper()
        desc = (p.description or "").upper()
        if "303A" in hw or "JTAG" in desc or "USB 串行" in (p.description or ""):
            return p.device
    return None


argv = sys.argv[1:]
wait = float(argv[0]) if len(argv) > 0 else 0
dur = float(argv[1]) if len(argv) > 1 else 12
forced = argv[2] if len(argv) > 2 else None

port = forced
t0 = time.time()
if not port:
    while time.time() - t0 < max(wait, 0):
        port = detect()
        if port:
            break
        time.sleep(0.5)

if not port:
    print("[serial] 没等到串口")
    sys.exit(1)

print(f"[serial] 打开 {port}，记录 {dur:.0f} 秒 …\n")
try:
    s = serial.Serial(port, 115200, timeout=0.5)
except Exception as e:
    time.sleep(1)
    s = serial.Serial(port, 115200, timeout=0.5)

t1 = time.time()
try:
    while time.time() - t1 < dur:
        data = s.read(1024)
        if data:
            sys.stdout.write(data.decode("utf-8", "replace"))
            sys.stdout.flush()
finally:
    s.close()
print("\n[serial] 结束")
