/**
 * ESP-NOW Clock Sync Slave — T1 Static Offset Test
 *
 * Receives timestamped sync pulses from master. Records local
 * arrival time, computes raw clock offset (slave_us - master_us),
 * and accumulates statistics over a sliding window.
 *
 * After WARMUP_PULSES, prints running stats every REPORT_INTERVAL pulses:
 *   - mean, median, min, max, stddev of clock offset
 *   - jitter (offset variation between consecutive pulses)
 *   - packet loss rate
 *
 * Arena 9 validation: distributed coherence empirical testing.
 */
#include <Arduino.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <cstring>
#include <cmath>
#include <algorithm>

#pragma pack(push, 1)
struct SyncPacket {
    uint32_t seq;
    int64_t  master_us;
};
#pragma pack(pop)

// ── Configuration ──
static const uint32_t WARMUP_PULSES    = 10;   // Discard first N (clocks settling)
static const uint32_t REPORT_INTERVAL  = 20;   // Stats every N pulses
static const uint32_t WINDOW_SIZE      = 200;  // Sliding window for stats

// ── State ──
static volatile uint32_t g_rx_count = 0;
static volatile uint32_t g_rx_total = 0;
static volatile uint32_t g_last_seq = 0;
static volatile bool     g_new_data = false;
static volatile int64_t  g_last_offset_us = 0;
static volatile int64_t  g_last_slave_us = 0;
static volatile uint32_t g_last_master_seq = 0;

// Offset history (circular buffer)
static int64_t  g_offsets[WINDOW_SIZE];
static int64_t  g_jitter[WINDOW_SIZE];
static uint32_t g_offset_idx = 0;
static uint32_t g_offset_count = 0;
static uint32_t g_jitter_idx = 0;
static uint32_t g_jitter_count = 0;
static int64_t  g_prev_offset = 0;
static uint32_t g_missed_packets = 0;
static uint32_t g_prev_seq = UINT32_MAX;

static void IRAM_ATTR onRecv(const uint8_t *mac, const uint8_t *data, int len) {
    int64_t slave_us = esp_timer_get_time();  // Timestamp ASAP

    if (len != sizeof(SyncPacket)) return;

    SyncPacket pkt;
    memcpy(&pkt, data, sizeof(pkt));

    g_rx_total++;

    // Track missed packets
    if (g_prev_seq != UINT32_MAX && pkt.seq > g_prev_seq + 1) {
        g_missed_packets += (pkt.seq - g_prev_seq - 1);
    }
    g_prev_seq = pkt.seq;

    int64_t offset = slave_us - pkt.master_us;

    g_last_offset_us = offset;
    g_last_slave_us = slave_us;
    g_last_master_seq = pkt.seq;
    g_rx_count++;
    g_new_data = true;
}

// ── Statistics helpers ──
struct Stats {
    double mean;
    double median;
    double stddev;
    int64_t min_val;
    int64_t max_val;
    uint32_t count;
};

