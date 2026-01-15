/**
 * Renderer Command Application Implementation
 * 
 * Network Control Adapter: Translates scheduled Hub commands (lw_cmd_t)
 * into v2 Actor system operations via NodeOrchestrator.
 */

#include "renderer_apply.h"
#include "../../common/util/logging.h"
#include "../../common/clock/monotonic.h"
#include "../sync/node_scheduler.h"
#include "../../core/actors/NodeOrchestrator.h"
#include "../../effects/zones/ZoneComposer.h"
#include "../../effects/zones/BlendMode.h"

using namespace lightwaveos::nodes;

RendererApply::RendererApply() : lastAppliedCount_(0), orchestrator_(nullptr) {
}

void RendererApply::init(NodeOrchestrator* orchestrator) {
    orchestrator_ = orchestrator;
    LW_LOGI("Renderer apply initialized (Network Control Adapter)");
    
    if (!orchestrator_) {
        LW_LOGW("No NodeOrchestrator provided - commands will be ignored!");
    }
}

void RendererApply::applyCommands(NodeScheduler* scheduler) {
    // Extract due commands (bounded, non-blocking)
    uint16_t due = scheduler->extractDue(dueCommands_, LW_MAX_DUE_PER_FRAME);
    
    if (due > 0) {
        LW_LOGD("Applying %d due commands", due);
        lastAppliedCount_ = due;
        
        for (uint16_t i = 0; i < due; i++) {
            const lw_cmd_t* cmd = &dueCommands_[i];
            
            switch (cmd->type) {
                case LW_CMD_SCENE_CHANGE:
                    applySceneChange(cmd);
                    break;
                case LW_CMD_PARAM_DELTA:
                    applyParamDelta(cmd);
                    break;
                case LW_CMD_BEAT_TICK:
                    applyBeatTick(cmd);
                    break;
                case LW_CMD_ZONE_UPDATE:
                    applyZoneUpdate(cmd);
                    break;
            }
        }
    }
}

void RendererApply::applySceneChange(const lw_cmd_t* cmd) {
    if (!orchestrator_) {
        LW_LOGW("No orchestrator - cannot apply scene change");
        return;
    }
    
    uint16_t effectId = cmd->data.scene.effectId;
    uint16_t paletteId = cmd->data.scene.paletteId;
    
    uint64_t now_us = lw_monotonic_us();
    int64_t late_us = (int64_t)now_us - (int64_t)cmd->applyAt_us;
    LW_LOGI("[NETWORK-CONTROL] Scene change: effect=%d, palette=%d seq=%lu applyAt=%llu now=%llu late=%lld",
            effectId, paletteId,
            (unsigned long)cmd->trace_seq,
            (unsigned long long)cmd->applyAt_us,
            (unsigned long long)now_us,
            (long long)late_us);
    
    // Dispatch to v2 Actor system via NodeOrchestrator
    // Use transition if specified, otherwise direct set
    if (cmd->data.scene.transition > 0 && cmd->data.scene.duration_ms > 0) {
        orchestrator_->startTransition(effectId, cmd->data.scene.transition);
    } else {
        orchestrator_->setEffect(effectId);
    }
    
    orchestrator_->setPalette(paletteId);
}

void RendererApply::applyParamDelta(const lw_cmd_t* cmd) {
    if (!orchestrator_) {
        LW_LOGW("No orchestrator - cannot apply param delta");
        return;
    }
    
    // Apply parameter changes via NodeOrchestrator (thread-safe actor messages)
    // Note: Hub commands use 0-255 range, v2 system accepts same
    
    if (cmd->data.params.flags & LW_PARAM_F_BRIGHTNESS) {  // Brightness present
        orchestrator_->setBrightness(cmd->data.params.brightness);
        LW_LOGD("[NETWORK-CONTROL] Brightness: %d", cmd->data.params.brightness);
    }
    
    if (cmd->data.params.flags & LW_PARAM_F_SPEED) {  // Speed present
        orchestrator_->setSpeed(cmd->data.params.speed);
        LW_LOGD("[NETWORK-CONTROL] Speed: %d", cmd->data.params.speed);
    }
    
    if (cmd->data.params.flags & LW_PARAM_F_HUE) {  // Hue present
        // Convert uint16_t hue (0-65535) to uint8_t (0-255)
        uint8_t hue8 = (cmd->data.params.hue >> 8) & 0xFF;
        orchestrator_->setHue(hue8);
        LW_LOGD("[NETWORK-CONTROL] Hue: %d", hue8);
    }
    
    if (cmd->data.params.flags & LW_PARAM_F_SATURATION) {  // Saturation present
        orchestrator_->setSaturation(cmd->data.params.saturation);
        LW_LOGD("[NETWORK-CONTROL] Saturation: %d", cmd->data.params.saturation);
    }

    if (cmd->data.params.flags & LW_PARAM_F_PALETTE) {
        orchestrator_->setPalette(cmd->data.params.paletteId);
        LW_LOGD("[NETWORK-CONTROL] Palette: %d", cmd->data.params.paletteId);
    }

    if (cmd->data.params.flags & LW_PARAM_F_INTENSITY) {
        orchestrator_->setIntensity(cmd->data.params.intensity);
        LW_LOGD("[NETWORK-CONTROL] Intensity: %d", cmd->data.params.intensity);
    }

    if (cmd->data.params.flags & LW_PARAM_F_COMPLEXITY) {
        orchestrator_->setComplexity(cmd->data.params.complexity);
        LW_LOGD("[NETWORK-CONTROL] Complexity: %d", cmd->data.params.complexity);
    }

    if (cmd->data.params.flags & LW_PARAM_F_VARIATION) {
        orchestrator_->setVariation(cmd->data.params.variation);
        LW_LOGD("[NETWORK-CONTROL] Variation: %d", cmd->data.params.variation);
    }
}

void RendererApply::applyBeatTick(const lw_cmd_t* cmd) {
    // Update beat/musical timing state
    // This would integrate with your existing audio-reactive system
}

void RendererApply::applyZoneUpdate(const lw_cmd_t* cmd) {
    if (!orchestrator_) {
        LW_LOGW("No orchestrator - cannot apply zone update");
        return;
    }

    lightwaveos::nodes::RendererNode* renderer = orchestrator_->getRenderer();
    if (!renderer) {
        LW_LOGW("No renderer - cannot apply zone update");
        return;
    }

    lightwaveos::zones::ZoneComposer* composer = renderer->getZoneComposer();
    if (!composer) {
        LW_LOGW("No ZoneComposer attached - cannot apply zone update");
        return;
    }

    // ZoneComposer only renders when enabled.
    composer->setEnabled(true);

    const uint8_t zoneId = cmd->data.zone.zoneId;

    if (cmd->data.zone.flags & LW_ZONE_F_EFFECT) {
        composer->setZoneEffect(zoneId, cmd->data.zone.effectId);
    }
    if (cmd->data.zone.flags & LW_ZONE_F_BRIGHTNESS) {
        composer->setZoneBrightness(zoneId, cmd->data.zone.brightness);
    }
    if (cmd->data.zone.flags & LW_ZONE_F_SPEED) {
        composer->setZoneSpeed(zoneId, cmd->data.zone.speed);
    }
    if (cmd->data.zone.flags & LW_ZONE_F_PALETTE) {
        composer->setZonePalette(zoneId, cmd->data.zone.paletteId);
    }
    if (cmd->data.zone.flags & LW_ZONE_F_BLEND) {
        composer->setZoneBlendMode(zoneId, (lightwaveos::zones::BlendMode)cmd->data.zone.blendMode);
    }
}
