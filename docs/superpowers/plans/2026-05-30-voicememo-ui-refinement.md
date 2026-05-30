# VoiceMemoReminder 界面与交互精修 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 重做 E1003 主界面 Header（logo+Notes / 居中时间日期 / 电池+WiFi）、修复卡片日期时间重叠与无时间显示、合并开机页、将录音处理态内联、新增电池读取。

**Architecture:** 纯逻辑（日期全称、电池毫伏→百分比）先以 native 单测固定；再加 `UiStatus` 结构与 `VoiceMemoApp::currentStatus()` 数据层；然后重做 `MemoUI` 的 Header（抽出 `drawHeader` 供主界面与开机页复用）与卡片细节；最后改 `VoiceMemoApp` 的开机与录音流程接入。每个 Task 结束都能编译。

**Tech Stack:** PlatformIO (espressif32 seeedboards + native), Seeed_GFX/TFT_eSPI (EPaper), Unity, Groq LLaMA 3.3 70B。

---

## 文件变更概览

| 文件 | 责任 |
|---|---|
| `src/DateLabels.h` | 日期 chip 全称（星期）；保持 header-only 纯逻辑 |
| `src/BatteryMath.h` | 新增：电池毫伏→百分比纯函数 |
| `test/test_date_labels/test_main.cpp` | 更新星期全称断言 |
| `test/test_battery/test_main.cpp` | 新增：电池换算单测 |
| `src/MemoUI.h` | 新增 `UiStatus`；`drawTodoList` 改签名；新增 `drawHeader`/`drawBoot`/图标/`drawClipboardLogo`；删 `drawProcessing` |
| `src/MemoUI.cpp` | Header 三栏重做、卡片间距与无时间留空、footer 加深、开机页、图标绘制 |
| `src/MemoClient.cpp` | 提示词：无时间→`NONE`、时段词全称 |
| `src/RtcClock.h/.cpp` | 新增 `nowHeaderDateLabel()`（`Apr 28 Tue`） |
| `src/VoiceMemoApp.h` | 电池引脚常量、`readBatteryPercent()`、`currentStatus()` |
| `src/VoiceMemoApp.cpp` | 电池读取、开机单页、录音处理态内联、调用点改 `UiStatus` |

---

## Task 1：纯逻辑——日期星期全称 + 电池百分比（native TDD）

**Files:**
- Modify: `src/DateLabels.h`
- Create: `src/BatteryMath.h`
- Modify: `test/test_date_labels/test_main.cpp`
- Create: `test/test_battery/test_main.cpp`

- [ ] **Step 1: 更新日期测试为星期全称**

将 `test/test_date_labels/test_main.cpp` 中 `test_label_weekday_wed` 替换为：

```cpp
void test_label_weekday_full() {
    TEST_ASSERT_EQUAL_STRING("Wednesday", vmDateChipLabel(3, 3));
    TEST_ASSERT_EQUAL_STRING("Sunday",    vmDateChipLabel(5, 0));
}
```

并把 `RUN_TEST(test_label_weekday_wed);` 改为 `RUN_TEST(test_label_weekday_full);`。

- [ ] **Step 2: 运行测试确认失败（red）**

Run: `pio test -e native --filter test_date_labels`
Expected: FAIL — `vmDateChipLabel(3,3)` 当前返回 `"Wed"`，断言 `"Wednesday"` 不通过。

- [ ] **Step 3: 改 DateLabels.h 用星期全称**

将 `src/DateLabels.h` 的 `vmDateChipLabel` 中的 `kWeekdays` 数组替换为全称：

```cpp
    static const char* kWeekdays[] = {
        "Sunday", "Monday", "Tuesday", "Wednesday",
        "Thursday", "Friday", "Saturday"
    };
```

- [ ] **Step 4: 运行测试确认通过（green）**

Run: `pio test -e native --filter test_date_labels`
Expected: PASS（全部用例通过）。

- [ ] **Step 5: 新增电池换算纯函数**

创建 `src/BatteryMath.h`：

