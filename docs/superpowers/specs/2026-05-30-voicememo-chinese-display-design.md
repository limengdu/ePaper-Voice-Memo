# Chinese Display Support — Design

**Date:** 2026-05-30
**Status:** Draft for review

## Goal

Add full Chinese (Simplified) display support to the reminder device. Language
is selected at **build time**: the English firmware behaves exactly as today;
the Chinese firmware renders every on-screen string in Chinese — including the
reminder text produced by the speech pipeline, regardless of whether the user
spoke Chinese or English.

## Core Decisions

1. **Build-time language split.** A single macro `VM_UI_LANG_ZH` selects the
   language for the whole firmware. English and Chinese are two separate builds
   ("two firmwares"), never switched at runtime.

2. **Render backend follows the language — English path is untouched.**
   - English build: keeps the current Seeed_GFX built-in bitmap font
     (`display_.drawString` + `setTextSize`). Zero change to the rendering path,
     so the English firmware carries no new risk and does not link the TTF
     renderer at all.
   - Chinese build: renders through **OpenFontRender** (runtime TrueType
     rasterizer) using a `.ttf` stored in the on-device SPIFFS filesystem.

3. **Reuse the vendor demo's filesystem bridge.** OpenFontRender is not
   plug-and-play with ESP32 SPIFFS; it needs a small set of file hooks. The
   vendor demo (`Helloworld_ttf_loader_Demo`) already provides a working bridge
   (`spiffs_perset.h`). We adopt it as-is rather than re-deriving it.

4. **Switching language wipes saved reminders.** Because stored reminders are
   in one language, booting a firmware whose language differs from the language
   recorded in NVS clears the reminder store, so the device starts clean in the
   new language.

5. **Initial font: the demo's subset Source-Han `test_ZH.ttf` (833 KB).** The
   full Microsoft YaHei (21 MB) does not fit the flash budget and must be subset
   first; swapping fonts later is a one-file change in `data/` plus the load
   path.

## What Does NOT Change

The recording lifecycle, PDM microphone capture, WiFi, HTTP/STT/LLM transport,
touch handling, the KEY0 button state machine, battery/WiFi sensing, NVS blob
format for reminders, and the per-panel color palettes are all untouched. All
edits are confined to: text rendering, fixed UI wording, the LLM prompt, date
formatting, and one language-tag check in the store.

## Architecture

### New files

| File | Responsibility |
|---|---|
| `src/UiLang.h` / `.cpp` | Compile-time language constant `VM_LANG_ZH` (0/1) derived from `VM_UI_LANG_ZH`. A string table of fixed UI words with English and Chinese columns, exposed as `uiStr(UiStringId)`. |
| `src/TextRenderer.h` / `.cpp` | Rendering facade. One pair of calls — `drawText(...)` and `measureText(...)` — that the UI uses instead of touching the display font API directly. Internally `#if VM_LANG_ZH` routes to OpenFontRender, `#else` to the built-in font. Owns the `OpenFontRender` instance, font loading, and a `fontReady_` health flag. |
| `src/OfrSpiffs.h` | The vendor SPIFFS bridge for OpenFontRender (adapted from `spiffs_perset.h`). Defines the `OFR_f*` file hooks. Included in exactly one translation unit (`TextRenderer.cpp`) because it defines globals. Chinese build only. |
| `partitions_zh.csv` | Partition table with a SPIFFS region large enough for the font (~1.5 MB). Used by the Chinese build env. |
| `data/test_ZH.ttf` | The subset Chinese font, uploaded to SPIFFS via `uploadfs`. (Provided externally; not a source artifact.) |

### Modified files

