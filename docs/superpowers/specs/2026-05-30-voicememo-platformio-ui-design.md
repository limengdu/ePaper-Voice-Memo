# VoiceMemoReminder 迁移与界面改版设计

> 日期：2026-05-30
> 状态：已评审，待实现

## 背景与目标

VoiceMemoReminder 是运行在 Seeed reTerminal E 系列墨水屏设备上的语音备忘提醒固件：按住 KEY0 录音，上传云端转写并由 LLM 改写为一条带日期/时间的英文提醒，绘制到墨水屏并持久化到 NVS。

本次工作有三个目标，按顺序实现：

1. **迁移到 PlatformIO**：从 Arduino `.ino` 形式迁移为结构清晰、可独立维护的 PlatformIO 工程，并消除源码中的明文密钥。
2. **修复日期标签缺陷**：精确到具体日期的提醒会被错误地渲染成模糊的周标签（如 `This wk`），需改为按真实日期显示。
3. **Header 改版**：主列表顶部改为「左上记事本图标 + 居中放大时间 + 居中日期星期」。

目标设备：**reTerminal E1003**（ESP32-S3R8，10.3 寸，16 灰阶，触摸，`BOARD_SCREEN_COMBO 522`，1404×1872）。E1001 / E1002 的多设备支持予以保留；本次界面改版仅作用于 E1003 的 16 灰阶卡片式界面，E1001 / E1002 的文本降级界面不改动。

屏幕文字统一使用英文（沿用墨水屏内置 ASCII 字体，不引入中文字库）。

---

## 阶段 1：迁移到 PlatformIO 并外置密钥

### 1.1 目录结构

```
VoiceMemoReminder/
├── platformio.ini
├── .gitignore
├── src/                      # 所有 .cpp/.h 与 driver.h 移入
│   ├── main.cpp              # 原 VoiceMemoReminder.ino 改名
│   ├── secrets.h             # WiFi / API key（被 .gitignore 排除）
│   ├── driver.h
│   ├── VoiceMemoApp.cpp/.h
│   ├── AudioCapture.cpp/.h
│   ├── RtcClock.cpp/.h
│   ├── MemoStore.cpp/.h
│   ├── SpeechClient.cpp/.h
│   ├── MemoClient.cpp/.h
│   ├── MemoUI.cpp/.h
│   ├── TouchInput.cpp/.h
│   ├── TouchMapper.h
│   └── JsonUtil.h
├── include/
│   └── secrets.example.h     # 占位符模板（入库）
├── test/                     # native 单元测试（见阶段 2）
├── gateway/                  # Python 服务，原样保留
├── docs/
└── README.md
```

源码统一进 `src/`，模块间 `#include "X.h"` 无需改动（PlatformIO 默认将 `src/` 加入 include 路径并编译其中所有 `.cpp`）。

### 1.2 platformio.ini

```ini
[env:reterminal_e1003]
platform = espressif32
board = seeed_xiao_esp32s3
framework = arduino
upload_speed = 115200          ; E1003 必须 115200，否则可能上传失败
monitor_speed = 115200
board_build.arduino.memory_type = qio_opi   ; OPI PSRAM
build_flags = -D BOARD_HAS_PSRAM
lib_deps = https://github.com/Seeed-Studio/Seeed_GFX
```

PSRAM / flash / 分区等具体项在编译阶段验证微调。

### 1.3 显示库依赖

Seeed_GFX 是 TFT_eSPI 的分支，未发布到 PlatformIO 官方库源，故以 GitHub URL 引入（项目既有依赖，非新增）。它依赖工程内的 `driver.h`（含 `BOARD_SCREEN_COMBO 522`）。

**待验证**：Seeed_GFX 能否定位 `src/driver.h`。若不能，则将库放入 `lib/Seeed_GFX/`，由 PlatformIO 自动识别。

### 1.4 密钥外置

- `src/secrets.h`（不入库）：
  ```cpp
  #define VM_WIFI_SSID     "..."
  #define VM_WIFI_PASSWORD "..."
  #define VM_GROQ_API_KEY  "..."
  ```
- `include/secrets.example.h`（入库，占位符）：同结构，值为占位符。
- `main.cpp` 的配置块改为引用上述宏，不再硬编码。
- `.gitignore` 排除 `src/secrets.h`、`.pio/`、IDE 与系统产物。

### 1.5 Git 初始化策略

首次提交**只纳入设计文档与 `.gitignore`**；含明文密钥的源码在密钥外置完成前不提交。阶段 1 完成密钥外置后，再将重构后的源码纳入版本控制。已暴露的 Groq key 建议在控制台吊销并更换。

