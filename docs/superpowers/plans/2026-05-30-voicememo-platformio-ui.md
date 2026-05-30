# VoiceMemoReminder PlatformIO 迁移与界面改版实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 VoiceMemoReminder 从 Arduino .ino 迁移到 PlatformIO，修复日期标签 bug（具体日期被 LLM 模糊标签覆盖），并重新设计 E1003 主界面 Header。

**Architecture:** Phase 1 重构项目目录、外置密钥、生成 platformio.ini；Phase 2 提取纯 C++ 日期逻辑为 `DateLabels.h` 并做 native 单元测试，然后更新 MemoUI 渲染和 MemoClient LLM 提示词；Phase 3 给 RtcClock 加新的日期格式化方法，在 MemoUI 里画记事本 logo 并重排 Header。每个 Phase 完成后产出一个可编译的固件。

**Tech Stack:** PlatformIO (espressif32 + native), ESP32-S3 (seeed_xiao_esp32s3), Seeed_GFX / TFT_eSPI, PCF8563 RTC, Unity (native unit test), Groq LLaMA 3.3 70B。

---

## 文件变更概览

| 操作 | 文件 |
|---|---|
| 创建 | `platformio.ini` |
| 创建 | `src/` （目录，存放所有源码） |
| 移动 | 所有 `.cpp` / `.h` / `.ino` → `src/`，`.ino` 重命名为 `main.cpp` |
| 创建 | `src/secrets.h` （不入库） |
| 创建 | `include/secrets.example.h` （占位符，入库） |
| 创建 | `src/DateLabels.h` （新增，纯 C++ 日期逻辑） |
| 创建 | `test/test_date_labels/test_main.cpp` （native 单元测试） |
| 修改 | `src/main.cpp` （引用 secrets 宏） |
| 修改 | `src/MemoUI.cpp` （formatDueLabel / drawCard / drawTodoList / drawNotebookLogo） |
| 修改 | `src/MemoUI.h` （新增私有方法声明） |
| 修改 | `src/MemoClient.cpp` （LLM 提示词缩窄 due_label 语义） |
| 修改 | `src/RtcClock.cpp` （新增 nowLongDateLabel） |
| 修改 | `src/RtcClock.h` （声明 nowLongDateLabel） |

---

## Task 1：PlatformIO 项目骨架

**Files:**
- Create: `platformio.ini`
- Create: `src/` 目录，移入所有源文件
- Create: `include/secrets.example.h`

- [ ] **Step 1: 创建 src/ 目录并移入所有源文件**

```bash
mkdir -p src include test/test_date_labels
mv AudioCapture.cpp AudioCapture.h JsonUtil.h \
   MemoClient.cpp MemoClient.h MemoStore.cpp MemoStore.h \
   MemoUI.cpp MemoUI.h RtcClock.cpp RtcClock.h \
   SpeechClient.cpp SpeechClient.h TouchInput.cpp TouchInput.h \
   TouchMapper.h VoiceMemoApp.cpp VoiceMemoApp.h driver.h src/
mv VoiceMemoReminder.ino src/main.cpp
```

- [ ] **Step 2: 在 main.cpp 顶部补一行显式 include**

在 `src/main.cpp` 第 1 行（`#include "VoiceMemoApp.h"` 之前）插入：

```cpp
#include <Arduino.h>
```

> Arduino IDE 会自动注入 `Arduino.h`；PlatformIO 的 `.cpp` 文件需要显式写明。

- [ ] **Step 3: 写入 platformio.ini**

```ini
[env:reterminal_e1003]
platform = espressif32
board = seeed_xiao_esp32s3
framework = arduino
upload_speed = 115200
monitor_speed = 115200
board_build.arduino.memory_type = qio_opi
build_flags = -D BOARD_HAS_PSRAM
lib_deps = https://github.com/Seeed-Studio/Seeed_GFX

[env:native]
platform = native
build_flags =
    -std=c++11
    -I src
```

