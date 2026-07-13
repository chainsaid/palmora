// U8g2 中文字体桥接：在 M5.Lcd 上绘制任意 GB2312 汉字
//
// 注意：M5StickCPlus 的 M5.Lcd 类型是 M5Display : public TFT_eSPI : public Print，
// 并不继承 Adafruit_GFX，而 U8g2_for_Adafruit_GFX::begin() 要求传入 Adafruit_GFX&。
// U8g2_for_Adafruit_GFX 内部实际只调用了 drawFastHLine()/drawFastVLine() 两个方法
// （见 U8g2_for_Adafruit_GFX.cpp 中 u8g2->gfx->drawFastHLine/drawFastVLine），
// 因此这里用一个继承自 Adafruit_GFX 的适配器类，把这两个方法转发给 M5.Lcd 的同名方法，
// 绕开"M5.Lcd 不是 Adafruit_GFX"的类型不兼容问题。
#pragma once

#include <Adafruit_GFX.h>
#include <M5StickCPlus.h>
#include <U8g2_for_Adafruit_GFX.h>

class M5LcdGfxAdapter : public Adafruit_GFX {
public:
    M5LcdGfxAdapter() : Adafruit_GFX(135, 240) {}

    void drawPixel(int16_t x, int16_t y, uint16_t color) override {
        M5.Lcd.drawPixel(x, y, color);
    }
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override {
        M5.Lcd.drawFastHLine(x, y, w, color);
    }
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override {
        M5.Lcd.drawFastVLine(x, y, h, color);
    }
};

// 字号放大适配器：U8g2 以为自己在 1:1 尺寸正常画字，实际每条线段/像素按整数倍率放大后
// 再落到屏幕——用于在没有更大号字库的情况下，复用现成的 24px 字形数据画出更大的字。
// 代价是放大后边缘呈阶梯状（马赛克感），倍率越大越明显，但对卦名这种大标题场景可接受。
class ScalingGfxAdapter : public Adafruit_GFX {
public:
    ScalingGfxAdapter() : Adafruit_GFX(135, 240) {}

    void setScale(uint8_t scale) { _scale = scale; }

    void drawPixel(int16_t x, int16_t y, uint16_t color) override {
        M5.Lcd.fillRect(x * _scale, y * _scale, _scale, _scale, color);
    }
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override {
        M5.Lcd.fillRect(x * _scale, y * _scale, w * _scale, _scale, color);
    }
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override {
        M5.Lcd.fillRect(x * _scale, y * _scale, _scale, h * _scale, color);
    }

private:
    uint8_t _scale = 1;
};

class UiFont {
public:
    void begin() {
        _u8g2.begin(_adapter);
        _u8g2.setFontMode(1);             // 透明背景，文字不覆盖已绘制的背景色/水印
        _u8g2.setForegroundColor(TFT_CYAN);
        setSmall();
    }

    // 注意：u8g2_SetFont() 内部每次切换字体都会把 is_transparent 强制重置为 0（不透明），
    // 哪怕切换到的是同一款字体也一样（见 U8g2_for_Adafruit_GFX.cpp 的 u8g2_SetFont 实现）。
    // setFontKeepTransparent() 统一在换字体后重新应用 setFontMode(1)，否则文字背后会画出实心背景色块。
    // 原 12px 太小难以辨认，正文/提示文字统一升级为 16px（原 setLarge 字号）
    void setSmall() { setFontKeepTransparent(u8g2_font_wqy16_t_gb2312); _lineHeight = 18; }
    void setLarge() { setFontKeepTransparent(u8g2_font_wqy16_t_gb2312); _lineHeight = 18; }

    // 24px 中文字库（u8g2 最大号 GB 字体），拆成 3 卷（gb24st_t_1/2/3），
    // 单个字符可能只落在其中一卷里，drawTextHuge() 会对每个字符自动探测所属卷号后再绘制
    void setHuge() { _lineHeight = 26; }

    void setColor(uint16_t color) { _u8g2.setForegroundColor(color); }