```cpp
#pragma once

// Convert the measured ADC millivolts (the DIVIDED battery voltage) into a
// 0-100 percentage. Hardware halves the real voltage, so multiply by 2.
// Maps 3.3 V -> 0 %, 4.2 V -> 100 %, clamped.
inline int vmBatteryPercent(int milliVolts)
{
    const float v = (milliVolts / 1000.0f) * 2.0f;
    float pct = (v - 3.3f) / (4.2f - 3.3f) * 100.0f;
    if (pct < 0.0f)   pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    return static_cast<int>(pct + 0.5f);
}
```

- [ ] **Step 6: 写电池换算测试**

创建 `test/test_battery/test_main.cpp`：

```cpp
#include <unity.h>
#include "BatteryMath.h"

void test_full()    { TEST_ASSERT_EQUAL_INT(100, vmBatteryPercent(2100)); } // 2.1*2=4.2V
void test_empty()   { TEST_ASSERT_EQUAL_INT(0,   vmBatteryPercent(1650)); } // 1.65*2=3.3V
void test_half()    { TEST_ASSERT_EQUAL_INT(50,  vmBatteryPercent(1875)); } // 1.875*2=3.75V
void test_clamp_hi(){ TEST_ASSERT_EQUAL_INT(100, vmBatteryPercent(2300)); }
void test_clamp_lo(){ TEST_ASSERT_EQUAL_INT(0,   vmBatteryPercent(1000)); }

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_full);
    RUN_TEST(test_empty);
    RUN_TEST(test_half);
    RUN_TEST(test_clamp_hi);
    RUN_TEST(test_clamp_lo);
    return UNITY_END();
}
```

- [ ] **Step 7: 运行电池测试确认通过**

Run: `pio test -e native --filter test_battery`
Expected: PASS — `5 Tests 0 Failures`.

- [ ] **Step 8: 提交**

```bash
git add src/DateLabels.h src/BatteryMath.h test/test_date_labels/test_main.cpp test/test_battery/test_main.cpp
git commit -m "feat: weekday full names and battery percent helper with native tests"
```

---

## Task 2：LLM 提示词——无时间→NONE，时段词全称

**Files:**
- Modify: `src/MemoClient.cpp`

- [ ] **Step 1: 替换 system 提示词的 due_label 规则与示例**

在 `src/MemoClient.cpp` 的 `summarizeOpenAICompatible` 中，找到从 `system += "3) due_label:` 开始到示例段结束（`...Buy apples...due_label\\\":\\\"\\\"}";`）的整段，替换为：

