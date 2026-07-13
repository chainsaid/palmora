"""Render BOOT 页大字号中文标题为灰度 PNG，供 emit_glyph_arrays.py 转成 1bpp XBM C 数组使用。

流程：
    1. python tools/render_glyphs.py         # 生成 tools/out/*.png
    2. python tools/emit_glyph_arrays.py     # 读 tools/out/*.png，打印 C 数组

产物直接粘贴进 include/BootGlyphs.h。当前使用的字模只有 title1/title2（"先占""一卦"，40px 大字），
body 三行文字是历史草稿——线上 BOOT 界面的正文行在运行时用 U8g2 wqy12 字体渲染，不走本流程。

前置：Pillow（`pip install Pillow`），以及一份中文字体文件。默认用 Windows 自带华文中宋 STZHONGS.TTF，
在其他平台请把 FONT_PATH 改为本地路径（如 macOS 的 /System/Library/Fonts/PingFang.ttc，
Linux 的 /usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc 等）。
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

# 输出目录（相对本脚本所在的 tools/），git 忽略
OUT = Path(__file__).resolve().parent / "out"
OUT.mkdir(parents=True, exist_ok=True)

# 中文字体文件路径。修改此处以切换字体。
FONT_PATH = r"C:\Windows\Fonts\STZHONGS.TTF"

font_title = ImageFont.truetype(FONT_PATH, 38)
font_body = ImageFont.truetype(FONT_PATH, 20)


def make(text: str, name: str, font, pad_x: int = 2, pad_y: int = 2) -> None:
    dummy = Image.new("L", (1, 1), 0)
    d = ImageDraw.Draw(dummy)
    bbox = d.textbbox((0, 0), text, font=font)
    w = bbox[2] - bbox[0] + pad_x * 2
    h = bbox[3] - bbox[1] + pad_y * 2
    im = Image.new("L", (w, h), 0)
    d = ImageDraw.Draw(im)
    d.text((pad_x - bbox[0], pad_y - bbox[1]), text, font=font, fill=255)
    im.save(OUT / f"{name}.png")


if __name__ == "__main__":
    # 当前进入固件的两组大字：BOOT_GLYPH_XIANZAN / BOOT_GLYPH_YIGUA
    make("先占", "title1", font_title)
    make("一卦", "title2", font_title)
    # 以下为历史草稿正文行，当前固件未使用（运行时用 U8g2 渲染 "默念一事" 等）；保留以便重制标题变体
    make("心有一问", "body1", font_body)
    make("默祷三息", "body2", font_body)
    make("按下起卜", "body3", font_body)
    print(f"wrote PNGs to {OUT}")
