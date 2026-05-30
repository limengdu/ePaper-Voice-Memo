#include "VoiceMemoApp.h"

#include <WiFi.h>

VoiceMemoApp::VoiceMemoApp(const VoiceMemoConfig& config)
  : config_(config),
    audio_(),
    rtc_(),
    store_(),
    stt_(),
    memo_(),
    ui_(),
    touch_(),
    recording_(false),
    busy_(false),
    lastRawButton_(HIGH),
    stableButton_(HIGH),
    ledState_(false),
    debounceMs_(0),
    lastBlinkMs_(0)
{
}

void VoiceMemoApp::ledOn()  { digitalWrite(VM_LED_PIN, LOW); }
void VoiceMemoApp::ledOff() { digitalWrite(VM_LED_PIN, HIGH); }

void VoiceMemoApp::beepStart()
{
  // Short, bright beep at recording start. Audible feedback lets the user
  // start speaking without having to look at the screen.
  tone(kBuzzerPin, 1500, 80);
}

void VoiceMemoApp::setupPins()
{
  pinMode(VM_LED_PIN, OUTPUT);
  ledOff();
  pinMode(kKey0Pin, INPUT);
  pinMode(kBuzzerPin, OUTPUT);
  digitalWrite(kBuzzerPin, LOW);
}

bool VoiceMemoApp::ensureWiFi(uint32_t timeoutMs)
{
  if (WiFi.status() == WL_CONNECTED) return true;

  WiFi.mode(WIFI_STA);
  WiFi.begin(config_.wifiSsid, config_.wifiPassword);

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    delay(250);
    Serial1.print(".");
  }
  Serial1.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial1.printf("[wifi] connected: %s\n", WiFi.localIP().toString().c_str());
    return true;
  }
  Serial1.println("[wifi] connection failed");
  return false;
}

void VoiceMemoApp::begin()
{
  // Arduino startup order:
  //   1. Start the debug UART (Serial1 on GPIO43/44).
  //   2. Configure pins (LED + KEY0 + buzzer).
  //   3. Initialize the I2C real-time clock (also brings up the shared
  //      I2C bus that the touch controller uses).
  //   4. Configure the e-paper display and show the BOOT splash.
  //   5. Allocate the WAV buffer and start the PDM microphone.
  //   6. Probe the touch controller now that the bus exists and the panel
  //      size is known.
  //   7. Load persisted reminders; configure speech / memo clients.
  //   8. Try WiFi once, then draw the reminder list.
  Serial1.begin(115200, SERIAL_8N1, kSerialRxPin, kSerialTxPin);
  delay(500);

  setupPins();
  const bool rtcOk = rtc_.begin(kI2cSdaPin, kI2cSclPin);

  Serial1.println("=========================================");
  Serial1.println("  VoiceMemoReminder");
  Serial1.printf("  Device: %s\n", VM_DEVICE_NAME);
  Serial1.printf("  RTC:    %s\n", rtcOk ? "ok" : "unavailable");
  Serial1.println("=========================================");

  ui_.begin();
  ui_.drawStatus("BOOT", "Starting",
                 "Allocating audio buffer and initializing microphone.",
                 "Use Serial1 on GPIO43/GPIO44 for logs.", false, 0.0f);

  if (!audio_.begin(config_.audio.sampleRate, config_.audio.maxRecordSeconds,
                    kMicClkPin, kMicDataPin, kMicPwrEnPin)) {
    ui_.drawStatus("ERR", "Mic failed",
                   "Audio buffer or PDM microphone init failed. Check OPI PSRAM and driver.h.",
                   "Board: XIAO ESP32S3, PSRAM: OPI PSRAM.", false, 0.0f);
    while (true) delay(1000);
  }

  touch_.begin(kTouchIntPin, kTouchResetPin,
               ui_.displayWidth(), ui_.displayHeight());

  store_.begin();
  stt_.configure(config_.speech, config_.httpTimeoutMs);
  memo_.configure(config_.memo,  config_.httpTimeoutMs);

  ui_.drawStatus("WIFI", "Connecting",
                 "Connecting to WiFi before the first recording.",
                 "Edit WiFi, API key, and provider settings in the .ino file.",
                 false, 0.0f);
  ensureWiFi(15000);

  ui_.drawTodoList(store_, rtc_, "READY",
                   "Hold KEY0 to add. Tap a box to check off.");
}

