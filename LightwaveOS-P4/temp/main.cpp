#include <Arduino.h>
#include <algorithm>
#include <cstring>

#define LW_LOG_TAG "P4Main"
#include "utils/Log.h"

#include "config/features.h"
#include "core/actors/ActorSystem.h"
#include "core/actors/RendererActor.h"
#include "core/persistence/NVSManager.h"
#include "core/persistence/ZoneConfigManager.h"
#include "effects/CoreEffects.h"
#include "effects/zones/ZoneComposer.h"
#include "effects/PatternRegistry.h"

#if FEATURE_STACK_PROFILING
#include "core/system/StackMonitor.h"
#endif
#if FEATURE_HEAP_MONITORING
#include "core/system/HeapMonitor.h"
#endif
#if FEATURE_MEMORY_LEAK_DETECTION
#include "core/system/MemoryLeakDetector.h"
#endif
#if FEATURE_VALIDATION_PROFILING
#include "core/system/ValidationProfiler.h"
#endif

using namespace lightwaveos::actors;
using namespace lightwaveos::effects;
using namespace lightwaveos::zones;
using namespace lightwaveos::persistence;

namespace {
ZoneComposer g_zoneComposer;
ZoneConfigManager* g_zoneConfig = nullptr;

void handleSerialCommands(ActorSystem& actors, RendererActor* renderer) {
    static uint8_t currentEffect = 0;

    static EffectRegister currentRegister = EffectRegister::ALL;
    static uint8_t reactiveRegisterIndex = 0;
    static uint8_t ambientRegisterIndex = 0;
    static uint8_t ambientEffectIds[80];
    static uint8_t ambientEffectCount = 0;
    static bool registersInitialized = false;

    if (!renderer) {
        return;
    }

    if (!registersInitialized) {
        uint8_t effectCount = renderer->getEffectCount();
        ambientEffectCount = PatternRegistry::buildAmbientEffectArray(
            ambientEffectIds, sizeof(ambientEffectIds), effectCount);
        registersInitialized = true;
        currentEffect = renderer->getCurrentEffect();
        LW_LOGI("Effect registers: %d reactive, %d ambient, %d total",
                PatternRegistry::getReactiveEffectCount(), ambientEffectCount, effectCount);
    }

    static char serialCmdBuffer[128];
    static size_t serialCmdLen = 0;

    while (Serial.available()) {
        char c = Serial.read();

        if (serialCmdLen == 0) {
            bool isImmediate = false;
            switch (c) {
                case ' ':
                case '+': case '=':
                case '-': case '_':
                case '[': case ']':
                case ',': case '.':
                    isImmediate = true;
                    break;
            }
            if (isImmediate) {
                serialCmdBuffer[0] = c;
                serialCmdLen = 1;
                break;
            }
        }

        if (c == '\n' || c == '\r') {
            break;
        } else if (c == 0x7F || c == 0x08) {
            if (serialCmdLen > 0) {
                serialCmdLen--;
            }
        } else if (c >= 32 && c < 127) {
            if (serialCmdLen < sizeof(serialCmdBuffer) - 1) {
                serialCmdBuffer[serialCmdLen++] = c;
            }
        }
    }

    if (serialCmdLen == 0) {
        return;
    }

    char firstChar = serialCmdBuffer[0];
    serialCmdBuffer[serialCmdLen] = '\0';
    serialCmdLen = 0;

    // Trim leading/trailing spaces and use first non-space char as command.
    char* start = serialCmdBuffer;
    while (*start == ' ') {
        start++;
    }
    char* end = start + strlen(start);
    while (end > start && *(end - 1) == ' ') {
        *(end - 1) = '\0';
        end--;
    }

    char cmd = *start ? *start : firstChar;
    bool inZoneMode = g_zoneComposer.isEnabled();

    // Quick effect selection keys (0-9/a-k)
    if (cmd != '\0') {
        if (cmd >= '0' && cmd <= '9') {
            uint8_t effectId = static_cast<uint8_t>(cmd - '0');
            if (!inZoneMode && effectId < renderer->getEffectCount()) {
                currentEffect = effectId;
                actors.setEffect(effectId);
                Serial.printf("Effect %d: " LW_CLR_GREEN "%s" LW_ANSI_RESET "\n",
                              effectId, renderer->getEffectName(effectId));
            }
            return;
        }
        if (cmd >= 'a' && cmd <= 'k') {
            uint8_t effectId = static_cast<uint8_t>(10 + (cmd - 'a'));
            if (!inZoneMode && effectId < renderer->getEffectCount()) {
                currentEffect = effectId;
                actors.setEffect(effectId);
                Serial.printf("Effect %d: " LW_CLR_GREEN "%s" LW_ANSI_RESET "\n",
                              effectId, renderer->getEffectName(effectId));
            }
            return;
        }
    }

    switch (cmd) {
        case 'z':
            g_zoneComposer.setEnabled(!g_zoneComposer.isEnabled());
            Serial.printf("Zone Mode: %s\n",
                          g_zoneComposer.isEnabled() ? "ENABLED" : "DISABLED");
            if (g_zoneComposer.isEnabled()) {
                Serial.println("  Press 1-5 to load presets");
            }
            break;

        case 'Z':
            g_zoneComposer.printStatus();
            break;

        case 'S':
            if (g_zoneConfig) {
                Serial.println("Saving settings to NVS...");
                bool zoneOk = g_zoneConfig->saveToNVS();
                bool sysOk = g_zoneConfig->saveSystemState(
                    renderer->getCurrentEffect(),
                    renderer->getBrightness(),
                    renderer->getSpeed(),
                    renderer->getPaletteIndex()
                );
                if (zoneOk && sysOk) {
                    Serial.println("  All settings saved!");
                } else {
                    Serial.printf("  Save result: zones=%s, system=%s\n",
                                  zoneOk ? "OK" : "FAIL",
                                  sysOk ? "OK" : "FAIL");
                }
            } else {
                Serial.println("ERROR: Config manager not initialized");
            }
            break;

        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
            if (inZoneMode) {
                uint8_t preset = static_cast<uint8_t>(cmd - '0');
                g_zoneComposer.loadPreset(preset);
                Serial.printf("Zone preset %u loaded\n", preset);
            }
            break;

        case ' ':
        case 'n':
            if (!inZoneMode) {
                uint8_t effectCount = renderer->getEffectCount();
                uint8_t newEffectId = currentEffect;

                switch (currentRegister) {
                    case EffectRegister::ALL:
                        currentEffect = (currentEffect + 1) % effectCount;
                        newEffectId = currentEffect;
                        break;
                    case EffectRegister::REACTIVE:
                        if (PatternRegistry::getReactiveEffectCount() > 0) {
                            reactiveRegisterIndex = (reactiveRegisterIndex + 1) %
                                                    PatternRegistry::getReactiveEffectCount();
                            newEffectId = PatternRegistry::getReactiveEffectId(reactiveRegisterIndex);
                        }
                        break;
                    case EffectRegister::AMBIENT:
                        if (ambientEffectCount > 0) {
                            ambientRegisterIndex = (ambientRegisterIndex + 1) % ambientEffectCount;
                            newEffectId = ambientEffectIds[ambientRegisterIndex];
                        }
                        break;
                }

                if (newEffectId != 0xFF && newEffectId < effectCount) {
                    currentEffect = newEffectId;
                    actors.setEffect(currentEffect);
                    const char* suffix = (currentRegister == EffectRegister::REACTIVE) ? "[R]" :
                                         (currentRegister == EffectRegister::AMBIENT) ? "[M]" : "";
                    Serial.printf("Effect %d%s: " LW_CLR_GREEN "%s" LW_ANSI_RESET "\n",
                                  currentEffect, suffix, renderer->getEffectName(currentEffect));
                }
            }
            break;

        case 'N':
            if (!inZoneMode) {
                uint8_t effectCount = renderer->getEffectCount();
                uint8_t newEffectId = currentEffect;

                switch (currentRegister) {
                    case EffectRegister::ALL:
                        currentEffect = (currentEffect + effectCount - 1) % effectCount;
                        newEffectId = currentEffect;
                        break;
                    case EffectRegister::REACTIVE: {
                        uint8_t count = PatternRegistry::getReactiveEffectCount();
                        if (count > 0) {
                            reactiveRegisterIndex = (reactiveRegisterIndex + count - 1) % count;
                            newEffectId = PatternRegistry::getReactiveEffectId(reactiveRegisterIndex);
                        }
                        break;
                    }
                    case EffectRegister::AMBIENT:
                        if (ambientEffectCount > 0) {
                            ambientRegisterIndex = (ambientRegisterIndex + ambientEffectCount - 1) %
                                                   ambientEffectCount;
                            newEffectId = ambientEffectIds[ambientRegisterIndex];
                        }
                        break;
                }

                if (newEffectId != 0xFF && newEffectId < effectCount) {
                    currentEffect = newEffectId;
                    actors.setEffect(currentEffect);
                    const char* suffix = (currentRegister == EffectRegister::REACTIVE) ? "[R]" :
                                         (currentRegister == EffectRegister::AMBIENT) ? "[M]" : "";
                    Serial.printf("Effect %d%s: " LW_CLR_GREEN "%s" LW_ANSI_RESET "\n",
                                  currentEffect, suffix, renderer->getEffectName(currentEffect));
                }
            }
            break;

        case 'r':
            currentRegister = EffectRegister::REACTIVE;
            Serial.println("Switched to " LW_CLR_CYAN "Reactive" LW_ANSI_RESET " register");
            Serial.printf("  %d audio-reactive effects available\n",
                          PatternRegistry::getReactiveEffectCount());
            if (PatternRegistry::getReactiveEffectCount() > 0) {
                uint8_t reactiveId = PatternRegistry::getReactiveEffectId(reactiveRegisterIndex);
                if (reactiveId != 0xFF && reactiveId < renderer->getEffectCount()) {
                    currentEffect = reactiveId;
                    actors.setEffect(reactiveId);
                    Serial.printf("  Current: %s (ID %d)\n",
                                  renderer->getEffectName(reactiveId), reactiveId);
                }
            }
            break;

        case 'm':
            currentRegister = EffectRegister::AMBIENT;
            Serial.println("Switched to " LW_CLR_MAGENTA "Ambient" LW_ANSI_RESET " register");
            Serial.printf("  %d ambient effects available\n", ambientEffectCount);
            if (ambientEffectCount > 0 && ambientRegisterIndex < ambientEffectCount) {
                uint8_t ambientId = ambientEffectIds[ambientRegisterIndex];
                if (ambientId < renderer->getEffectCount()) {
                    currentEffect = ambientId;
                    actors.setEffect(ambientId);
                    Serial.printf("  Current: %s (ID %d)\n",
                                  renderer->getEffectName(ambientId), ambientId);
                }
            }
            break;

        case '*':
            currentRegister = EffectRegister::ALL;
            Serial.println("Switched to " LW_CLR_GREEN "All Effects" LW_ANSI_RESET " register");
            Serial.printf("  %d effects available\n", renderer->getEffectCount());
            Serial.printf("  Current: %s (ID %d)\n",
                          renderer->getEffectName(currentEffect), currentEffect);
            break;

        case '+':
        case '=': {
            uint8_t b = renderer->getBrightness();
            if (b < 250) {
                b = std::min((int)b + 16, 250);
                actors.setBrightness(b);
                Serial.printf("Brightness: %d\n", b);
            }
            break;
        }

        case '-': {
            uint8_t b = renderer->getBrightness();
            if (b > 16) {
                b = std::max((int)b - 16, 16);
                actors.setBrightness(b);
                Serial.printf("Brightness: %d\n", b);
            }
            break;
        }

        case '[': {
            uint8_t s = renderer->getSpeed();
            if (s > 1) {
                s = std::max((int)s - 1, 1);
                actors.setSpeed(s);
                Serial.printf("Speed: %d\n", s);
            }
            break;
        }

        case ']': {
            uint8_t s = renderer->getSpeed();
            if (s < 100) {
                s = std::min((int)s + 1, 100);
                actors.setSpeed(s);
                Serial.printf("Speed: %d\n", s);
            }
            break;
        }

        case '.':
        case 'p': {
            uint8_t paletteCount = renderer->getPaletteCount();
            uint8_t p = (renderer->getPaletteIndex() + 1) % paletteCount;
            actors.setPalette(p);
            Serial.printf("Palette %d/%d: %s\n", p, paletteCount, renderer->getPaletteName(p));
            break;
        }

        case ',': {
            uint8_t paletteCount = renderer->getPaletteCount();
            uint8_t current = renderer->getPaletteIndex();
            uint8_t p = (current + paletteCount - 1) % paletteCount;
            actors.setPalette(p);
            Serial.printf("Palette %d/%d: %s\n", p, paletteCount, renderer->getPaletteName(p));
            break;
        }

        case 'l': {
            uint8_t effectCount = renderer->getEffectCount();
            Serial.printf("\n=== Effects (%d total) ===\n", effectCount);
            for (uint8_t i = 0; i < effectCount; i++) {
                char key = (i < 10) ? ('0' + i) : ('a' + i - 10);
                const char* type = (renderer->getEffectInstance(i) != nullptr) ? " [IEffect]" : " [Legacy]";
                Serial.printf("  %2d [%c]: %s%s%s\n", i, key, renderer->getEffectName(i), type,
                              (!inZoneMode && i == currentEffect) ? " <--" : "");
            }
            Serial.println();
            break;
        }

        case 'P': {
            uint8_t paletteCount = renderer->getPaletteCount();
            uint8_t currentPalette = renderer->getPaletteIndex();
            Serial.printf("\n=== Palettes (%d total) ===\n", paletteCount);
            Serial.println("--- Artistic (cpt-city) ---");
            for (uint8_t i = 0; i <= 32 && i < paletteCount; i++) {
                Serial.printf("  %2d: %s%s\n", i, renderer->getPaletteName(i),
                              (i == currentPalette) ? " <--" : "");
            }
            Serial.println("--- Scientific (Crameri) ---");
            for (uint8_t i = 33; i <= 56 && i < paletteCount; i++) {
                Serial.printf("  %2d: %s%s\n", i, renderer->getPaletteName(i),
                              (i == currentPalette) ? " <--" : "");
            }
            if (paletteCount > 57) {
                Serial.println("--- LGP-Optimized (Colorspace) ---");
                for (uint8_t i = 57; i < paletteCount; i++) {
                    Serial.printf("  %2d: %s%s\n", i, renderer->getPaletteName(i),
                                  (i == currentPalette) ? " <--" : "");
                }
            }
            Serial.println();
            break;
        }

        case 's': {
            Serial.printf("Effect: %d (%s)\n", renderer->getCurrentEffect(),
                          renderer->getEffectName(renderer->getCurrentEffect()));
            Serial.printf("Brightness: %d\n", renderer->getBrightness());
            Serial.printf("Speed: %d\n", renderer->getSpeed());
            Serial.printf("Palette: %d (%s)\n", renderer->getPaletteIndex(),
                          renderer->getPaletteName(renderer->getPaletteIndex()));
            Serial.printf("Zone Mode: %s\n", g_zoneComposer.isEnabled() ? "ENABLED" : "DISABLED");
            break;
        }

        default:
            break;
    }
}
}