```cpp
  system += "3) due_label: classifies the TIME-OF-DAY only.\\n";
  system += "   - Exact clock number given (8, 14:30, 3\\u70b9) -> due_label = \\\"\\\" (device shows HH:MM).\\n";
  system += "   - Only a part of day, no number -> use a FULL word: Morning / Afternoon / Noon / Evening.\\n";
  system += "       morning/\\u65e9\\u4e0a/\\u4e0a\\u5348 -> Morning\\n";
  system += "       afternoon/\\u4e0b\\u5348 -> Afternoon\\n";
  system += "       noon/\\u4e2d\\u5348 -> Noon\\n";
  system += "       evening/night/\\u665a\\u4e0a/tonight -> Evening\\n";
  system += "   - NO time mentioned at all -> due_label = \\\"NONE\\\" (device shows no clock).\\n";
  system += "   The DAY is computed by the device from `due`. NEVER put a day word (Today/Tomorrow/This wk) in due_label.\\n\\n";
  system += "EXAMPLES:\\n";
  system += "Input: \\\"\\u660e\\u5929\\u53bb\\u8df3\\u821e\\\"\\n";
  system += "Output: {\\\"memo\\\":\\\"Go dancing\\\",\\\"due\\\":\\\"2026-05-31 09:00\\\",\\\"due_label\\\":\\\"NONE\\\"}\\n";
  system += "Input: \\\"\\u63d0\\u9192\\u6211\\u4eca\\u5929\\u665a\\u4e0a\\u53bb\\u6d17\\u6fa1\\\"\\n";
  system += "Output: {\\\"memo\\\":\\\"Take a bath\\\",\\\"due\\\":\\\"2026-05-30 21:00\\\",\\\"due_label\\\":\\\"Evening\\\"}\\n";
  system += "Input: \\\"\\u665a\\u4e0a8\\u70b9\\u53eb\\u6211\\u5403\\u996d\\\"\\n";
  system += "Output: {\\\"memo\\\":\\\"Dinner\\\",\\\"due\\\":\\\"2026-05-30 20:00\\\",\\\"due_label\\\":\\\"\\\"}\\n";
  system += "Input: \\\"\\u660e\\u5929\\u4e0a\\u5348\\u5f00\\u4f1a\\\"\\n";
  system += "Output: {\\\"memo\\\":\\\"Morning meeting\\\",\\\"due\\\":\\\"2026-05-31 09:00\\\",\\\"due_label\\\":\\\"Morning\\\"}\\n";
  system += "Input: \\\"\\u4e0b\\u53483\\u70b9\\u53d6\\u5feb\\u9012\\\"\\n";
  system += "Output: {\\\"memo\\\":\\\"Pick up package\\\",\\\"due\\\":\\\"2026-05-30 15:00\\\",\\\"due_label\\\":\\\"\\\"}\\n";
  system += "Input: \\\"\\u4e70\\u70b9\\u82f9\\u679c\\\"\\n";
  system += "Output: {\\\"memo\\\":\\\"Buy apples\\\",\\\"due\\\":\\\"2026-05-30 19:00\\\",\\\"due_label\\\":\\\"NONE\\\"}";
```

- [ ] **Step 2: 编译验证**

Run: `pio run -e reterminal_e1003`
Expected: `SUCCESS`.

- [ ] **Step 3: 提交**

```bash
git add src/MemoClient.cpp
git commit -m "fix: due_label = NONE when no time given; full time-of-day words"
```

---

## Task 3：卡片——无时间留空 + 日期/时间拉开间距

**Files:**
- Modify: `src/MemoUI.cpp`

- [ ] **Step 1: 替换 drawCard 右侧渲染块**

在 `src/MemoUI.cpp` 的 `drawCard` 中，找到 `// ---- Right side` 注释起、到该 `{ ... }` 块结束（绘制 date chip + big time 的整段），替换为：

```cpp
  // ---- Right side: date chip (top) + big time / time-of-day (bottom) ----
  {
    String dateChip, timeBig;
    bool   over = false;
    formatDueLabel(nowEpoch, entry, dateChip, timeBig, over);

    if (entry.fuzzyLabel == "NONE") {
      timeBig = "";                 // user gave no time -> leave blank
    } else if (entry.fuzzyLabel.length() > 0) {
      timeBig = entry.fuzzyLabel;   // time-of-day word (Morning/Evening/...)
    }

    const int rightEdge = x + w - rightPad;

    // Date chip near the top of the card.
    display_.setTextSize(3);
    const int chipPad = 18;
    const int chipH   = 38;
    const int chipW   = display_.textWidth(dateChip) + chipPad * 2;
    const int chipX   = rightEdge - chipW;
    const int chipY   = y + 16;
    const uint16_t chipFill = overdue ? kUiCardDark
                               : (entry.done ? kUiMuted : kUiBadge);
    display_.fillRoundRect(chipX, chipY, chipW, chipH, 8, chipFill);
    display_.setTextDatum(MC_DATUM);
    display_.setTextColor(kUiTextInv, chipFill, true);
    display_.drawString(dateChip, chipX + chipW / 2, chipY + chipH / 2 - 1);

    // Big time anchored to the BOTTOM of the card, so it can never overlap
    // the chip regardless of card height.
    if (timeBig.length() > 0) {
      display_.setTextSize(5);
      display_.setTextColor(fg, fill, true);
      display_.setTextDatum(BR_DATUM);
      display_.drawString(timeBig, rightEdge, y + h - 16);
    }
  }
```

- [ ] **Step 2: 编译验证**

