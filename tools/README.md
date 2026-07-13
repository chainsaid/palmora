# tools/

固件里 BOOT 界面的大字号标题（"先占"/"一卦" 等 40px 汉字）不走运行时字体渲染，而是**离线渲染成 1bpp 位图**、
以 XBM 位序的 C 数组常量固化进 [../include/BootGlyphs.h](../include/BootGlyphs.h)，运行时用
`M5.Lcd.drawXBitmap()` 绘制。这么做的原因见 CLAUDE.md 第 3 节。

本目录的两个脚本负责这套字模的可复现生成流程：

| 脚本 | 作用 |
|---|---|
| [render_glyphs.py](render_glyphs.py) | 用指定字体（默认华文中宋）把汉字渲染成灰度 PNG，输出到 `tools/out/` |
| [emit_glyph_arrays.py](emit_glyph_arrays.py) | 读 `tools/out/*.png`，转成 1bpp LSB-first XBM 位序，打印 C 数组 |

## 用法

```bash
pip install Pillow
python tools/render_glyphs.py         # 生成 tools/out/*.png
python tools/emit_glyph_arrays.py     # 打印 C 数组到 stdout
```

把 stdout 里 `BOOT_GLYPH_XIANZAN` / `BOOT_GLYPH_YIGUA` 两个常量粘进
[../include/BootGlyphs.h](../include/BootGlyphs.h) 即可替换现有字模。

## 常见改动场景

- **换字体**：改 `render_glyphs.py` 顶部的 `FONT_PATH`（Windows 默认华文中宋 `STZHONGS.TTF`；
  macOS 可用 `/System/Library/Fonts/PingFang.ttc`；Linux 用 Noto CJK 等）。
- **换字号**：改 `font_title` / `font_body` 的第二个参数（当前 38/20，对应 BOOT 大字 ≈40px 高）。
- **改标题文字**：改 `render_glyphs.py` 底部 `make(...)` 调用里的中文，以及 `emit_glyph_arrays.py`
  里 `emit(...)` 的常量名。改完后固件端的 `include/BootGlyphs.h` 常量与 [src/main.cpp](../src/main.cpp)
  的引用要一致。

## 关于 body 行

脚本里另外渲染了 `body1..3`（"心有一问 / 默祷三息 / 按下起卜"）并可生成 `BOOT_TEXT_LINE1..3` 常量，
但**当前固件并未链接使用**——BOOT 界面的正文行 "默念一事 / 按下成卦" 是在运行时用 U8g2 wqy12 字体渲染的。
这几行是历史草稿变体，保留在这里方便将来做多套标题的 A/B。

`tools/out/` 是可再生的中间产物，已在 `.gitignore` 中排除。
