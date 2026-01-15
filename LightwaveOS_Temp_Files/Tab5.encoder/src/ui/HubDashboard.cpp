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
    , uptime_label_(nullptr)
    , node_grid_(nullptr)
    , health_panel_(nullptr)
    , log_panel_(nullptr)
    , log_list_(nullptr)
    , action_bar_(nullptr)
    , log_head_(0)
    , log_count_(0)
    , pending_head_(0)
    , pending_tail_(0)
    , pending_count_(0)
    , lastUpdateMs_(0)
{
    memset(node_cells_, 0, sizeof(node_cells_));
    memset(node_labels_, 0, sizeof(node_labels_));
    memset(log_entries_, 0, sizeof(log_entries_));
    memset(pending_logs_, 0, sizeof(pending_logs_));
}

void HubDashboard::init(HubRegistry* registry, HubOtaDispatch* otaDispatch) {
    registry_ = registry;
    otaDispatch_ = otaDispatch;
    
    screen_ = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_, lv_color_make(0, 0, 0), 0);
    
    createStatusBar();
    createNodeGrid();
    createHealthPanel();
    createLogPanel();
    createActionBar();
    
    lv_scr_load(screen_);
    
    addLogEntry("Hub started");
    LW_LOGI("Hub dashboard initialized");
}

