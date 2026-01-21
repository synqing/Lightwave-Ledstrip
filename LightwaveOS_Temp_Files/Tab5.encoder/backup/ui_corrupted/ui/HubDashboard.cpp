#include "HubDashboard.h"

#define LW_LOG_TAG "HubDashboard"
#include "../common/util/logging.h"

HubDashboard::HubDashboard() 
    : registry_(nullptr)
    , otaDispatch_(nullptr)
    , screen_(nullptr)
    , status_bar_(nullptr)
    , title_label_(nullptr)
    , time_label_(nullptr)
    , node_grid_(nullptr)
    , lastUpdateMs_(0)
{
    memset(node_cells_, 0, sizeof(node_cells_));
    memset(node_labels_, 0, sizeof(node_labels_));
}

void HubDashboard::init(HubRegistry* registry, HubOtaDispatch* otaDispatch) {
    registry_ = registry;
    otaDispatch_ = otaDispatch;
    
    // Create main screen
    screen_ = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(0x000000), 0);
    
    createStatusBar();
    createNodeGrid();
    
    lv_scr_load(screen_);
    
    LW_LOGI("Hub dashboard initialized");
}

void HubDashboard::createStatusBar() {
    // Status bar at top
    status_bar_ = lv_obj_create(screen_);
    lv_obj_set_size(status_bar_, 720, 60);
    lv_obj_set_pos(status_bar_, 0, 0);
    lv_obj_set_style_bg_color(status_bar_, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    
    // Title
    title_label_ = lv_label_create(status_bar_);
    lv_label_set_text(title_label_, "LIGHTWAVEOS HUB");
    lv_obj_set_style_text_color(title_label_, lv_color_white(), 0);
    lv_obj_align(title_label_, LV_ALIGN_LEFT_MID, 20, 0);
    
    // Time
    time_label_ = lv_label_create(status_bar_);
    lv_label_set_text(time_label_, "00:00");
    lv_obj_set_style_text_color(time_label_, lv_color_white(), 0);
    lv_obj_align(time_label_, LV_ALIGN_RIGHT_MID, -20, 0);
}

void HubDashboard::createNodeGrid() {
    // Node grid: 6 columns x 2 rows = 12 nodes
    node_grid_ = lv_obj_create(screen_);
    lv_obj_set_size(node_grid_, 720, 600);
    lv_obj_set_pos(node_grid_, 0, 70);
    lv_obj_set_style_bg_color(node_grid_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(node_grid_, 0, 0);
    lv_obj_set_style_pad_all(node_grid_, 10, 0);
    lv_obj_set_layout(node_grid_, LV_LAYOUT_GRID);
    
    // Grid template: 6 columns, 2 rows
    static lv_coord_t col_dsc[] = {110, 110, 110, 110, 110, 110, LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {280, 280, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(node_grid_, col_dsc, row_dsc);
    
    // Create node cells
    for (uint8_t i = 0; i < LW_MAX_NODES; i++) {
        // Cell container
        node_cells_[i] = lv_obj_create(node_grid_);
        lv_obj_set_size(node_cells_[i], 100, 260);
        lv_obj_set_grid_cell(node_cells_[i], LV_GRID_ALIGN_CENTER, i % 6, 1,
                            LV_GRID_ALIGN_CENTER, i / 6, 1);
        lv_obj_set_style_bg_color(node_cells_[i], lv_color_hex(0x2a2a3e), 0);
        lv_obj_set_style_radius(node_cells_[i], 10, 0);
        lv_obj_set_style_border_width(node_cells_[i], 2, 0);
        lv_obj_set_style_border_color(node_cells_[i], lv_color_hex(0x4a4a5e), 0);
        
        // Node label
        node_labels_[i] = lv_label_create(node_cells_[i]);
        lv_label_set_text_fmt(node_labels_[i], "N%d\n--", i + 1);
        lv_obj_set_style_text_color(node_labels_[i], lv_color_white(), 0);
        lv_obj_center(node_labels_[i]);
    }
}

void HubDashboard::update() {
    uint32_t now = millis();
    if (now - lastUpdateMs_ < 100) return; // 10 Hz update
    lastUpdateMs_ = now;
    
    updateStatusBar();
    updateNodeGrid();
}

void HubDashboard::updateStatusBar() {
    // Update time
    uint32_t secs = millis() / 1000;
    uint32_t mins = (secs / 60) % 60;
    uint32_t hours = (secs / 3600) % 24;
    lv_label_set_text_fmt(time_label_, "%02d:%02d", hours, mins);
}

void HubDashboard::updateNodeGrid() {
    for (uint8_t i = 0; i < LW_MAX_NODES; i++) {
        uint8_t nodeId = i + 1;
        node_entry_t* node = registry_->getNode(nodeId);
        
        if (node) {
            lv_color_t color = getNodeStateColor(node->state);
            const char* stateStr = getNodeStateStr(node->state);
            
            lv_obj_set_style_bg_color(node_cells_[i], color, 0);
            lv_label_set_text_fmt(node_labels_[i], "N%d\n%s", nodeId, stateStr);
        } else {
            lv_obj_set_style_bg_color(node_cells_[i], lv_color_hex(0x2a2a3e), 0);
            lv_label_set_text_fmt(node_labels_[i], "N%d\n--", nodeId);
        }
    }
}

const char* HubDashboard::getNodeStateStr(node_state_t state) {
    switch (state) {
        case NODE_STATE_PENDING:   return "PEND";
        case NODE_STATE_AUTHED:    return "AUTH";
        case NODE_STATE_READY:     return "READY";
        case NODE_STATE_DEGRADED:  return "DEGR";
        case NODE_STATE_LOST:      return "LOST";
        default:                   return "???";
    }
}

lv_color_t HubDashboard::getNodeStateColor(node_state_t state) {
    switch (state) {
        case NODE_STATE_PENDING:   return lv_color_hex(0xFFD700); // Gold
        case NODE_STATE_AUTHED:    return lv_color_hex(0x4169E1); // Royal blue
        case NODE_STATE_READY:     return lv_color_hex(0x00FF00); // Green
        case NODE_STATE_DEGRADED:  return lv_color_hex(0xFFA500); // Orange
        case NODE_STATE_LOST:      return lv_color_hex(0xFF0000); // Red
        default:                   return lv_color_hex(0x2a2a3e); // Dark grey
    }
}

void HubDashboard::onTouch(int16_t x, int16_t y) {
    // Touch handling (future)
    LW_LOGD("Touch at %d,%d", x, y);
}