    // U8g2 的 (x,y) 是文字基线(baseline)左下角，不是左上角，这里用 ascent 换算成"左上角"坐标传入更符合直觉
    void drawText(int16_t x, int16_t y, const char *utf8Text) {
        _u8g2.setCursor(x, y + _u8g2.getFontAscent());
        _u8g2.print(utf8Text);
    }

    // 居中绘制单行文字，centerX 为屏幕/容器的水平中点
    void drawTextCentered(int16_t centerX, int16_t y, const char *utf8Text) {
        int16_t w = _u8g2.getUTF8Width(utf8Text);
        drawText(centerX - w / 2, y, utf8Text);
    }

    // 按最大宽度自动换行绘制多行文字（逐 UTF-8 字符前进，避免把一个汉字的 3 字节从中间截断）
    // 返回实际绘制的行数，便于调用方据此计算后续布局的起始 y
    uint8_t drawTextWrapped(int16_t x, int16_t y, int16_t maxWidth, const char *utf8Text) {
        char lineBuf[64];
        uint8_t lineLen = 0;
        uint8_t lineCount = 0;
        int16_t curY = y;

        const uint8_t *p = (const uint8_t *)utf8Text;
        while (*p) {
            uint8_t charBytes = 1;
            if ((*p & 0x80) == 0x00) charBytes = 1;
            else if ((*p & 0xE0) == 0xC0) charBytes = 2;
            else if ((*p & 0xF0) == 0xE0) charBytes = 3;
            else if ((*p & 0xF8) == 0xF0) charBytes = 4;

            if (lineLen + charBytes >= sizeof(lineBuf)) break;  // 缓冲区保护

            char tentative[64];
            memcpy(tentative, lineBuf, lineLen);
            memcpy(tentative + lineLen, p, charBytes);
            tentative[lineLen + charBytes] = '\0';

            if (lineLen > 0 && _u8g2.getUTF8Width(tentative) > maxWidth) {
                lineBuf[lineLen] = '\0';
                drawText(x, curY, lineBuf);
                curY += _lineHeight;
                lineCount++;
                lineLen = 0;
                continue;  // 不推进 p，重新把当前字符放入新行
            }

            memcpy(lineBuf + lineLen, p, charBytes);
            lineLen += charBytes;
            p += charBytes;
        }
        if (lineLen > 0) {
            lineBuf[lineLen] = '\0';
            drawText(x, curY, lineBuf);
            lineCount++;
        }
        return lineCount;
    }

    // 用 24px 字库绘制单行文字（居中），逐字符在 gb24st_t_1/2/3 三卷中探测所属卷号。
    // 若某个字符三卷都没有（生僻字），退化为用 setLarge() 的 16px 字库画该字符，保证不丢字。
    void drawHugeCentered(int16_t centerX, int16_t y, const char *utf8Text) {
        int16_t totalW = hugeTextWidth(utf8Text);
        int16_t x = centerX - totalW / 2;

        const uint8_t *p = (const uint8_t *)utf8Text;
        while (*p) {
            uint32_t codepoint;
            uint8_t charBytes = decodeUtf8(p, codepoint);
            char glyphBuf[5];
            memcpy(glyphBuf, p, charBytes);
            glyphBuf[charBytes] = '\0';

            const uint8_t *vol = findHugeVolume(codepoint);
            setFontKeepTransparent(vol ? vol : u8g2_font_wqy16_t_gb2312);
            _u8g2.setCursor(x, y + _u8g2.getFontAscent());
            _u8g2.print(glyphBuf);
            x += _u8g2.getUTF8Width(glyphBuf);
            p += charBytes;
        }
    }