- [ ] **Step 4: 创建 include/secrets.example.h（占位符，入库）**

```cpp
// Copy this file to src/secrets.h and fill in your own values.
// src/secrets.h is excluded from version control via .gitignore.
#define VM_WIFI_SSID     "your_wifi_ssid"
#define VM_WIFI_PASSWORD "your_wifi_password"
#define VM_GROQ_API_KEY  "your_groq_api_key"
```

- [ ] **Step 5: 创建 src/secrets.h（真实密钥，不入库）**

```cpp
#define VM_WIFI_SSID     "citric_2.4G"
#define VM_WIFI_PASSWORD "<你的真实密码>"
#define VM_GROQ_API_KEY  "<换一个新的 Groq key，旧 key 已暴露请先吊销>"
```

> 填入真实值。此文件已被 .gitignore 排除，永不提交。

- [ ] **Step 6: 确认 .gitignore 中包含 src/secrets.h**

```bash
grep "secrets" .gitignore
```

Expected: 输出中包含 `src/secrets.h`（.gitignore 已在前一次提交中创建，该行已存在）。

- [ ] **Step 7: 在 src/main.cpp 中引用 secrets 宏**

找到 `src/main.cpp` 中的 `kConfig` 初始化块（约第 26–56 行），在文件最顶部（`#include <Arduino.h>` 之后，`#include "VoiceMemoApp.h"` 之前）加入：

```cpp
#include "secrets.h"
```

然后将 `kConfig` 中的硬编码字符串替换为宏：

```cpp
static const VoiceMemoConfig kConfig = {
  .wifiSsid     = VM_WIFI_SSID,
  .wifiPassword = VM_WIFI_PASSWORD,

  .speech = {
    .provider = VM_SPEECH_OPENAI_COMPATIBLE,
    .url      = "https://api.groq.com/openai/v1/audio/transcriptions",
    .apiKey   = VM_GROQ_API_KEY,
    .model    = "whisper-large-v3-turbo",
    .language = "",
  },

  .memo = {
    .provider = VM_MEMO_OPENAI_COMPATIBLE,
    .url      = "https://api.groq.com/openai/v1/chat/completions",
    .apiKey   = VM_GROQ_API_KEY,
    .model    = "llama-3.3-70b-versatile",
  },

  .audio = {
    .sampleRate       = 16000,
    .maxRecordSeconds = 20,
  },

  .httpTimeoutMs = 45000,
};
```

- [ ] **Step 8: 尝试编译目标环境（验证库定位）**

```bash
pio run -e reterminal_e1003
```

Expected: 编译流程正常启动，下载 Seeed_GFX 库，最终输出 `SUCCESS`。

如果编译报错 `driver.h: No such file or directory`（Seeed_GFX 找不到 src/driver.h），执行备选方案：

```bash
# 把 Seeed_GFX 放到 lib/，让 PlatformIO 从项目内查找
mkdir -p lib
# 将 .pio/libdeps/reterminal_e1003/Seeed_GFX 复制到 lib/Seeed_GFX
cp -r .pio/libdeps/reterminal_e1003/Seeed_GFX lib/
# 然后去掉 platformio.ini 中的 lib_deps 行（库已在 lib/ 下）
```

再次运行 `pio run -e reterminal_e1003`，应输出 `SUCCESS`。

- [ ] **Step 9: 暂存并提交——只提交不含密钥的文件**

```bash
git add platformio.ini include/secrets.example.h src/ -v
git status
```

确认 `src/secrets.h` **不**在暂存区（它在 .gitignore 里）。若意外出现，运行 `git restore --staged src/secrets.h`。

```bash
git commit -m "feat: migrate to PlatformIO and externalize secrets"
```

---

## Task 2：DateLabels.h 纯逻辑单元（TDD）

