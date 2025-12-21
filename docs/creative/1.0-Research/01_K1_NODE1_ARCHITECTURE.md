# K1.node1 Dashboard Architecture Analysis

> Comprehensive analysis of the PRISM.node2 Control Dashboard for adoption in LightwaveOS

## Executive Summary

**Project**: PRISM.node2 Control Dashboard
**Stack**: React 18 + TypeScript + Vite + TanStack Query + Redux Toolkit
**Total Files**: 183 source files (112 components)
**Estimated LOC**: ~30,000
**Primary Purpose**: Dual-platform (web + Tauri) control interface for ESP32-based LED hardware

---

## 1. Directory Structure

```
/K1.node1/webapp/src/
├── __tests__/          # Root-level test suites
├── backend/            # React Query hooks for device/analysis APIs
├── builder/            # Builder.io visual editor integration (6 files)
├── components/         # React components (112 files across 11 subdirs)
│   ├── views/          # 9 main view components
│   ├── control/        # 11 control panel components
│   ├── analysis/       # 17 analysis components
│   ├── profiling/      # 3 performance chart components
│   ├── graph/          # 7 graph editor components
│   ├── visualization/  # LED visualization renderer
│   ├── ui/             # 48 shadcn/ui components
│   ├── common/         # Shared utilities
│   ├── a11y/           # Accessibility utilities
│   └── dev/            # Developer tools
├── config/             # Runtime configuration (4 files)
├── guidelines/         # Documentation and style guides
├── hooks/              # Custom React hooks (9 files)
├── lib/                # Core utilities and business logic (26 files)
├── pages/              # Page-level components (2 files)
├── services/           # API service layer (2 files)
├── store/              # Redux state management (slices + context)
├── stubs/              # TypeScript type stubs
├── styles/             # CSS and design tokens
└── test/               # Test setup and utilities
```

---

## 2. API Architecture (Dual-Layer Strategy)

### 2.1 API v1: Device Communication Layer

**Location**: `/webapp/src/lib/api.ts`
**Base URL**: `http://<device-ip>/api`
**Purpose**: Direct ESP32 firmware control

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/api/patterns` | GET | Fetch effect list + current index |
| `/api/params` | GET/POST | Get/update parameters (with interference gate) |
| `/api/palettes` | GET | Get color palettes |
| `/api/select` | POST | Change effect by id/index |
| `/api/audio-config` | GET/POST | Audio settings |
| `/api/device/performance` | GET | Performance metrics |
| `/api/test-connection` | GET | Connectivity check (5s timeout) |

**Special Features**:
- **GET/POST Interference Gate**: Prevents reading stale data during writes
- **CORS Fallback**: Attempts `no-cors` mode if preflight fails
- **Proxy Fallback**: Falls back to `/api` proxy in development

### 2.2 API v2: Orchestration Layer

**Location**: `/webapp/src/services/api.ts`
**Base URL**: `http://localhost:3000/api/v2`
**Purpose**: Backend workflow orchestration, metrics, error recovery

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/api/v2/metrics/retry-stats` | GET | Retry statistics |
| `/api/v2/circuit-breaker/status` | GET | Circuit breaker status |
| `/api/v2/queue/dlq` | GET | Dead letter queue entries |
| `/api/v2/scheduler/schedules` | CRUD | Schedule management |
| `/api/v2/metrics/scheduling` | GET | Scheduling metrics |
| `/api/v2/metrics/error-recovery` | GET | Error recovery metrics |

---

## 3. Real-Time Communication

### 3.1 WebSocket Service

**Location**: `/webapp/src/services/websocket.ts`

| Feature | Implementation |
|---------|----------------|
| Auto-reconnect | 10 attempts, exponential backoff |
| Heartbeat | 30s ping/pong interval |
| Message batching | 100ms window |
| Fallback | REST polling (5s interval) |

**Message Types**:
```typescript
type MessageType =
  | 'scheduling:update'      // Schedule state changes
  | 'errorRecovery:update'   // Error recovery metrics
  | 'metrics:batch'          // Batched multi-metric update
  | 'connection:ping'        // Heartbeat ping
  | 'connection:pong';       // Heartbeat pong
