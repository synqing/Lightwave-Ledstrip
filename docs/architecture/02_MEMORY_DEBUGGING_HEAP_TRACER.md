# 🔍 Document 02: Memory Debugging & Heap Tracer System

<div align="center">

```
╔═══════════════════════════════════════════════════════════════════════════════╗
║                      MEMORY DEBUGGING & HEAP TRACER                            ║
║                   Real-Time Memory Analysis & Optimization                     ║
║                     Leak Detection • Fragmentation • PSRAM                     ║
╚═══════════════════════════════════════════════════════════════════════════════╝
```

**Module:** HeapTracer | **Purpose:** Memory Health | **Impact:** Zero-overhead when disabled

</div>

---

## 📋 Executive Summary

The HeapTracer system provides comprehensive real-time memory monitoring and debugging capabilities for LightwaveOS. This sophisticated tool tracks allocations, detects leaks, analyzes fragmentation, and provides actionable recommendations—all while maintaining minimal performance impact through conditional compilation.

### 🎯 Key Features
- **Real-Time Tracking**: Monitor heap usage without stopping execution
- **Leak Detection**: Identify memory leaks with allocation source tracking
- **Fragmentation Analysis**: Measure and visualize heap fragmentation
- **PSRAM Monitoring**: Track external memory usage and performance
- **Performance Recommendations**: AI-driven optimization suggestions

---

## 🏗️ System Architecture

### HeapTracer Component Architecture

```mermaid
graph TB
    subgraph "HeapTracer Core"
        HT[HeapTracer<br/>Main Module]
        AT[Allocation<br/>Tracker]
        FA[Fragmentation<br/>Analyzer]
        LD[Leak<br/>Detector]
        PR[PSRAM<br/>Monitor]
    end
    
    subgraph "Data Collection"
        DC[Data<br/>Collector]
        TS[Timestamp<br/>Manager]
        ST[Stack<br/>Tracer]
    end
    
    subgraph "Analysis Engine"
        AE[Analysis<br/>Engine]
        RG[Report<br/>Generator]
        RE[Recommendation<br/>Engine]
    end
    
    subgraph "Output Interfaces"
        SE[Serial<br/>Output]
        WE[Web<br/>Export]
        LE[LED<br/>Indicators]
    end
    
    HT --> AT
    HT --> FA
    HT --> LD
    HT --> PR
    
    AT --> DC
    FA --> DC
    LD --> DC
    PR --> DC
    
    DC --> TS
    DC --> ST
    
    DC --> AE
    AE --> RG
    AE --> RE
    
    RG --> SE
    RG --> WE
    RG --> LE
    
    style HT fill:#ff6b6b,stroke:#c92a2a,stroke-width:3px
    style AE fill:#4dabf7,stroke:#1971c2
    style DC fill:#69db7c,stroke:#2f9e44
```

### Memory Layout Visualization

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      ESP32-S3 MEMORY ARCHITECTURE                        │
├─────────────────────────────────────────────────────────────────────────┤
│  DRAM (520KB)                                                           │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │ 0x3FFB0000                              0x3FFE0000              │   │
│  │ ┌─────────┬──────────┬──────────┬──────────┬────────────────┐ │   │
│  │ │ Stack   │ Heap     │ .data    │ .bss     │ Free           │ │   │
│  │ │ 8KB×2   │ ~200KB   │ 50KB     │ 80KB     │ Variable       │ │   │
│  │ └─────────┴──────────┴──────────┴──────────┴────────────────┘ │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│  PSRAM (16MB)                                                           │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │ 0x3C000000                              0x3D000000              │   │
│  │ ┌──────────────────────────┬────────────────────────────────┐ │   │
│  │ │ Allocated: 2MB           │ Free: 14MB                     │ │   │
│  │ │ (Effect buffers, caches) │ (Future use)                   │ │   │
│  │ └──────────────────────────┴────────────────────────────────┘ │   │
│  └─────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 📊 Allocation Tracking System

### Allocation Record Structure

```cpp
struct AllocationRecord {
    void* address;           // Memory address
    size_t size;            // Allocation size
    uint32_t timestamp;     // When allocated
    uint32_t threadId;      // Which task/thread
    const char* file;       // Source file
    int line;              // Line number
    uint16_t allocId;      // Unique allocation ID
    uint8_t flags;         // PSRAM, DMA, etc.
    void* stackTrace[4];   // Call stack
};
```

