// 先占一卦 - M5StickC Plus 1.1 摇卦占卜固件
// 状态机：BOOT -> SHAKING -> PRIMARY -> JUDGMENT -> (Side键) 直接开下一卦回到 SHAKING；长按任意界面回到 BOOT
#include <M5StickCPlus.h>

#include "DivinationEngine.h"
#include "BootGlyphs.h"
#include "HexagramView.h"
#include "Hexagrams.h"
#include "ShakeDetector.h"
#include "UiFont.h"

enum class AppState { BOOT, SHAKING, PRIMARY, JUDGMENT };

static AppState state = AppState::BOOT;
static DivinationEngine engine;
static ShakeDetector shakeDetector;
static UiFont ui;

static const int16_t SCREEN_W = 135;
static const uint16_t COLOR_BG = TFT_NAVY;
static const uint16_t COLOR_FG = TFT_CYAN;
static const uint16_t COLOR_INSIGHT = TFT_GREEN;
static const uint16_t COLOR_DIM = TFT_DARKGREY;

static const uint32_t LONG_PRESS_MS = 800;
static const uint32_t SHAKE_FLASH_MS = 220;  // 摇晃触发后铜钱动效的持续时间

static uint32_t lastShakeFlashAt = 0;
static bool needsRedraw = true;

static void goToState(AppState next) {
    state = next;
    needsRedraw = true;
}

static void resetAndStartShaking() {
    engine.reset();
    lastShakeFlashAt = 0;  // 清掉上一局残留的时间戳，避免新一局刚进入 SHAKING 时被误判为"仍在闪烁窗口内"
    goToState(AppState::SHAKING);
}

// 开始钱币动效；动效结束后再真正生成一爻。
static void startCoinMotion() {
    if (state != AppState::SHAKING || engine.isComplete()) return;

    lastShakeFlashAt = millis();
    needsRedraw = true;
}

// 钱币动效结束后，真正生成一爻。
static void finishCoinMotionAndCommit() {
    if (state != AppState::SHAKING || engine.isComplete() || lastShakeFlashAt == 0) return;

    lastShakeFlashAt = 0;
    engine.addShake();
    needsRedraw = true;
}

// ---------- 绘制辅助 ----------

static void drawCoin(int16_t cx, int16_t cy, int16_t r, bool bright) {
    uint16_t c = bright ? TFT_YELLOW : COLOR_FG;
    M5.Lcd.drawCircle(cx, cy, r + 1, c);
    M5.Lcd.drawCircle(cx, cy, r, c);
    M5.Lcd.drawCircle(cx, cy, 2, c);  // 铜钱中心方孔的简化示意
}

static int16_t coinBounce(uint16_t elapsed, uint16_t duration, uint8_t phaseOffset) {
    if (elapsed <= phaseOffset) return 0;
    uint16_t t = elapsed - phaseOffset;
    if (t >= duration) return 0;

    // 先上弹再回落，峰值偏前，让动作更干脆
    if (t < duration / 2) {
        return (int16_t)((t * 8) / duration);
    }
    return (int16_t)(((duration - t) * 8) / duration);
}

static void drawCoins(int16_t centerX, int16_t y, uint16_t elapsed) {
    static const int16_t baseSpacing = 20;
    static const int16_t baseRadius = 6;
    static const uint8_t coinPhase[3] = {0, 28, 56};

    for (uint8_t i = 0; i < 3; i++) {
        int16_t cx = centerX + (i - 1) * baseSpacing;
        int16_t dy = coinBounce(elapsed, SHAKE_FLASH_MS, coinPhase[i]);
        bool bright = elapsed < SHAKE_FLASH_MS;

        // 只有垂直弹起，不改形，不加竖线，保持钱币本体清晰
        drawCoin(cx, y - dy, baseRadius, bright);

        // 中间时刻补一个外圈光晕，增强存在感，但不破坏钱币轮廓
        if (elapsed > coinPhase[i] && elapsed < coinPhase[i] + (SHAKE_FLASH_MS / 2)) {
            M5.Lcd.drawCircle(cx, y - dy, baseRadius + 3, bright ? TFT_YELLOW : COLOR_DIM);
        }
    }
}

// ---------- 各界面绘制 ----------

