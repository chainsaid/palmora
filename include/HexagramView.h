// 爻位图绘制：阳爻画通长横线，阴爻画中间断开的双线
#pragma once

#include <M5StickCPlus.h>

namespace HexagramView {

// 在 (x, topY) 起绘制六爻，逐行向下排列
// lines: 6 位二进制卦象，bit0=初爻...bit5=上爻
// visibleCount: 只绘制前 N 爻，传 6 表示全部绘制
// topDownOrder:
//   false（默认，用于 PRIMARY/JUDGMENT/CHANGING）：按易学惯例，第一行=上爻(bit5)...最后一行=初爻(bit0)
//   true（用于 SHAKING 生成动画）：按摇卦时间顺序，第一行=最先生成的初爻(bit0)...最后一行=最后生成的上爻(bit5)，
//                                 与 visibleCount 配合可实现"从上往下追加新爻"的动画效果
// movingMask: 若非 0，对应 bit 为 1 的爻会用 highlightColor 高亮（用于标注变爻）
inline void draw(int16_t x, int16_t topY, int16_t width, int16_t rowHeight,
                  uint8_t lines, uint8_t visibleCount = 6,
                  uint16_t color = TFT_CYAN, uint16_t placeholderColor = TFT_DARKGREY,
                  uint8_t movingMask = 0, uint16_t highlightColor = TFT_YELLOW,
                  bool topDownOrder = false) {
    int16_t barH = 3;
    int16_t gap = width / 6; // 阴爻中间断开的间隙宽度

    for (uint8_t row = 0; row < 6; row++) {
        uint8_t bitIndex = topDownOrder ? row : (5 - row);
        int16_t y = topY + row * rowHeight;

        if (row >= visibleCount) {
            // 尚未生成的爻：画一条虚化占位横线
            for (int16_t px = x; px < x + width; px += 6) {
                M5.Lcd.drawFastHLine(px, y, 3, placeholderColor);
            }
            continue;
        }

        bool isYang = (lines >> bitIndex) & 0x1;
        bool moving = (movingMask >> bitIndex) & 0x1;
        uint16_t drawColor = moving ? highlightColor : color;

        if (isYang) {
            M5.Lcd.fillRect(x, y, width, barH, drawColor);
        } else {
            int16_t halfW = (width - gap) / 2;
            M5.Lcd.fillRect(x, y, halfW, barH, drawColor);
            M5.Lcd.fillRect(x + width - halfW, y, halfW, barH, drawColor);
        }
    }
}

// 计算六爻图占用的总高度，便于上层布局
inline int16_t totalHeight(int16_t rowHeight) { return rowHeight * 6; }

}  // namespace HexagramView