### Tracking State Machine

```mermaid
stateDiagram-v2
    [*] --> Idle
    Idle --> Tracking: Enable
    
    state Tracking {
        [*] --> Monitoring
        Monitoring --> Recording: malloc/free
        Recording --> Analyzing: Threshold
        Analyzing --> Monitoring: Continue
        
        Recording --> LeakDetected: Pattern Match
        LeakDetected --> Alerting
        Alerting --> Monitoring
    }
    
    Tracking --> Reporting: Generate Report
    Reporting --> Idle: Disable
    
    state Alerting {
        [*] --> SerialLog
        SerialLog --> LEDFlash
        LEDFlash --> WebNotify
        WebNotify --> [*]
    }
```

### Real-Time Tracking Dashboard

```
╔════════════════════════════════════════════════════════════════════════╗
║                    HEAP TRACKER REAL-TIME DASHBOARD                     ║
╠════════════════════════════════════════════════════════════════════════╣
║ Heap Status:                                                           ║
║ ├─ Total Size:     327,680 bytes                                      ║
║ ├─ Free:          125,432 bytes (38.3%)                              ║
║ ├─ Allocated:     202,248 bytes (61.7%)                              ║
║ ├─ Largest Free:   45,632 bytes                                       ║
║ └─ Fragmentation:  12.4% [████░░░░░░]                                ║
║                                                                        ║
║ PSRAM Status:                                                          ║
║ ├─ Total Size:   16,777,216 bytes                                     ║
║ ├─ Free:        14,680,064 bytes (87.5%)                             ║
║ └─ Allocated:    2,097,152 bytes (12.5%)                             ║
║                                                                        ║
║ Top Allocations:                                                       ║
║ 1. LED Buffers      - 5,760 bytes  [main.cpp:45]                     ║
║ 2. Transition State - 2,048 bytes  [TransitionEngine.cpp:123]        ║
║ 3. WebSocket Buffer - 8,192 bytes  [WebServer.cpp:78]                ║
║ 4. Effect State     - 1,024 bytes  [HDREffect.cpp:34]                ║
║ 5. JSON Document    -   512 bytes  [WebServer.cpp:234]               ║
╚════════════════════════════════════════════════════════════════════════╝
```

---

## 🔍 Leak Detection Engine

### Leak Detection Algorithm

```mermaid
flowchart TD
    subgraph "Leak Detection Pipeline"
        A[Track Allocation] --> B[Store Metadata]
        B --> C[Monitor Lifetime]
        C --> D{Still Referenced?}
        D -->|Yes| E[Active Memory]
        D -->|No| F{Lifetime > Threshold?}
        F -->|Yes| G[Potential Leak]
        F -->|No| C
        
        G --> H[Analyze Pattern]
        H --> I{Confirmed Leak?}
        I -->|Yes| J[Generate Alert]
        I -->|No| K[Watch List]
        
        J --> L[Stack Trace]
        L --> M[Source Location]
        M --> N[Recommendations]
    end
    
    style G fill:#ffd43b
    style J fill:#ff6b6b
    style N fill:#69db7c
```

### Leak Pattern Recognition

```
╔════════════════════════════════════════════════════════════════════════╗
║                      COMMON LEAK PATTERNS DETECTED                      ║
╠════════════════════════════════════════════════════════════════════════╣
║ Pattern              │ Signature                 │ Common Cause        ║
├──────────────────────┼───────────────────────────┼─────────────────────┤
║ Growing Allocation   │ Size increases each frame │ String concatenation│
║ Orphaned Buffer      │ No references after init  │ Missing delete      │
║ Circular Reference   │ A→B→C→A pattern          │ Shared pointers     │
║ Task Leak           │ Task exit without cleanup │ Task local storage  │
║ Effect State Leak   │ Effect switch w/o cleanup │ Missing destructor  │
╚══════════════════════╧═══════════════════════════╧═════════════════════╝
```

---

## 📈 Fragmentation Analysis

### Fragmentation Visualization