| File | Change |
|---|---|
| `platformio.ini` | Add `OpenFontRender` to `lib_deps`; add a Chinese build env that sets `-D VM_UI_LANG_ZH`, the SPIFFS partition table, and `board_build.filesystem = spiffs`. |
| `src/MemoUI.h` / `.cpp` | Hold a `TextRenderer`; in `begin()` initialize it. Replace every `display_.drawString / textWidth / setTextDatum / setTextSize` text call with `renderer_.drawText / measureText`. Replace hard-coded English UI words with `uiStr(...)`. Drawing geometry (rects, icons, layout math) is unchanged. |
| `src/MemoClient.cpp` | In the Chinese build, instruct the model to return a Chinese `memo` and Chinese time-of-day labels (早上/下午/中午/晚上, none→NONE). JSON schema, parsing, and the `due` contract are unchanged. |
| `src/RtcClock.cpp` | Add Chinese formatting branches for `nowHeaderDateLabel()` / `nowLongDateLabel()` (e.g. `4月28日 周二`) and Chinese weekday names. |
| `src/DateLabels.h` | Provide English and Chinese variants of the chip label (today/tomorrow/day-after/weekday); a macro selects the default for the build. |
| `src/MemoStore.h` / `.cpp` | On `begin()`, compare a new NVS `lang` key against the firmware language; if different, `clear()` and rewrite the tag. |
| `src/VoiceMemoApp.cpp` | Replace fixed English status/hint strings with `uiStr(...)`. No control-flow change. |
| `README.md` | Document the two-step flash (font + firmware), the language flag, and font subsetting. |

## Key Interfaces

### TextRenderer

```cpp
enum class TextAlign {
  TopLeft, TopCenter, TopRight,
  MiddleLeft, MiddleCenter, MiddleRight,
  BottomLeft, BottomCenter, BottomRight
};

class TextRenderer {
 public:
  // English build: no-op success. Chinese build: SPIFFS.begin(), bind the
  // OpenFontRender drawer to the display, load the Chinese .ttf, set fontReady_.
  bool begin(EPaper& display);

  bool fontReady() const;

  // Draw `text` so that the chosen anchor lands at (x, y). `sizeUnit` keeps the
  // existing setTextSize() scale (1 unit ~= 8 px tall); the Chinese path maps it
  // to OpenFontRender pixels via a tunable constant. bg is used for the
  // anti-aliased fill so glyph edges blend on the grayscale panel.
  void drawText(const String& text, int x, int y, int sizeUnit,
                TextAlign align, uint16_t color, uint16_t bg);

  // Pixel width of `text` at `sizeUnit`, for layout that needs to measure.
  int measureText(const String& text, int sizeUnit);
};
```

- **Size mapping.** Chinese pixel height = `sizeUnit * VM_ZH_PX_PER_UNIT`, where
  `VM_ZH_PX_PER_UNIT` is a single tunable constant (starting value ~8, adjusted
  on hardware so Chinese glyphs visually match the former bitmap sizes).
- **Alignment.** The facade computes the top-left origin from the anchor using
  `measureText` and the pixel height, then positions the cursor. This avoids
  depending on OpenFontRender's alignment mode, whose behavior is reported to be
  fiddly.
- **OpenFontRender call shape** (to confirm against the installed header during
  implementation): `loadFont()` returns **non-zero on failure**; sequence is
  `setDrawer(static_cast<TFT_eSPI&>(display))`, `loadFont("/test_ZH.ttf")`,
  then per draw `setFontSize / setFontColor / setCursor / printf`.

### UiLang

```cpp
#if defined(VM_UI_LANG_ZH)
  #define VM_LANG_ZH 1
#else
  #define VM_LANG_ZH 0
#endif

enum class UiStringId {
  kAppName,        // Notes / 笔记
  kHintAdd,        // "Hold KEY0 to add. Tap a box to check off." / ...
  kHintTooShort,   // "Hold KEY0 for at least one second." / ...
  kHintNoWifi,     // "Reminder skipped because WiFi is unavailable." / ...
  kHintMaxLen,     // "Stopped at max length. ..." / ...
  kEmptyList,      // "Hold KEY0 and speak to add your first reminder." / ...
  kProcessing,     // Processing / 处理中
  kReminders,      // Reminders / 提醒
  kBootStarting,   // Starting... / 启动中...
  kBootWifi        // Connecting WiFi... / 连接 WiFi...
};

const char* uiStr(UiStringId id);   // returns the current build's language
```

### DateLabels (test-friendly: pure functions, macro selects default)

```cpp
const char* vmDateChipLabelEn(int days, int wday);  // Today/Tomorrow/Day after/<Weekday>/nullptr
const char* vmDateChipLabelZh(int days, int wday);  // 今天/明天/后天/<周X>/nullptr
#if VM_LANG_ZH
  #define vmDateChipLabel vmDateChipLabelZh
#else
  #define vmDateChipLabel vmDateChipLabelEn
#endif
```

