# 🫡 Captain's Handover Document - AP_SOT Mission Status
**Date**: January 14, 2025  
**From**: Previous Captain  
**To**: Next Captain  
**Mission**: SpectraSynq Audio Pipeline - Source of Truth

---

## 🎯 Mission Overview

You are inheriting the **AP_SOT (Audio Pipeline - Source of Truth)** fortress, a sophisticated real-time audio processing system for the ESP32-S3 platform. This codebase represents the culmination of extensive engineering to create both a working audio visualizer AND a flexible platform for future innovation.

### Current Status: **OPERATIONAL** ✅
- Legacy system: **DEPLOYED** and running with all fixes
- Pluggable architecture: **COMPLETE** but not yet activated
- Performance: **<8ms latency** achieved
- Zone energy bug: **FIXED**

---

## 🏗️ Architecture Summary

### 1. **Dual-Path Processing (CRITICAL CONCEPT)**

The most important architectural innovation is the **dual-path design**:

```
I2S Microphone → DC Offset → Goertzel → [SPLIT]
                                           ↓        ↓
                                      RAW PATH   AGC PATH
                                           ↓        ↓
                                    Beat Detection  Visualization
```

**WHY THIS MATTERS:**
- Beat detection REQUIRES dynamic range (loud vs quiet)
- Visualization REQUIRES normalized data (consistent brightness)
- These are mutually exclusive - hence two paths!

### 2. **Two Parallel Systems**

Currently, there are TWO complete implementations:

#### A. **Legacy Monolithic System** (Currently Active)
- **Location**: `src/main.cpp`
- **Status**: Working, deployed, zone bug fixed
- **Components**:
  - `AudioProcessor` - I2S capture and preprocessing
  - `GoertzelEngineGodTier` - 96 frequency bins (A0-A7)
  - `EnhancedBeatDetector` - PLL tempo tracking
  - `AudioFeatures` - Zone energy calculations

#### B. **Pluggable Pipeline System** (Ready for Activation)
- **Location**: `src/main_pluggable.cpp`
- **Status**: Complete, tested, not yet active
- **Components**:
  - `AudioNode` base class - Modular interface
  - `AudioPipeline` - Dynamic execution engine
  - 6 implemented nodes (I2S, DC, Goertzel, AGC, Beat, Zone)
  - JSON configuration support

---

## 📁 Project Structure

```
AP_SOT/
├── include/audio/
│   ├── audio_state.h          # Global state (used by both systems)
│   ├── audio_node.h           # Base class for pluggable system
│   ├── audio_pipeline.h       # Pipeline execution engine
│   ├── nodes/                 # Pluggable node implementations
│   │   ├── i2s_input_node.h   
│   │   ├── dc_offset_node.h   
│   │   ├── goertzel_node.h    
│   │   ├── multiband_agc_node.h
│   │   ├── beat_detector_node.h
│   │   ├── zone_mapper_node.h 
│   │   └── vog_node.h         # NEW: Voice of God confidence engine
│   └── [legacy headers]       # Current system headers
├── src/
│   ├── main.cpp               # ACTIVE: Legacy system
│   ├── main_pluggable.cpp     # READY: New architecture
│   └── audio/                 # Implementation files
├── DEPRECATED/                # Old/obsolete files
├── platformio.ini             # Build configuration
└── [Documentation files]
```

---

## 🔧 Critical Technical Details

### 1. **Hardware Configuration**
- **Microphone**: SPH0645 I2S MEMS
- **Pins**: BCLK=16, LRCLK=4, DIN=10
- **Sample Rate**: 16kHz
- **DC Offset**: -5200 (empirically determined)

### 2. **Performance Targets**
- **Latency**: <8ms per frame (MUST maintain)
- **Frame Rate**: 125 FPS capability
- **CPU Usage**: ~15-17%
- **Memory**: Static allocation only

### 3. **Key Algorithms**
- **Goertzel**: 96 bins, musically spaced (A0-A7)
- **AGC**: 4-band with cochlear-inspired processing
- **Beat Detection**: Multi-band onset + PLL tracking
- **Zone Mapping**: 8 zones with perceptual weighting

---

## 🚧 Current Work in Progress

### **Project "Divine Spark" - VoG Implementation**

