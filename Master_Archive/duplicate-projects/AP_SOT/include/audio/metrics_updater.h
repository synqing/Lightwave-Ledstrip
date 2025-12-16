#pragma once
#include "audio_frame.h"
#include "audio_metrics_tracker.h"
#include "enhanced_beat_detector.h"

void updateMetrics(AudioMetricsTracker& metrics, EnhancedBeatDetector& beat_detector, AudioFrame& frame, float* raw_frequency_bins); 