static void drawBoot() {
    M5.Lcd.fillScreen(COLOR_BG);
    constexpr int16_t TITLE_W = 80;
    constexpr int16_t TITLE_H = 40;
    constexpr int16_t TITLE_TOP = 24;
    constexpr int16_t BODY_TOP = 138;

    int16_t titleX = (SCREEN_W - TITLE_W) / 2;
    M5.Lcd.drawXBitmap(titleX, TITLE_TOP, BOOT_GLYPH_XIANZAN, TITLE_W, TITLE_H, COLOR_FG);
    M5.Lcd.drawXBitmap(titleX, TITLE_TOP + TITLE_H + 4, BOOT_GLYPH_YIGUA, TITLE_W, TITLE_H, COLOR_FG);

    ui.setSmall();
    ui.drawTextCentered(SCREEN_W / 2, BODY_TOP, "心有一问");
    ui.drawTextCentered(SCREEN_W / 2, BODY_TOP + 22, "默祷三息");
    ui.drawTextCentered(SCREEN_W / 2, BODY_TOP + 44, "按下起卜");
}

static const int16_t SHAKING_COINS_Y = 188;

// 铜钱动效窗口内的重绘只需要刷新铜钱这一小块区域，不要整屏 fillScreen 重画——
// 之前每次闪烁刷新（220ms 窗口内每 20ms 一次）都整屏清空重画，密集的全屏重绘本身
// 在视觉上就是"频闪"，摇晃触发时因动作本身有持续感掩盖了这一点，按键是瞬时动作更容易看出来。
static void drawShakingCoinsOnly() {
    int16_t clearW = 78;
    int16_t clearH = 28;
    M5.Lcd.fillRect(SCREEN_W / 2 - clearW / 2, SHAKING_COINS_Y - clearH / 2, clearW, clearH, COLOR_BG);
    uint16_t elapsed = (millis() - lastShakeFlashAt);
    if (elapsed > SHAKE_FLASH_MS) elapsed = SHAKE_FLASH_MS;
    drawCoins(SCREEN_W / 2, SHAKING_COINS_Y, elapsed);
}

static void drawSettledCoins() {
    drawCoins(SCREEN_W / 2, SHAKING_COINS_Y, SHAKE_FLASH_MS);
}

static void drawShaking() {
    M5.Lcd.fillScreen(COLOR_BG);
    ui.setSmall();
    ui.setColor(COLOR_FG);

    char countText[8];
    snprintf(countText, sizeof(countText), "%d/6", engine.count());
    ui.drawText(8, 8, countText);

    // 生成过程中按"从上到下追加"顺序展示已生成的爻，未生成的爻画虚化占位
    HexagramView::draw(20, 36, 95, 20, engine.primaryLines(), engine.count(),
                        COLOR_FG, COLOR_DIM, 0, TFT_YELLOW, /*topDownOrder=*/true);

    if (lastShakeFlashAt != 0) {
        drawShakingCoinsOnly();
    } else if (engine.count() > 0) {
        drawSettledCoins();
    }

    if (engine.isComplete()) {
        ui.drawTextCentered(SCREEN_W / 2, 206, "六爻已成");
    } else {
        ui.drawTextCentered(SCREEN_W / 2, 206, "正在成爻");
    }
}

static void drawPrimary() {
    const Hexagram *hex = findHexagram(engine.primaryLines());

    M5.Lcd.fillScreen(COLOR_BG);
    ui.setSmall();
    ui.setColor(COLOR_FG);

    char seqText[8];
    snprintf(seqText, sizeof(seqText), "%d", hex->sequence);
    ui.drawText(8, 8, seqText);

    // 右上角迷你卦象图标
    HexagramView::draw(SCREEN_W - 8 - 22, 8, 22, 5, hex->lines, 6, COLOR_FG, COLOR_DIM);

    // 卦名简称是这个界面的视觉重心，用 2 倍缩放把 24px 字库放大到约 48px 效果
    ui.drawHugeScaledCentered(SCREEN_W / 2, 60, hex->symbol, 2);

    ui.setSmall();
    ui.drawTextCentered(SCREEN_W / 2, 140, hex->name);

    ui.drawTextCentered(SCREEN_W / 2, 208, "↓ 卦辞");
}