**Files:**
- Create: `src/DateLabels.h`
- Create: `test/test_date_labels/test_main.cpp`

- [ ] **Step 1: 先写失败的测试**

创建 `test/test_date_labels/test_main.cpp`：

```cpp
#include <unity.h>
#include <time.h>
#include "DateLabels.h"  // 尚未创建 -> 编译失败

static time_t makeEpoch(int year, int mon, int day, int hour, int min)
{
    struct tm t = {};
    t.tm_year  = year - 1900;
    t.tm_mon   = mon - 1;
    t.tm_mday  = day;
    t.tm_hour  = hour;
    t.tm_min   = min;
    t.tm_isdst = -1;
    return mktime(&t);
}

void test_same_day_is_zero() {
    time_t now = makeEpoch(2026, 5, 30, 14, 0);
    time_t due = makeEpoch(2026, 5, 30, 20, 0);
    TEST_ASSERT_EQUAL_INT(0, vmDayDistance(now, due));
}

void test_tomorrow_is_one() {
    time_t now = makeEpoch(2026, 5, 30, 14, 0);
    time_t due = makeEpoch(2026, 5, 31,  9, 0);
    TEST_ASSERT_EQUAL_INT(1, vmDayDistance(now, due));
}

void test_day_after_is_two() {
    time_t now = makeEpoch(2026, 5, 30, 14, 0);
    time_t due = makeEpoch(2026,  6,  1,  9, 0);
    TEST_ASSERT_EQUAL_INT(2, vmDayDistance(now, due));
}

void test_cross_midnight() {
    time_t now = makeEpoch(2026, 5, 30, 23, 59);
    time_t due = makeEpoch(2026, 5, 31,  0,  1);
    TEST_ASSERT_EQUAL_INT(1, vmDayDistance(now, due));
}

void test_label_today() {
    TEST_ASSERT_EQUAL_STRING("Today",     vmDateChipLabel(0, 5));
}

void test_label_tomorrow() {
    TEST_ASSERT_EQUAL_STRING("Tomorrow",  vmDateChipLabel(1, 6));
}

void test_label_day_after() {
    TEST_ASSERT_EQUAL_STRING("Day after", vmDateChipLabel(2, 0));
}

void test_label_weekday_wed() {
    TEST_ASSERT_EQUAL_STRING("Wed",       vmDateChipLabel(3, 3));
}

void test_label_null_for_seven_plus() {
    TEST_ASSERT_NULL(vmDateChipLabel(7,  1));
    TEST_ASSERT_NULL(vmDateChipLabel(14, 2));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_same_day_is_zero);
    RUN_TEST(test_tomorrow_is_one);
    RUN_TEST(test_day_after_is_two);
    RUN_TEST(test_cross_midnight);
    RUN_TEST(test_label_today);
    RUN_TEST(test_label_tomorrow);
    RUN_TEST(test_label_day_after);
    RUN_TEST(test_label_weekday_wed);
    RUN_TEST(test_label_null_for_seven_plus);
    return UNITY_END();
}
```

- [ ] **Step 2: 运行测试，确认编译失败（red）**

```bash
pio test -e native --filter test_date_labels
```

Expected: 编译失败，`DateLabels.h: No such file or directory`。

- [ ] **Step 3: 创建 src/DateLabels.h**