Run: `pio run -e reterminal_e1003`
Expected: `SUCCESS`.

- [ ] **Step 3: 提交**

```bash
git add src/MemoUI.cpp
git commit -m "fix: card date chip top / time bottom (no overlap); blank time when NONE"
```

---

## Task 4：RtcClock 新增 nowHeaderDateLabel

**Files:**
- Modify: `src/RtcClock.h`
- Modify: `src/RtcClock.cpp`

- [ ] **Step 1: 声明方法**

在 `src/RtcClock.h` 的 `String nowLongDateLabel();` 之后加入：

```cpp
  // Compact header date, e.g. "Apr 28 Tue".
  String nowHeaderDateLabel();
```

- [ ] **Step 2: 实现**

在 `src/RtcClock.cpp` 的 `nowLongDateLabel()` 实现之后追加：

```cpp
String RtcClock::nowHeaderDateLabel()
{
  VoiceMemoRtcTime rt = {};
  if (!available_ || !readTime(rt) || !rt.voltageOK) return "--/--";

  static const char* kMonthShort[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };

  const int wd = (rt.weekday >= 0 && rt.weekday < 7) ? rt.weekday : 0;
  const int mo = (rt.month  >= 1 && rt.month  <= 12) ? rt.month - 1 : 0;

  char buf[24];
  snprintf(buf, sizeof(buf), "%s %02d %s",
           kMonthShort[mo], rt.day, kWeekdayNames[wd]);
  return String(buf);
}
```

> 注：`kWeekdayNames`（短星期 Sun..Sat）已在该文件匿名命名空间中存在，直接复用。

- [ ] **Step 3: 编译验证**

Run: `pio run -e reterminal_e1003`
Expected: `SUCCESS`.

- [ ] **Step 4: 提交**

```bash
git add src/RtcClock.h src/RtcClock.cpp
git commit -m "feat: add RtcClock::nowHeaderDateLabel (Apr 28 Tue)"
```

---

## Task 5：UiStatus 结构 + VoiceMemoApp 电池读取与状态采集

**Files:**
- Modify: `src/MemoUI.h`
- Modify: `src/VoiceMemoApp.h`
- Modify: `src/VoiceMemoApp.cpp`

- [ ] **Step 1: 在 MemoUI.h 定义 UiStatus**

在 `src/MemoUI.h` 的 `class MemoUI {` 之前加入：

```cpp
// Snapshot of device status drawn in the header.
struct UiStatus {
  bool wifiConnected = false;
  int  batteryPercent = -1;   // -1 = unknown
  bool processing = false;    // true -> show "Processing" instead of icons
};
```

- [ ] **Step 2: VoiceMemoApp.h 加电池引脚与方法**

在 `src/VoiceMemoApp.h` 的 `static constexpr int kBuzzerPin = 45;` 之后加入：

```cpp
  // Battery sense (E1003-specific enable pin; verify against schematic).
  static constexpr int kBatteryEnablePin = 40;
  static constexpr int kBatteryAdcPin    = 1;
```

并在 `private:` 区的方法声明区（`bool ensureWiFi(...);` 附近）加入：

```cpp
  int      readBatteryPercent();
  UiStatus currentStatus(bool processing);
```

- [ ] **Step 3: VoiceMemoApp.cpp 实现电池读取与状态采集**

在 `src/VoiceMemoApp.cpp` 顶部 `#include <WiFi.h>` 之后加入：

```cpp
#include "BatteryMath.h"
```

在 `bool VoiceMemoApp::ensureWiFi(...)` 实现之前插入：

```cpp
int VoiceMemoApp::readBatteryPercent()
{
  digitalWrite(kBatteryEnablePin, HIGH);
  delay(5);
  const int mv = analogReadMilliVolts(kBatteryAdcPin);
  digitalWrite(kBatteryEnablePin, LOW);
  return vmBatteryPercent(mv);
}

UiStatus VoiceMemoApp::currentStatus(bool processing)
{
  UiStatus s;
  s.wifiConnected  = (WiFi.status() == WL_CONNECTED);
  s.batteryPercent = readBatteryPercent();
  s.processing     = processing;
  return s;
}
```