    // 在 drawHugeCentered 的基础上按整数倍率放大绘制（见 ScalingGfxAdapter 说明）。
    // centerX/y 是目标屏幕上的真实坐标；内部换算成"虚拟 1x 坐标"喂给 U8g2，
    // 再由 ScalingGfxAdapter 把每条画线指令按 scale 倍放大后落到真实像素。
    void drawHugeScaledCentered(int16_t centerX, int16_t y, const char *utf8Text, uint8_t scale) {
        if (scale <= 1) { drawHugeCentered(centerX, y, utf8Text); return; }

        int16_t virtualTotalW = hugeTextWidth(utf8Text);
        int16_t virtualX = centerX / scale - virtualTotalW / 2;
        int16_t virtualY = y / scale;

        _scalingAdapter.setScale(scale);
        _u8g2.begin(_scalingAdapter);  // 临时把绘制目标切到缩放适配器

        const uint8_t *p = (const uint8_t *)utf8Text;
        while (*p) {
            uint32_t codepoint;
            uint8_t charBytes = decodeUtf8(p, codepoint);
            char glyphBuf[5];
            memcpy(glyphBuf, p, charBytes);
            glyphBuf[charBytes] = '\0';

            const uint8_t *vol = findHugeVolume(codepoint);
            setFontKeepTransparent(vol ? vol : u8g2_font_wqy16_t_gb2312);
            _u8g2.setCursor(virtualX, virtualY + _u8g2.getFontAscent());
            _u8g2.print(glyphBuf);
            virtualX += _u8g2.getUTF8Width(glyphBuf);
            p += charBytes;
        }

        _u8g2.begin(_adapter);  // 换回 1:1 正常绘制，避免影响后续其它文字的渲染
    }

    int16_t hugeTextWidth(const char *utf8Text) {
        int16_t w = 0;
        const uint8_t *p = (const uint8_t *)utf8Text;
        while (*p) {
            uint32_t codepoint;
            uint8_t charBytes = decodeUtf8(p, codepoint);
            char glyphBuf[5];
            memcpy(glyphBuf, p, charBytes);
            glyphBuf[charBytes] = '\0';

            const uint8_t *vol = findHugeVolume(codepoint);
            setFontKeepTransparent(vol ? vol : u8g2_font_wqy16_t_gb2312);
            w += _u8g2.getUTF8Width(glyphBuf);
            p += charBytes;
        }
        return w;
    }

    int16_t textWidth(const char *utf8Text) { return _u8g2.getUTF8Width(utf8Text); }
    int16_t lineHeight() const { return _lineHeight; }

    U8G2_FOR_ADAFRUIT_GFX &raw() { return _u8g2; }

private:
    // 解析 utf8Text 起始处一个 UTF-8 字符，返回其字节数并把 Unicode 码点写入 codepoint
    static uint8_t decodeUtf8(const uint8_t *p, uint32_t &codepoint) {
        if ((*p & 0x80) == 0x00) { codepoint = p[0]; return 1; }
        if ((*p & 0xE0) == 0xC0) { codepoint = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F); return 2; }
        if ((*p & 0xF0) == 0xE0) { codepoint = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); return 3; }
        codepoint = ((p[0] & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
        return 4;
    }

    // 依次探测 gb24st_t_1/2/3 三卷，返回第一个包含该码点的字库指针；三卷都没有则返回 nullptr
    const uint8_t *findHugeVolume(uint32_t codepoint) {
        static const uint8_t *volumes[3] = {u8g2_font_gb24st_t_1, u8g2_font_gb24st_t_2, u8g2_font_gb24st_t_3};
        for (uint8_t i = 0; i < 3; i++) {
            u8g2_SetFont(&_u8g2.u8g2, volumes[i]);
            if (u8g2_IsGlyph(&_u8g2.u8g2, (uint16_t)codepoint)) {
                return volumes[i];
            }
        }
        return nullptr;
    }

    // setFont() 会把 is_transparent 重置为 0，这里统一收口，换字体后立即恢复透明模式
    void setFontKeepTransparent(const uint8_t *font) {
        _u8g2.setFont(font);
        _u8g2.setFontMode(1);
    }

    M5LcdGfxAdapter _adapter;
    ScalingGfxAdapter _scalingAdapter;
    U8G2_FOR_ADAFRUIT_GFX _u8g2;
    int16_t _lineHeight = 14;
};