extern "C" void app_main(void) {
    Serial.begin(115200);
    delay(200);

    LW_LOGI("LightwaveOS v2 (ESP32-P4) boot");

#if FEATURE_STACK_PROFILING
    lightwaveos::core::system::StackMonitor::init();
    lightwaveos::core::system::StackMonitor::startProfiling();
    LW_LOGI("Stack profiling: ON");
#endif
#if FEATURE_HEAP_MONITORING
    lightwaveos::core::system::HeapMonitor::init();
    LW_LOGI("Heap monitor: ON");
#endif
#if FEATURE_MEMORY_LEAK_DETECTION
    lightwaveos::core::system::MemoryLeakDetector::init();
#endif
#if FEATURE_VALIDATION_PROFILING
    lightwaveos::core::system::ValidationProfiler::init();
#endif
#if FEATURE_MEMORY_LEAK_DETECTION
    delay(100);
    lightwaveos::core::system::MemoryLeakDetector::resetBaseline();
#endif

    ActorSystem& actors = ActorSystem::instance();
    if (!actors.init()) {
        LW_LOGE("ActorSystem init failed; halting");
        while (true) {
            delay(1000);
        }
    }

    RendererActor* renderer = actors.getRenderer();
    if (renderer == nullptr) {
        LW_LOGE("RendererActor missing; halting");
        while (true) {
            delay(1000);
        }
    }

    uint8_t effectCount = registerAllEffects(renderer);
    LW_LOGI("Effects registered: %u", effectCount);

    if (!NVS_MANAGER.init()) {
        LW_LOGW("NVS init failed; settings won't persist");
    }

    if (g_zoneComposer.init(renderer)) {
        renderer->setZoneComposer(&g_zoneComposer);
        g_zoneConfig = new ZoneConfigManager(&g_zoneComposer);
        if (g_zoneConfig->loadFromNVS()) {
            LW_LOGI("ZoneComposer restored from NVS");
        } else {
            g_zoneComposer.loadPreset(1);
            LW_LOGI("ZoneComposer preset loaded: Dual Split");
        }
    } else {
        LW_LOGW("ZoneComposer init failed");
    }

    if (!actors.start()) {
        LW_LOGE("ActorSystem start failed; halting");
        while (true) {
            delay(1000);
        }
    }

    LW_LOGI("P4 main loop running");

    while (true) {
        handleSerialCommands(actors, renderer);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
