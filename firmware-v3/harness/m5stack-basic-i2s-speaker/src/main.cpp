// WeActStudio I2S Speaker Module V1 (PCM5102A) integration test
// Host: M5Stack Basic v2.7 (ESP32-D0WDQ6-V3)
// Module: https://github.com/WeActStudio/WeActStudio.I2SSpeakerModuleV1
//
// Wiring (Module pin -> M5Stack pin / M5-Bus row):
//   VIN -> 5V    (right column row 14)
//   GND -> GND   (left column rows 1-3, any)
//   BCK -> GPIO 2   (left column row 10)
//   DIN -> GPIO 12  (left column row 11, M5-Bus I2S_SK lane)
//   WS  -> GPIO 15  (left column row 12, M5-Bus I2S_OUT lane)
//   MC  -> floating (PCM5102A internal PLL derives SCK from BCK)
//   SD  -> floating (on-board pull-up keeps XSMT high = unmuted)
//
// The WeAct module's three I2S pads run top-to-bottom in the order
// BCK, DIN, WS, so a 1x3 ribbon plugs straight onto left-column rows
// 10/11/12 with no jumper crossings.
//
// G12 is also the on-board NS4168 amplifier's BCLK line; M5Unified is
// instructed to skip internal speaker init below so the pin is released.
//
// Test programme: three on-screen buttons drive different audio fixtures
// so the module can be characterised by ear and by scope.

#include <M5Unified.h>

namespace {

constexpr int kPinBclk    = 2;   // WeAct BCK
constexpr int kPinDataOut = 12;  // WeAct DIN (M5-Bus I2S_SK lane)
constexpr int kPinLrc     = 15;  // WeAct WS  (M5-Bus I2S_OUT lane)

constexpr int kSampleRate = 44100;
constexpr int kVolumeMax  = 255;

void configureExternalSpeaker() {
    auto spk_cfg = M5.Speaker.config();
    spk_cfg.pin_data_out = kPinDataOut;
    spk_cfg.pin_bck      = kPinBclk;
    spk_cfg.pin_ws       = kPinLrc;
    spk_cfg.sample_rate  = kSampleRate;
    spk_cfg.stereo       = false;        // MAX98357A is mono
    spk_cfg.buzzer       = false;        // External Class-D amp, not piezo
    spk_cfg.use_dac      = false;        // External I2S DAC handles conversion
    spk_cfg.dac_zero_level = 0;
    spk_cfg.magnification  = 16;
    spk_cfg.task_priority  = 2;
    spk_cfg.task_pinned_core = 1;
    spk_cfg.i2s_port = I2S_NUM_0;
    M5.Speaker.config(spk_cfg);
    M5.Speaker.begin();
    M5.Speaker.setVolume(kVolumeMax / 2);  // start at 50 %
}

void drawHeader() {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(10, 10);
    M5.Display.println("WeAct I2S Test");
    M5.Display.drawFastHLine(0, 36, M5.Display.width(), TFT_DARKGREY);

    M5.Display.setTextSize(1);
    M5.Display.setCursor(10, 46);
    M5.Display.printf("BCK G%d  DIN G%d  WS G%d", kPinBclk, kPinDataOut, kPinLrc);

    M5.Display.setTextSize(2);
    M5.Display.setCursor(10, 70);  M5.Display.println("A: 1 kHz tone");
    M5.Display.setCursor(10, 100); M5.Display.println("B: 200-2k sweep");
    M5.Display.setCursor(10, 130); M5.Display.println("C: A4+C#5+E5");

    M5.Display.setCursor(10, 170);
    M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    M5.Display.println("Hold A: vol-  Hold C: vol+");
}

void showStatus(const char* msg) {
    M5.Display.fillRect(0, 200, M5.Display.width(), 30, TFT_BLACK);
    M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(10, 205);
    M5.Display.println(msg);
}

void showVolume() {
    char buf[32];
    snprintf(buf, sizeof(buf), "Volume: %u/%u", M5.Speaker.getVolume(), kVolumeMax);
    showStatus(buf);
}

void playSweep() {
    showStatus("Sweep 200-2000 Hz");
    for (int freq = 200; freq <= 2000; freq += 25) {
        M5.Speaker.tone(static_cast<float>(freq), 25);
        delay(20);
    }
    showStatus("Sweep done");
}

void playChord() {
    showStatus("Chord A4+C#5+E5");
    M5.Speaker.tone(440.0f,  600, 0);  // A4  on virtual channel 0
    M5.Speaker.tone(554.37f, 600, 1);  // C#5 on virtual channel 1
    M5.Speaker.tone(659.25f, 600, 2);  // E5  on virtual channel 2
}

}  // namespace

void setup() {
    auto cfg = M5.config();
    cfg.internal_spk = false;   // do not auto-init the on-board NS4168 amp
    cfg.external_spk = true;    // we will reconfigure pins explicitly below
    M5.begin(cfg);

    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println("[WeAct I2S] M5Stack Basic v2.7 + WeActStudio PCM5102A");
    Serial.printf("[WeAct I2S] BCK=GPIO%d  DIN=GPIO%d  WS=GPIO%d  Fs=%d Hz\n",
                  kPinBclk, kPinDataOut, kPinLrc, kSampleRate);

    configureExternalSpeaker();
    drawHeader();
    showVolume();
}

void loop() {
    M5.update();

    if (M5.BtnA.wasReleased() && !M5.BtnA.wasReleaseFor(700)) {
        showStatus("1 kHz / 250 ms");
        M5.Speaker.tone(1000.0f, 250);
    }
    if (M5.BtnA.wasReleaseFor(700)) {
        uint8_t v = M5.Speaker.getVolume();
        v = (v >= 16) ? (v - 16) : 0;
        M5.Speaker.setVolume(v);
        showVolume();
    }

    if (M5.BtnB.wasPressed()) {
        playSweep();
    }

    if (M5.BtnC.wasReleased() && !M5.BtnC.wasReleaseFor(700)) {
        playChord();
    }
    if (M5.BtnC.wasReleaseFor(700)) {
        uint16_t v = M5.Speaker.getVolume();
        v = (v + 16 > kVolumeMax) ? kVolumeMax : (v + 16);
        M5.Speaker.setVolume(static_cast<uint8_t>(v));
        showVolume();
    }

    delay(10);
}