```

**Reconnection Strategy**:
- Maximum 10 attempts
- Exponential backoff: `3000ms * 2^(attempt - 1)`
- After max attempts: Fallback to 5s polling

---

## 4. State Management Architecture

```
┌──────────────────────────────────────────────────────────┐
│  TanStack Query (Server State)                           │
│  - Device status, patterns, palettes, params             │
│  - 5s stale time, 10s cache, auto-refetch                │
└──────────────────────────────────────────────────────────┘
                          ↓
┌──────────────────────────────────────────────────────────┐
│  Redux Toolkit (Global UI State)                         │
│  - connectionSlice: WebSocket status, device IP          │
│  - schedulingSlice: Schedules, queue status              │
│  - errorRecoverySlice: Retry metrics, circuit breakers   │
│  - uiSlice: Expanded rows, selected items, theme         │
└──────────────────────────────────────────────────────────┘
                          ↓
┌──────────────────────────────────────────────────────────┐
│  React Context (Feature State)                           │
│  - NodeAuthoringProvider: Graph editor state             │
└──────────────────────────────────────────────────────────┘
                          ↓
┌──────────────────────────────────────────────────────────┐
│  Local State (Component State)                           │
│  - Form inputs, transient UI, animations                 │
└──────────────────────────────────────────────────────────┘
```

### 4.1 Redux Store Structure

```typescript
// store/index.ts
const rootReducer = combineReducers({
  errorRecovery: errorRecoveryReducer,  // Retry stats, circuit breakers, DLQ
  scheduling: schedulingReducer,         // Schedules, queue status, resources
  ui: uiReducer,                         // Expanded rows, selected items
  connection: connectionReducer,         // WebSocket connected, device IP
});
```

### 4.2 React Query Configuration

```typescript
const queryClient = new QueryClient({
  defaultOptions: {
    queries: {
      retry: 1,                      // Fail fast
      refetchOnWindowFocus: false,   // Avoid device hammering
      staleTime: 5000,               // 5s cache
      gcTime: 10000,                 // 10s garbage collection
    },
  },
});
```

---

## 5. Component Architecture

### 5.1 View Layer

| View | Purpose | Key Features |
|------|---------|--------------|
| **ControlPanelView** | Main effect control | Effect selection, parameters, colors, global settings |
| **ProfilingView** | Performance monitoring | FPS charts, CPU/memory graphs, frame time |
| **TerminalView** | Command interface | Serial port emulation, command history |
| **NodeEditorView** | Visual programming | Drag-drop nodes, wire connections |
| **AnalysisView** | Audio analysis | Track management, beat grid, frequency charts |
| **DiagnosticsView** | System diagnostics | Device health checks, connectivity tests |

### 5.2 Control Components

| Component | Purpose |
|-----------|---------|
| **EffectSelector** | 9-grid effect cards |
| **EffectParameters** | Dynamic parameter sliders |
| **ColorManagement** | Palette picker + HSV sliders |
| **GlobalSettings** | Brightness, softness, warmth |
| **ModeSelectors** | Void Trail, Audio Reactivity toggles |
| **StatusBar** | Real-time FPS/CPU/Memory metrics |
| **ParamSlider** | Reusable slider with debounce |

---

## 6. Custom Hooks

### 6.1 Parameter Management

| Hook | Purpose |
|------|---------|
| **useCoalescedParams** | Debounce/coalesce slider updates (80ms) |
| **useParameterTransport** | WS/REST transport with retry + backoff |
| **useParameterPersistence** | LocalStorage persistence |
| **useOptimisticPatternSelection** | Optimistic UI for pattern changes |

### 6.2 useCoalescedParams Details

```typescript
interface CoalescedParamsSenderOptions {
  onSend: (params: Partial<UIParams>) => Promise<void>;
  delay?: number;           // Trailing delay (default: 80ms)
  leadingEdge?: boolean;    // Send immediately on first change
  maxWait?: number;         // Force flush after maxWait (default: 500ms)
}

// Usage
const { scheduleSend, flush, cancel } = useCoalescedParams({
  onSend: async (params) => {
    await postParams(deviceIp, uiToFirmwareParams(params));
  },
  delay: 80,
  leadingEdge: true,
  maxWait: 500
});

