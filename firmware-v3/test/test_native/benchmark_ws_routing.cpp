/**
 * @file benchmark_ws_routing.cpp
 * @brief Performance benchmark for WsCommandRouter dispatch
 *
 * Registers 141 handlers (matching production count), then measures
 * dispatch latency across 10,000 iterations. Reports p50/p95/p99
 * percentiles and ops/sec. Outputs JSON for baseline comparison.
 *
 * Build: pio run -e native_benchmark_ws_routing
 * Run:   .pio/build/native_benchmark_ws_routing/program
 */

#ifdef NATIVE_BUILD

#include <cstdio>
#include <chrono>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdlib>

// Stub types for WebServerContext construction
namespace lightwaveos {
namespace actors {
    class ActorSystem { public: char _pad = 0; };
    class RendererActor {};
}
namespace zones {
    class ZoneComposer {};
}
namespace plugins {
    class PluginManagerActor {};
}
namespace network {
    class WebServer {};
    namespace webserver {
        class RateLimiter { public: char _pad = 0; };
        class LedStreamBroadcaster {};
        class LogStreamBroadcaster {};
    }
}
}

#include "network/webserver/WsCommandRouter.h"
#include "network/webserver/WebServerContext.h"

using namespace lightwaveos::network::webserver;

// No-op handler — measuring dispatch overhead only
static void noop_handler(AsyncWebSocketClient*, JsonDocument&, const WebServerContext&) {}

static lightwaveos::actors::ActorSystem s_dummyActorSystem;
static RateLimiter s_dummyRateLimiter;

static WebServerContext makeDummyContext() {
    return WebServerContext(
        s_dummyActorSystem, nullptr, nullptr, nullptr,
        s_dummyRateLimiter, nullptr, nullptr, 0, false
    );
}

int main() {
    constexpr size_t NUM_HANDLERS = 141;
    constexpr size_t WARMUP = 1000;
    constexpr size_t ITERATIONS = 10000;

    // Register 141 handlers with unique stable names
    static char cmdNames[NUM_HANDLERS][32];
    for (size_t i = 0; i < NUM_HANDLERS; i++) {
        snprintf(cmdNames[i], sizeof(cmdNames[i]), "bench.cmd.%03zu", i);
        WsCommandRouter::registerCommand(cmdNames[i], noop_handler);
    }

    printf("WebSocket Routing Benchmark\n");
    printf("===========================\n\n");
    printf("Handlers: %zu / %zu (%.0f%% utilisation)\n",
           WsCommandRouter::getHandlerCount(),
           WsCommandRouter::getMaxHandlers(),
           100.0 * WsCommandRouter::getHandlerCount() / WsCommandRouter::getMaxHandlers());

    auto ctx = makeDummyContext();
    AsyncWebSocketClient client;

    // Benchmark three positions: first, middle, last
    struct Scenario {
        const char* label;
        size_t index;
    };
    Scenario scenarios[] = {
        {"first (best)",   0},
        {"middle (avg)",   NUM_HANDLERS / 2},
        {"last (worst)",   NUM_HANDLERS - 1},
    };

    // JSON preamble
    printf("\n{\n");
    printf("  \"benchmark\": \"ws_routing\",\n");
    printf("  \"handlers\": %zu,\n", NUM_HANDLERS);
    printf("  \"capacity\": %zu,\n", WsCommandRouter::getMaxHandlers());
    printf("  \"utilisation_pct\": %.1f,\n",
           100.0 * NUM_HANDLERS / WsCommandRouter::getMaxHandlers());
    printf("  \"iterations\": %zu,\n", ITERATIONS);
    printf("  \"warmup\": %zu,\n", WARMUP);
    printf("  \"scenarios\": [\n");

    for (size_t s = 0; s < 3; s++) {
        const char* targetCmd = cmdNames[scenarios[s].index];

        printf("\n--- %s: \"%s\" (index %zu) ---\n", scenarios[s].label,
               targetCmd, scenarios[s].index);

        // Warmup
        for (size_t i = 0; i < WARMUP; i++) {
            JsonDocument doc;
            doc["type"] = targetCmd;
            WsCommandRouter::route(&client, doc, ctx);
        }

        // Timed iterations
        std::vector<double> timings(ITERATIONS);
        JsonDocument doc;
        for (size_t i = 0; i < ITERATIONS; i++) {
            doc.clear();
            doc["type"] = targetCmd;

            auto start = std::chrono::high_resolution_clock::now();
            WsCommandRouter::route(&client, doc, ctx);
            auto end = std::chrono::high_resolution_clock::now();

            timings[i] = std::chrono::duration<double, std::micro>(end - start).count();
        }

        // Calculate percentiles
        std::sort(timings.begin(), timings.end());
        double p50 = timings[ITERATIONS * 50 / 100];
        double p95 = timings[ITERATIONS * 95 / 100];
        double p99 = timings[ITERATIONS * 99 / 100];
        double avg = 0;
        for (auto t : timings) avg += t;
        avg /= static_cast<double>(ITERATIONS);
        size_t ops = (avg > 0) ? static_cast<size_t>(1000000.0 / avg) : 0;

        printf("  p50: %.3f us\n", p50);
        printf("  p95: %.3f us\n", p95);
        printf("  p99: %.3f us\n", p99);
        printf("  avg: %.3f us\n", avg);
        printf("  ops/sec: %zu\n", ops);

        // JSON scenario entry
        printf("    %s{\n", s > 0 ? "," : "");
        printf("      \"position\": \"%s\",\n", scenarios[s].label);
        printf("      \"target\": \"%s\",\n", targetCmd);
        printf("      \"index\": %zu,\n", scenarios[s].index);
        printf("      \"p50_us\": %.3f,\n", p50);
        printf("      \"p95_us\": %.3f,\n", p95);
        printf("      \"p99_us\": %.3f,\n", p99);
        printf("      \"avg_us\": %.3f,\n", avg);
        printf("      \"ops_per_sec\": %zu\n", ops);
        printf("    }\n");
    }

    printf("  ]\n}\n");

    return 0;
}

#endif // NATIVE_BUILD
