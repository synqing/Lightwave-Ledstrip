#ifndef SERIAL_MENU_H
#define SERIAL_MENU_H

#include <Arduino.h>
#include <FastLED.h>
#include "../config/hardware_config.h"
#include "../config/features.h"
#include "../core/FxEngine.h"
#include "../core/PaletteManager.h"
#include "../hardware/PerformanceMonitor.h"

// External variables
extern FxEngine fxEngine;
extern PaletteManager paletteManager;
extern uint8_t brightnessVal;
extern uint8_t fadeAmount;
extern uint16_t fps;
extern uint8_t paletteSpeed;
extern PerformanceMonitor perfMon;

class SerialMenu {
private:
  String inputBuffer = "";
  bool menuActive = false;
  uint8_t currentMenuLevel = 0; // 0=main, 1=effects, 2=palettes, 3=settings
  
public:
  void begin() {
    Serial.println(F("\n=== Light Crystals Control System ==="));
    Serial.println(F("Type 'h' or 'help' for menu"));
    Serial.println(F("Type 'm' or 'menu' for main menu"));
    Serial.println(F("======================================"));
  }
  
  void update() {
    if (Serial.available()) {
      char c = Serial.read();
      
      if (c == '\n' || c == '\r') {
        if (inputBuffer.length() > 0) {
          processCommand(inputBuffer);
          inputBuffer = "";
        }
      } else if (c == 8 || c == 127) { // Backspace
        if (inputBuffer.length() > 0) {
          inputBuffer.remove(inputBuffer.length() - 1);
          Serial.print(F("\b \b"));
        }
      } else if (c >= 32 && c <= 126) { // Printable characters
        inputBuffer += c;
        Serial.print(c);
      }
    }
  }
  
private:
  void processCommand(String cmd) {
    cmd.trim();
    cmd.toLowerCase();
    
    Serial.println(); // New line after command
    
    if (cmd == "h" || cmd == "help") {
      showHelp();
    } else if (cmd == "m" || cmd == "menu") {
      showMainMenu();
    } else if (cmd == "s" || cmd == "status") {
      showStatus();
    } else if (cmd == "e" || cmd == "effects") {
      showEffectsMenu();
    } else if (cmd == "p" || cmd == "palettes") {
      showPalettesMenu();
    } else if (cmd == "c" || cmd == "config") {
      showConfigMenu();
    } else if (cmd == "perf" || cmd == "performance") {
      showPerformanceInfo();
    } else if (cmd == "perfdetail" || cmd == "pd") {
      showDetailedPerformance();
    } else if (cmd == "perfgraph" || cmd == "pg") {
      showPerformanceGraph();
    } else if (cmd == "perfreset") {
      resetPerformanceMetrics();
    } else if (cmd.startsWith("effect ")) {
      setEffect(cmd.substring(7));
    } else if (cmd.startsWith("palette ")) {
      setPalette(cmd.substring(8));
    } else if (cmd.startsWith("brightness ")) {
      setBrightness(cmd.substring(11));
    } else if (cmd.startsWith("fade ")) {
      setFade(cmd.substring(5));
    } else if (cmd.startsWith("speed ")) {
      setSpeed(cmd.substring(6));
    } else if (cmd.startsWith("fps ")) {
      setFPS(cmd.substring(4));
    } else if (cmd.startsWith("transition ")) {
      setTransition(cmd.substring(11));
    } else if (cmd == "next") {
      fxEngine.nextEffect(0, 800);
      Serial.println(F("Switched to next effect"));
    } else if (cmd == "prev") {
      fxEngine.prevEffect(0, 800);
      Serial.println(F("Switched to previous effect"));
    } else if (cmd == "reset") {
      resetToDefaults();
    } else if (cmd == "clear") {
      clearScreen();
    } else if (cmd == "wave") {
      showWaveMenu();
    } else if (cmd == "pipeline" || cmd == "pipe") {
      showPipelineMenu();
    } else {
      Serial.println(F("Unknown command. Type 'help' for commands."));
    }
  }
  