---

## 阶段 2：日期标签逻辑

将「哪天」与「几点」拆分为两个独立维度，分别决定日期格与时间格。

### 2.1 日期格（由 `dueEpoch` 计算，`MemoUI::formatDueLabel`）

| 距今天数 | 显示 |
| --- | --- |
| 无有效日期 | `Some day` |
| 已过期 | `Overdue` |
| 0 | `Today` |
| 1 | `Tomorrow` |
| 2 | `Day after`（新增） |
| 3–6 | 星期缩写（`Wed`） |
| ≥7 | 月缩写 + 日（`Jun 15`） |

### 2.2 时间格（大时间位置）

- 用户说了精确钟点 → `HH:MM`
- 只说了时段 → 时段词：`morning→AM`、`afternoon→PM`、`noon→Noon`、`evening/night→Eve`
- 既无钟点也无时段词 → 回退为 `HH:MM`（来自 best-guess 的 due）或留空

### 2.3 渲染改动（`MemoUI.cpp`）

删除「`fuzzyLabel` 非空时独占大标签」的分支，统一为「日期格（chip）+ 大时间」两段式：日期格永远按 §2.1 显示，大时间按 §2.2 显示。

### 2.4 LLM 提示词改动（`MemoClient.cpp`）

`due_label` 的语义由「哪天 + 模糊程度」收窄为「**仅时段词**」：

- 仅允许输出 `AM` / `PM` / `Noon` / `Eve` 或空字符串。
- 删除 `This wk` / `Next wk` / `Wknd` / `Today` / `Tomorrow` / `Soon` / `Some day` 等表示「哪天」的标签——日期一律由设备依据 `due`（绝对时间）本地计算。
- 同步更新 few-shot 示例，使其只在「无精确钟点」时给出时段词。

### 2.5 存储

`MemoEntry.fuzzyLabel` 字段沿用（仅承载内容由「周标签」变为「时段词」）。NVS blob 格式不变，版本号不变。

---

## 阶段 3：Header 改版（方案 A，仅改 `MemoUI::drawTodoList` 的 16 灰阶分支）

### 3.1 布局

```
┌────────────────────────────────────┐
│ ▤ Notes                  [ Ready ]  │  logo + "Notes" 左上 / status 右上
│              14:30                   │  超大时间，水平居中
│         Friday, May 30               │  日期 + 星期，中号，居中
├────────────────────────────────────┤
│  ...（卡片列表，沿用现有布局）        │
```

- 取消原 `Reminders` 大标题。
- 时间使用居中基准、字号较现状更大；日期星期在其正下方居中。
- status badge（`Ready` / `Recording` / `Saved` 等）置于右上角。

### 3.2 记事本 logo

新增矢量绘制函数 `drawNotebookLogo(x, y, size, color)`：顶部一排小圆圈表示线圈，下方圆角矩形为纸张，纸张内 2–3 条横线表示文字行；绘制方式与现有 `drawCheckbox` / spinner 同源（基本图元，无位图资源）。

### 3.3 日期格式化

需要 `Friday, May 30` 形式（星期全名 + 月缩写 + 日）。新增对应格式化逻辑（星期全名数组），不破坏现有 `RtcClock::nowDateLabel()`（`2026/05/30 Fri`）。

---

## 测试策略

- **可离线验证（native 单元测试）**：阶段 2 的纯逻辑——`formatDueLabel` 的日期/时段映射、`dayDistance`、跨午夜边界。使用 PlatformIO `test/` + Unity，`pio test -e native` 运行。
- **需真机验证**：PlatformIO 能否编译通过、PSRAM/flash 配置、墨水屏实际显示效果、LLM 时段词输出是否稳定。

## 边缘情况

- Seeed_GFX 定位 `driver.h` 的方式（编译验证）。
- 屏幕旋转/分辨率方向——以 `display_.width()` / `display_.height()` 动态取值，不写死。
- 跨午夜的 `dayDistance`（已通过 mktime 归零时分秒处理）。
- `Day after` 与时段词在 chip 中的文本宽度。
- LLM 偶尔不输出时段词 → 回退显示 `HH:MM` 或留空。
- E1001 / E1002 文本模式共用 `formatDueLabel`，会显示 `Day after` 等新标签（符合预期，不受 Header 改版影响）。

## 风险与待验证项

1. Seeed_GFX 在 PlatformIO 下的库定位与编译（最高优先验证）。
2. ESP32-S3 OPI PSRAM 与 8 MB flash 的 `platformio.ini` 配置。
3. 70B 级模型对「仅时段词」提示词的遵循度。
