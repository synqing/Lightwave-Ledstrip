/**
 * @file renderer_apply.h
 * @brief Network Control Adapter - Hub Commands to v2 Actor System
 * 
 * Translates scheduled Hub commands (lw_cmd_t) into v2 renderer operations
 * via NodeOrchestrator at frame boundaries.
 * 
 * This is the integration boundary between the Hub/Node wireless architecture
 * and the legacy v2 rendering/effects engine.
 */

#pragma once

#include "../../common/clock/schedule_queue.h"

// Forward declaration
namespace lightwaveos { namespace nodes { class NodeOrchestrator; } }

class RendererApply {
public:
    RendererApply();
    
    // Initialize with NodeOrchestrator for dispatching commands
    void init(lightwaveos::nodes::NodeOrchestrator* orchestrator);
    
    // Called at render frame boundary (before synthesizing frame)
    void applyCommands(class NodeScheduler* scheduler);
    
private:
    lw_cmd_t dueCommands_[LW_MAX_DUE_PER_FRAME];
    uint16_t lastAppliedCount_;
    lightwaveos::nodes::NodeOrchestrator* orchestrator_;
    
    void applySceneChange(const lw_cmd_t* cmd);
    void applyParamDelta(const lw_cmd_t* cmd);
    void applyBeatTick(const lw_cmd_t* cmd);
    void applyZoneUpdate(const lw_cmd_t* cmd);
};