- [ ] **Step 4: 在 setupPins 配置电池引脚与 ADC**

在 `src/VoiceMemoApp.cpp` 的 `setupPins()` 末尾（`digitalWrite(kBuzzerPin, LOW);` 之后）加入：

```cpp
  pinMode(kBatteryEnablePin, OUTPUT);
  digitalWrite(kBatteryEnablePin, LOW);
  analogReadResolution(12);
  analogSetPinAttenuation(kBatteryAdcPin, ADC_11db);
```

- [ ] **Step 5: 编译验证**

Run: `pio run -e reterminal_e1003`
Expected: `SUCCESS`（`currentStatus`/`readBatteryPercent` 暂未被调用，成员函数不报未用警告）。

- [ ] **Step 6: 提交**

```bash
git add src/MemoUI.h src/VoiceMemoApp.h src/VoiceMemoApp.cpp
git commit -m "feat: UiStatus struct + battery/wifi status sampling in VoiceMemoApp"
```

---

## Task 6：Header 绘制基件——clipboard logo / 电池 / WiFi 图标

**Files:**
- Modify: `src/MemoUI.h`
- Modify: `src/MemoUI.cpp`

- [ ] **Step 1: 在 MemoUI.h 声明绘制基件**

在 `src/MemoUI.h` private 区，在原 `void drawNotebookLogo(...)` 一行**下方新增**三个声明（保留 `drawNotebookLogo`，Task 7 末尾再删）：

```cpp
  void drawClipboardLogo(int x, int y, int size, uint16_t color);
  void drawBatteryIcon(int x, int y, int w, int h, int percent, uint16_t color);
  void drawWifiIcon(int x, int y, int w, int h, bool connected, uint16_t color);
```

- [ ] **Step 2: 在 MemoUI.cpp 用 clipboard 实现替换 drawNotebookLogo**

在 `src/MemoUI.cpp` 的 `drawNotebookLogo(...)` 实现**之后新增**以下三个函数（`drawNotebookLogo` 暂时保留，Task 7 删除）：

```cpp
void MemoUI::drawClipboardLogo(int x, int y, int size, uint16_t color)
{
  // Clip tab at top center.
  const int clipW = size / 3;
  const int clipH = size / 8;
  display_.fillRoundRect(x + size / 2 - clipW / 2, y, clipW, clipH, 3, color);

  // Board body (double outline so it reads on ePaper).
  const int boardY = y + clipH / 2;
  const int boardH = size - clipH / 2;
  display_.drawRoundRect(x,     boardY,     size,     boardH,     6, color);
  display_.drawRoundRect(x + 1, boardY + 1, size - 2, boardH - 2, 6, color);

  // Ruled lines.
  const int lx1 = x + size / 5;
  const int lx2 = x + size * 4 / 5;
  for (int i = 1; i <= 3; i++) {
    const int ly = boardY + boardH * i / 4;
    const int len = (i == 3) ? (lx2 - lx1) * 2 / 3 : (lx2 - lx1);
    display_.drawFastHLine(lx1, ly, len, color);
  }
}

void MemoUI::drawBatteryIcon(int x, int y, int w, int h, int percent, uint16_t color)
{
  display_.drawRect(x, y, w, h, color);
  const int nubW = 3, nubH = h / 3;
  display_.fillRect(x + w, y + (h - nubH) / 2, nubW, nubH, color);
  if (percent < 0) return;                       // unknown -> empty body
  const int fillW = (w - 4) * percent / 100;
  if (fillW > 0) display_.fillRect(x + 2, y + 2, fillW, h - 4, color);
}

void MemoUI::drawWifiIcon(int x, int y, int w, int h, bool connected, uint16_t color)
{
  // Signal-bar style (ascending). Solid = connected, outline + slash = not.
  const int bars = 4, gap = 3;
  const int bw = (w - gap * (bars - 1)) / bars;
  for (int i = 0; i < bars; i++) {
    const int bh = h * (i + 1) / bars;
    const int bx = x + i * (bw + gap);
    const int by = y + h - bh;
    if (connected) display_.fillRect(bx, by, bw, bh, color);
    else           display_.drawRect(bx, by, bw, bh, color);
  }
  if (!connected) display_.drawLine(x, y, x + w, y + h, color);
}
```