```cpp
#pragma once
#include <time.h>

// Days from the calendar-day of `from` to the calendar-day of `to`
// (both floored to local midnight). Positive = to is in the future.
inline int vmDayDistance(time_t from, time_t to)
{
    struct tm ta = {}, tb = {};
    localtime_r(&from, &ta);
    localtime_r(&to,   &tb);
    ta.tm_hour = ta.tm_min = ta.tm_sec = 0;
    tb.tm_hour = tb.tm_min = tb.tm_sec = 0;
    const time_t da = mktime(&ta);
    const time_t db = mktime(&tb);
    return static_cast<int>((db - da) / 86400);
}

// Short date-chip label for a future event.
// `days`  = vmDayDistance(now, due)
// `wday`  = tm_wday of the due datetime (0=Sun…6=Sat)
// Returns a string literal; returns nullptr for days >= 7 (caller formats "Mon DD").
inline const char* vmDateChipLabel(int days, int wday)
{
    static const char* kWeekdays[] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };
    if (days == 0) return "Today";
    if (days == 1) return "Tomorrow";
    if (days == 2) return "Day after";
    if (days >= 3 && days <= 6)
        return kWeekdays[(wday >= 0 && wday < 7) ? wday : 0];
    return nullptr;
}
```

- [ ] **Step 4: 运行测试，确认全部通过（green）**

```bash
pio test -e native --filter test_date_labels
```

Expected:
```
test/test_date_labels/test_main.cpp:XX:test_same_day_is_zero PASSED
...
-----------------------
9 Tests 0 Failures 0 Ignored
OK
```

- [ ] **Step 5: 提交**

```bash
git add src/DateLabels.h test/test_date_labels/test_main.cpp
git commit -m "feat: add DateLabels.h with unit tests (Today/Tomorrow/Day after/weekday)"
```

---

## Task 3：修复 MemoUI 渲染（使用 DateLabels.h，拆分日期/时段）

**Files:**
- Modify: `src/MemoUI.cpp`

- [ ] **Step 1: 在 MemoUI.cpp 顶部添加 DateLabels.h 的 include**

在 `src/MemoUI.cpp` 现有 `#include "MemoUI.h"` 后面加一行：

```cpp
#include "DateLabels.h"
```

- [ ] **Step 2: 删除 MemoUI.cpp 匿名命名空间中的旧 dayDistance / sameLocalDay**

找到并**删除** `src/MemoUI.cpp` 中以下整个 anonymous namespace 的两个函数（约第 45–68 行）：

```cpp
// 删除这两个函数：
bool sameLocalDay(time_t a, time_t b) { ... }
int dayDistance(time_t a, time_t b) { ... }
```

`kWeekdayShort[]` 和 `kMonthShort[]` 保留（仍用于 `formatDueLabel` 的"月 日"分支）。

- [ ] **Step 3: 更新 formatDueLabel 使用 DateLabels.h 的函数**

将 `src/MemoUI.cpp` 的 `formatDueLabel` 函数中从 `const int days = ...` 到 `return outDateChip + " " + outTimeBig;` 的末尾段（约第 119–133 行）整体替换为：

```cpp
  struct tm t = {};
  localtime_r(&entry.dueEpoch, &t);

  char timeBuf[8];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", t.tm_hour, t.tm_min);
  outTimeBig = timeBuf;

  const int days = vmDayDistance(nowEpoch, entry.dueEpoch);
  const char* chipLabel = vmDateChipLabel(days, t.tm_wday);
  if (chipLabel) {
    outDateChip = chipLabel;
  } else {
    char d[12];
    snprintf(d, sizeof(d), "%s %02d",
             kMonthShort[(t.tm_mon >= 0 && t.tm_mon < 12) ? t.tm_mon : 0],
             t.tm_mday);
    outDateChip = d;
  }
  return outDateChip + " " + outTimeBig;
```

- [ ] **Step 4: 更新 drawCard，删除 fuzzyLabel-as-big-label 分支**

找到 `drawCard` 中约第 391–421 行的 `if (entry.fuzzyLabel.length() > 0) { ... } else { ... }` 块，整体替换为：