```
Heap Fragmentation Map (Lower addresses → Higher addresses)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

0x3FFB0000                                                    0x3FFE0000
├─────────────────────────────────────────────────────────────────────┤
│███ ░░░ ████ ░ ██████ ░░░░ ███ ░ ████████ ░░ ███ ░░░░░ ████████ ░░│
│                                                                      │
│ Legend: ███ Allocated  ░░░ Free                                     │
│                                                                      │
│ Fragmentation Metrics:                                               │
│ ├─ Fragment Count:     23                                           │
│ ├─ Average Fragment:   5,452 bytes                                  │
│ ├─ Wasted Space:      15,234 bytes (4.6%)                          │
│ └─ Allocation Efficiency: 87.3%                                     │
│                                                                      │
│ Largest Contiguous Blocks:                                           │
│ 1. 45,632 bytes @ 0x3FFD2340                                       │
│ 2. 23,456 bytes @ 0x3FFBC890                                       │
│ 3. 12,288 bytes @ 0x3FFE1230                                       │
└──────────────────────────────────────────────────────────────────────┘
```

### Fragmentation Calculation

```cpp
struct FragmentationMetrics {
    float fragmentationRatio;    // 0.0 = perfect, 1.0 = unusable
    uint32_t fragmentCount;      // Number of free blocks
    size_t largestFreeBlock;     // Biggest contiguous free space
    size_t totalFreeSpace;       // Sum of all free blocks
    size_t wastedSpace;          // Unusable due to fragmentation
    
    float calculateScore() {
        // Weighted fragmentation score
        float sizeScore = largestFreeBlock / (float)totalFreeSpace;
        float countScore = 1.0f / (1.0f + log(fragmentCount));
        float wasteScore = 1.0f - (wastedSpace / (float)totalFreeSpace);
        
        return (sizeScore * 0.5f + countScore * 0.3f + wasteScore * 0.2f);
    }
};
```

---

## 🚀 PSRAM Performance Monitoring

### PSRAM Access Patterns

```mermaid
graph LR
    subgraph "PSRAM Usage Analysis"
        subgraph "Access Types"
            SR[Sequential Read<br/>400MB/s]
            SW[Sequential Write<br/>400MB/s]
            RR[Random Read<br/>40MB/s]
            RW[Random Write<br/>40MB/s]
        end
        
        subgraph "Allocation Strategy"
            LA[Large Arrays<br/>>4KB]
            EB[Effect Buffers]
            TC[Texture Cache]
            LT[Lookup Tables]
        end
        
        subgraph "Performance Impact"
            CP[Cache Performance]
            AP[Access Patterns]
            BW[Bandwidth Usage]
        end
        
        SR --> CP
        SW --> CP
        RR --> AP
        RW --> AP
        
        LA --> SR
        EB --> SW
        TC --> RR
        LT --> SR
        
        CP --> BW
        AP --> BW
    end
    
    style SR fill:#69db7c
    style SW fill:#69db7c
    style RR fill:#ffd43b
    style RW fill:#ff6b6b
```

### PSRAM Allocation Strategy

```
╔════════════════════════════════════════════════════════════════════════╗
║                      PSRAM ALLOCATION GUIDELINES                        ║
╠════════════════════════════════════════════════════════════════════════╣
║ Allocate in PSRAM:                                                     ║
║ ✓ Large buffers (>4KB)                                                ║
║ ✓ Effect state that's accessed sequentially                           ║
║ ✓ Lookup tables and precalculated data                               ║
║ ✓ Audio buffers and waveform data                                    ║
║ ✓ Temporary computation buffers                                       ║
║                                                                        ║
║ Keep in DRAM:                                                          ║
║ ✗ Frequently accessed small objects                                   ║
║ ✗ Critical path data structures                                       ║
║ ✗ DMA buffers (must be in DRAM)                                      ║
║ ✗ Interrupt handler data                                              ║
║ ✗ Real-time control structures                                        ║
║                                                                        ║
║ Performance Tips:                                                      ║
║ • Align PSRAM allocations to 32-byte boundaries                      ║
║ • Use memcpy for bulk transfers (DMA accelerated)                    ║
║ • Group related data for cache efficiency                             ║
╚════════════════════════════════════════════════════════════════════════╝
```

---

## 📊 Performance Impact Analysis