static Stats computeStats(int64_t *buf, uint32_t count) {
    Stats s = {};
    if (count == 0) return s;
    s.count = count;

    // Copy for sorting (median)
    int64_t sorted[WINDOW_SIZE];
    uint32_t n = (count < WINDOW_SIZE) ? count : WINDOW_SIZE;
    memcpy(sorted, buf, n * sizeof(int64_t));
    std::sort(sorted, sorted + n);

    s.min_val = sorted[0];
    s.max_val = sorted[n - 1];
    s.median = (n % 2 == 0) ? (sorted[n/2 - 1] + sorted[n/2]) / 2.0 : sorted[n/2];

    double sum = 0;
    for (uint32_t i = 0; i < n; i++) sum += sorted[i];
    s.mean = sum / n;

    double var = 0;
    for (uint32_t i = 0; i < n; i++) {
        double d = sorted[i] - s.mean;
        var += d * d;
    }
    s.stddev = sqrt(var / n);

    return s;
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("=== ESP-NOW SYNC SLAVE — T1 Static Offset ===");

    // WiFi init — AP_STA bypasses STA broadcast filter (ESP-IDF #10341)
    WiFi.mode(WIFI_AP_STA);
    WiFi.disconnect();
    delay(100);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

    esp_err_t err = esp_now_init();
    Serial.printf("esp_now_init: %s\n", esp_err_to_name(err));

    err = esp_now_register_recv_cb(onRecv);
    Serial.printf("register_recv_cb: %s\n", esp_err_to_name(err));

    // Re-force channel after esp_now_init
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

    uint8_t ch = 0; wifi_second_chan_t sch;
    esp_wifi_get_channel(&ch, &sch);
    uint8_t myMac[6]; WiFi.macAddress(myMac);
    Serial.printf("CH=%d MAC=%02X:%02X:%02X:%02X:%02X:%02X\n",
        ch, myMac[0], myMac[1], myMac[2], myMac[3], myMac[4], myMac[5]);
    Serial.printf("Warmup: %u pulses, Report every: %u pulses, Window: %u\n",
        WARMUP_PULSES, REPORT_INTERVAL, WINDOW_SIZE);
    Serial.println("Listening...");
}

void loop() {
    if (!g_new_data) {
        // Alive heartbeat every 5 seconds
        static uint32_t last_hb = 0;
        if (millis() - last_hb >= 5000) {
            last_hb = millis();
            Serial.printf("ALIVE rx=%u missed=%u\n", g_rx_total, g_missed_packets);
        }
        delay(1);
        return;
    }
    g_new_data = false;

    uint32_t rx = g_rx_count;
    int64_t offset = g_last_offset_us;
    uint32_t seq = g_last_master_seq;

    // Skip warmup
    if (rx <= WARMUP_PULSES) {
        Serial.printf("WARMUP %u/%u seq=%u offset=%lldus\n",
            rx, WARMUP_PULSES, seq, offset);
        g_prev_offset = offset;
        return;
    }

    // Record offset
    g_offsets[g_offset_idx] = offset;
    g_offset_idx = (g_offset_idx + 1) % WINDOW_SIZE;
    if (g_offset_count < WINDOW_SIZE) g_offset_count++;

    // Record jitter (offset delta between consecutive pulses)
    int64_t jit = offset - g_prev_offset;
    g_jitter[g_jitter_idx] = jit;
    g_jitter_idx = (g_jitter_idx + 1) % WINDOW_SIZE;
    if (g_jitter_count < WINDOW_SIZE) g_jitter_count++;
    g_prev_offset = offset;

    // Report
    uint32_t data_rx = rx - WARMUP_PULSES;
    if (data_rx % REPORT_INTERVAL == 0) {
        Stats os = computeStats(g_offsets, g_offset_count);
        Stats js = computeStats(g_jitter, g_jitter_count);

        Serial.println("========================================");
        Serial.printf("SYNC STATS @ pulse %u (window=%u)\n", data_rx, os.count);
        Serial.printf("  Offset mean:   %.1f us (%.3f ms)\n", os.mean, os.mean / 1000.0);
        Serial.printf("  Offset median: %.1f us (%.3f ms)\n", os.median, os.median / 1000.0);
        Serial.printf("  Offset stddev: %.1f us (%.3f ms)\n", os.stddev, os.stddev / 1000.0);
        Serial.printf("  Offset range:  [%lld, %lld] us\n", os.min_val, os.max_val);
        Serial.printf("  Jitter mean:   %.1f us (%.3f ms)\n", js.mean, js.mean / 1000.0);
        Serial.printf("  Jitter stddev: %.1f us (%.3f ms)\n", js.stddev, js.stddev / 1000.0);
        Serial.printf("  Jitter range:  [%lld, %lld] us\n", js.min_val, js.max_val);
        Serial.printf("  Packets: rx=%u missed=%u loss=%.1f%%\n",
            g_rx_total, g_missed_packets,
            g_rx_total > 0 ? (g_missed_packets * 100.0 / (g_rx_total + g_missed_packets)) : 0.0);
        Serial.println("========================================");
    }
}