static void drawJudgment() {
    const Hexagram *hex = findHexagram(engine.primaryLines());

    M5.Lcd.fillScreen(COLOR_BG);
    ui.setSmall();
    ui.setColor(COLOR_FG);

    char titleText[16];
    snprintf(titleText, sizeof(titleText), "%s卦", hex->symbol);
    ui.drawText(8, 8, titleText);
    HexagramView::draw(SCREEN_W - 8 - 22, 8, 22, 5, hex->lines, 6, COLOR_DIM, COLOR_DIM);

    // 卦辞页内容整体下移，给标题和正文之间留出更松的呼吸感
    int16_t bodyStartY = 64;
    int16_t textAreaWidth = SCREEN_W - 16;
    ui.setColor(COLOR_FG);
    uint8_t judgmentLines = ui.drawTextWrapped(8, bodyStartY, textAreaWidth, hex->judgment);

    int16_t dividerY = bodyStartY + judgmentLines * ui.lineHeight() + 4;
    M5.Lcd.drawFastHLine(8, dividerY, textAreaWidth, COLOR_DIM);

    ui.setColor(COLOR_INSIGHT);
    ui.drawTextWrapped(8, dividerY + 8, textAreaWidth, hex->insight);

}

static void render() {
    switch (state) {
        case AppState::BOOT: drawBoot(); break;
        case AppState::SHAKING: drawShaking(); break;
        case AppState::PRIMARY: drawPrimary(); break;
        case AppState::JUDGMENT: drawJudgment(); break;
    }
    needsRedraw = false;
}

// ---------- 状态转移 ----------

static void handleInput() {
    M5.update();

    bool longPress = M5.BtnA.wasReleasefor(LONG_PRESS_MS);
    bool shortPressA = !longPress && M5.BtnA.wasReleased();
    bool shortPressB = M5.BtnB.wasReleased();

    if (longPress && state != AppState::BOOT) {
        goToState(AppState::BOOT);
        return;
    }

    switch (state) {
        case AppState::BOOT:
            if (shortPressA) resetAndStartShaking();
            break;

        case AppState::SHAKING:
            if (shortPressA) {
                if (engine.isComplete()) {
                    // 六爻已集齐（停在 6/6 界面等待确认）：按大按钮才进入主卦界面
                    goToState(AppState::PRIMARY);
                } else {
                    // 大按钮短按先触发钱币动效，动效结束后再生成一爻
                    startCoinMotion();
                }
            }
            break;

        case AppState::PRIMARY:
            if (shortPressA) goToState(AppState::JUDGMENT);
            else if (shortPressB) resetAndStartShaking();  // Side 键：跳过卦辞，直接开下一卦
            break;

        case AppState::JUDGMENT:
            if (shortPressA) goToState(AppState::PRIMARY);  // 主按钮：返回主卦页
            else if (shortPressB) resetAndStartShaking();    // Side 键：直接开下一卦
            break;
    }
}

static void updateShaking() {
    if (state != AppState::SHAKING) return;

    if (lastShakeFlashAt != 0) {
        uint32_t elapsed = millis() - lastShakeFlashAt;
        if (elapsed < SHAKE_FLASH_MS) {
            // 钱币先动，先只刷钱币区域，不让爻位提前变化。
            drawShakingCoinsOnly();
            return;
        }

        // 动效结束后，真正生成一爻，再由 render() 重画整页。
        finishCoinMotionAndCommit();
        return;
    }

    // 六爻集齐后不自动跳转，停在 6/6 界面等玩家主动按大按钮确认进入主卦界面
    // （由 handleInput 的 SHAKING case 中 "engine.isComplete()" 分支处理）。
    if (engine.isComplete()) {
        return;
    }

    if (shakeDetector.update()) {
        startCoinMotion();
    }
}

// ---------- Arduino 入口 ----------

void setup() {
    M5.begin();
    M5.Lcd.setRotation(0);  // 竖屏，135(宽) x 240(高)
    shakeDetector.begin();
    ui.begin();
    randomSeed(esp_random());

    render();
}

void loop() {
    handleInput();
    updateShaking();

    if (needsRedraw) {
        render();
    }

    delay(20);
}