- [ ] **Step 3: 编译验证**

Run: `pio run -e reterminal_e1003`
Expected: `SUCCESS`（`drawNotebookLogo` 仍在、仍被旧 `drawTodoList` 调用；三个新图标暂未被调用，C++ 成员函数未用不报错）。

- [ ] **Step 4: 提交**

```bash
git add src/MemoUI.h src/MemoUI.cpp
git commit -m "feat: clipboard logo + battery/wifi header icons"
```

---

## Task 7：drawHeader + drawTodoList 重做（接入状态、去分隔线、footer 加深）

**Files:**
- Modify: `src/MemoUI.h`
- Modify: `src/MemoUI.cpp`
- Modify: `src/VoiceMemoApp.cpp`

- [ ] **Step 1: MemoUI.h 改 drawTodoList 签名并声明 drawHeader/drawBoot**

在 `src/MemoUI.h` 中，将 `drawTodoList` 声明改为：

```cpp
  void drawTodoList(MemoStore& store,
                    RtcClock& rtc,
                    const UiStatus& status,
                    const String& hint);

  // One-page boot/splash screen reusing the header.
  void drawBoot(RtcClock& rtc, const String& statusText, const UiStatus& status);
```

在 private 区（图标声明附近）加：

```cpp
  void drawHeader(RtcClock& rtc, const UiStatus& status);
```

并**删除**已无用的 `void drawNotebookLogo(...)` 声明。

- [ ] **Step 2: MemoUI.cpp 实现 drawHeader**

在 `src/MemoUI.cpp` 的 `void MemoUI::drawTodoList(...)` 实现之前插入：

```cpp
void MemoUI::drawHeader(RtcClock& rtc, const UiStatus& st)
{
  const int w = display_.width();
  const int margin = 80;
  const int topY = 24;

  // Left column: clipboard logo (top) + "Notes" (bottom).
  const int logoSize = 56;
  drawClipboardLogo(margin, topY, logoSize, kUiText);
  display_.setTextDatum(TL_DATUM);
  display_.setTextSize(3);
  display_.setTextColor(kUiText, kUiBg, true);
  display_.drawString("Notes", margin, topY + logoSize + 8);

  // Center column: big time (top) + date (bottom), centered.
  display_.setTextDatum(TC_DATUM);
  display_.setTextSize(8);
  display_.setTextColor(kUiText, kUiBg, true);
  display_.drawString(rtc.nowTimeLabel(), w / 2, topY);
  display_.setTextSize(3);
  display_.drawString(rtc.nowHeaderDateLabel(), w / 2, topY + 8 * 8 + 14);

  // Right column.
  if (st.processing) {
    // Static "working" ring + label.
    const int cx = w - margin - 18;
    const int cy = topY + 20;
    display_.drawCircle(cx, cy, 16, kUiText);
    display_.fillCircle(cx, cy - 16, 4, kUiText);
    display_.setTextDatum(MR_DATUM);
    display_.setTextSize(3);
    display_.setTextColor(kUiText, kUiBg, true);
    display_.drawString("Processing", cx - 30, cy);
  } else {
    // Battery (top): "NN%" then icon, right aligned.
    const int battW = 46, battH = 22;
    const int battX = w - margin - battW - 3;
    const int battY = topY + 2;
    drawBatteryIcon(battX, battY, battW, battH, st.batteryPercent, kUiText);
    display_.setTextDatum(MR_DATUM);
    display_.setTextSize(2);
    display_.setTextColor(kUiText, kUiBg, true);
    char pbuf[8];
    snprintf(pbuf, sizeof(pbuf), "%d%%", st.batteryPercent < 0 ? 0 : st.batteryPercent);
    display_.drawString(pbuf, battX - 6, battY + battH / 2);

    // WiFi (bottom), right aligned.
    const int wifiW = 34, wifiH = 24;
    drawWifiIcon(w - margin - wifiW, battY + battH + 14, wifiW, wifiH,
                 st.wifiConnected, kUiText);
  }
}
```