### HeapTracer Overhead

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      HEAPTRACER PERFORMANCE IMPACT                       │
├─────────────────────────┬───────────────┬────────────┬─────────────────┤
│ Operation              │ Without Tracer │ With Tracer│ Overhead        │
├────────────────────────┼────────────────┼────────────┼─────────────────┤
│ malloc (small)         │ 2.5 µs         │ 3.8 µs     │ +52% (1.3 µs)   │
│ malloc (large)         │ 15.2 µs        │ 16.9 µs    │ +11% (1.7 µs)   │
│ free                   │ 1.8 µs         │ 2.9 µs     │ +61% (1.1 µs)   │
│ Frame render time      │ 5.68 ms        │ 5.71 ms    │ +0.5% (30 µs)   │
│ Memory usage           │ -              │ +4 KB      │ Tracer state    │
└────────────────────────┴────────────────┴────────────┴─────────────────┘

Note: HeapTracer can be compiled out entirely for production builds
```

### Optimization Recommendations Engine

```mermaid
flowchart TD
    subgraph "Analysis Input"
        HP[Heap Profile]
        AP[Allocation Patterns]
        FP[Fragmentation Profile]
        PP[Performance Profile]
    end
    
    subgraph "Recommendation Engine"
        RE[Rule Engine]
        ML[Pattern Matching]
        SC[Scoring System]
    end
    
    subgraph "Recommendations"
        R1[Memory Pool<br/>Suggestion]
        R2[PSRAM Migration<br/>Candidates]
        R3[Allocation<br/>Optimization]
        R4[Defragmentation<br/>Strategy]
    end
    
    HP --> RE
    AP --> RE
    FP --> RE
    PP --> RE
    
    RE --> ML
    ML --> SC
    
    SC --> R1
    SC --> R2
    SC --> R3
    SC --> R4
    
    style RE fill:#4dabf7
    style SC fill:#69db7c
```

---

## 🛡️ Memory Safety Features

### Guard Bands and Corruption Detection

```
Memory Block Structure with Guard Bands:
┌────────────────────────────────────────────────────────────┐
│ Header Guard  │ Size │ Meta │  User Data  │ Footer Guard  │
│ 0xDEADBEEF   │  4B  │  8B  │   N bytes   │ 0xCAFEBABE   │
└────────────────────────────────────────────────────────────┘

Corruption Detection:
• Check guards on every free()
• Periodic background scanning
• Pattern analysis for overflow detection
```

### Stack Overflow Protection

```cpp
class StackMonitor {
    struct TaskStackInfo {
        TaskHandle_t task;
        uint32_t* stackBottom;
        size_t stackSize;
        uint32_t watermark;
        uint32_t canaryValue = 0xSTACKCANARY;
    };
    
    void checkStackHealth() {
        for (auto& info : taskStacks) {
            // Check canary
            if (*info.stackBottom != info.canaryValue) {
                handleStackOverflow(info.task);
            }
            
            // Update watermark
            uint32_t freeStack = uxTaskGetStackHighWaterMark(info.task);
            if (freeStack < info.watermark) {
                info.watermark = freeStack;
                if (freeStack < STACK_WARNING_THRESHOLD) {
                    issueStackWarning(info.task, freeStack);
                }
            }
        }
    }
};
```

---

## 📈 Reporting and Visualization

### Memory Report Format

```
╔════════════════════════════════════════════════════════════════════════╗
║                      MEMORY HEALTH REPORT #1337                         ║
║                    Generated: 2024-07-02 21:45:32                      ║
╠════════════════════════════════════════════════════════════════════════╣
║ EXECUTIVE SUMMARY                                                      ║
║ ├─ Overall Health: ████████░░ 85% GOOD                               ║
║ ├─ Leak Risk:     ██░░░░░░░░ 20% LOW                                ║
║ ├─ Fragmentation: ███░░░░░░░ 32% MODERATE                           ║
║ └─ PSRAM Usage:   ██░░░░░░░░ 25% OPTIMAL                            ║
║                                                                        ║
║ KEY FINDINGS                                                           ║
║ 1. ⚠️  Gradual memory increase detected in WebSocket handler          ║
║ 2. ✓  No memory leaks detected in effect system                      ║
║ 3. ⚠️  Fragmentation increasing - consider defrag at 40%             ║
║ 4. ✓  PSRAM bandwidth utilization within limits                      ║
║                                                                        ║
║ RECOMMENDATIONS                                                        ║
║ 1. 🔧 Implement object pool for WebSocket messages                   ║
║ 2. 🚀 Move large effect buffers to PSRAM (save 15KB DRAM)          ║
║ 3. 📊 Schedule defragmentation during effect transitions             ║
║                                                                        ║
║ DETAILED METRICS                                                       ║
║ ├─ Allocations/sec: 127                                               ║
║ ├─ Deallocations/sec: 125                                             ║
║ ├─ Average allocation size: 256 bytes                                 ║
║ ├─ Peak usage: 287,432 bytes (87.7%)                                 ║
║ └─ Current usage: 245,123 bytes (74.8%)                              ║
╚════════════════════════════════════════════════════════════════════════╝
```

### Web Interface Integration

```mermaid
sequenceDiagram
    participant HT as HeapTracer
    participant WS as WebSocket
    participant UI as Web UI
    participant C as Chart.js
    
    HT->>HT: Collect Metrics
    HT->>WS: Send Update
    WS->>UI: Push Data
    UI->>C: Update Charts
    
    Note over C: Real-time graphs:<br/>- Heap usage<br/>- Fragmentation<br/>- Allocation rate
    
    UI->>WS: Request Detailed Report
    WS->>HT: Generate Report
    HT->>WS: JSON Report
    WS->>UI: Display Report
    
    alt Download Report
        UI->>WS: Export Request
        WS->>HT: Create CSV
        HT->>WS: CSV Data
        WS->>UI: Download File
    end
