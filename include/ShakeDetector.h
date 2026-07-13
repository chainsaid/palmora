// 基于 MPU6886 加速度计的摇晃检测，带去抖/冷却
#pragma once

#include <M5StickCPlus.h>

class ShakeDetector {
public:
    void begin() {
        M5.Imu.Init();
        _lastTrigger = 0;
    }

    // 每帧调用一次，返回是否触发了一次"摇晃"
    bool update() {
        float ax, ay, az;
        M5.Imu.getAccelData(&ax, &ay, &az);

        float mag = sqrtf(ax * ax + ay * ay + az * az); // 单位: g
        float delta = fabsf(mag - _lastMag);
        _lastMag = mag;

        uint32_t now = millis();
        if (delta > SHAKE_THRESHOLD_G && (now - _lastTrigger) > COOLDOWN_MS) {
            _lastTrigger = now;
            return true;
        }
        return false;
    }

private:
    static constexpr float SHAKE_THRESHOLD_G = 1.2f; // 加速度模长突变阈值 (g)
    static constexpr uint32_t COOLDOWN_MS = 500;      // 两次生爻之间的最短间隔，防止一次摇晃触发多爻

    float _lastMag = 1.0f;
    uint32_t _lastTrigger = 0;
};
