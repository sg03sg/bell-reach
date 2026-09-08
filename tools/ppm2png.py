#!/usr/bin/env python3
"""게임이 저장한 PPM 캡처를 PNG로 바꾼다.

    python3 tools/ppm2png.py build/shot.ppm build/shot.png

표준 라이브러리만 쓴다. 외부 의존성을 늘리지 않기 위한 선택이다.
"""
import struct
import sys
import zlib


def read_ppm(path):
    data = open(path, "rb").read()
    if data[:2] != b"P6":
        raise SystemExit(f"{path}: P6 형식이 아닙니다")

    fields, i = [], 2
    while len(fields) < 3:
        while data[i:i + 1].isspace():
            i += 1
        if data[i:i + 1] == b"#":                 # 주석 줄 건너뛰기
            while data[i:i + 1] != b"\n":
                i += 1
            continue
        start = i
        while not data[i:i + 1].isspace():
            i += 1
        fields.append(int(data[start:i]))

    width, height, _maxval = fields
    return width, height, data[i + 1:i + 1 + width * height * 3]


def write_png(path, width, height, rgb):
    # 스캔라인마다 필터 바이트 0을 앞에 붙인다.
    scanlines = b"".join(
        b"\x00" + rgb[y * width * 3:(y + 1) * width * 3] for y in range(height)
    )

    def chunk(tag, payload):
        crc = zlib.crc32(tag + payload) & 0xFFFFFFFF
        return struct.pack(">I", len(payload)) + tag + payload + struct.pack(">I", crc)

    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", header)
           + chunk(b"IDAT", zlib.compress(scanlines, 9))
           + chunk(b"IEND", b""))
    open(path, "wb").write(png)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("사용법: ppm2png.py <입력.ppm> <출력.png>")
    w, h, pixels = read_ppm(sys.argv[1])
    write_png(sys.argv[2], w, h, pixels)
    print(f"{sys.argv[2]}  ({w}x{h})")