- [ ] **Step 3: MemoUI.cpp 重做 drawTodoList 的 16 灰阶分支头部**

在 `drawTodoList` 中，将 `#if VM_SCREEN_MODE == VM_SCREEN_GRAY16` 之后、`// ---- Card list ----` 之前的所有 Header 代码（含 `fillSprite`、原 header、`drawFastHLine` 分隔线）替换为下面这段；并**删除** `src/MemoUI.cpp` 中已无引用的 `drawNotebookLogo(...)` 实现：

```cpp
  const int w = display_.width();
  const int h = display_.height();
  const int margin = 80;

  display_.fillSprite(kUiBg);
  drawHeader(rtc, status);

  const int headerH = 200;   // header band height (no divider line)
```

> 说明：删除了原 `drawFastHLine(margin, headerH, ...)` 分隔线；`headerH` 仅用于下方列表起点计算。

- [ ] **Step 4: MemoUI.cpp 加深 footer 并去掉 status badge 残留**

在 `drawTodoList` 的 footer 段，将提示行颜色由 `kUiMuted` 改为 `kUiText`：

```cpp
  display_.setTextDatum(BL_DATUM);
  display_.setTextSize(3);
  display_.setTextColor(kUiText, kUiBg, true);
  display_.drawString(hint, margin, h - 36);
```

（右下计数行保持不变；原 header 里的 status badge 代码已在 Step 3 删除。）

- [ ] **Step 5: MemoUI.cpp 实现 drawBoot**

在 `drawHeader` 实现之后插入：

```cpp
void MemoUI::drawBoot(RtcClock& rtc, const String& statusText, const UiStatus& st)
{
  display_.fillSprite(kUiBg);
  drawHeader(rtc, st);
  display_.setTextDatum(MC_DATUM);
  display_.setTextSize(4);
  display_.setTextColor(kUiText, kUiBg, true);
  display_.drawString(statusText, display_.width() / 2, display_.height() / 2);
  display_.update();
}
```

- [ ] **Step 6: 更新 VoiceMemoApp.cpp 的所有 drawTodoList 调用点**

在 `src/VoiceMemoApp.cpp` 中，将每处 `ui_.drawTodoList(store_, rtc_, "<BADGE>", <hint>)` 改为传 `UiStatus`：

- `begin()` 末尾：
```cpp
  ui_.drawTodoList(store_, rtc_, currentStatus(false),
                   "Hold KEY0 to add. Tap a box to check off.");
```
- `stopRecording()` 中 `tooShort` 分支：
```cpp
    ui_.drawTodoList(store_, rtc_, currentStatus(false),
                     "Hold KEY0 for at least one second.");
```
- `stopRecording()` 中 `NO WIFI` 分支：
```cpp
    ui_.drawTodoList(store_, rtc_, currentStatus(false),
                     "Reminder skipped because WiFi is unavailable.");
```
- `stopRecording()` 末尾成功分支（保留既有 `hint` 变量）：
```cpp
  ui_.drawTodoList(store_, rtc_, currentStatus(false), hint);
```
- `pollTouch()` 中：
```cpp
  ui_.drawTodoList(store_, rtc_, currentStatus(false),
                   "Hold KEY0 to add. Tap a box to check off.");
```

- [ ] **Step 7: 编译验证**

Run: `pio run -e reterminal_e1003`
Expected: `SUCCESS`.

- [ ] **Step 8: 提交**

```bash
git add src/MemoUI.h src/MemoUI.cpp src/VoiceMemoApp.cpp
git commit -m "feat: three-column header with battery/wifi, no divider, darker footer"
```

---

## Task 8：开机页合并为一页

**Files:**
- Modify: `src/VoiceMemoApp.cpp`

- [ ] **Step 1: 用 drawBoot 替换 begin() 里的两处 drawStatus**

