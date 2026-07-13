"""读 tools/out/*.png（由 render_glyphs.py 生成），转成 1bpp LSB-first XBM 位序的 C 数组，
配合 M5.Lcd.drawXBitmap() 使用。产物直接粘进 include/BootGlyphs.h。

用法：python tools/emit_glyph_arrays.py > /tmp/glyphs.h
"""
from pathlib import Path
from PIL import Image

BASE = Path(__file__).resolve().parent / "out"


def emit(name: str, label: str) -> None:
    im = Image.open(BASE / f"{name}.png").convert("1")
    w, h = im.size
    pixels = im.load()
    data = []
    for y in range(h):
        for x0 in range(0, w, 8):
            byte = 0
            for bit in range(8):
                x = x0 + bit
                if x < w and pixels[x, y] != 0:
                    byte |= 1 << bit
            data.append(byte)
    print(f"// {label}: {w}x{h}px, 1bpp LSB-first (XBM 位序，配合 drawXBitmap 使用), 华文中宋渲染")
    print(f"static const uint8_t {label}[] PROGMEM = {{")
    for i in range(0, len(data), 12):
        chunk = ", ".join(f"0x{b:02X}" for b in data[i : i + 12])
        print(f"    {chunk},")
    print("};\n")


if __name__ == "__main__":
    # 与固件当前使用一致的两组常量
    emit("title1", "BOOT_GLYPH_XIANZAN")
    emit("title2", "BOOT_GLYPH_YIGUA")
    # 历史正文行（当前固件未链接使用）
    emit("body1", "BOOT_TEXT_LINE1")
    emit("body2", "BOOT_TEXT_LINE2")
    emit("body3", "BOOT_TEXT_LINE3")
