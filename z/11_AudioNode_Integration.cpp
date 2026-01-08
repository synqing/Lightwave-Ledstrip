/**
 * @file AudioNode_Integration.cpp
 * @brief K1 Front-End Integration in AudioNode
 * 
 * This file shows the integration point where K1 processes audio
 * and TempoTracker consumes the features.
 */

// From AudioNode.cpp:processHop() - K1 Front-End Integration

// K1 Front-End: Process hop and publish feature frame
k1::AudioFeatureFrame k1Frame;
// #region agent log
static uint32_t k1_log_counter = 0;
if ((k1_log_counter++ % 125) == 0) {  // Log every ~1 second
    char k1_check[256];
    snprintf(k1_check, sizeof(k1_check),
        "{\"k1_initialized\":%d,\"hop_buffer_min\":%d,\"hop_buffer_max\":%d,\"sample_index\":%llu,\"hypothesisId\":\"F\"}",
        m_k1FrontEnd.isInitialized() ? 1 : 0, minRaw, maxRaw, (unsigned long long)m_sampleIndex);
    // Use debug_log from TempoTracker pattern - need to include AudioDebugConfig.h
    // For now, use printf directly
    uint64_t t_us = (m_sampleIndex * 1000000ULL) / 16000;
    printf("DEBUG_JSON:{\"location\":\"AudioNode.cpp:processHop\",\"message\":\"k1_check\",\"data\":%s,\"timestamp\":%llu}\n",
           k1_check, (unsigned long long)t_us);
}
// #endregion
if (m_k1FrontEnd.isInitialized()) {
    k1::AudioChunk chunk;
    memcpy(chunk.samples, m_hopBuffer, HOP_SIZE * sizeof(int16_t));
    chunk.n = HOP_SIZE;
    chunk.sample_counter_end = m_sampleIndex;
    
    bool is_clipping = (maxRaw > 30000 || minRaw < -30000);
    k1Frame = m_k1FrontEnd.processHop(chunk, is_clipping);
    m_featureBus.publish(k1Frame);
}

// ... later in processHop() ...

// TempoTracker Beat Tracker Processing (K1 Integration)
// ========================================================================
// Use K1 features for tempo tracking (rhythm_novelty as primary onset)

// Update tempo tracker from K1 features
if (m_k1FrontEnd.isInitialized()) {
    m_tempo.updateFromFeatures(k1Frame);
} else {
    // Fallback: legacy updateNovelty (for compatibility during migration)
    uint64_t t_samples = m_sampleIndex;
    uint64_t tMicros = (t_samples * 1000000ULL) / SAMPLE_RATE;
    m_tempo.updateNovelty(
        goertzelTriggered ? bandsPre : nullptr,
        8,
        rmsPre,
        goertzelTriggered,
        tMicros
    );
}

// ... beat_tick gating fix ...

// CRITICAL FIX: Set beat_tick BEFORE copying entire struct
// This prevents the gating overwrite bug
raw.tempo.beat_tick = m_lastTempoOutput.beat_tick;

// Copy entire tempo output
raw.tempo = m_lastTempoOutput;