在 `src/VoiceMemoApp.cpp` 的 `begin()` 中：

将首个启动页：
```cpp
  ui_.drawStatus("BOOT", "Starting",
                 "Allocating audio buffer and initializing microphone.",
                 "Use Serial1 on GPIO43/GPIO44 for logs.", false, 0.0f);
```
替换为：
```cpp
  ui_.drawBoot(rtc_, "Starting...", currentStatus(false));
```

将 WiFi 连接页：
```cpp
  ui_.drawStatus("WIFI", "Connecting",
                 "Connecting to WiFi before the first recording.",
                 "Edit WiFi, API key, and provider settings in the .ino file.",
                 false, 0.0f);
```
替换为：
```cpp
  ui_.drawBoot(rtc_, "Connecting WiFi...", currentStatus(false));
```

> 麦克风失败的 `ui_.drawStatus("ERR", ...)` 保留不动（致命错误页）。

- [ ] **Step 2: 编译验证**

Run: `pio run -e reterminal_e1003`
Expected: `SUCCESS`.

- [ ] **Step 3: 提交**

```bash
git add src/VoiceMemoApp.cpp
git commit -m "feat: single boot page reusing the main header"
```

---

## Task 9：录音处理态内联（移除全屏 drawProcessing）

**Files:**
- Modify: `src/VoiceMemoApp.cpp`
- Modify: `src/MemoUI.h`
- Modify: `src/MemoUI.cpp`

- [ ] **Step 1: stopRecording 用内联处理态替换 drawProcessing**

在 `src/VoiceMemoApp.cpp` 的 `stopRecording()` 中，将：
```cpp
  ui_.drawProcessing("Processing your voice",
                     "Transcribing and summarizing your memo.\nThis usually takes 5-10 seconds.",
                     "Hold tight, screen will update automatically.");
```
替换为：
```cpp
  // Inline processing state: keep the list visible, show "Processing" in the
  // header. Safe to refresh here -- audio capture is already complete.
  ui_.drawTodoList(store_, rtc_, currentStatus(true),
                   "Hold KEY0 to add. Tap a box to check off.");
```

- [ ] **Step 2: 删除 MemoUI 的 drawProcessing 声明与实现**

- 在 `src/MemoUI.h` 删除 `drawProcessing(...)` 的整段声明（含其上方注释）。
- 在 `src/MemoUI.cpp` 删除整个 `void MemoUI::drawProcessing(...) { ... }` 实现。

- [ ] **Step 3: 编译验证**

Run: `pio run -e reterminal_e1003`
Expected: `SUCCESS`.

- [ ] **Step 4: 提交**

```bash
git add src/VoiceMemoApp.cpp src/MemoUI.h src/MemoUI.cpp
git commit -m "feat: inline processing state in header, drop full-screen processing page"
```

---

## 边缘情况 / 注意事项

| 情况 | 处理 |
|---|---|
| `due_label == "NONE"` | UI 大时间格不绘制（Task 3） |
| LLM 未给时段/NONE（回退） | `fuzzyLabel==""` → 显示 `HH:MM`，布局不破 |
| 电池脚 GPIO40 与原理图不符 | 真机验证；读数异常时百分比会偏，但不崩 |
| 电池未知（读数为 0/负） | `batteryPercent` 传 -1 时电池图标画空壳、文字显示 0% |
| 时段词 `Afternoon` 较宽 | 大时间用 textSize 5，卡片右侧留白足够；真机如仍紧可再降一档 |
| ePaper 处理态无法动画 | 静态圈 + `Processing`（Task 7 已实现） |
| 录音开始仍不可刷屏 | 未改 `startRecording`，约束保持 |

## 真机验证清单（无法离线验证，需烧录）

- Header 三栏观感、居中时间、电池/WiFi 图标可读
- 卡片日期与时间不再重叠、无时间项时间格留空
- footer 提示可见（加深生效）
- 开机仅一页、文字随阶段更新
- 录音松手后列表保持可见、右上显示 Processing
- 电池百分比是否合理（校准 GPIO40 / 分压）
