#include "caps_switch.hpp"

#include <cstring>
#include "caps/iot_caps_helper_switch.h"
#include "esp_log.h"

namespace ST_IoT {

static const char* MOD_TAG = "Caps_Switch";

CapsSwitch::CapsSwitch(IOT_CTX* ctx, const char* component)
    : Capability() {
    int err;

    memset(switch_value, 0, sizeof(switch_value));
    strcpy(switch_value, caps_helper_switch.attr_switch.value_off);

    handle = st_cap_handle_init(ctx, component, caps_helper_switch.id, CapsSwitch::c_handler_init, this);
    if (handle == NULL) {
        ESP_LOGE(MOD_TAG, "failed to initialize switch capability handler");
        return;
    }

    err = st_cap_cmd_set_cb(handle, caps_helper_switch.cmd_on.name, CapsSwitch::c_handler_cmd_on, this);
    if (err)
        ESP_LOGE(MOD_TAG, "failed to set cmd_cb for on of switch\n");

    err = st_cap_cmd_set_cb(handle, caps_helper_switch.cmd_off.name, CapsSwitch::c_handler_cmd_off, this);
    if (err)
        ESP_LOGE(MOD_TAG, "failed to set cmd_cb for off of switch\n");
}

const char* CapsSwitch::getValue(void) {
    return switch_value;
}

void CapsSwitch::setValue(const char* value) {
    strncpy(switch_value, value, sizeof(switch_value) - 1);
    sendStateToServer();
    update_required = false;
}

void CapsSwitch::setValue(int value) {
    if(value == 0)
        setValue(caps_helper_switch.attr_switch.value_off);
    else
        setValue(caps_helper_switch.attr_switch.value_on);
}

void CapsSwitch::sendStateToServer(void) {
    int sequence_no = -1;

    if(!handle) {
        ESP_LOGE(MOD_TAG, "failed to aquire handle");
        return;
    }
    
    ST_CAP_SEND_ATTR_STRING(handle,
            (char *)caps_helper_switch.attr_switch.name,
            switch_value,
            NULL,
            NULL,
            sequence_no);

    if (sequence_no < 0)
        ESP_LOGE(MOD_TAG, "failed to send switch value");
}


void CapsSwitch::handler_cmd(IOT_CAP_HANDLE* handle, const char* cmd, iot_cap_cmd_data_t* cmd_data) {

    ESP_LOGI(MOD_TAG, "called cmd handler with command \"%s\"", cmd);

    strncpy(switch_value, cmd, sizeof(switch_value) - 1);
    update_required = true;
    if(cb_cmd)
        cb_cmd(cmd, cmd_data);
    
    // In case cb_cmd already sent state with ->setValue
    if(update_required)
        sendStateToServer();
}

void CapsSwitch::c_handler_cmd_on(IOT_CAP_HANDLE* handle, iot_cap_cmd_data_t* cmd_data, void* user_data) {
    CapsSwitch* pCaps = (CapsSwitch*) user_data;
    pCaps->handler_cmd(handle, caps_helper_switch.attr_switch.value_on, cmd_data);
}

void CapsSwitch::c_handler_cmd_off(IOT_CAP_HANDLE* handle, iot_cap_cmd_data_t* cmd_data, void* user_data) {
    CapsSwitch* pCaps = (CapsSwitch*) user_data;
    pCaps->handler_cmd(handle, caps_helper_switch.attr_switch.value_off, cmd_data);
}


void CapsSwitch::handler_init(IOT_CAP_HANDLE* handle) {
    ESP_LOGD(MOD_TAG, "called init handler");

    if(cb_init)
        cb_init();
    
    sendStateToServer();
}

void CapsSwitch::c_handler_init(IOT_CAP_HANDLE* handle, void* user_data) {
    CapsSwitch* pCaps = (CapsSwitch*) user_data;
    pCaps->handler_init(handle);
}

}