```cpp
  {
    String dateChip, timeBig;
    bool   over = false;
    formatDueLabel(nowEpoch, entry, dateChip, timeBig, over);

    // If a time-of-day label is set (AM/PM/Noon/Eve), use it instead of HH:MM.
    if (entry.fuzzyLabel.length() > 0) {
      timeBig = entry.fuzzyLabel;
    }

    // Date chip (top right).
    display_.setTextSize(3);
    const int chipPad = 18;
    const int chipH   = 38;
    const int chipW   = display_.textWidth(dateChip) + chipPad * 2;
    const int chipX   = x + w - rightPad - chipW;
    const int chipY   = y + 18;
    const uint16_t chipFill = overdue ? kUiCardDark
                               : (entry.done ? kUiMuted : kUiBadge);
    display_.fillRoundRect(chipX, chipY, chipW, chipH, 8, chipFill);
    display_.setTextDatum(MC_DATUM);
    display_.setTextColor(kUiTextInv, chipFill, true);
    display_.drawString(dateChip, chipX + chipW / 2, chipY + chipH / 2 - 1);

    // Big time / time-of-day label (bottom right).
    display_.setTextSize(6);
    display_.setTextColor(fg, fill, true);
    display_.setTextDatum(TR_DATUM);
    display_.drawString(timeBig, x + w - rightPad, y + h - 22 - 48);
  }
```

- [ ] **Step 5: 编译验证（不能真机，但可排除语法错误）**

```bash
pio run -e reterminal_e1003
```

Expected: `SUCCESS`，无编译报错。

- [ ] **Step 6: 提交**

```bash
git add src/MemoUI.cpp
git commit -m "fix: split date/time labels — Day after added, fuzzyLabel no longer overrides date chip"
```

---

## Task 4：修复 MemoClient LLM 提示词

**Files:**
- Modify: `src/MemoClient.cpp`

- [ ] **Step 1: 替换 summarizeOpenAICompatible 中的 system 字符串构建**

在 `src/MemoClient.cpp` 的 `summarizeOpenAICompatible` 函数中，找到 `String system;` 到 `system += "Output: ...苹果...";`（约第 163–209 行），整段替换为：

