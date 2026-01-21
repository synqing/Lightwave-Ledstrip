#pragma once
#include <Arduino.h>
#include <lvgl.h>
#include "../hub/net/hub_registry.h"
#include "../hub/ota/hub_ota_dispatch.h"

#define MAX_LOG_ENTRIES 20

class HubDashboard {
public:
    HubDashboard();
    void init(HubRegistry* registry, HubOtaDispatch* otaDispatch);
    void update();
    void logRegistryEvent(uint8_t nodeId, uint8_t eventType, const char* message);

private:
    HubRegistry* registry_;
    HubOtaDispatch* otaDispatch_;
    
    lv_obj_t* screen_;
    lv_obj_t* status_bar_;
    lv_obj_t* title_label_;
    lv_obj_t* time_label_;
    lv_obj_t* uptime_label_;
    
    lv_obj_t* node_grid_;
    lv_obj_t* node_cells_[LW_MAX_NODES];
    lv_obj_t* node_labels_[LW_MAX_NODES];
    
    lv_obj_t* health_panel_;
    lv_obj_t* rssi_label_;
    lv_obj_t* rssi_bar_;
    lv_obj_t* loss_label_;
    lv_obj_t* loss_bar_;
    lv_obj_t* drift_label_;
    lv_obj_t* drift_bar_;
    lv_obj_t* mem_label_;
    lv_obj_t* mem_bar_;
    
    lv_obj_t* log_panel_;
    lv_obj_t* log_list_;
    struct LogEntry {
        char text[128];
        uint32_t timestamp;
    };
    LogEntry log_entries_[MAX_LOG_ENTRIES];
    uint8_t log_head_;
    uint8_t log_count_;

    struct PendingLog {
        char text[100];
    };
    PendingLog pending_logs_[8];
    uint8_t pending_head_;
    uint8_t pending_tail_;
    uint8_t pending_count_;
    
    lv_obj_t* action_bar_;
    lv_obj_t* btn_ota_;
    lv_obj_t* btn_refresh_;
    lv_obj_t* btn_clear_;
    
    uint32_t lastUpdateMs_;
    
    void createStatusBar();
    void createNodeGrid();
    void createHealthPanel();
    void createLogPanel();
    void createActionBar();
    
    void updateStatusBar();
    void updateNodeGrid();
    void updateHealthPanel();
    void refreshLogDisplay();
    
    void addLogEntry(const char* text);
    bool enqueuePendingLog(const char* text);
    void drainPendingLogs(uint8_t maxPerUpdate);
    const char* getNodeStateStr(node_state_t state);
    lv_color_t getNodeStateColor(node_state_t state);
    
    static void btn_ota_cb(lv_event_t* e);
    static void btn_refresh_cb(lv_event_t* e);
    static void btn_clear_cb(lv_event_t* e);
};