void VoiceMemoApp::startRecording()
{
  if (busy_ || recording_) return;

  // CRITICAL: capture must begin IMMEDIATELY. An ePaper full refresh costs
  // ~1.5 s on E1003, but the I2S DMA ring can only buffer ~256 ms of audio.
  // If we drew a "REC" screen here, the first second of the user's speech
  // would be overwritten in DMA before captureChunk() ever ran. So we do
  // NOT touch the screen at the start of a recording -- the buzzer beep
  // and solid LED are the user feedback.
  audio_.startRecord();
  recording_ = true;
  beepStart();
  ledOn();

  Serial1.println("[rec] start");
}

void VoiceMemoApp::stopRecording(bool forced)
{
  if (!recording_) return;
  recording_ = false;
  ledOff();
  busy_ = true;

  const float seconds = audio_.recordedSeconds();
  Serial1.printf("[rec] stop: %.2fs, %u audio bytes\n", seconds,
                 static_cast<unsigned>(audio_.audioBytes()));

  if (audio_.tooShort()) {
    ui_.drawTodoList(store_, rtc_, "TOO SHORT",
                     "Hold KEY0 for at least one second.");
    busy_ = false;
    return;
  }

  audio_.finishRecord();

  // Screen refresh is safe here because audio capture is already complete.
  // Unlike at recording START, where an ePaper refresh would starve the I2S
  // DMA ring and lose audio samples.
  ui_.drawProcessing("Processing your voice",
                     "Transcribing and summarizing your memo.\nThis usually takes 5-10 seconds.",
                     "Hold tight, screen will update automatically.");
  ledOn();   // solid LED through the network call as a second cue

  if (!ensureWiFi(10000)) {
    ledOff();
    ui_.drawTodoList(store_, rtc_, "NO WIFI",
                     "Reminder skipped because WiFi is unavailable.");
    busy_ = false;
    return;
  }

  String transcript;
  const bool sttOk = stt_.transcribe(audio_.wavData(), audio_.wavSize(),
                                     transcript);
  if (!sttOk && transcript.length() == 0) {
    transcript = "No speech recognized.";
  }
  Serial1.printf("[stt] \"%s\"\n", transcript.c_str());

  const time_t nowEpoch = rtc_.nowEpoch();
  MemoEntry entry = memo_.summarize(transcript, nowEpoch);
  Serial1.printf("[memo] \"%s\" due=%lld label=\"%s\"\n",
                 entry.text.c_str(), static_cast<long long>(entry.dueEpoch),
                 entry.fuzzyLabel.c_str());

  store_.add(entry);
  ledOff();

  const String hint = forced
      ? "Stopped at max length. Hold KEY0 for another memo."
      : "Hold KEY0 to add. Tap a box to check off.";
  ui_.drawTodoList(store_, rtc_, "UPDATED", hint);

  busy_ = false;
}

void VoiceMemoApp::captureChunk()
{
  if (!recording_) return;

  const bool full = audio_.readChunk();
  if (full) {
    stopRecording(true);
    return;
  }

  const unsigned long now = millis();
  if (now - lastBlinkMs_ >= 300) {
    lastBlinkMs_ = now;
    ledState_ = !ledState_;
    if (ledState_) ledOn(); else ledOff();
  }
}

void VoiceMemoApp::pollButton()
{
  // KEY0 is active low (hardware pull-up). Debounce converts the raw GPIO
  // into clean press / release events: press starts recording, release
  // stops the recording and triggers upload + summarize + render.
  const bool rawButton = digitalRead(kKey0Pin);
  if (rawButton != lastRawButton_) {
    debounceMs_ = millis();
    lastRawButton_ = rawButton;
  }
  if ((millis() - debounceMs_) > kDebounceDelayMs && rawButton != stableButton_) {
    stableButton_ = rawButton;
    if (stableButton_ == LOW) startRecording();
    else                      stopRecording(false);
  }
}

void VoiceMemoApp::pollTouch()
{
  // Ignore touches during recording / network calls so a stray finger does
  // not interrupt the current operation.
  if (recording_ || busy_) return;
  if (!touch_.available()) return;

  uint16_t tx = 0, ty = 0;
  if (!touch_.poll(&tx, &ty)) return;

  const int idx = ui_.hitTestCheckbox(tx, ty);
  if (idx < 0) return;

  Serial1.printf("[touch] toggle row %d\n", idx);
  store_.toggleDone(static_cast<size_t>(idx));
  ui_.drawTodoList(store_, rtc_, "UPDATED",
                   "Hold KEY0 to add. Tap a box to check off.");
}

void VoiceMemoApp::loop()
{
  pollButton();
  captureChunk();
  pollTouch();
}