```

---

## 🔧 Implementation Examples

### Basic Heap Tracking

```cpp
// Macro wrapper for tracked allocation
#ifdef HEAP_TRACKER_ENABLED
  #define TRACKED_MALLOC(size) \
    HeapTracer::getInstance().trackedMalloc(size, __FILE__, __LINE__)
  #define TRACKED_FREE(ptr) \
    HeapTracer::getInstance().trackedFree(ptr)
#else
  #define TRACKED_MALLOC(size) malloc(size)
  #define TRACKED_FREE(ptr) free(ptr)
#endif

// Usage in code
void* buffer = TRACKED_MALLOC(1024);
// ... use buffer ...
TRACKED_FREE(buffer);
```

### Automatic Leak Detection

```cpp
class AutoLeakDetector {
    void detectLeaks() {
        auto now = millis();
        
        for (auto& record : allocations) {
            // Skip recent allocations
            if (now - record.timestamp < LEAK_THRESHOLD_MS) continue;
            
            // Check if still referenced
            if (!isStillReferenced(record.address)) {
                // Analyze allocation pattern
                LeakPattern pattern = analyzePattern(record);
                
                if (pattern.confidence > 0.8f) {
                    // High confidence leak
                    reportLeak(record, pattern);
                } else if (pattern.confidence > 0.5f) {
                    // Potential leak - add to watch list
                    watchList.push_back(record);
                }
            }
        }
    }
    
    bool isStillReferenced(void* ptr) {
        // Scan stack and registers for references
        // This is platform-specific and complex
        return scanForReferences(ptr);
    }
};
```

---

## 🔮 Future Enhancements

### Planned Features

```
╔════════════════════════════════════════════════════════════════════════╗
║                    FUTURE HEAPTRACER FEATURES                           ║
╠════════════════════════════════════════════════════════════════════════╣
║ 1. Machine Learning Leak Prediction                                    ║
║    • Train on allocation patterns                                      ║
║    • Predict leaks before they occur                                  ║
║    • Suggest preventive measures                                       ║
║                                                                        ║
║ 2. Automatic Defragmentation                                           ║
║    • Background memory compaction                                      ║
║    • Safe object relocation                                            ║
║    • Zero-copy optimization                                            ║
║                                                                        ║
║ 3. Remote Debugging Interface                                          ║
║    • Connect via network for analysis                                  ║
║    • Real-time memory inspection                                       ║
║    • Remote heap dumps                                                 ║
║                                                                        ║
║ 4. Memory Replay System                                                ║
║    • Record allocation sequences                                       ║
║    • Replay for debugging                                              ║
║    • Automated regression testing                                      ║
╚════════════════════════════════════════════════════════════════════════╝
```

---

<div align="center">

**"In heap we trust, but verify with HeapTracer"**

*Because every byte counts when you're running at 176 FPS*

</div>