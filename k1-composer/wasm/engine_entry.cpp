#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

namespace {
constexpr uint16_t kLedCount = 320;
constexpr uint16_t kFrameBytes = kLedCount * 3;

uint8_t g_frame[kFrameBytes];
char g_trace[256];
uint16_t g_effectId = 0;
float g_speed = 20.0f;
float g_intensity = 170.0f;
float g_phase = 0.0f;

void write_trace(const char* reason) {
  snprintf(g_trace, sizeof(g_trace),
           "{\"effectId\":%u,\"phase\":%.4f,\"speed\":%.2f,\"intensity\":%.2f,\"reason\":\"%s\"}",
           static_cast<unsigned>(g_effectId),
           static_cast<double>(g_phase),
           static_cast<double>(g_speed),
           static_cast<double>(g_intensity),
           reason ? reason : "tick");
}

void render_frame() {
  for (uint16_t i = 0; i < kLedCount; ++i) {
    const float x = static_cast<float>(i) / static_cast<float>(kLedCount - 1);
    const float wave = 0.5f + 0.5f * sinf((x * 24.0f + g_phase * g_speed * 0.05f));
    const uint8_t v = static_cast<uint8_t>(wave * g_intensity);
    const uint32_t px = static_cast<uint32_t>(i) * 3u;
    g_frame[px + 0] = v;
    g_frame[px + 1] = static_cast<uint8_t>(v * 0.62f);
    g_frame[px + 2] = static_cast<uint8_t>(255u - v);
  }
}
}

extern "C" {

void composer_load_effect(uint16_t effect_id) {
  g_effectId = effect_id;
  write_trace("load_effect");
}

void composer_set_param(uint8_t param_id, float value) {
  switch (param_id) {
    case 0: g_speed = value; break;
    case 1: g_intensity = value; break;
    default: break;
  }
  write_trace("set_param");
}

void composer_tick(float dt_ms) {
  g_phase += dt_ms * 0.001f;
  if (g_phase > 10000.0f) {
    g_phase = 0.0f;
  }
  render_frame();
  write_trace("tick");
}

void composer_scrub(float t_norm) {
  g_phase = t_norm * 10.0f;
  render_frame();
  write_trace("scrub");
}

uint8_t* composer_get_frame_ptr() {
  return g_frame;
}

uint16_t composer_get_frame_size() {
  return kFrameBytes;
}

const char* composer_get_trace_ptr() {
  return g_trace;
}

}
