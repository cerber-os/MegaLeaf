#include "caps_switchLevel.hpp"

#include <cstring>
#include "caps/iot_caps_helper_switchLevel.h"
#include "esp_log.h"

namespace ST_IoT {

static const char* MOD_TAG = "Caps_SwitchLevel";

CapsSwitchLevel::CapsSwitchLevel(IOT_CTX* ctx, const char* component) 
    : Capability() {
    int err;

    level = 0;

    handle = st_cap_handle_init(ctx, component, caps_helper_switchLevel.id, CapsSwitchLevel::c_handler_init, this);
    if (handle == NULL) {
        ESP_LOGE(MOD_TAG, "failed to initialize switch capability handler");
        return;
    }

    err = st_cap_cmd_set_cb(handle, caps_helper_switchLevel.cmd_setLevel.name, CapsSwitchLevel::c_handler_cmd_level, this);
    if (err)
        ESP_LOGE(MOD_TAG, "failed to set cmd_cb for setLevel of switchLevel\n");
}

int CapsSwitchLevel::getValue(void) {
    return level;
}

void CapsSwitchLevel::setValue(int value) {
    level = value;
    sendLevelToServer();
}

void CapsSwitchLevel::sendLevelToServer(void) {
    int sequence_no = -1;

    if(!handle) {
        ESP_LOGE(MOD_TAG, "failed to aquire handle");
        return;
    }
    
    ST_CAP_SEND_ATTR_NUMBER(handle,
            (char *)caps_helper_switchLevel.attr_level.name,
            level, NULL, NULL, sequence_no);

    if (sequence_no < 0)
        ESP_LOGE(MOD_TAG, "failed to send switchLevel value");
}


void CapsSwitchLevel::handler_cmd(IOT_CAP_HANDLE* handle, const char* cmd, iot_cap_cmd_data_t* cmd_data) {

    ESP_LOGI(MOD_TAG, "called cmd handler with command \"%s\"", cmd);

    if(strcmp(cmd, "level")) {
        ESP_LOGE(MOD_TAG, "unknown command");
        return;
    }

    if(cmd_data->num_args != 1 || cmd_data->cmd_data[0].type != IOT_CAP_VAL_TYPE_INT_OR_NUM) {
        ESP_LOGE(MOD_TAG, "received malformed request from server [num_args=%d|type=%d]", 
                cmd_data->num_args, cmd_data->cmd_data[0].type);
        return;
    }
    
    this->level = cmd_data->cmd_data[0].integer;
    if(cb_cmd)
        cb_cmd(cmd, cmd_data);
    
    sendLevelToServer();
}

void CapsSwitchLevel::c_handler_cmd_level(IOT_CAP_HANDLE* handle, iot_cap_cmd_data_t* cmd_data, void* user_data) {
    CapsSwitchLevel* pCaps = (CapsSwitchLevel*) user_data;
    pCaps->handler_cmd(handle, caps_helper_switchLevel.attr_level.name, cmd_data);
}


void CapsSwitchLevel::handler_init(IOT_CAP_HANDLE* handle) {
    ESP_LOGD(MOD_TAG, "called init handler");

    if(cb_init)
        cb_init();
    
    sendLevelToServer();
}

void CapsSwitchLevel::c_handler_init(IOT_CAP_HANDLE* handle, void* user_data) {
    CapsSwitchLevel* pCaps = (CapsSwitchLevel*) user_data;
    pCaps->handler_init(handle);
}

}