The GOD TIER COUNCIL has gifted us the Voice of God (VoG) algorithm:

**STATUS**: Header created, implementation pending

**PURPOSE**: 
- Confidence engine for beat validation
- Measures "divine significance" by comparing RAW vs AGC energy
- Runs asynchronously at 10-12Hz (not in main pipeline!)

**NEXT STEPS**:
1. ✅ VoG node header created (`vog_node.h`)
2. ⏳ Integrate into legacy system first
3. ⏳ Update BeatDetectorNode to use VoG confidence
4. ⏳ Test with real music

**Integration Example**:
```cpp
// In main.cpp after pipeline creation
VoGNode* vog = new VoGNode();
vog->setSpectrumPointers(&raw_spectrum_buffer, &agc_spectrum_buffer);

// In main loop (but not every frame!)
vog->process(dummy_in, dummy_out);  // Runs at 10-12Hz internally
```

---

## 📋 How to Continue the Mission

### 1. **To Complete VoG Integration**:
```bash
# Add to main.cpp after Goertzel initialization
# The VoG needs pointers to both RAW and AGC spectrum data
# It will update audio_state.ext.beat.vog_confidence automatically
```

### 2. **To Activate Pluggable Pipeline**:
```bash
# Backup current system
cp src/main.cpp src/main_legacy_backup.cpp

# Switch to pluggable
mv src/main.cpp src/main_legacy.cpp
mv src/main_pluggable.cpp src/main.cpp

# Build and upload
pio run -t upload --upload-port /dev/cu.usbmodem1401
```

### 3. **To Add New Features**:
- For legacy: Modify respective .cpp files
- For pluggable: Create new AudioNode subclass

---

## ⚠️ Critical Warnings

### **DO NOT**:
1. Process beat detection on AGC data (destroys dynamics)
2. Mix RAW and AGC paths (defeats the purpose)
3. Change Goertzel frequencies (musically tuned)
4. Add heap allocation (causes fragmentation)
5. Exceed 8ms processing time (breaks real-time)

### **ALWAYS**:
1. Test with real music, not just tones
2. Monitor both RAW and AGC outputs
3. Check latency after changes
4. Preserve the dual-path architecture

---

## 🎯 Future Vision

The pluggable architecture enables:
- **Runtime Reconfiguration** - Change pipeline without recompiling
- **Community Nodes** - Users can create custom processors
- **Genre Profiles** - Optimized settings per music style
- **Web Interface** - Real-time parameter tuning
- **Multi-Device Sync** - Coordinated installations

---

## 📊 Recent Achievements

1. **Fixed Zone Energy Bug**: Zones now show dynamic values (0.0-0.95)
2. **Completed Pluggable Architecture**: 100% ready for activation
3. **Maintained Performance**: <8ms latency with added flexibility
4. **Created Test Suite**: Benchmark and verification tools

---

## 🔍 Debugging Commands

```bash
# Monitor serial output
pio device monitor --port /dev/cu.usbmodem1401

# Key things to watch:
# - "ZONES:" lines show dynamic values (not all 1.0)
# - "Energy:" shows beat detection working
# - "I2S Debug:" confirms audio capture
# - "AGC DEBUG:" shows multiband processing
```

---

## 📚 Essential Documentation

1. **AUDIO_PIPELINE_ARCHITECTURE.md** - Dual-path design explanation
2. **PLUGGABLE_PIPELINE_ARCHITECTURE.md** - Node system design
3. **PIPELINE_MIGRATION_GUIDE.md** - How to switch systems
4. **DEPRECATION_TRACKER.md** - What's being phased out

---

## 🎖️ Final Words

Captain, you inherit a fortress built on solid engineering principles. The dual-path architecture is the crown jewel - it solves a fundamental problem elegantly. The pluggable system is ready when you need flexibility.

The VoG implementation awaits completion - it will add a layer of "divine confidence" to our beat detection, distinguishing true musical events from noise.

Remember: **Beat detection needs dynamics. Visualization needs normalization. Never mix the two.**

Fair winds and following seas, Captain. The mission continues!

🫡

---

**P.S.** - The GOD TIER COUNCIL watches. Build with excellence, and they shall provide divine inspiration.