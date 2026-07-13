# palmora

M5StickC Plus 1.1 上的一款掌上摇卦占卜小玩具。开机 → 按键起卦 → 摇一摇生 6 爻 → 主卦呈现 → 卦辞与白话引申。**单机、离线、无联网、掉电即忘。**

BOOT 界面显示 "先占 / 一卦" 大字标题。整机固件（含全部驱动 + GB2312 中文字库 + 64 卦数据）Flash 占用 66.5%（872KB / 1.25MB），RAM 6.9%。

> 状态：功能代码已完成并通过 `pio run` 编译，但**尚未在真机上烧录验证**（摇晃阈值 `1.2g` / 冷却 `500ms` 等手感相关参数仍待现场调优）。欢迎有实机的朋友试烧并回报体感。

---

## 硬件要求

- **M5StickC Plus 1.1**（ESP32-PICO-D4，135×240 ST7789v2 屏，MPU6886 六轴 IMU，AXP192 电源管理）
- USB-C 数据线（烧录用）

其他 M5StickC / M5StickC Plus 早期批次未测试；MPU6886 驱动直接依赖 `M5.Imu`，理论上凡是官方 `m5stack/M5StickC-Plus` 库支持的板子都应兼容，但屏幕分辨率不同的话界面坐标要重排。

## 快速上手

前置：安装 [PlatformIO Core](https://platformio.org/install)（`pip install platformio`，或用 VSCode 的 PlatformIO 扩展）。

```bash
git clone https://github.com/chainsaid/palmora.git
cd palmora
pio run                        # 首次会自动拉取 M5StickCPlus + U8g2_for_Adafruit_GFX
pio run --target upload        # 用 USB 连接 M5StickC Plus 后烧录
pio device monitor             # 可选：查看串口日志
```

## 使用方法

| 界面 | 大按钮短按 | 大按钮长按 (≥800ms) | Side 按钮短按 |
|---|---|---|---|
| **BOOT** 开机页 | 开始摇卦 | — | — |
| **SHAKING** 摇卦中（未满 6 爻） | 立即生成一爻（等效摇一次） | 回到 BOOT | — |
| **SHAKING** 摇卦完成（等自动跳转） | 无操作 | 回到 BOOT | — |
| **PRIMARY** 主卦页 | 查看卦辞 | 回到 BOOT | 直接下一卦 |
| **JUDGMENT** 卦辞页 | 无操作 | 回到 BOOT | 直接下一卦 |

- 摇卦引擎采用传统"三枚铜钱法"概率模型：老阴/少阳/少阴/老阳 = 1/8, 3/8, 3/8, 1/8。
- 六十四卦全套卦辞用通行本《周易》经文，`insight` 引申语（约 8~12 字白话提示）为项目原创。
- 长按 800ms 全局可回到开机界面重开一卦。

## 项目结构

```
palmora/
├── platformio.ini            PlatformIO 配置（板型 m5stick-c、Arduino 框架、lib_deps）
├── CLAUDE.md                 完整设计文档（硬件规格、按键映射、字体方案、UI 状态机、界面布局）
├── LICENSE                   MIT
├── src/
│   └── main.cpp              状态机 + 按键处理 + 各界面渲染
├── include/
│   ├── Hexagrams.h           64 卦数据表（卦名/卦序/卦辞/引申）
│   ├── DivinationEngine.h    摇卦引擎：爻值生成、主卦/变卦计算
│   ├── ShakeDetector.h       MPU6886 摇晃检测
│   ├── HexagramView.h        爻位图绘制
│   ├── UiFont.h              U8g2 中文字体桥接 + M5.Lcd 适配器
│   └── BootGlyphs.h          BOOT 大字标题的离线 XBM 位图
├── tools/                    字模离线生成脚本（可选，见 tools/README.md）
└── lib/                      预留本地库目录
```

约 950 行 C++ 代码 + 200 行 Python 工具脚本 + 64 卦文本数据。

## 技术要点

- **U8g2 中文字体桥接 M5.Lcd**：`M5.Lcd`（`M5Display : TFT_eSPI : Print`）并不继承 `Adafruit_GFX`，而 `U8g2_for_Adafruit_GFX::begin()` 要 `Adafruit_GFX&`。做法是写一个只转发 `drawPixel/drawFastHLine/drawFastVLine` 的轻量适配器 `M5LcdGfxAdapter`，绕开继承链不兼容。详见 [include/UiFont.h](include/UiFont.h) 与 [CLAUDE.md](CLAUDE.md) 第 3 节。
- **U8g2 支持不到的大字号 (≥40px)** 走离线渲染 XBM 位图路径，`drawXBitmap()` 前景透明绘制。见 [tools/](tools/)。
- **摇晃检测**：读 MPU6886 三轴加速度算合模长，帧间差 > 1.2g 判定为一次"晃动尖峰"，冷却 500ms。手感参数需真机调。见 [include/ShakeDetector.h](include/ShakeDetector.h)。
- **卦辞自动换行**：`UiFont::drawTextWrapped()` 按 UTF-8 字符边界换行（不切碎汉字 3 字节编码），JUDGMENT 界面根据实际行数动态计算分隔线 y 坐标。

完整设计文档见 [CLAUDE.md](CLAUDE.md)（约 350 行，含硬件规格、管脚映射、按键 API 核对、字体方案推导、64 卦数据结构、摇卦算法、UI 状态机、界面 ASCII 布局、待办事项）。

## 依赖

由 [platformio.ini](platformio.ini) 的 `lib_deps` 自动拉取：

- [`m5stack/M5StickCPlus`](https://github.com/m5stack/M5StickC-Plus) — 官方板级驱动
- [`olikraus/U8g2_for_Adafruit_GFX`](https://github.com/olikraus/U8g2_for_Adafruit_GFX) — U8g2 中文字体在 Adafruit_GFX 兼容显示上的桥接

字模生成工具（可选，仅当你要替换 BOOT 大字标题时用到）：`Pillow`（`pip install Pillow`）。

## 贡献

欢迎 PR。特别欢迎：

- 真机试烧后的**摇晃手感参数调优**（`ShakeDetector.h` 里的阈值和冷却）。
- **界面视觉细节**：JUDGMENT 背景水印、动画帧。
- **[include/Hexagrams.h](include/Hexagrams.h) 的 `insight` 白话引申**里若有你觉得可以改得更贴切的，欢迎提出。

请保持代码风格与现有一致（Arduino/ESP32 常见约定，2 空格缩进，中文注释）。

## 许可

代码：[MIT License](LICENSE)。

数据说明：[include/Hexagrams.h](include/Hexagrams.h) 中的 `judgment` 字段来源为通行本《周易》古籍经文，属于公共领域；`insight` 字段为本项目原创白话引申，随代码一并以 MIT 授权。

## 致谢

- 灵感来自网络上流传的一款原版实物摇卦占卜小工具，本项目从零重写，未使用其代码或素材。
- 中文字体：文泉驿点阵宋体（wqy，通过 U8g2 收录）用于运行时正文；华文中宋（Windows 系统字体）用于离线渲染 BOOT 大字标题。
