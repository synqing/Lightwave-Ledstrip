#pragma once
#include <Arduino.h>
#include <lvgl.h>
#include "../hub/net/hub_registry.h"
#include "../hub/ota/hub_ota_dispatch.h"

class HubDashboard {
public:
    HubDashboard();
    void init(HubRegistry* registry, HubOtaDispatch* otaDispatch);
    void update();
    void onTouch(int16_t x, int16_t y);

private:
    HubRegistry* registry_;
    HubOtaDispatch* otaDispatch_;
    
    lv_obj_t* screen_;
    lv_obj_t* status_bar_;
    lv_obj_t* title_label_;
    lv_obj_t* time_label_;
    lv_obj_t* node_grid_;
    lv_obj_t* node_cells_[LW_MAX_NODES];
    lv_obj_t* node_labels_[LW_MAX_NODES];
    
    uint32_t lastUpdateMs_;
    
    void createStatusBar();
    void createNodeGrid();
    void updateStatusBar();
    void updateNodeGrid();
    const char* getNodeStateStr(node_state_t state);
    lv_color_t getNodeStateColor(node_state_t state);
};