void HubDashboard::createStatusBar() {
    status_bar_ = lv_obj_create(screen_);
    lv_obj_set_size(status_bar_, 1280, 60);
    lv_obj_set_pos(status_bar_, 0, 0);
    lv_obj_set_style_bg_color(status_bar_, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    
    title_label_ = lv_label_create(status_bar_);
    lv_label_set_text(title_label_, "LIGHTWAVEOS HUB");
    lv_obj_set_style_text_color(title_label_, lv_color_make(0, 255, 255), 0);
    lv_obj_set_style_text_font(title_label_, &lv_font_montserrat_28, 0);
    lv_obj_align(title_label_, LV_ALIGN_LEFT_MID, 20, 0);
    
    time_label_ = lv_label_create(status_bar_);
    lv_label_set_text(time_label_, "00:00:00");
    lv_obj_set_style_text_color(time_label_, lv_color_white(), 0);
    lv_obj_set_style_text_font(time_label_, &lv_font_montserrat_28, 0);
    lv_obj_align(time_label_, LV_ALIGN_CENTER, 0, 0);
    
    uptime_label_ = lv_label_create(status_bar_);
    lv_label_set_text(uptime_label_, "0min");
    lv_obj_set_style_text_color(uptime_label_, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(uptime_label_, &lv_font_montserrat_24, 0);
    lv_obj_align(uptime_label_, LV_ALIGN_RIGHT_MID, -20, 0);
}

void HubDashboard::createNodeGrid() {
    // Node grid: 6x2 (240px tall, 640px wide, centered horizontally)
    node_grid_ = lv_obj_create(screen_);
    lv_obj_set_size(node_grid_, 640, 240);
    lv_obj_set_pos(node_grid_, 10, 70);
    lv_obj_set_style_bg_color(node_grid_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(node_grid_, 1, 0);
    lv_obj_set_style_border_color(node_grid_, lv_color_hex(0x333333), 0);
    lv_obj_set_style_pad_all(node_grid_, 5, 0);
    lv_obj_set_layout(node_grid_, LV_LAYOUT_GRID);
    
    static int32_t col_dsc[] = {100, 100, 100, 100, 100, 100, LV_GRID_TEMPLATE_LAST};
    static int32_t row_dsc[] = {110, 110, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(node_grid_, col_dsc, row_dsc);
    
    for (uint8_t i = 0; i < LW_MAX_NODES; i++) {
        node_cells_[i] = lv_obj_create(node_grid_);
        lv_obj_set_size(node_cells_[i], 95, 105);
        lv_obj_set_grid_cell(node_cells_[i], LV_GRID_ALIGN_CENTER, i % 6, 1,
                            LV_GRID_ALIGN_CENTER, i / 6, 1);
        lv_obj_set_style_bg_color(node_cells_[i], lv_color_hex(0x1a1a1a), 0);
        lv_obj_set_style_radius(node_cells_[i], 8, 0);
        lv_obj_set_style_border_width(node_cells_[i], 2, 0);
        lv_obj_set_style_border_color(node_cells_[i], lv_color_hex(0x333333), 0);
        lv_obj_set_style_pad_all(node_cells_[i], 0, 0);
        
        node_labels_[i] = lv_label_create(node_cells_[i]);
        lv_label_set_text_fmt(node_labels_[i], "%d\n--", i + 1);
        lv_obj_set_style_text_color(node_labels_[i], lv_color_white(), 0);
        lv_obj_set_style_text_font(node_labels_[i], &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_align(node_labels_[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(node_labels_[i]);
    }
}

void HubDashboard::createHealthPanel() {
    // Health panel (right side, 610px wide x 240px tall)
    health_panel_ = lv_obj_create(screen_);
    lv_obj_set_size(health_panel_, 610, 240);
    lv_obj_set_pos(health_panel_, 660, 70);
    lv_obj_set_style_bg_color(health_panel_, lv_color_hex(0x0a0a0a), 0);
    lv_obj_set_style_border_width(health_panel_, 1, 0);
    lv_obj_set_style_border_color(health_panel_, lv_color_hex(0x333333), 0);
    lv_obj_set_style_pad_all(health_panel_, 15, 0);
    
    int y = 10;
    
    // RSSI
    rssi_label_ = lv_label_create(health_panel_);
    lv_label_set_text(rssi_label_, "RSSI: -- dBm");
    lv_obj_set_style_text_color(rssi_label_, lv_color_white(), 0);
    lv_obj_set_style_text_font(rssi_label_, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(rssi_label_, 10, y);
    
    rssi_bar_ = lv_bar_create(health_panel_);
    lv_obj_set_size(rssi_bar_, 350, 24);
    lv_obj_set_pos(rssi_bar_, 180, y);
    lv_bar_set_value(rssi_bar_, 0, LV_ANIM_OFF);
    
    y += 50;
    
    // LOSS
    loss_label_ = lv_label_create(health_panel_);
    lv_label_set_text(loss_label_, "LOSS: 0.0%");
    lv_obj_set_style_text_color(loss_label_, lv_color_white(), 0);
    lv_obj_set_style_text_font(loss_label_, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(loss_label_, 10, y);
    
    loss_bar_ = lv_bar_create(health_panel_);
    lv_obj_set_size(loss_bar_, 350, 24);
    lv_obj_set_pos(loss_bar_, 180, y);
    lv_bar_set_value(loss_bar_, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(loss_bar_, lv_color_hex(0x00AA00), LV_PART_INDICATOR);
    
    y += 50;
    
    // DRIFT
    drift_label_ = lv_label_create(health_panel_);
    lv_label_set_text(drift_label_, "DRIFT: 0.0ms");
    lv_obj_set_style_text_color(drift_label_, lv_color_white(), 0);
    lv_obj_set_style_text_font(drift_label_, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(drift_label_, 10, y);
    
    drift_bar_ = lv_bar_create(health_panel_);
    lv_obj_set_size(drift_bar_, 350, 24);
    lv_obj_set_pos(drift_bar_, 180, y);
    lv_bar_set_value(drift_bar_, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(drift_bar_, lv_color_hex(0x0088FF), LV_PART_INDICATOR);
    
    y += 50;
    
    // MEM
    mem_label_ = lv_label_create(health_panel_);
    lv_label_set_text(mem_label_, "MEM: 100% free");
    lv_obj_set_style_text_color(mem_label_, lv_color_white(), 0);
    lv_obj_set_style_text_font(mem_label_, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(mem_label_, 10, y);
    
    mem_bar_ = lv_bar_create(health_panel_);
    lv_obj_set_size(mem_bar_, 350, 24);
    lv_obj_set_pos(mem_bar_, 180, y);
    lv_bar_set_value(mem_bar_, 100, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(mem_bar_, lv_color_hex(0xAA00AA), LV_PART_INDICATOR);
}

void HubDashboard::createLogPanel() {
    // Event log (bottom, 1260px wide x 280px tall)
    log_panel_ = lv_obj_create(screen_);
    lv_obj_set_size(log_panel_, 1260, 280);
    lv_obj_set_pos(log_panel_, 10, 320);
    lv_obj_set_style_bg_color(log_panel_, lv_color_hex(0x0a0a0a), 0);
    lv_obj_set_style_border_width(log_panel_, 1, 0);
    lv_obj_set_style_border_color(log_panel_, lv_color_hex(0x333333), 0);
    lv_obj_set_style_pad_all(log_panel_, 10, 0);
    
    log_list_ = lv_obj_create(log_panel_);
    lv_obj_set_size(log_list_, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(log_list_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(log_list_, 0, 0);
    lv_obj_set_style_pad_all(log_list_, 5, 0);
    lv_obj_set_flex_flow(log_list_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(log_list_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_add_flag(log_list_, LV_OBJ_FLAG_SCROLLABLE);
}

void HubDashboard::createActionBar() {
    // Action bar (bottom, 1260px wide x 80px tall)
    action_bar_ = lv_obj_create(screen_);
    lv_obj_set_size(action_bar_, 1260, 80);
    lv_obj_set_pos(action_bar_, 10, 610);
    lv_obj_set_style_bg_color(action_bar_, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(action_bar_, 1, 0);
    lv_obj_set_style_border_color(action_bar_, lv_color_hex(0x333333), 0);
    lv_obj_set_style_pad_all(action_bar_, 10, 0);
    lv_obj_set_flex_flow(action_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(action_bar_, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    btn_ota_ = lv_btn_create(action_bar_);
    lv_obj_set_size(btn_ota_, 300, 60);
    lv_obj_t* label = lv_label_create(btn_ota_);
    lv_label_set_text(label, "START OTA");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_center(label);
    lv_obj_add_event_cb(btn_ota_, btn_ota_cb, LV_EVENT_CLICKED, this);
    
    btn_refresh_ = lv_btn_create(action_bar_);
    lv_obj_set_size(btn_refresh_, 300, 60);
    label = lv_label_create(btn_refresh_);
    lv_label_set_text(label, "REFRESH");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_center(label);
    lv_obj_add_event_cb(btn_refresh_, btn_refresh_cb, LV_EVENT_CLICKED, this);
    
    btn_clear_ = lv_btn_create(action_bar_);
    lv_obj_set_size(btn_clear_, 300, 60);
    label = lv_label_create(btn_clear_);
    lv_label_set_text(label, "CLEAR LOG");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_center(label);
    lv_obj_add_event_cb(btn_clear_, btn_clear_cb, LV_EVENT_CLICKED, this);
}

void HubDashboard::update() {
    uint32_t now = millis();
    if (now - lastUpdateMs_ < 100) return;
    lastUpdateMs_ = now;
    
    updateStatusBar();
    updateNodeGrid();
    updateHealthPanel();
    drainPendingLogs(2);
}

void HubDashboard::updateStatusBar() {
    uint32_t totalSecs = millis() / 1000;
    uint32_t secs = totalSecs % 60;
    uint32_t mins = (totalSecs / 60) % 60;
    uint32_t hours = (totalSecs / 3600) % 24;
    lv_label_set_text_fmt(time_label_, "%02lu:%02lu:%02lu", hours, mins, secs);
    
    uint32_t uptime_mins = totalSecs / 60;
    lv_label_set_text_fmt(uptime_label_, "%lumin", uptime_mins);
}

void HubDashboard::updateNodeGrid() {
    for (uint8_t i = 0; i < LW_MAX_NODES; i++) {
        uint8_t nodeId = i + 1;
        node_entry_t* node = registry_->getNode(nodeId);
        
        if (node) {
            lv_color_t color = getNodeStateColor(node->state);
            const char* stateStr = getNodeStateStr(node->state);
            
            lv_obj_set_style_bg_color(node_cells_[i], color, 0);
            lv_label_set_text_fmt(node_labels_[i], "%d\n%s", nodeId, stateStr);
        } else {
            lv_obj_set_style_bg_color(node_cells_[i], lv_color_hex(0x1a1a1a), 0);
            lv_label_set_text_fmt(node_labels_[i], "%d\n--", nodeId);
        }
    }
}

void HubDashboard::updateHealthPanel() {
    // Update RSSI (mock data for now)
    lv_label_set_text(rssi_label_, "RSSI: -52 dBm");
    lv_bar_set_value(rssi_bar_, 70, LV_ANIM_OFF);
    
    // Update LOSS (mock data for now)
    lv_label_set_text(loss_label_, "LOSS: 2.1%");
    lv_bar_set_value(loss_bar_, 2, LV_ANIM_OFF);
    
    // Update DRIFT (mock data for now)
    lv_label_set_text(drift_label_, "DRIFT: 1.2ms");
    lv_bar_set_value(drift_bar_, 12, LV_ANIM_OFF);
    
    // Update MEM
    uint32_t freeMem = ESP.getFreeHeap();
    uint32_t totalMem = ESP.getHeapSize();
    uint32_t pct = (freeMem * 100) / totalMem;
    lv_label_set_text_fmt(mem_label_, "MEM: %lu%% free", pct);
    lv_bar_set_value(mem_bar_, pct, LV_ANIM_OFF);
}

void HubDashboard::addLogEntry(const char* text) {
    uint32_t now = millis() / 1000;
    uint32_t mins = (now / 60) % 60;
    uint32_t hours = (now / 3600) % 24;
    uint32_t secs = now % 60;
    
    LogEntry* entry = &log_entries_[log_head_];
    snprintf(entry->text, sizeof(entry->text), "%02lu:%02lu:%02lu  %s", hours, mins, secs, text);
    entry->timestamp = now;
    
    log_head_ = (log_head_ + 1) % MAX_LOG_ENTRIES;
    if (log_count_ < MAX_LOG_ENTRIES) log_count_++;
    
    refreshLogDisplay();
}

void HubDashboard::refreshLogDisplay() {
    lv_obj_clean(log_list_);
    
    // Display in reverse order (newest first)
    for (int i = log_count_ - 1; i >= 0; i--) {
        uint8_t idx = (log_head_ - 1 - i + MAX_LOG_ENTRIES) % MAX_LOG_ENTRIES;
        if (idx < MAX_LOG_ENTRIES && log_entries_[idx].text[0] != '\0') {
            lv_obj_t* label = lv_label_create(log_list_);
            lv_label_set_text(label, log_entries_[idx].text);
            lv_obj_set_style_text_color(label, lv_color_hex(0x00FF00), 0);
            lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
            lv_obj_set_width(label, lv_pct(100));
        }
    }
}

void HubDashboard::logRegistryEvent(uint8_t nodeId, uint8_t eventType, const char* message) {
    char buf[100];
    snprintf(buf, sizeof(buf), "Node %d: %s", nodeId, message);
    enqueuePendingLog(buf);
}

bool HubDashboard::enqueuePendingLog(const char* text) {
    const uint8_t capacity = sizeof(pending_logs_) / sizeof(pending_logs_[0]);
    if (!text) {
        return false;
    }

    if (pending_count_ >= capacity) {
        // Drop oldest to make room for the newest entry.
        pending_head_ = (pending_head_ + 1) % capacity;
        pending_count_--;
    }

    PendingLog& slot = pending_logs_[pending_tail_];
    strlcpy(slot.text, text, sizeof(slot.text));
    pending_tail_ = (pending_tail_ + 1) % capacity;
    pending_count_++;
    return true;
}

void HubDashboard::drainPendingLogs(uint8_t maxPerUpdate) {
    uint8_t processed = 0;
    const uint8_t capacity = sizeof(pending_logs_) / sizeof(pending_logs_[0]);
    while (pending_count_ > 0 && processed < maxPerUpdate) {
        PendingLog& slot = pending_logs_[pending_head_];
        addLogEntry(slot.text);
        pending_head_ = (pending_head_ + 1) % capacity;
        pending_count_--;
        processed++;
    }
}

const char* HubDashboard::getNodeStateStr(node_state_t state) {
    switch (state) {
        case NODE_STATE_PENDING:   return "PEND";
        case NODE_STATE_AUTHED:    return "AUTH";
        case NODE_STATE_READY:     return "RDY";
        case NODE_STATE_DEGRADED:  return "DEGR";
        case NODE_STATE_LOST:      return "LOST";
        default:                   return "???";
    }
}

lv_color_t HubDashboard::getNodeStateColor(node_state_t state) {
    switch (state) {
        case NODE_STATE_PENDING:   return lv_color_hex(0xFFD700);
        case NODE_STATE_AUTHED:    return lv_color_hex(0x4169E1);
        case NODE_STATE_READY:     return lv_color_hex(0x00FF00);
        case NODE_STATE_DEGRADED:  return lv_color_hex(0xFFA500);
        case NODE_STATE_LOST:      return lv_color_hex(0xFF0000);
        default:                   return lv_color_hex(0x1a1a1a);
    }
}

void HubDashboard::btn_ota_cb(lv_event_t* e) {
    HubDashboard* dash = (HubDashboard*)lv_event_get_user_data(e);
    dash->addLogEntry("OTA button pressed");
}

void HubDashboard::btn_refresh_cb(lv_event_t* e) {
    HubDashboard* dash = (HubDashboard*)lv_event_get_user_data(e);
    dash->addLogEntry("Refresh button pressed");
}

void HubDashboard::btn_clear_cb(lv_event_t* e) {
    HubDashboard* dash = (HubDashboard*)lv_event_get_user_data(e);
    dash->log_count_ = 0;
    dash->log_head_ = 0;
    memset(dash->log_entries_, 0, sizeof(dash->log_entries_));
    dash->refreshLogDisplay();
}
