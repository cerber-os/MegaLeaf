#include "caps_colorEffect.hpp"

#include <cstring>
#include "st_dev.h"
#include "esp_log.h"

namespace ST_IoT {

static const char* MOD_TAG = "Caps_ColorEffect";

CapsColorEffect::CapsColorEffect(IOT_CTX* ctx, const char* component) 
    : Capability() {
    int err;

    effect = MLF_EFFECT_NONE;

    handle = st_cap_handle_init(ctx, component, "clevercoast26830.colorEffects", CapsColorEffect::c_handler_init, this);
    if (handle == NULL) {
        ESP_LOGE(MOD_TAG, "failed to initialize colorEffect capability handler");
        return;
    }

    err = st_cap_cmd_set_cb(handle, "selectEffect", CapsColorEffect::c_handler_cmd_effect, this);
    if (err)
        ESP_LOGE(MOD_TAG, "failed to set cmd_cb for cmd_effect of colorEffect\n");
}

int CapsColorEffect::effectNameToID(const char* name) {
    for(int i = MLF_EFFECT_NONE; i < MLF_EFFECT_MAX; i++)
        if(!strcmp(name, MLF_Effects_names[i]))
            return i;
    return MLF_EFFECT_NONE;
}

const char* CapsColorEffect::effectIDToName(int id) {
    if(id < MLF_EFFECT_NONE || id >= MLF_EFFECT_MAX)
        id = MLF_EFFECT_NONE;

    return MLF_Effects_names[id];
}

int CapsColorEffect::getEffect() {
    return effect;
}

void CapsColorEffect::setEffect(int _effect) {
    effect = _effect;
    sendEffectToServer();
}

void CapsColorEffect::setEffect(const char* effect_name) {
    effect = effectNameToID(effect_name);
    sendEffectToServer();
}

void CapsColorEffect::sendEffectToServer(void) {
    int seq_no = -1;
    
    if(!handle) {
        ESP_LOGE(MOD_TAG, "failed to acquire handle");
        return;
    }

    ST_CAP_SEND_ATTR_STRING(handle, (char*) "name",
            (char*)effectIDToName(effect), NULL, NULL, seq_no);
    if (seq_no < 0)
        ESP_LOGE(MOD_TAG, "failed to send switchLevel value");
}

void CapsColorEffect::handler_cmd(IOT_CAP_HANDLE* handle, const char* cmd, iot_cap_cmd_data_t* cmd_data) {

    ESP_LOGI(MOD_TAG, "called cmd handler with command \"%s\"", cmd);

    if(!strcmp(cmd, "setEffect")) {
        if(cmd_data->num_args != 1 || cmd_data->cmd_data[0].type != IOT_CAP_VAL_TYPE_STRING) {
            ESP_LOGE(MOD_TAG, "received malformed request from server [num_args=%d|type=%d]", 
                cmd_data->num_args, cmd_data->cmd_data[0].type);
            return;
        }
    
        this->effect = effectNameToID(cmd_data->cmd_data[0].string);
    } else {
        ESP_LOGE(MOD_TAG, "unknown command");
        return;
    }

    if(cb_cmd)
        cb_cmd(cmd, cmd_data);
    
    sendEffectToServer();
}

void CapsColorEffect::c_handler_cmd_effect(IOT_CAP_HANDLE* handle, iot_cap_cmd_data_t* cmd_data, void* user_data) {
    CapsColorEffect* pCaps = (CapsColorEffect*) user_data;
    pCaps->handler_cmd(handle, "setEffect", cmd_data);
}

void CapsColorEffect::handler_init(IOT_CAP_HANDLE* handle) {
    ESP_LOGD(MOD_TAG, "called init handler");

    if(cb_init)
        cb_init();
    
    sendEffectToServer();
}

void CapsColorEffect::c_handler_init(IOT_CAP_HANDLE* handle, void* user_data) {
    CapsColorEffect* pCaps = (CapsColorEffect*) user_data;
    pCaps->handler_init(handle);
}


}
