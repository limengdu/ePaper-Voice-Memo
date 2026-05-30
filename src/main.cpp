/*
 * VoiceMemoReminder -- hold KEY0 to record, release to create a memo.
 *
 * This file is the PlatformIO entry point and the ONLY file you have to edit
 * to make the device work on your network with your provider keys.
 * Everything else lives in dedicated modules:
 *
 *   VoiceMemoApp.*    application orchestrator (button, lifecycle)
 *   AudioCapture.*    PDM microphone + WAV buffer
 *   RtcClock.*        PCF8563 real-time clock
 *   MemoStore.*       reminder list + NVS persistence
 *   SpeechClient.*    speech-to-text upload
 *   MemoClient.*      LLM rewrite into {memo, due} JSON
 *   MemoUI.*          all e-paper drawing
 *   JsonUtil.h        tiny shared JSON helpers
 *   driver.h          device model + screen capability selector
 *
 * To add a new STT provider, edit SpeechClient.cpp and add one case.
 * To change the LLM prompt, edit MemoClient.cpp.
 * To redesign the screen, edit MemoUI.cpp.
 * Reminders persist across power cycles via NVS automatically.
 */

#include <Arduino.h>
#include "secrets.h"
#include "VoiceMemoApp.h"

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

VoiceMemoApp app(kConfig);

void setup()
{
  app.begin();
}

void loop()
{
  app.loop();
}