```cpp
  String system;
  system.reserve(2400);
  system += "You are a memo extraction engine for an embedded reminder ";
  system += "device. The user transcript may be Chinese, English, or mixed.\\n\\n";
  system += "CURRENT LOCAL TIME: ";
  system += nowStr;
  system += " (";
  system += weekday;
  system += ").\\n\\n";
  system += "Return ONLY this JSON object. No prose. No markdown fences.\\n";
  system += "{\\\"memo\\\":\\\"<short English reminder>\\\",\\\"due\\\":\\\"YYYY-MM-DD HH:MM\\\",\\\"due_label\\\":\\\"<time-of-day label or empty>\\\"}\\n\\n";
  system += "FIELD RULES:\\n";
  system += "1) memo: one concise English reminder sentence. Remove filler. ";
  system += "Do not start with 'Remember to'.\\n";
  system += "2) due: ALWAYS provide an absolute local datetime. Best-guess against CURRENT LOCAL TIME.\\n";
  system += "3) due_label: a time-of-day label. Set ONLY when the user described a time ";
  system += "of day WITHOUT an exact clock number.\\n";
  system += "   Allowed values: AM  PM  Noon  Eve\\n";
  system += "   Set due_label = \\\"\\\" (empty string) when:\\n";
  system += "     - The user said an exact hour (e.g. 3, 8, 14:30 / ";
  system += "\\u4e09\\u70b9, \\u516b\\u70b9 / 09:30), with or without a time-part modifier.\\n";
  system += "     - The user mentioned NO time at all (device shows HH:MM from due).\\n";
  system += "   Mapping when modifier only, no digit:\\n";
  system += "     morning/\\u65e9\\u4e0a/\\u4e0a\\u5348 -> AM\\n";
  system += "     afternoon/\\u4e0b\\u5348 -> PM\\n";
  system += "     noon/\\u4e2d\\u5348 -> Noon\\n";
  system += "     evening/night/\\u665a\\u4e0a/tonight -> Eve\\n";
  system += "   NOTE: which DAY is determined by the device from the `due` field. ";
  system += "NEVER put Today/Tomorrow/This wk/Wknd/Soon/Some day in due_label.\\n\\n";
  system += "EXAMPLES:\\n";
  system += "Input: \\\"\\u63d0\\u9192\\u6211\\u4eca\\u5929\\u665a\\u4e0a\\u53bb\\u6d17\\u6fa1\\\"\\n";
  system += "Output: {\\\"memo\\\":\\\"Take a bath\\\",\\\"due\\\":\\\"2026-05-30 21:00\\\",\\\"due_label\\\":\\\"Eve\\\"}\\n";
  system += "Input: \\\"\\u665a\\u4e0a8\\u70b9\\u53eb\\u6211\\u5403\\u996d\\\"\\n";
  system += "Output: {\\\"memo\\\":\\\"Dinner\\\",\\\"due\\\":\\\"2026-05-30 20:00\\\",\\\"due_label\\\":\\\"\\\"}\\n";
  system += "Input: \\\"\\u660e\\u5929\\u4e0a\\u5348\\u5f00\\u4f1a\\\"\\n";
  system += "Output: {\\\"memo\\\":\\\"Morning meeting\\\",\\\"due\\\":\\\"2026-05-31 09:00\\\",\\\"due_label\\\":\\\"AM\\\"}\\n";
  system += "Input: \\\"\\u4e0b\\u53483\\u70b9\\u53d6\\u5feb\\u9012\\\"\\n";
  system += "Output: {\\\"memo\\\":\\\"Pick up package\\\",\\\"due\\\":\\\"2026-05-30 15:00\\\",\\\"due_label\\\":\\\"\\\"}\\n";
  system += "Input: \\\"\\u660e\\u5929\\u65e9\\u4e0a\\u4e03\\u70b9\\u6709\\u4e2a\\u4f1a\\u63d0\\u9192\\u6211\\\"\\n";
  system += "Output: {\\\"memo\\\":\\\"Morning meeting\\\",\\\"due\\\":\\\"2026-05-31 07:00\\\",\\\"due_label\\\":\\\"\\\"}\\n";
  system += "Input: \\\"tonight watch the football game\\\"\\n";
  system += "Output: {\\\"memo\\\":\\\"Watch football game\\\",\\\"due\\\":\\\"2026-05-30 20:00\\\",\\\"due_label\\\":\\\"Eve\\\"}\\n";
  system += "Input: \\\"\\u4e70\\u70b9\\u82f9\\u679c\\\"\\n";
  system += "Output: {\\\"memo\\\":\\\"Buy apples\\\",\\\"due\\\":\\\"2026-05-30 19:00\\\",\\\"due_label\\\":\\\"\\\"}";
```

- [ ] **Step 2: 编译验证**

```bash
pio run -e reterminal_e1003
```

Expected: `SUCCESS`。

- [ ] **Step 3: 提交**

```bash
git add src/MemoClient.cpp
git commit -m "fix: narrow due_label to time-of-day only (AM/PM/Noon/Eve), remove week labels"
```

---

## Task 5：RtcClock 新增 nowLongDateLabel

**Files:**
- Modify: `src/RtcClock.h`
- Modify: `src/RtcClock.cpp`

- [ ] **Step 1: 在 RtcClock.h 中声明新方法**

在 `src/RtcClock.h` 的 `nowDateLabel()` 声明之后，加入：

```cpp
  // Returns a long-form date string for the header, e.g. "Friday, May 30".
  String nowLongDateLabel();
```

- [ ] **Step 2: 在 RtcClock.cpp 中实现**

在 `src/RtcClock.cpp` 的 `nowDateLabel()` 实现之后，追加：

