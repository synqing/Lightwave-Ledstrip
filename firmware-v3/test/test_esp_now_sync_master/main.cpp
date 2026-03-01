/**
 * ESP-NOW Clock Sync Master — T1 Static Offset Test
 *
 * Broadcasts timestamped sync pulses every 500ms via ESP-NOW.
 * Slave measures local arrival time vs embedded master timestamp
 * to compute raw clock offset statistics.
 *
 * Arena 9 validation: distributed coherence empirical testing.
 * Perceptual target: <2ms strobe, <5ms chase/wave, <20ms beat pulse.
 *
 * Packet format: [SEQ:4][MASTER_MICROS:8] = 12 bytes
 */
#include <Arduino.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <cstring>

static const uint8_t BROADCAST_ADDR[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static uint32_t g_seq = 0;
static volatile bool g_send_done = false;
static volatile bool g_send_ok = false;
static uint32_t g_send_fail = 0;

#pragma pack(push, 1)
struct SyncPacket {
    uint32_t seq;
    int64_t  master_us;  // esp_timer_get_time() at send
};
#pragma pack(pop)

static void onSend(const uint8_t *mac, esp_now_send_status_t status) {
    g_send_ok = (status == ESP_NOW_SEND_SUCCESS);
    g_send_done = true;
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("=== ESP-NOW SYNC MASTER — T1 Static Offset ===");

    // WiFi init — AP_STA bypasses STA broadcast filter (ESP-IDF #10341)
    WiFi.mode(WIFI_AP_STA);
    WiFi.disconnect();
    delay(100);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

    esp_err_t err = esp_now_init();
    Serial.printf("esp_now_init: %s\n", esp_err_to_name(err));

    esp_now_register_send_cb(onSend);

    // Re-force channel after esp_now_init
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

    // Add broadcast peer
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, BROADCAST_ADDR, 6);
    peer.channel = 0;
    peer.encrypt = false;
    peer.ifidx = WIFI_IF_STA;
    err = esp_now_add_peer(&peer);
    Serial.printf("add_peer: %s\n", esp_err_to_name(err));

    uint8_t ch = 0; wifi_second_chan_t sch;
    esp_wifi_get_channel(&ch, &sch);
    uint8_t myMac[6]; WiFi.macAddress(myMac);
    Serial.printf("CH=%d MAC=%02X:%02X:%02X:%02X:%02X:%02X\n",
        ch, myMac[0], myMac[1], myMac[2], myMac[3], myMac[4], myMac[5]);
    Serial.printf("Packet size: %u bytes\n", (unsigned)sizeof(SyncPacket));
    Serial.println("TX interval: 500ms");
    Serial.println("Broadcasting...");
}

void loop() {
    SyncPacket pkt;
    pkt.seq = g_seq;
    pkt.master_us = esp_timer_get_time();

    g_send_done = false;
    esp_now_send(BROADCAST_ADDR, (const uint8_t*)&pkt, sizeof(pkt));

    uint32_t t0 = millis();
    while (!g_send_done && (millis() - t0 < 50)) delayMicroseconds(100);

    if (!g_send_done || !g_send_ok) g_send_fail++;

    // Print every 10th packet to avoid serial flood
    if (g_seq % 10 == 0) {
        Serial.printf("TX seq=%u ok=%d fails=%u t=%lld\n",
            g_seq, g_send_done ? (g_send_ok ? 1 : 0) : -1,
            g_send_fail, pkt.master_us);
    }

    g_seq++;
    delay(500);  // 2 Hz sync pulse rate
}
