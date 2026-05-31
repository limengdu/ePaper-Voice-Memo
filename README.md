# VoiceMemoReminder

[简体中文](README.zh-CN.md) · English

Hold KEY0 to record a voice note, release KEY0 to upload the WAV to a cloud
speech-to-text API, then let a small LLM rewrite the transcript into one short
English reminder with an event date and time. The reminder is drawn on the
e-paper screen and saved to non-volatile storage, so the list is restored
automatically after a power cycle.

Supported devices:

- reTerminal E1001: 4-level gray UI
- reTerminal E1002: 6-color UI
- reTerminal E1003: 16-level gray UI (card-style reminder list)
- reTerminal E1004: not included because it has no onboard microphone

## What is new in this version

- Reminders persist across power cycles (NVS, namespace `vmm`, key `items`).
- The time printed on the right side of every card is the EVENT time parsed
  from your spoken words, not the moment you pressed KEY0.
- When you spoke about time vaguely ("tonight", "tomorrow morning"), the
  right side shows a short English label ("Tonight", "Tmrw AM") instead of
  a fake clock time. Precise times still render as a date chip + HH:MM.
- Cards are sorted so the soonest upcoming reminder is on top, overdue
  ones come next with a dark "Overdue" chip, and finished ones drop to
  the bottom.
- Tap the checkbox on the left side of a card to mark it done; tap again
  to undo. Completed items get pushed to the bottom. When the list is full
  and a new reminder arrives, finished items are evicted before active ones.
- The buzzer on GPIO45 plays a short beep when recording starts so you can
  start speaking without looking at the screen.
- The intermediate "Processing" e-paper refresh was removed -- the screen
  now refreshes ONCE at the end of each recording instead of twice, saving
  ~1.5-2 seconds per memo.

## PlatformIO setup