  void showHelp() {
    Serial.println(F("\n=== COMMAND HELP ==="));
    Serial.println(F("Navigation:"));
    Serial.println(F("  h, help       - Show this help"));
    Serial.println(F("  m, menu       - Show main menu"));
    Serial.println(F("  s, status     - Show current status"));
    Serial.println(F("  clear         - Clear screen"));
    Serial.println(F(""));
    Serial.println(F("Quick Commands:"));
    Serial.println(F("  next          - Next effect"));
    Serial.println(F("  prev          - Previous effect"));
    Serial.println(F("  effect <0-7>  - Set effect by number"));
    Serial.println(F("  palette <0-32>- Set palette by number"));
    Serial.println(F("  brightness <0-255> - Set brightness"));
    Serial.println(F("  fade <0-255>  - Set fade amount"));
    Serial.println(F("  speed <1-50>  - Set palette speed"));
    Serial.println(F("  fps <10-120>  - Set frame rate"));
    Serial.println(F("  reset         - Reset to defaults"));
    Serial.println(F(""));
    Serial.println(F("Menus:"));
    Serial.println(F("  e, effects    - Effects menu"));
    Serial.println(F("  p, palettes   - Palettes menu"));
    Serial.println(F("  c, config     - Configuration menu"));
    Serial.println(F("  wave          - Wave effects menu"));
    Serial.println(F("  pipe, pipeline- Pipeline effects menu"));
    Serial.println(F(""));
    Serial.println(F("Performance:"));
    Serial.println(F("  perf          - Quick performance info"));
    Serial.println(F("  pd, perfdetail- Detailed performance report"));
    Serial.println(F("  pg, perfgraph - Performance graph"));
    Serial.println(F("  perfreset     - Reset peak metrics"));
    Serial.println(F("=================="));
  }
  
  void showMainMenu() {
    Serial.println(F("\n=== MAIN MENU ==="));
    Serial.println(F("1. Effects Menu     (e)"));
    Serial.println(F("2. Palettes Menu    (p)"));
    Serial.println(F("3. Configuration    (c)"));
    Serial.println(F("4. Wave Effects     (wave)"));
    Serial.println(F("5. Performance Info (perf)"));
    Serial.println(F("6. Status           (s)"));
    Serial.println(F("7. Help             (h)"));
    Serial.println(F("================"));
  }
  
  void showStatus() {
    Serial.println(F("\n=== CURRENT STATUS ==="));
    Serial.print(F("Effect: "));
    Serial.print(fxEngine.getCurrentEffectIndex());
    Serial.print(F(" - "));
    Serial.println(fxEngine.getCurrentEffectName());
    
    Serial.print(F("Palette: "));
    Serial.print(currentPaletteIndex);
    Serial.print(F(" - "));
    Serial.println(paletteNames[currentPaletteIndex]);
    
    Serial.print(F("Brightness: "));
    Serial.println(brightnessVal);
    
    Serial.print(F("Fade Amount: "));
    Serial.println(fadeAmount);
    
    Serial.print(F("Palette Speed: "));
    Serial.println(paletteSpeed);
    
    Serial.print(F("Target FPS: "));
    Serial.println(fps);
    
    Serial.print(F("Actual FPS: "));
    Serial.println(fxEngine.getApproximateFPS(), 1);
    
    if (fxEngine.getIsTransitioning()) {
      Serial.print(F("Transitioning: "));
      Serial.print(fxEngine.getTransitionProgress() * 100, 0);
      Serial.println(F("%"));
    }
    
    Serial.println(F("===================="));
  }
  
  void showEffectsMenu() {
    Serial.println(F("\n=== EFFECTS MENU ==="));
    for (uint8_t i = 0; i < fxEngine.getNumEffects(); i++) {
      Serial.print(i);
      Serial.print(F(". "));
      
      // Get effect name by temporarily setting it
      uint8_t currentEffect = fxEngine.getCurrentEffectIndex();
      if (i == currentEffect) {
        Serial.print(F(">>> "));
      }
      
      // We need to access effect names - let's add them manually for now
      const char* effectNames[] = {
        "Gradient", "Fibonacci", "Wave", "Kaleidoscope", "Pulse",
        "FxWave Ripple", "FxWave Interference", "FxWave Orbital"
      };
      
      if (i < 8) {
        Serial.print(effectNames[i]);
      } else {
        Serial.print(F("Effect "));
        Serial.print(i);
      }
      
      if (i == currentEffect) {
        Serial.print(F(" <<<"));
      }
      Serial.println();
    }
    Serial.println(F(""));
    Serial.println(F("Commands:"));
    Serial.println(F("  effect <0-7>  - Select effect"));
    Serial.println(F("  next          - Next effect"));
    Serial.println(F("  prev          - Previous effect"));
    Serial.println(F("  transition <0-2> - Set transition type"));
    Serial.println(F("=================="));
  }
  
