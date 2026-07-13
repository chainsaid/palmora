// 摇卦核心逻辑：爻值生成、主卦/变卦计算
#pragma once

#include <Arduino.h>

// 爻值：传统蓍草/铜钱法的四种结果
// 6 = 老阴 (变, 阴->阳)  7 = 少阳 (不变, 阳)
// 8 = 少阴 (不变, 阴)    9 = 老阳 (变, 阳->阴)
enum LineValue : uint8_t {
    LINE_OLD_YIN = 6,
    LINE_YOUNG_YANG = 7,
    LINE_YOUNG_YIN = 8,
    LINE_OLD_YANG = 9,
};

class DivinationEngine {
public:
    void reset() {
        _count = 0;
        for (uint8_t i = 0; i < 6; i++) _values[i] = 0;
    }

    bool isComplete() const { return _count >= 6; }
    uint8_t count() const { return _count; }

    // 每次检测到一次有效摇晃，调用此方法生成一爻
    // 随机性来自 Arduino random()，全局种子已在 setup() 中通过 randomSeed(esp_random()) 设置
    void addShake() {
        if (isComplete()) return;
        uint8_t r = random(0, 8); // 0..7，模拟三枚铜钱之和的分布近似（简化为均匀分布防止偏置难以感知）
        LineValue v;
        // 铜钱法概率：老阳(9) 1/8，少阴(8) 3/8，少阳(7) 3/8，老阴(6) 1/8
        if (r == 0) v = LINE_OLD_YANG;
        else if (r <= 3) v = LINE_YOUNG_YIN;
        else if (r <= 6) v = LINE_YOUNG_YANG;
        else v = LINE_OLD_YIN;
        _values[_count++] = v;
    }

    // 主卦二进制：阳(7,9)=1，阴(6,8)=0
    uint8_t primaryLines() const {
        uint8_t lines = 0;
        for (uint8_t i = 0; i < 6; i++) {
            if (_values[i] == LINE_YOUNG_YANG || _values[i] == LINE_OLD_YANG) {
                lines |= (1 << i);
            }
        }
        return lines;
    }

    // 变卦二进制：老阴/老阳翻转，少阴/少阳不变
    uint8_t changedLines() const {
        uint8_t lines = 0;
        for (uint8_t i = 0; i < 6; i++) {
            bool isYang = (_values[i] == LINE_YOUNG_YANG || _values[i] == LINE_OLD_YANG);
            bool moving = (_values[i] == LINE_OLD_YIN || _values[i] == LINE_OLD_YANG);
            if (moving) isYang = !isYang;
            if (isYang) lines |= (1 << i);
        }
        return lines;
    }

    bool hasMovingLines() const {
        for (uint8_t i = 0; i < 6; i++) {
            if (_values[i] == LINE_OLD_YIN || _values[i] == LINE_OLD_YANG) return true;
        }
        return false;
    }

    bool isMoving(uint8_t index) const {
        return _values[index] == LINE_OLD_YIN || _values[index] == LINE_OLD_YANG;
    }

    LineValue valueAt(uint8_t index) const { return (LineValue)_values[index]; }

private:
    uint8_t _values[6] = {0};
    uint8_t _count = 0;
};
