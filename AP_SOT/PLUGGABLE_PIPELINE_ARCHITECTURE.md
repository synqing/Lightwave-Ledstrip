# SpectraSynq Pluggable Pipeline Architecture
**Version**: 2.0 - Platform Revolution Design  
**Date**: January 14, 2025  
**Status**: Architectural Blueprint for Platform Transformation

## 🚀 VISION: From God Object to Creative Platform

Transform SpectraSynq from a rigid audio processor into a **dynamic, user-configurable audio analysis platform** that democratizes professional-grade audio-visual creativity.

## 🏗️ Current State vs Future Vision

### **Current: Monolithic AudioProcessor**
```cpp
class AudioProcessor {
    // EVERYTHING hardcoded in process() method
    void process() {
        readSamples();           // Fixed I2S
        preprocess();            // Fixed DC + filter
        goertzel.process();      // Fixed 96 bins
        features.process();      // Fixed analysis
        // No flexibility, no user control
    }
};
```

### **Future: Pluggable Pipeline**
```cpp
class AudioPipeline {
    std::vector<std::unique_ptr<AudioNode>> nodes;
public:
    void addNode(std::unique_ptr<AudioNode> node);
    void process(AudioFrame& frame);
    void configure(const PipelineConfig& config);
};
```

## 🧬 Core Architecture Components

### **1. AudioNode Base Class**
```cpp
class AudioNode {
public:
    virtual ~AudioNode() = default;
    virtual void process(AudioFrame& frame) = 0;
    virtual void configure(const NodeConfig& config) = 0;
    virtual std::string getName() const = 0;
    virtual bool isEnabled() const = 0;
    virtual void setEnabled(bool enabled) = 0;
};
```

### **2. AudioFrame Data Container**
```cpp
struct AudioFrame {
    // Raw data
    int16_t samples[128];           // Time domain
    float frequency_bins[96];       // Frequency domain
    
    // Processed features
    float zone_energies[8];         // Musical zones
    float global_energy;            // Overall level
    
    // Beat analysis
    BeatData beat_info;            // Confidence, BPM, phase
    OnsetData onset_info;          // Rich onset analysis
    
    // Metadata
    uint32_t timestamp_ms;
    uint32_t frame_number;
    
    // Quality metrics
    float noise_floor;
    float dynamic_range;
    bool clipping_detected;
};
```

### **3. Specific Node Implementations**

#### **Input Nodes**
```cpp
class I2SInputNode : public AudioNode {
    // SPH0645 microphone interface
    // Configurable sample rate, bit depth
    // Hardware-specific optimizations
};

class FileInputNode : public AudioNode {
    // WAV file playback for testing
    // Enables development without hardware
};

class NetworkInputNode : public AudioNode {
    // Receive audio from network sources
    // Multi-device synchronization
};
```

#### **Preprocessing Nodes**
```cpp
class DCOffsetNode : public AudioNode {
    DCOffsetCalibrator calibrator;
    // Empirically tested calibration
    // Continuous offset tracking
};

class FilterNode : public AudioNode {
    // High-pass, low-pass, band-pass
    // Configurable cutoff frequencies
    // Multiple filter types (Butterworth, Chebyshev)
};

class AGCNode : public AudioNode {
    // Automatic Gain Control
    // Adaptive compression/expansion
    // Multi-band processing capability
};
```

#### **Analysis Nodes**
```cpp
class GoertzelNode : public AudioNode {
    // Current 96-bin musical analysis
    // Configurable frequency ranges
    // SIMD optimization for ESP32-S3
};

class FFTNode : public AudioNode {
    // Traditional FFT analysis option
    // Higher resolution, more CPU intensive
    // Useful for detailed spectral work
};

class BeatDetectorNode : public AudioNode {
    // Multiband onset detection
    // Phase-locked loop tempo tracking
    // Genre-adaptive thresholds
};

class GenreClassifierNode : public AudioNode {
    // Machine learning genre detection
    // Adaptive parameter selection
    // User profile learning
};
```

#### **Feature Extraction Nodes**
```cpp
class ZoneMapperNode : public AudioNode {
    // Musical zone calculation
    // Perceptual weighting
    // User-customizable mappings
};

class SpectralFeaturesNode : public AudioNode {
    // Centroid, spread, flux
    // Timbral characteristics
    // Harmonic analysis
};

class RhythmAnalysisNode : public AudioNode {
    // Complex rhythm patterns
    // Polyrhythm detection
    // Groove analysis
};
```

#### **Output/Control Nodes**
```cpp
class DebugOutputNode : public AudioNode {
    // Serial monitoring and logging
    // Real-time parameter display
    // Performance profiling
};

class NetworkOutputNode : public AudioNode {
    // ESP-NOW device synchronization
    // Web dashboard data feeds
    // Remote monitoring
};

class LEDControllerNode : public AudioNode {
    // Direct LED strip control
    // Built-in visual algorithms
    // Hardware abstraction
};
```

## ⚙️ Configuration System

