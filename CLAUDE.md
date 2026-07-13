# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

# palmora 固件设计文档 (CPlus 1.1 版)

## 1. 目标与范围

在 M5StickC Plus 1.1 上做一个摇卦占卜小玩具：开机 → 按键起卦 → 摇晃生爻(6次) → 主卦呈现 → 卦辞 → 长按重启循环。

不做联网、不做存储持久化、不做多语言——单机、单次会话、纯本地计算。

**命名注**：仓库目录名叫 `palmora`；早期文档里出现过"问卜"、界面设计草稿里出现过"掌卦"，都不再是当前产品名。当前 BOOT 界面实际显示的四字大标题是 **"先占 / 一卦"**（[include/BootGlyphs.h](include/BootGlyphs.h) 的 `BOOT_GLYPH_XIANZAN` + `BOOT_GLYPH_YIGUA`，由 [src/main.cpp:110-111](src/main.cpp#L110-L111) 渲染）。

---

## 2. 硬件与平台

以下规格已核对官方文档（https://docs.m5stack.com/zh_CN/core/m5stickc_plus）：

| 项目 | 参数 |
|---|---|
| SoC | ESP32-PICO-D4，双核 240MHz |
| Flash | 4MB |
| SRAM | 520KB |
| LCD | 1.14 寸，135×240，驱动芯片 ST7789v2 |
| MEMS/IMU | MPU6886（六轴：3轴加速计+3轴陀螺仪），I2C 接口 |
| PMU（电源管理） | AXP192 |
| RTC | BM8563 |
| 麦克风 | SPM1423 |
| 蜂鸣器 | 板载无源蜂鸣器 |
| 电池 | 120mAh @ 3.7V |
| 按键 | 自定义按键 x2（Button A / Button B）+ 电源/复位键（由 AXP192 管理） |

### 管脚映射（官方文档「管脚映射」章节，权威来源）

| 功能 | 引脚 |
|---|---|
| 红色 LED | G10 |
| 红外发射管 IR | G9 |
| 按键 Button A（正面大按钮） | **G37** |
| 按键 Button B（Side 按钮） | **G39** |
| 无源蜂鸣器 | G2 |
| TFT_MOSI / TFT_CLK / TFT_DC / TFT_RST / TFT_CS | G15 / G13 / G23 / G18 / G5 |
| 麦克风 CLK / DATA | G0 / G34 |
| **MPU6886 SCL / SDA**（六轴 IMU，I2C） | **G22 / G21** |
| AXP192（电源管理，同一 I2C 总线） | SCL=G22 / SDA=G21 |
| 外接引脚（HY2.0-4P Grove 口） | GND / 5V / G32 / G33 |

> 注：G36 与 G25 共用同一端口，若使用 G36 需将 G25 设为浮空输入（官方文档 setup() 示例：`pinMode(36, INPUT); gpio_pulldown_dis(GPIO_NUM_25); gpio_pullup_dis(GPIO_NUM_25);`），本项目未使用这两个引脚，仅记录以防未来扩展冲突。

### 按键映射（业务定义）
- **大按钮（正面, Button A）**：短按 = 确认/下一步；长按（≥800ms）= 重启回到开始界面。
- **Side 按钮（Button B）**：短按 = 直接开始下一卦（细节见第 7 节状态机）。
- 电源/复位键由 AXP192 管理（非 GPIO 直连）：开机为短按≥2秒，关机为长按≥6秒，均为系统固件默认行为，不做业务绑定。
- 代码里统一走 `M5.BtnA` / `M5.BtnB` 抽象，不直接读 GPIO——上面表格里的引脚号仅供背景参考，某些批次板子 A/B 引脚会互换，以库抽象为准。

### 开发平台与库
- 官方支持 UiFlow1 / UiFlow2 / **Arduino IDE** / ESP-IDF / **PlatformIO**（本项目采用后两者组合：PlatformIO + Arduino 框架）。
- 官方 Arduino 驱动库：`M5StickC-Plus`（GitHub: `m5stack/M5StickC-Plus`），封装屏幕（基于 ST7789v2 驱动 + M5GFX/TFT_eSPI 兼容接口）、按键（`M5.BtnA` / `M5.BtnB`）、IMU（`M5.Imu`，内部对接 MPU6886 I2C 驱动）、电源管理（`M5.Axp`）。
- 官方文档给出的可用上传/监视波特率：1200~115200, 250K, 500K, 750K, 1500K（当前 `platformio.ini` 采用 `upload_speed = 1500000`，在支持范围内）。

### 已核对的官方库 API（来自 `m5stack/M5StickC-Plus` 仓库 `examples/Basics/` 与 `src/utility/`）

**按键（`src/utility/Button.h`，`Button` 类，实例为 `M5.BtnA` / `M5.BtnB`）**：
```cpp
uint8_t isPressed();           // 当前是否按下
uint8_t isReleased();          // 当前是否松开
uint8_t wasPressed();          // 本次 update() 周期内是否发生了"按下"边沿
uint8_t wasReleased();         // 本次 update() 周期内是否发生了"松开"边沿（官方短按判定用这个）
uint8_t pressedFor(uint32_t ms);     // 当前处于按下状态且已持续 ≥ ms
uint8_t releasedFor(uint32_t ms);    // 当前处于松开状态且已持续 ≥ ms
uint8_t wasReleasefor(uint32_t ms);  // 松开边沿触发，且松开前按住时长 ≥ ms（官方长按判定用这个，注意方法名大小写 "Releasefor"）
```
每帧循环开头必须调用一次 `M5.update()` 刷新按键状态，再读取上述方法。

**IMU（`M5.Imu`，对接 MPU6886，`examples/Basics/MPU6886/MPU6886.ino` 验证）**：
```cpp
M5.Imu.Init();                                   // setup() 中，M5.begin() 之后调用
M5.Imu.getAccelData(&accX, &accY, &accZ);        // 单位：g（示例代码里 *1000 换算成 mg 显示，佐证原始单位是 g）
M5.Imu.getGyroData(&gyroX, &gyroY, &gyroZ);       // 单位：度/秒
M5.Imu.getTempData(&temp);                        // 芯片温度，摄氏度
```

**最小骨架**（引用头文件与初始化顺序，来自官方示例）：
```cpp
#include <M5StickCPlus.h>

void setup() {
    M5.begin();
    M5.Imu.Init();
    M5.Lcd.setRotation(3);
}

void loop() {
    M5.update();
    if (M5.BtnA.wasReleased()) { /* 短按 */ }
    if (M5.BtnA.wasReleasefor(800)) { /* 长按 */ }
}
```

> 上述 API 已用官方仓库真实源码核对，[ShakeDetector.h](include/ShakeDetector.h) 中 `M5.Imu.getAccelData()` 调用方式与此一致；[第 7 节](#7-ui-状态机)按键判定逻辑（原设计中的 `pressedFor`）已按此处核对结果修正为 `wasReleasefor` / `wasReleased`。

---

## 3. 中文字体方案（已验证可行）

**问题**：Adafruit GFX 内置字体只有 ASCII，卦名、卦辞全部是汉字，必须能显示任意汉字（卦辞用字集 > 300 字，无法用小字库穷举）。

**方案：U8g2 完整中文字体（wqy 文泉驿点阵）+ 离线渲染 XBM 位图**

- 正文/卦辞用 `u8g2_font_wqy12_t_gb2312`（12px 点阵，GB2312 全字库覆盖常用汉字 6763 个），中号标题用 `u8g2_font_wqy16_t_gb2312`（16px）。
- `lib_deps` 引入 `olikraus/U8g2_for_Adafruit_GFX`。
- **大字号标题字（40px 及以上）走离线渲染 XBM 位图**，不走 U8g2——U8g2 wqy 系列最大到 16px，48px 大字用字体渲染会糊。BOOT 界面的 "先占/一卦" 四字（80×40，[include/BootGlyphs.h](include/BootGlyphs.h)）与备用的 "掌/卦/问/卜" 四字（48×48，[include/TitleGlyphs.h](include/TitleGlyphs.h)）都是离线用系统字体（华文中宋 / 微软雅黑粗体）渲染成 1bpp XBM 位图后固化进 PROGMEM，用 `M5.Lcd.drawXBitmap()` 绘制。XBM（而不是 `drawBitmap(...,bgcolor)`）的好处：只画前景像素，背景天然透明，不会因为背景色和屏幕底色对不齐产生色块。

**关键兼容性问题与解决方案（已用真实编译验证，见 [UiFont.h](include/UiFont.h)）**：

M5StickCPlus 的 `M5.Lcd` 实际类型是 `M5Display : public TFT_eSPI : public Print`，**并不继承 `Adafruit_GFX`**，而 `U8g2_for_Adafruit_GFX::begin()` 要求传入 `Adafruit_GFX&`。这与本节原先"M5.Lcd 兼容 Adafruit GFX 接口"的假设不符，是实现阶段发现的真实类型不兼容问题。

解决方式：`U8g2_for_Adafruit_GFX` 库内部实际只调用了 `Adafruit_GFX` 的 `drawFastHLine()` / `drawFastVLine()` 两个虚函数（已读取 `U8g2_for_Adafruit_GFX.cpp` 源码确认），因此写了一个轻量适配器 `M5LcdGfxAdapter : public Adafruit_GFX`，把这两个方法（以及 `drawPixel()`，`Adafruit_GFX` 的唯一纯虚函数）转发给 `M5.Lcd` 的同名方法，绕开继承链不兼容问题。`UiFont::begin()` 内部用这个适配器实例调用 `_u8g2.begin(_adapter)`。

- Flash 占用（真实编译数据）：整个固件（含 M5StickCPlus + Adafruit GFX Library + U8g2_for_Adafruit_GFX + 双字号中文字库）**Flash 占用 872053 / 1310720 字节，66.5%**；**RAM 占用 22672 / 327680 字节，6.9%**。均有余量。
- **卦辞自动换行**：较长卦辞（如"井""讼"卦，20+ 字）在 12px 字体、119px 可用宽度下单行放不下，`UiFont::drawTextWrapped()` 按 UTF-8 字符边界（不会切碎一个汉字的 3 字节编码）自动换行，JUDGMENT 界面的分割线/引申语位置根据实际换行行数动态计算 y 坐标。

---

## 4. 卦象数据结构

沿用已完成草稿 [Hexagrams.h](include/Hexagrams.h)，需在此基础上补充 `sequence`（六十四卦序号）与 `insight`（引申语）两个字段：

```cpp
struct Hexagram {
    uint8_t lines;         // 6 位二进制：bit0=初爻(最下) ... bit5=上爻(最上)，1=阳 0=阴
    uint8_t sequence;      // 通行本六十四卦序号 1~64，如"随"卦为 17（参考图 PRIMARY 界面左上角数字）
    const char *name;      // 完整卦名，如 "泽雷随"
    const char *symbol;    // 简称，如 "随"
    const char *judgment;  // 卦辞原文，如 "元亨利贞，无咎。"
    const char *insight;   // 白话引申语，如 "顺势而行，不失本心。"（原创，非经文，64卦各写一条）
};
```

- 64 条记录按 `lines` 值（0~63）直接做数组下标，`findHexagram(lines)` O(1) 查表。
- 数据以 `PROGMEM` 存放于 Flash，不占用运行时 RAM（ESP32 上 PROGMEM 语义上是空操作但保留写法习惯，实际由链接器决定段位置，一般常量数组会自动放入 `.rodata` / flash）。
- 卦辞文本已核对为通行本《周易》经文，作为固件内置常量表，无外部依赖。
- `insight` 引申语为本项目原创补充内容（通行本经文无对应白话文本），需在实现阶段为 64 卦各撰写一条简短（约 8~12 字）的白话提示语，语气与参考图中"顺势而行，不失本心"一致——贴合卦辞主旨，避免直译。

---

## 5. 摇卦引擎（爻值生成）

沿用草稿 [DivinationEngine.h](include/DivinationEngine.h)，采用传统"三枚铜钱法"概率模型：

| 爻值 | 名称 | 概率 | 阴阳 | 是否变爻 |
|---|---|---|---|---|
| 6 | 老阴 | 1/8 | 阴 | 是（变阳） |
| 7 | 少阳 | 3/8 | 阳 | 否 |
| 8 | 少阴 | 3/8 | 阴 | 否 |
| 9 | 老阳 | 1/8 | 阳 | 是（变阴） |

- 每次有效摇晃触发 `addShake()`，用 `random(0,8)` 映射到上述四值分布，累计 6 次爻值。
- `primaryLines()`：阳(7,9)→1，阴(6,8)→0，得到主卦二进制。
- `changedLines()`：老阴/老阳翻转，少阴/少阳不变，得到变卦二进制。
- `hasMovingLines()`：判断是否存在变爻。**注**：变卦推演 UI 已从产品需求中去掉（见第 7 节），这几个方法当前不被 `main.cpp` 调用，但保留在类里作为完整的数据模型，未来若要恢复变卦展示可直接复用。
- 随机种子：`setup()` 阶段用 `randomSeed(esp_random())` 或读取一次浮空 ADC/加速度噪声做种，保证每次开机序列不同。

---

## 6. 摇晃检测（MPU6886）

沿用草稿 [ShakeDetector.h](include/ShakeDetector.h)，思路：

1. 每帧读取加速度三轴，计算合加速度模长 `mag = sqrt(ax²+ay²+az²)`（单位 g，静止时应 ≈1.0）。
2. 计算相邻帧模长差 `delta = |mag - lastMag|`，超过阈值（草稿中 `1.2g`）判定为一次"晃动尖峰"。
3. 冷却期（草稿中 `500ms`）内的后续尖峰不重复计数，防止一次甩动触发多爻。
4. **待实现阶段用真机验证调参**：阈值和冷却时间需要结合实际摇晃动作手感反复试（比如摇一次骰子筒的力度 vs 轻抖手腕），设计阶段先定初始值，标记为"需现场调优"的参数。
5. **铜钱动画（简化版）**：SHAKING 界面画三枚小圆圈代表铜钱，检测到一次有效摇晃时触发一帧简单变化（如圆圈临时替换为一条竖线模拟"侧面"，或短暂改变描边颜色/亮度模拟翻转高光），持续 100~200ms 后恢复，不做真实 3D 翻转效果，纯做"正在摇"的视觉反馈。

---

## 7. UI 状态机

> 本节界面布局参考了原版实物拍摄图（5 张截图覆盖 BOOT / SHAKING(1/6) / SHAKING(6/6) / JUDGMENT / PRIMARY 五个界面），实拍图第三方来源、不入仓库，仅在本地 `refs/` 作对照使用。

> **变更**：已去掉变卦推演（CHANGING 状态）与"变爻"相关的 UI 展示。`DivinationEngine` 仍保留 `changedLines()`/`hasMovingLines()`/`isMoving()` 等方法（数据模型完整，未删除），但 `main.cpp` 不再调用它们驱动界面跳转。Side 按钮改为"直接开始下一卦"的快捷键。

```
[BOOT]  开机显示"问卜"位图标题 + "默念一事，按下成卦" + "↓ 开始"
   │  大按钮短按
   ▼
[SHAKING]  摇卦界面：左上角 "N/6"、卦象爻位图（已生成的爻直接画出，未生成留空）、
           三枚铜钱图标（简化翻转动画）、底部"正在成爻"
   │  （每次有效摇晃 → 生成一爻，N++，界面刷新；大按钮短按也可直接生成一爻，与摇晃等效）
   │  N 达到 6 时，播完最后一次铜钱闪烁动画后自动跳转（无需按键）
   ▼
[PRIMARY]  主卦界面：左上角卦序号（如 17）、右上角迷你卦象图标、居中 24px 大字卦名简称（如"随"）、
           下方全名（如"泽雷随"）、底部"↓ 卦辞"
   │  大按钮短按 → 下一步(JUDGMENT)
   │  Side 按钮短按 → 直接开始下一卦（跳过卦辞，回到 SHAKING）
   │  长按大按钮 → 回到 BOOT（清空状态）
   ▼
[JUDGMENT]  卦辞界面：卦名简称+"卦" 标题、卦辞原文、白话引申语（insight 字段）、底部"[Side] 下一卦"
   │  Side 按钮短按 → 直接开始下一卦（回到 SHAKING）
   │  长按大按钮 → 回到 BOOT
   │  大按钮短按 → 无操作
```

### 状态与按键行为对照表

| 状态 | 大按钮短按 | 大按钮长按 | Side 短按 |
|---|---|---|---|
| BOOT | 进入 SHAKING（reset 引擎） | — | — |
| SHAKING（未满 6 爻） | 直接生成一爻（与摇晃等效） | 回到 BOOT | — |
| SHAKING（已满 6 爻，等待自动跳转） | 无操作（避免误触，等动画播完自动进 PRIMARY） | 回到 BOOT | — |
| PRIMARY | 进入 JUDGMENT | 回到 BOOT | 直接开下一卦（回到 SHAKING） |
| JUDGMENT | 无操作 | 回到 BOOT | 直接开下一卦（回到 SHAKING） |

> 长按阈值：800ms（用 `M5.BtnA.wasReleasefor(800)` 判定——按键库为 `Button` 类，`wasReleasefor(ms)` 在按键释放且按住时长 ≥ ms 时触发一次，是官方示例采用的长按检测写法，比 `pressedFor(ms)` 更适合"松开后触发一次动作"的场景，避免长按期间重复触发）。短按统一用 `wasReleased()` 判定（对照官方 `Button.ino` 示例）。

---

## 8. 界面布局（135×240，竖屏）

配色沿用参考图实拍效果：深蓝/藏青背景 + 浅青/白色描边文字与线条（近似 `TFT_NAVY` 背景 + `TFT_CYAN`/`TFT_WHITE` 前景），边框为细描边矩形。

### 8.1 BOOT（对照 refs 截图 2）
```
┌───────────────┐
│               │
│   先  占       │  ← 大字，"先占 / 一卦" 分两行居中偏上
│   一  卦       │     （80x40 XBM 位图，BOOT_GLYPH_XIANZAN / _YIGUA）
│               │
│  默念一事      │  ← 中等字号，居中
│  按下成卦      │
│               │
│    ↓ 开始      │  ← 底部提示，↓ 箭头 + 文字
└───────────────┘
```

### 8.2 SHAKING（对照 refs 截图 1 / 3）
```
┌───────────────┐
│ 1/6           │  ← 左上角计数，随生成递增
│               │
│  ▬▬▬▬▬▬▬▬     │  ← 已生成的爻，自上而下按生成顺序追加
│  ▬▬▬ ▬▬▬      │     （实拍图是从上往下依次画出新爻，
│  ░░░░░░░░     │      而非按初爻在下的最终顺序摆放；
│  ░░░░░░░░     │      生成过程中先按摇卦顺序追加显示，
│  ░░░░░░░░     │      到 6/6 完成后再按初爻在下重排为最终卦象）
│  ░░░░░░░░     │
│               │
│  (·)(·)(·)    │  ← 三枚铜钱图标，横排，摇晃时触发翻转/闪烁动画
│   正在成爻     │  ← 底部提示文字
└───────────────┘
```
> 注：实拍图 1/6 状态下爻位图为空（全部虚线占位），6/6 状态下六爻已从上到下画满实线/断线。铜钱图标在 6/6 截图中最后一枚呈现模糊/虚化，即"正在翻转"的动画帧。

### 8.3 PRIMARY（对照 refs 截图 5，"随"卦示例）
```
┌───────────────┐
│ 17        ▤   │  ← 左上：卦序号(1~64)；右上：迷你六爻图标
│               │
│               │
│     随         │  ← 卦名简称，最大字号，居中
│               │
│   泽雷随       │  ← 卦全名，中等字号，居中
│               │
│    ↓ 卦辞      │  ← 底部提示
└───────────────┘
```

### 8.4 JUDGMENT（对照 refs 截图 4，"随"卦示例）
```
┌───────────────┐
│ 随卦      ▤   │  ← 左上角简称+"卦"字，右上角淡化的六爻水印图标
│               │
│ 元亨利贞      │  ← 卦辞原文，wqy12
│ 无咎          │
│ ────────      │  ← 分隔线
│ 顺势而行      │  ← insight 白话引申语，字号/颜色可与卦辞区分（参考图为绿色系）
│ 不失本心      │
│               │
│    ↓ 之卦      │  ← 底部提示；若 !hasMovingLines() 则改为 "本卦无变"，
│               │     大按钮短按停留在本界面
└───────────────┘
```
> 背景水印：实拍图卦辞文字后方隐约可见半透明的爻位符号，实现时可选：(a) 先以低亮度画一次爻位图再叠加文字，(b) 若渲染开销/可读性问题较大，可作为低优先级视觉细节推迟实现，不影响核心功能。

### 8.5 CHANGING（变卦推演，暂无实拍参考，沿用原设计）
```
┌───────────────┐
│   之卦：XX     │
│               │
│  主卦    变卦   │  ← 左右两组爻位图对照
│  ▬▬▬ →  ▬ ▬   │     变化的爻高亮/加箭头标记
│  ▬ ▬ →  ▬▬▬   │
│  ...          │
│               │
│ [Side]返回     │
└───────────────┘
```

爻位图绘制：每爻用一个横向色块表示——阳爻画一条通长横线，阴爻画两条中间断开的短横线。**PRIMARY/JUDGMENT 的迷你图标**自上而下按"上爻→初爻"顺序紧凑排列（对应参考图右上角小图标）；**SHAKING 界面的大图**在生成过程中自上而下追加新爻，完成后统一按易学惯例重排（初爻在最下）用于后续 PRIMARY 显示。变爻可加特殊标记（如颜色变化或旁注"○"/"×"，老阳变标记、老阴变标记）。

---

## 9. 项目结构

```
palmora/
├── platformio.ini
├── CLAUDE.md
├── README.md
├── LICENSE
├── .gitignore
├── refs/                     # 原版实物参考截图（第三方来源，本地对照用，整个目录 gitignore）
├── tools/                    # 字模离线渲染脚本（render_glyphs.py + emit_glyph_arrays.py）
├── include/
│   ├── Hexagrams.h          # 64卦数据表：卦名/卦序/卦辞/引申语（已完成）
│   ├── DivinationEngine.h   # 摇卦引擎：爻值生成、主卦/变卦计算（已完成）
│   ├── ShakeDetector.h      # MPU6886摇晃检测（已完成）
│   ├── HexagramView.h       # 爻位图绘制（已完成）
│   ├── UiFont.h             # U8g2中文字体桥接封装 + M5.Lcd 适配器（已完成）
│   └── BootGlyphs.h         # BOOT 界面 80x40 XBM 位图（"先占/一卦"），src/main.cpp 引用
├── src/
│   └── main.cpp             # 状态机 + 按键处理 + 各界面渲染（已完成）
└── lib/                     # 空，第三方库走 lib_deps
```

## 编译与烧录

首次构建前：安装 PlatformIO Core（`pip install platformio`，或用 VSCode PlatformIO 扩展）。`lib_deps` 会在首次 `pio run` 时自动拉取 M5StickCPlus + U8g2_for_Adafruit_GFX。

```bash
pio run                              # 编译
pio run --target upload              # 编译并烧录（需连接设备）
pio device monitor                   # 串口监视器
```

已用 `pio run` 实测编译通过：Flash 66.5%（872053/1310720 字节），RAM 6.9%（22672/327680 字节）。本项目未做过真机烧录/上机测试（无实体设备），摇晃阈值等运行时手感相关参数仍待真机验证调优（见第 10 节）。

---

## 10. 待确认事项（当前 open）

1. **摇晃阈值参数**：`1.2g` / `500ms` 是初始猜测值，代码已按此实现（见 [ShakeDetector.h](include/ShakeDetector.h)），但需真机试摇调优，目前无法在没有实体设备的环境下验证手感。
2. **JUDGMENT 界面大键短按行为**：当前实现为"停留原地无操作"（见 [main.cpp](src/main.cpp) 的 `handleInput()`），如需改为循环跳回 BOOT 可再调整。
3. **JUDGMENT 背景水印**：实拍图中的半透明爻位图水印尚未实现，当前 JUDGMENT 界面右上角只画了一个不透明的迷你卦象图标代替，视觉还原度低于参考图但不影响功能。

### 已完结（历史记录，勿再动）

- U8g2 + M5.Lcd 兼容性——已解决，见第 3 节。
- 64 卦 insight 引申语文案——已在 [Hexagrams.h](include/Hexagrams.h) 中撰写完成。
- CHANGING 变卦推演界面——产品需求已去掉，`DivinationEngine` 仍保留数据模型 API，但 UI 状态机不再走这条分支。相关"变卦卦辞是否显示"等派生问题一并归档。
- `include/TitleGlyphs.h`（备用 "掌/卦/问/卜" 48×48 位图）——开源整理时已删除，git 历史里可翻。

---

## 11. 非目标（明确不做）

- 不做占卜历史记录存储（掉电即丢）。
- 不做多卦例/多语言/多主题皮肤。
- 不做蓝牙/WiFi 联动。
- 不做电池电量等系统状态 UI（沿用 M5StickCPlus 默认行为即可）。