  void showPalettesMenu() {
    Serial.println(F("\n=== PALETTES MENU ==="));
    for (uint8_t i = 0; i < 33; i++) {
      Serial.print(i);
      Serial.print(F(". "));
      if (i == currentPaletteIndex) {
        Serial.print(F(">>> "));
      }
      Serial.print(paletteNames[i]);
      if (i == currentPaletteIndex) {
        Serial.print(F(" <<<"));
      }
      Serial.println();
      
      // Show in groups of 5
      if ((i + 1) % 5 == 0 && i < 32) {
        Serial.println();
      }
    }
    Serial.println(F(""));
    Serial.println(F("Commands:"));
    Serial.println(F("  palette <0-32> - Select palette"));
    Serial.println(F("===================="));
  }
  
  void showConfigMenu() {
    Serial.println(F("\n=== CONFIGURATION ==="));
    Serial.print(F("Brightness: "));
    Serial.print(brightnessVal);
    Serial.println(F(" (0-255)"));
    
    Serial.print(F("Fade Amount: "));
    Serial.print(fadeAmount);
    Serial.println(F(" (0-255)"));
    
    Serial.print(F("Palette Speed: "));
    Serial.print(paletteSpeed);
    Serial.println(F(" (1-50)"));
    
    Serial.print(F("Target FPS: "));
    Serial.print(fps);
    Serial.println(F(" (10-120)"));
    
    Serial.println(F(""));
    Serial.println(F("Commands:"));
    Serial.println(F("  brightness <0-255> - Set brightness"));
    Serial.println(F("  fade <0-255>       - Set fade amount"));
    Serial.println(F("  speed <1-50>       - Set palette speed"));
    Serial.println(F("  fps <10-120>       - Set frame rate"));
    Serial.println(F("  reset              - Reset to defaults"));
    Serial.println(F("===================="));
  }
  
  void showWaveMenu() {
    Serial.println(F("\n=== WAVE EFFECTS ==="));
    Serial.print(F("Active Wave Sources: "));
    Serial.println(fxWave2D.getNumActiveSources());
    
    Serial.print(F("Time Scale: "));
    Serial.println(fxWave2D.getTimeScale(), 2);
    
    Serial.println(F(""));
    Serial.println(F("Wave Effects:"));
    Serial.println(F("  5. FxWave Ripple"));
    Serial.println(F("  6. FxWave Interference"));
    Serial.println(F("  7. FxWave Orbital"));
    Serial.println(F(""));
    Serial.println(F("Commands:"));
    Serial.println(F("  effect 5-7 - Select wave effect"));
    Serial.println(F("=================="));
  }
  
  void showPerformanceInfo() {
    
    Serial.println(F("\n=== PERFORMANCE INFO ==="));
    
    // Get timing percentages
    float effectPct, ledPct, serialPct, idlePct;
    perfMon.getTimingPercentages(effectPct, ledPct, serialPct, idlePct);
    
    Serial.print(F("FPS: "));
    Serial.print(perfMon.getCurrentFPS(), 1);
    Serial.print(F(" / "));
    Serial.print(fps);
    Serial.print(F(" ("));
    Serial.print(perfMon.getCurrentFPS() / fps * 100, 0);
    Serial.println(F("%)"));
    
    Serial.print(F("CPU Usage: "));
    Serial.print(perfMon.getCPUUsage(), 1);
    Serial.println(F("%"));
    
    Serial.println(F("\nTiming Breakdown:"));
    Serial.print(F("  Effect: "));
    Serial.print(effectPct, 1);
    Serial.print(F("% ("));
    Serial.print(perfMon.getEffectTime());
    Serial.println(F("μs)"));
    
    Serial.print(F("  FastLED: "));
    Serial.print(ledPct, 1);
    Serial.print(F("% ("));
    Serial.print(perfMon.getFastLEDTime());
    Serial.println(F("μs)"));
    
    Serial.print(F("  Serial: "));
    Serial.print(serialPct, 1);
    Serial.println(F("%"));
    
    Serial.print(F("  Idle: "));
    Serial.print(idlePct, 1);
    Serial.println(F("%"));
    
    Serial.print(F("\nDropped Frames: "));
    Serial.println(perfMon.getDroppedFrames());
    
    Serial.print(F("Free Heap: "));
    Serial.print(ESP.getFreeHeap());
    Serial.print(F(" / Min: "));
    Serial.println(perfMon.getMinFreeHeap());
    
    Serial.println(F("======================"));
  }
  
  void showDetailedPerformance() {
    perfMon.printDetailedReport();
  }
  