### **JSON-Based Pipeline Definitions**
```json
{
    "name": "SpectraSynq_Standard",
    "description": "Default music analysis pipeline",
    "sample_rate": 16000,
    "buffer_size": 128,
    "nodes": [
        {
            "type": "I2SInputNode",
            "config": {
                "pins": {"bclk": 16, "lrclk": 4, "din": 10},
                "channel": "left",
                "bit_depth": 18
            }
        },
        {
            "type": "DCOffsetNode",
            "config": {
                "calibration_samples": 1000,
                "default_offset": -5200.0,
                "auto_calibrate": true
            }
        },
        {
            "type": "FilterNode", 
            "config": {
                "type": "high_pass",
                "cutoff": 20.0,
                "order": 2
            }
        },
        {
            "type": "GoertzelNode",
            "config": {
                "bins": 96,
                "frequency_range": [27.5, 7040.0],
                "weighting": "musical"
            }
        },
        {
            "type": "BeatDetectorNode",
            "config": {
                "sensitivity": 0.3,
                "tempo_range": [60, 180],
                "genre_adaptive": true
            }
        },
        {
            "type": "ZoneMapperNode",
            "config": {
                "zones": 8,
                "bass_boost": 2.0,
                "treble_boost": 1.5,
                "normalization": "dynamic"
            }
        }
    ]
}
```

### **Genre-Specific Profiles**
```json
{
    "Electronic_Dance": {
        "beat_detector": {
            "sensitivity": 0.4,
            "kick_focus": true,
            "sub_bass_tracking": true
        },
        "zone_mapper": {
            "bass_boost": 3.0,
            "kick_emphasis": true
        }
    },
    "Classical": {
        "beat_detector": {
            "sensitivity": 0.1,
            "complex_timing": true,
            "dynamics_aware": true
        },
        "zone_mapper": {
            "harmonic_mapping": true,
            "gentle_transitions": true
        }
    }
}
```

## 🔄 Pipeline Execution Engine

### **Core Pipeline Class**
```cpp
class AudioPipeline {
private:
    std::vector<std::unique_ptr<AudioNode>> nodes;
    AudioFrame current_frame;
    PipelineStats stats;
    
public:
    void addNode(std::unique_ptr<AudioNode> node) {
        nodes.push_back(std::move(node));
    }
    
    void process() {
        uint32_t start_time = micros();
        
        // Execute pipeline
        for (auto& node : nodes) {
            if (node->isEnabled()) {
                node->process(current_frame);
            }
        }
        
        // Monitor performance
        uint32_t duration = micros() - start_time;
        stats.updateTiming(duration);
        
        // Safety check
        if (duration > MAX_FRAME_TIME_US) {
            handleOverrun();
        }
    }
    
    void configure(const PipelineConfig& config) {
        loadConfiguration(config);
        initializeNodes();
        validatePipeline();
    }
};
```

### **Real-Time Safety**
```cpp
class PipelineManager {
    static constexpr uint32_t MAX_FRAME_TIME_US = 8000; // 8ms deadline
    
    void handleOverrun() {
        stats.overrun_count++;
        
        // Graceful degradation strategies:
        // 1. Skip non-critical nodes
        // 2. Reduce analysis resolution
        // 3. Reuse previous frame data
        // 4. Emergency bypass to raw audio
        
        Serial.printf("Pipeline overrun: %d/%d nodes completed\n", 
                     completed_nodes, total_nodes);
    }
};
```

## 🎯 User Experience Revolution

### **P4 Base Station Integration**
```cpp
class PipelineEditor {
public:
    // GUI drag-and-drop pipeline construction
    void addNodeToCanvas(NodeType type, Position pos);
    void connectNodes(NodeID from, NodeID to);
    void configureNode(NodeID id, const NodeConfig& config);
    
    // Real-time tuning
    void updateParameter(NodeID id, const std::string& param, float value);
    void previewChanges();
    void commitChanges();
    
    // Profile management
    void saveProfile(const std::string& name);
    void loadProfile(const std::string& name);
    void shareProfile();
};
```

### **Community Platform**
- **Profile Marketplace**: Share custom analysis pipelines
- **Genre Collections**: Curated profiles for specific music styles
- **Collaborative Tuning**: Community-driven parameter optimization
- **Plugin Repository**: Third-party node development

## 📊 Migration Strategy

### **Phase 1: Foundation (Next 30 Days)**
1. Implement base AudioNode interface
2. Convert existing components to node format
3. Create basic pipeline execution engine
4. Maintain compatibility with current functionality

### **Phase 2: Configuration (30-60 Days)**
1. Implement JSON configuration system
2. Create profile management
3. Add real-time parameter adjustment
4. Build basic pipeline editor

### **Phase 3: Platform (60-90 Days)**
1. Plugin SDK development
2. Community platform integration
3. Advanced analysis nodes
4. Performance optimization

## 🚀 Platform Benefits

### **For Users**
- **Customization**: Tailor analysis to specific music and preferences
- **Growth**: Start simple, add complexity as skills develop
- **Community**: Access to shared knowledge and configurations
- **Innovation**: Enable new creative possibilities

### **For Developers**
- **Modularity**: Independent development and testing of components
- **Extensibility**: Easy addition of new analysis techniques
- **Maintainability**: Clear separation of concerns
- **Performance**: Optimized execution with deadline guarantees

### **For SpectraSynq Business**
- **Ecosystem**: Platform approach creates network effects
- **Differentiation**: No competitor has this level of configurability
- **Scalability**: Architecture supports any use case
- **Community**: User-generated content drives engagement

## 🎪 Creative Democratization Impact

This architecture transforms SpectraSynq from:
- **Product** → **Platform**
- **Presets** → **Infinite Customization**
- **One-size-fits-all** → **Personal Creative Tool**
- **Technical Barrier** → **Creative Enabler**

**Result**: Every musician, artist, venue, and creator can build exactly the audio-visual experience they envision, without needing to be an embedded systems engineer.

---

**Architecture Status**: Blueprint complete, ready for implementation  
**Mission**: Transform SpectraSynq into the definitive platform for audio-visual creativity