The same English/Chinese-pair pattern applies to weekday names used by
`RtcClock`, so unit tests can assert both languages from one native build.

### MemoStore language reconciliation

A new NVS key `lang` (1 byte: 0=en, 1=zh) is written next to the reminder blob.
`begin()` reads it; if it is missing or differs from the firmware language, the
store calls the existing `clear()` and writes the current tag. Net effect:
flashing a different-language firmware starts with an empty list.

## Data Flow

- **Boot (Chinese build):** `MemoUI::begin` → `TextRenderer::begin`
  (`SPIFFS.begin`, bind drawer, `loadFont`, set `fontReady_`) →
  `MemoStore::begin` (reconcile language tag, may clear) → draw list in Chinese.
- **New reminder:** transcript → `MemoClient` (Chinese prompt) → Chinese `memo`
  + Chinese time-of-day label → store → `drawTodoList` renders via TextRenderer.
- **Language switch:** edit the build flag → recompile + reflash → on boot the
  store sees a changed `lang` tag → clears → device runs clean in the new
  language.

## Error Handling

- **Font not ready** (SPIFFS not mounted, file missing, `loadFont` failed):
  `TextRenderer` sets `fontReady_ = false`. `drawText` must degrade gracefully
  (skip glyph drawing / draw a neutral placeholder) and never call into the
  renderer with no font — OpenFontRender crashes on a missing font. A one-line
  boot diagnostic is logged on the debug UART.
- **Missing glyph** (subset font lacks a character): renders blank; acceptable.
- **Non-BMP characters** (e.g. emoji): not supported by the UTF-8 path;
  acceptable for reminder text.
- **LLM Chinese output through `jsonEscape`:** verify multi-byte UTF-8 passes
  through escaping and extraction intact (it operates on bytes, so it should,
  but this is explicitly checked).

## Build & Flash

- `lib_deps += https://github.com/takkaO/OpenFontRender.git`
- A Chinese env (e.g. `[env:reterminal_e1003_zh]`) extends the E1003 env and
  adds `-D VM_UI_LANG_ZH`, `board_build.filesystem = spiffs`, and
  `board_build.partitions = partitions_zh.csv`. English envs are unchanged.
- **Two-step flash for the Chinese build:** `pio run -e <zh> -t uploadfs`
  (writes the font to SPIFFS) followed by `pio run -e <zh> -t upload`.
- FreeType (FTL license) requires crediting FreeType; the demo's `showCredit()`
  satisfies this at boot.

## Font Plan

Start with the demo's `test_ZH.ttf` (subset Source Han, 833 KB) in `data/`.
Upgrading to a YaHei look is a later, isolated step: subset the 21 MB YaHei to
the common-character set (<1 MB) with font-min-cut, drop it into `data/`, and
point the load path at it.

## Testing

- **Native unit tests** (`pio test -e native`):
  - DateLabels: assert both `...En` and `...Zh` variants for days 0/1/2/3–6/≥7.
  - UiLang: `uiStr` returns the right column for each `UiStringId` (test both
    language builds, or test the underlying table directly).
  - Store language logic: factor the "should clear?" decision into a pure
    function (`bool vmShouldWipeForLanguage(int storedTag, int firmwareTag)`)
    so it is testable without NVS.
- **On-hardware (cannot be verified offline):** font renders on gray16; the
  `VM_ZH_PX_PER_UNIT` size mapping; render performance with a full card list;
  `setDrawer` writing into the sprite on a 16-level panel (the demo verified a
  monochrome panel, not E1003); OpenFontRender + Seeed_GFX compiling together
  under PlatformIO (the demo used the Arduino IDE).

## Open Risks

1. **Renderer/library compile fit under PlatformIO** — the demo is Arduino-IDE
   based; the OpenFontRender + Seeed_GFX combination must build in this project.
2. **`setDrawer` polymorphism into the sprite on gray16** — relies on the
   display's virtual `drawPixel`; demo proof was on a monochrome panel.
3. **Size mapping and performance** — both need on-device tuning.

## Edge Cases

- Empty reminder list in Chinese: localized empty-state line.
- Numbers and ASCII in the Chinese build (time `18:30`, battery `85%`, date
  digits) render with the Chinese font's Latin glyphs.
- A reminder stored before this feature (no `lang` tag) is treated as a
  mismatch and cleared on first boot of either language build.