  void showPerformanceGraph() {
    perfMon.drawPerformanceGraph();
  }
  
  void resetPerformanceMetrics() {
    perfMon.resetPeaks();
    Serial.println(F("Performance metrics reset"));
  }
  
  void showPipelineMenu() {
    Serial.println(F("\n=== PIPELINE EFFECTS ==="));
    Serial.println(F("Modular Visual Pipeline System"));
    Serial.println(F(""));
    Serial.println(F("Pipeline Effects:"));
    Serial.println(F("  11. Pipeline Gradient"));
    Serial.println(F("  12. Pipeline Fibonacci"));
    Serial.println(F("  13. Pipeline Audio"));
    Serial.println(F("  14. Pipeline Matrix"));
    Serial.println(F("  15. Pipeline Reaction"));
    Serial.println(F(""));
    Serial.println(F("Features:"));
    Serial.println(F("  - Modular stage-based rendering"));
    Serial.println(F("  - Composable effects"));
    Serial.println(F("  - Per-stage performance tracking"));
    Serial.println(F(""));
    Serial.println(F("Commands:"));
    Serial.println(F("  effect 11-15 - Select pipeline effect"));
    Serial.println(F("======================"));
  }
  
  void setEffect(String value) {
    int effectNum = value.toInt();
    if (effectNum >= 0 && effectNum < fxEngine.getNumEffects()) {
      fxEngine.setEffect(effectNum, 0, 800);
      Serial.print(F("Set effect to: "));
      Serial.println(effectNum);
    } else {
      Serial.println(F("Invalid effect number"));
    }
  }
  
  void setPalette(String value) {
    int paletteNum = value.toInt();
    if (paletteNum >= 0 && paletteNum < 33) {
      currentPaletteIndex = paletteNum;
      targetPalette = CRGBPalette16(gGradientPalettes[currentPaletteIndex]);
      Serial.print(F("Set palette to: "));
      Serial.print(paletteNum);
      Serial.print(F(" - "));
      Serial.println(paletteNames[paletteNum]);
    } else {
      Serial.println(F("Invalid palette number (0-32)"));
    }
  }
  
  void setBrightness(String value) {
    int brightness = value.toInt();
    if (brightness >= 0 && brightness <= 255) {
      brightnessVal = brightness;
      FastLED.setBrightness(brightnessVal);
      Serial.print(F("Set brightness to: "));
      Serial.println(brightness);
    } else {
      Serial.println(F("Invalid brightness (0-255)"));
    }
  }
  
  void setFade(String value) {
    int fade = value.toInt();
    if (fade >= 0 && fade <= 255) {
      fadeAmount = fade;
      Serial.print(F("Set fade amount to: "));
      Serial.println(fade);
    } else {
      Serial.println(F("Invalid fade amount (0-255)"));
    }
  }
  
  void setSpeed(String value) {
    int speed = value.toInt();
    if (speed >= 1 && speed <= 50) {
      paletteSpeed = speed;
      Serial.print(F("Set palette speed to: "));
      Serial.println(speed);
    } else {
      Serial.println(F("Invalid speed (1-50)"));
    }
  }
  
  void setFPS(String value) {
    int newFPS = value.toInt();
    if (newFPS >= 10 && newFPS <= 120) {
      fps = newFPS;
      Serial.print(F("Set FPS to: "));
      Serial.println(newFPS);
    } else {
      Serial.println(F("Invalid FPS (10-120)"));
    }
  }
  
  void setTransition(String value) {
    int transType = value.toInt();
    if (transType >= 0 && transType <= 2) {
      const char* transNames[] = {"Fade", "Wipe", "Blend"};
      Serial.print(F("Next transition will be: "));
      Serial.println(transNames[transType]);
    } else {
      Serial.println(F("Invalid transition type (0=Fade, 1=Wipe, 2=Blend)"));
    }
  }
  
  void resetToDefaults() {
    brightnessVal = DEFAULT_BRIGHTNESS;
    fadeAmount = 20;
    paletteSpeed = 10;
    fps = 120;
    currentPaletteIndex = 0;
    FastLED.setBrightness(brightnessVal);
    fxEngine.setEffect(0, 0, 800);
    Serial.println(F("Reset to default settings"));
  }
  
  void clearScreen() {
    Serial.print(F("\033[2J\033[H")); // ANSI clear screen
    Serial.println(F("=== Light Crystals Control System ==="));
  }
};

// Global instance
extern SerialMenu serialMenu;

#endif // SERIAL_MENU_H