```cpp
String RtcClock::nowLongDateLabel()
{
  VoiceMemoRtcTime rt = {};
  if (!available_ || !readTime(rt) || !rt.voltageOK) return "---";

  static const char* kWeekdayFull[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"
  };
  static const char* kMonthShort[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };

  const int wd = (rt.weekday >= 0 && rt.weekday < 7) ? rt.weekday : 0;
  const int mo = (rt.month  >= 1 && rt.month  <= 12) ? rt.month - 1 : 0;

  char buf[32];
  snprintf(buf, sizeof(buf), "%s, %s %d",
           kWeekdayFull[wd], kMonthShort[mo], rt.day);
  return String(buf);
}
```

- [ ] **Step 3: 编译验证**

```bash
pio run -e reterminal_e1003
```

Expected: `SUCCESS`。

- [ ] **Step 4: 提交**

```bash
git add src/RtcClock.h src/RtcClock.cpp
git commit -m "feat: add RtcClock::nowLongDateLabel for header redesign"
```

---

## Task 6：Header 改版（记事本 logo + 居中时间 + 日期星期）

**Files:**
- Modify: `src/MemoUI.h`
- Modify: `src/MemoUI.cpp`

- [ ] **Step 1: 在 MemoUI.h 中声明 drawNotebookLogo**

在 `src/MemoUI.h` private 区块的最后（`drawWrapped` 声明之后），加入：

```cpp
  void drawNotebookLogo(int x, int y, int size, uint16_t color);
```

- [ ] **Step 2: 在 MemoUI.cpp 中实现 drawNotebookLogo**

在 `src/MemoUI.cpp` 的 `drawCheckbox` 函数实现之前（约第 324 行），插入新函数：

```cpp
void MemoUI::drawNotebookLogo(int x, int y, int size, uint16_t color)
{
  // Spiral rings: a row of small circles along the top edge.
  const int ringR  = size / 8;
  const int ringY  = y + ringR;
  const int gaps   = 4;
  const int step   = size / gaps;
  for (int i = 0; i < gaps; i++) {
    display_.drawCircle(x + step / 2 + i * step, ringY, ringR, color);
  }

  // Notebook body: rounded rectangle below the rings.
  const int bodyY = y + ringR * 2 + 3;
  const int bodyH = size - ringR * 2 - 3;
  display_.drawRoundRect(x, bodyY, size, bodyH, 4, color);

  // Three ruled lines inside the body.
  const int lineX1 = x + size / 6;
  const int lineX2 = x + size * 5 / 6;
  display_.drawFastHLine(lineX1, bodyY + bodyH / 4,           lineX2 - lineX1,           color);
  display_.drawFastHLine(lineX1, bodyY + bodyH / 2,           lineX2 - lineX1,           color);
  display_.drawFastHLine(lineX1, bodyY + bodyH * 3 / 4,       (lineX2 - lineX1) * 2 / 3, color);
}
```

- [ ] **Step 3: 替换 drawTodoList GRAY16 分支的 Header 区块**

在 `src/MemoUI.cpp` 的 `drawTodoList` 函数内，找到 `#if VM_SCREEN_MODE == VM_SCREEN_GRAY16` 之后、`display_.drawFastHLine(margin, headerH, ...)` 之前的所有 header 绘制代码（约第 455–488 行）。整段替换为：

