#include "caps_colorEffectSpeed.hpp"

#include <cstring>
#include "st_dev.h"
#include "esp_log.h"

namespace ST_IoT {

static const char* MOD_TAG = "Caps_ColorEffectSpeed";

CapsColorEffectSpeed::CapsColorEffectSpeed(IOT_CTX* ctx, const char* component) 
    : Capability() {
    int err;

    speed = 0;

    handle = st_cap_handle_init(ctx, component, "clevercoast26830.colorEffectsSpeed", CapsColorEffectSpeed::c_handler_init, this);
    if (handle == NULL) {
        ESP_LOGE(MOD_TAG, "failed to initialize ColorEffectSpeed capability handler");
        return;
    }

    err = st_cap_cmd_set_cb(handle, "setSpeed", CapsColorEffectSpeed::c_handler_cmd_speed, this);
    if (err)
        ESP_LOGE(MOD_TAG, "failed to set cmd_cb for set_speed of colorEffectSpeed\n");
}

int CapsColorEffectSpeed::getSpeed() {
    return speed;
}

void CapsColorEffectSpeed::setSpeed(int _speed) {
    speed = _speed;
    sendEffectSpeedToServer();
}

void CapsColorEffectSpeed::sendEffectSpeedToServer(void) {
    int seq_no = -1;
    
    if(!handle) {
        ESP_LOGE(MOD_TAG, "failed to acquire handle");
        return;
    }

    ST_CAP_SEND_ATTR_NUMBER(handle, (char*) "speed",
            speed, NULL, NULL, seq_no);
    if (seq_no < 0)
        ESP_LOGE(MOD_TAG, "failed to send colorEffectSpeed value");
}

void CapsColorEffectSpeed::handler_cmd(IOT_CAP_HANDLE* handle, const char* cmd, iot_cap_cmd_data_t* cmd_data) {

    ESP_LOGI(MOD_TAG, "called cmd handler with command \"%s\"", cmd);

    if(!strcmp(cmd, "setSpeed")) {
        if(cmd_data->num_args != 1 || cmd_data->cmd_data[0].type != IOT_CAP_VAL_TYPE_INT_OR_NUM) {
            ESP_LOGE(MOD_TAG, "received malformed request from server [num_args=%d|type=%d]", 
                cmd_data->num_args, cmd_data->cmd_data[0].type);
            return;
        }
    
        this->speed = cmd_data->cmd_data[0].integer;
    } else {
        ESP_LOGE(MOD_TAG, "unknown command");
        return;
    }

    if(cb_cmd)
        cb_cmd(cmd, cmd_data);
    
    sendEffectSpeedToServer();
}

void CapsColorEffectSpeed::c_handler_cmd_speed(IOT_CAP_HANDLE* handle, iot_cap_cmd_data_t* cmd_data, void* user_data) {
    CapsColorEffectSpeed* pCaps = (CapsColorEffectSpeed*) user_data;
    pCaps->handler_cmd(handle, "setSpeed", cmd_data);
}

void CapsColorEffectSpeed::handler_init(IOT_CAP_HANDLE* handle) {
    ESP_LOGD(MOD_TAG, "called init handler");

    if(cb_init)
        cb_init();
    
    sendEffectSpeedToServer();
}

void CapsColorEffectSpeed::c_handler_init(IOT_CAP_HANDLE* handle, void* user_data) {
    CapsColorEffectSpeed* pCaps = (CapsColorEffectSpeed*) user_data;
    pCaps->handler_init(handle);
}


}