// Reduces API calls by 70-90% during slider scrubbing
```

---

## 7. Performance Optimizations

### 7.1 Code Splitting & Lazy Loading

```tsx
// Lazy-loaded heavy views
const ProfilingView = lazy(() => import('./components/views/ProfilingView'));
const TerminalView = lazy(() => import('./components/views/TerminalView'));
const NodeEditorView = lazy(() => import('./components/views/NodeEditorView'));

// Idle prefetching
useEffect(() => {
  if (!idlePrefetch) return;
  const id = requestIdleCallback(() => {
    if (currentView === 'control') {
      void import('./components/views/ProfilingView');
      void import('recharts');
    }
  });
  return () => cancelIdleCallback(id);
}, [currentView, idlePrefetch]);
```

### 7.2 Debouncing & Coalescing

| Strategy | Delay | Purpose |
|----------|-------|---------|
| Parameter changes | 80ms | Reduce API calls during slider scrubbing |
| Leading edge | Immediate | First change sends instantly |
| Max wait | 500ms | Force flush for long interactions |
| Search inputs | 500ms | Prevent excessive searches |

---

## 8. Performance Monitoring Requirements

| Metric | Update Rate | Thresholds |
|--------|-------------|------------|
| FPS | 100ms (~10Hz) | Green ≥100, Amber 60-99, Red <60 |
| Frame Time | 100ms | Green <10ms, Amber 10-16ms, Red >16ms |
| CPU Usage | 200ms (~5Hz) | Green 0-50%, Amber 51-80%, Red >80% |
| Memory Usage | 500ms (~2Hz) | Green <70%, Amber 70-85%, Red >85% |
| Effect Time | 100ms | Target <8ms for 120 FPS |

---

## 9. Key Patterns to Adopt for LightwaveOS

### 9.1 Critical Patterns

| Pattern | Location | Benefit |
|---------|----------|---------|
| **CORS Fallback** | `lib/api.ts` | Works with ESP32 firmware |
| **Interference Gate** | `lib/api.ts` | Prevents stale reads during writes |
| **Parameter Coalescing** | `hooks/useCoalescedParams.ts` | 70-90% fewer API calls |
| **WebSocket + Polling** | `services/websocket.ts` | Graceful degradation |
| **Optimistic UI** | `hooks/useOptimisticPatternSelection.ts` | Instant feedback |

### 9.2 Code Examples

**CORS Fallback Pattern**:
```typescript
async function postParams(ip: string, partial: Partial<FirmwareParams>) {
  try {
    return await handleJsonMutation(...); // Try with CORS
  } catch (err) {
    await sendWithoutCors(...); // Fallback to opaque no-cors
    return { ok: true, confirmed: false, fallback: 'no-cors' };
  }
}
```

**Parameter Coalescing Pattern**:
```typescript
// Slider onChange
scheduleSend('brightness', 75); // Immediately sends first change
scheduleSend('brightness', 80); // Coalesced, waits 80ms
scheduleSend('brightness', 85); // Coalesced again
// After 80ms: sends brightness=85 (single API call)
```

---

## 10. File Reference Index

### Critical Files for New Developers

| Category | File Path | Purpose |
|----------|-----------|---------|
| **Entry** | `/src/main.tsx` | React root, providers |
| **Shell** | `/src/App.tsx` | Main layout, routing |
| **Device API** | `/src/lib/api.ts` | Firmware communication |
| **Backend API** | `/src/services/api.ts` | Orchestration layer |
| **WebSocket** | `/src/services/websocket.ts` | Real-time service |
| **Redux** | `/src/store/index.ts` | State configuration |
| **Hooks** | `/src/hooks/useDashboard.ts` | Dashboard state |
| **Types** | `/src/lib/types.ts` | TypeScript definitions |
| **Tokens** | `/src/styles/globals.css` | Design system |

---

## Summary Statistics

```
Total Source Files: 183
├── TypeScript/TSX: 170
├── CSS: 2
└── JSON/Config: 11

Components: 112
├── Views: 9
├── Control Panel: 11
├── Analysis: 17
├── UI Library: 48
├── Other: 27

Custom Hooks: 9
API Endpoints (Firmware): 10
API Endpoints (Backend): 20+
WebSocket Channels: 2
State Slices: 4 (Redux) + 1 (Context)
Test Files: 13
```