```cpp
  // ---- Header geometry ----
  const int logoSize = 48;
  const int logoY    = 20;
  const int timeY    = logoY + logoSize + 8;       // top of big time text
  const int dateY    = timeY + 8 * 8 + 8;          // top of date label (textSize 8 ≈ 64px)
  const int headerH  = dateY + 4 * 8 + 16;         // textSize 4 ≈ 32px, +margin

  display_.fillSprite(kUiBg);

  // ---- Logo + "Notes" (left) ----
  drawNotebookLogo(margin, logoY, logoSize, kUiText);
  display_.setTextSize(3);
  display_.setTextColor(kUiText, kUiBg, true);
  display_.setTextDatum(ML_DATUM);
  display_.drawString("Notes", margin + logoSize + 12, logoY + logoSize / 2);

  // ---- Status badge (right, vertically aligned with logo) ----
  {
    display_.setTextSize(3);
    const int padX = 18;
    const int bh   = 38;
    const int bw   = display_.textWidth(status) + padX * 2;
    const int bx   = w - margin - bw;
    const int by   = logoY + (logoSize - bh) / 2;
    display_.fillRoundRect(bx, by, bw, bh, 8, kUiBadge);
    display_.setTextDatum(MC_DATUM);
    display_.setTextColor(kUiTextInv, kUiBadge, true);
    display_.drawString(status, bx + bw / 2, by + bh / 2 - 1);
  }

  // ---- Big time (centered) ----
  display_.setTextSize(8);
  display_.setTextColor(kUiText, kUiBg, true);
  display_.setTextDatum(TC_DATUM);
  display_.drawString(rtc.nowTimeLabel(), w / 2, timeY);

  // ---- Long date label (centered, below time) ----
  display_.setTextSize(4);
  display_.setTextColor(kUiText, kUiBg, true);
  display_.setTextDatum(TC_DATUM);
  display_.drawString(rtc.nowLongDateLabel(), w / 2, dateY);
```

- [ ] **Step 4: 编译验证**

```bash
pio run -e reterminal_e1003
```

Expected: `SUCCESS`。

- [ ] **Step 5: 提交**

```bash
git add src/MemoUI.h src/MemoUI.cpp
git commit -m "feat: redesign E1003 header — notebook logo, centered time and date"
```

---

## Task 7：更新 README

**Files:**
- Modify: `README.md`

- [ ] **Step 1: 替换 "Arduino setup" 小节**

将 README.md 中的 `## Arduino setup` 小节（第 36–40 行）替换为：

```markdown
## PlatformIO setup

1. Install [PlatformIO](https://platformio.org/).
2. Copy `include/secrets.example.h` to `src/secrets.h` and fill in your WiFi
   credentials and Groq API key.
3. Edit `src/driver.h` and verify `VOICE_MEMO_DEVICE_E1003` is selected.
4. Connect the reTerminal E1003 via USB.
5. Build and upload: `pio run -e reterminal_e1003 --target upload`
6. Monitor serial: `pio device monitor`

> **Note:** Upload speed is set to 115200 in `platformio.ini`; higher speeds
> may fail on the E1003.
```

- [ ] **Step 2: 更新 file map 表格**

将 `VoiceMemoReminder.ino` 一行改为：

```
| `src/main.cpp`   | PlatformIO entry point. Only contains the user config and `setup() / loop()`. |
```

在表格末尾加一行：

```
| `src/DateLabels.h`     | Header-only pure C++ helpers: `vmDayDistance()` and `vmDateChipLabel()`. |
```

- [ ] **Step 3: 提交**

```bash
git add README.md
git commit -m "docs: update README for PlatformIO setup and new file map"
```

---

## 边缘情况 / 注意事项

| 情况 | 处理方式 |
|---|---|
| Seeed_GFX 找不到 `driver.h` | 见 Task 1 Step 8 备选方案（复制到 lib/） |
| `nowLongDateLabel` 在 RTC 不可用时 | 返回 `"---"` |
| `Day after` / `PM` 等较宽的字符串在 chip 里 | chip 宽度动态计算（`textWidth(dateChip) + chipPad*2`），无需硬编码 |
| LLM 仍输出旧标签（`This wk` 等） | `fuzzyLabel` 长度 ≤ 12 截断保留；渲染时放大时间槽，不会破坏布局 |
| 旧 NVS 数据（含 `This wk` 的 fuzzyLabel） | 保持显示旧标签内容（该字段承载时段词，老数据不影响日期格逻辑）；用户下次录音后会产生新格式数据 |
| headerH 动态计算超出卡片区域 | 以 `display_.height()` 动态减去 headerH + footerH 得出 listAvail，无写死风险 |