1. Install [PlatformIO](https://platformio.org/).
2. Copy `include/secrets.example.h` to `src/secrets.h` and fill in your WiFi
   credentials and Groq API key.
3. Connect your reTerminal via USB and build + upload for the matching device:
   - reTerminal E1001: `pio run -e reterminal_e1001 --target upload`
   - reTerminal E1002: `pio run -e reterminal_e1002 --target upload`
   - reTerminal E1003: `pio run -e reterminal_e1003 --target upload`
4. Monitor serial: `pio device monitor`

The device target is selected by the build environment (the `-D
VOICE_MEMO_DEVICE_*` flag in `platformio.ini`), so no source edits are needed
to switch devices.

> **Note:** Upload speed is set to 115200 in `platformio.ini`; higher speeds
> may fail on the E1003.

## Chinese (Simplified) build

The E1003 has an extra environment, `reterminal_e1003_zh`, that renders the
entire UI in Simplified Chinese — including the reminder text, which the LLM is
prompted to return in Chinese regardless of the spoken language. Language is a
build-time choice (the `-D VM_UI_LANG_ZH` flag): the English and Chinese
firmwares are separate builds, never switched at runtime.

The Chinese build renders text through OpenFontRender. The font is embedded in
the firmware: a pre-build hook (`scripts/gen_font.py`) converts
`data/test_ZH.ttf` into `src/FontZH.h` automatically, so it flashes in a single
step like the other devices (no SPIFFS / `uploadfs`):

```sh
pio run -e reterminal_e1003_zh -t upload
```

Notes:

- The font `data/test_ZH.ttf` is a subset Source-Han font (~833 KB), embedded
  into the binary as `src/FontZH.h` (git-ignored, regenerated on build).
  Characters outside the subset render blank. To use a different font, replace
  `data/test_ZH.ttf` and rebuild — the header regenerates automatically.
- Glyph size is controlled by `VM_ZH_PX_PER_UNIT` in `TextRenderer.cpp`; tune it
  on hardware so Chinese text matches the former bitmap sizes.
- Switching a device between the English and Chinese firmware clears the saved
  reminder list on first boot (reminders are stored in one language).
- The English builds are unchanged and never link OpenFontRender.

## File map

The example is split into small files so each contributor only has to read
the module they care about:

| File | Purpose |
| --- | --- |
| `src/main.cpp`          | PlatformIO entry point. Only contains the user config and `setup() / loop()`. |
| `src/driver.h`          | Device model select + screen capability mapping. |
| `VoiceMemoApp.*`        | Top-level orchestrator. Owns one instance of every module. Handles the KEY0 state machine. |
| `AudioCapture.*`        | PDM microphone setup, PSRAM WAV buffer, WAV header. |
| `RtcClock.*`            | PCF8563 driver + time formatting helpers. |
| `MemoStore.*`           | Reminder list + NVS persistence + sort by event time. |
| `SpeechClient.*`        | Speech-to-text upload. Three providers (gateway / OpenAI-compatible / Deepgram) selectable via config. |
| `MemoClient.*`          | LLM rewrite into `{memo, due, due_label}` JSON, with rule-based fallback. |
| `MemoUI.*`              | All e-paper drawing (boot / recording / processing / status / reminder list) and checkbox hit-test. |
| `TouchInput.*`          | GT911 capacitive touch controller, returns click-edge events. |
| `TouchMapper.h`         | Header-only helpers for scaling raw touch coordinates to display pixels. |
| `JsonUtil.h`            | Header-only JSON helpers shared by the two HTTP clients. |
| `gateway/`              | Optional Python service used for offline testing. |
| `src/DateLabels.h`      | Header-only pure C++ helpers: `vmDayDistance()` and English/Chinese `vmDateChipLabel()`. |
| `src/UiLang.h`          | Header-only: compile-time language switch (`VM_LANG_ZH`) and the English/Chinese fixed-string table (`uiStr`). |
| `src/TextRenderer.*`    | Text rendering facade. English build draws with the Seeed_GFX bitmap font; the Chinese build (`VM_LANG_ZH`) draws with OpenFontRender + a SPIFFS TrueType font. |
| `scripts/gen_font.py`   | Pre-build hook (Chinese env): embeds `data/test_ZH.ttf` into `src/FontZH.h`. |

## Contributing

The intent is that **each change touches exactly one module**. The table below
shows where to look:

| Change | File to edit |
| --- | --- |
| Swap the buzzer beep for a different sound or add a "saved" beep | `VoiceMemoApp.cpp` (`beepStart()` and the end of `stopRecording()`) |
| Swap the touch controller for a different chip | `TouchInput.cpp` (keep the same public API) |
| Add a new STT provider (Azure, AssemblyAI, self-hosted Whisper) | `SpeechClient.cpp` — add a `transcribeXxx()` function and one case in `transcribe()` |
| Tweak the LLM prompt or the memo JSON schema | `MemoClient.cpp` (`summarizeOpenAICompatible`) |
| Swap NVS for LittleFS or SD card storage | `MemoStore.cpp` — only the private `load()` / `save()` methods |
| Change e-paper layout / add a new screen | `MemoUI.cpp` |
| Change how text is rendered, or add a UI language | `TextRenderer.*` and `UiLang.h` |
| Change sample rate, recording length, or encoding | `AudioCapture.cpp` (and the `audio` block in the .ino config) |
| Use a different RTC chip | `RtcClock.cpp` |
| Add a different button or wake mechanism | `VoiceMemoApp.cpp` (`pollButton`) |
| Change WiFi / API key / model name | `VoiceMemoReminder.ino` only |

## Recommended direct-cloud setup

The default config uses Groq's OpenAI-compatible transcription endpoint:

```cpp
.speech = {
  .provider = VM_SPEECH_OPENAI_COMPATIBLE,
  .url      = "https://api.groq.com/openai/v1/audio/transcriptions",
  .apiKey   = "YOUR_GROQ_API_KEY",
  .model    = "whisper-large-v3-turbo",
  .language = "",
},
```

Memo rewriter (also via Groq):

```cpp
.memo = {
  .provider = VM_MEMO_OPENAI_COMPATIBLE,
  .url      = "https://api.groq.com/openai/v1/chat/completions",
  .apiKey   = "YOUR_GROQ_API_KEY",
  .model    = "llama-3.1-8b-instant",
},
```

The chat model is given the current local clock and is asked to return strict
JSON of the form:

```json
{"memo":"Buy milk after work","due":"2026-05-30 18:00"}
```

`MemoClient` parses the JSON, converts `due` to a Unix epoch, and hands the
result to `MemoStore`, which is what makes the right-side date chip /
"Today / Tomorrow / Overdue" labels possible.

## Deepgram alternative

```cpp
.speech = {
  .provider = VM_SPEECH_DEEPGRAM,
  .url      = "https://api.deepgram.com/v1/listen?model=nova-3&smart_format=true",
  .apiKey   = "YOUR_DEEPGRAM_API_KEY",
  .model    = "nova-3",
  .language = "",
},
```

## Optional gateway debug mode

The Python gateway is still useful when you want to test the screen, button,
recording, and upload path without spending API credits:

```sh
python3 gateway/voice_memo_gateway.py --host 0.0.0.0 --port 8000
```

Then set the speech provider in the `.ino` to:

```cpp
.speech = {
  .provider = VM_SPEECH_GATEWAY,
  .url      = "http://YOUR_COMPUTER_IP:8000/api/voice-memo",
  .apiKey   = "",
  .model    = "",
  .language = "",
},
```

## Persistence details

Reminders are stored in NVS as a single binary blob:

```
uint8  version (= 2)
uint8  count
for each item:
  int64  dueEpoch
  uint8  flags         (bit0 = hasDue, bit1 = done)
  uint16 textLen
  uint8  text[textLen]
  uint16 fuzzyLen
  uint8  fuzzy[fuzzyLen]
```

A blob with a different version number is ignored (no migration code on
purpose — this is a developer example, and the first power cycle after the
firmware update simply starts with an empty list).

`MemoStore::clear()` is exposed for a future "factory reset" hook but is not
wired to any button by default — feel free to bind it to a long press of KEY0
in your fork.

## Notes

- Direct cloud mode stores an API key in firmware. That is acceptable for a
  developer example, but a product should issue short-lived tokens from a
  small backend or lock keys down with provider-side limits.
- Multilingual speech is handled in two stages: STT auto-detects the spoken
  language, then the memo model rewrites the transcript into English.
- HTTPS examples use `WiFiClientSecure::setInsecure()` to keep setup simple.
  Production firmware should pin a CA certificate or public key.
- The English build uses built-in Seeed_GFX bitmap fonts (best for ASCII). For
  on-screen Chinese, use the `reterminal_e1003_zh` build, which renders through
  OpenFontRender + a TrueType font in SPIFFS (see "Chinese (Simplified) build").
