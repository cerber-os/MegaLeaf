/**
 * @file main.cpp
 * @brief Application entry point
 * 
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "st_dev.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "caps_color.hpp"
#include "caps_switch.hpp"
#include "caps_switchLevel.hpp"
#include "caps_colorEffect.hpp"
#include "caps_colorEffectSpeed.hpp"
#include "caps/iot_caps_helper_switch.h"
#include <esp_log.h>

#include "mlf_comm.h"


// onboarding_config_start is null-terminated string
extern const uint8_t onboarding_config_start[]    asm("_binary_onboarding_config_json_start");
extern const uint8_t onboarding_config_end[]    asm("_binary_onboarding_config_json_end");

// device_info_start is null-terminated string
extern const uint8_t device_info_start[]    asm("_binary_device_info_json_start");
extern const uint8_t device_info_end[]        asm("_binary_device_info_json_end");


static iot_status_t g_iot_status = IOT_STATUS_IDLE;
static iot_stat_lv_t g_iot_stat_lv;

IOT_CTX* iot_ctx = NULL;

static ST_IoT::CapsSwitch* capSwitch;
static ST_IoT::CapsColor* capColor;
static ST_IoT::CapsSwitchLevel* capLevel;
static ST_IoT::CapsColorEffect* capColorEffect;
static ST_IoT::CapsColorEffectSpeed* capColorEffectSpeed;


/*
 * SmartThings commands callbacks
 */
static void cap_switch_init_cb(void) {
    uint8_t on_state = 0;
    if(!MLF_Comm_GetOnState(&on_state))
        capSwitch->setValue(on_state);
}

static void cap_level_init_cb(void) {
    uint16_t brightness = 0;
    if(!MLF_Comm_GetBrightness(&brightness))
        capLevel->setValue(brightness);
}

static void cap_effects_init_cb(void) {
    uint8_t on_state = 0;
    uint16_t effect = 0, speed = 0;
    uint32_t color = 0;

    if(!MLF_Comm_GetEffect(&effect, &speed, &color)) {
        capColor->setColorInt(color);
        capColorEffectSpeed->setSpeed(speed);

        if(!MLF_Comm_GetOnState(&on_state) && on_state)
            capColorEffect->setEffect(effect + 1);
        else
            capColorEffect->setEffect(0);
    }
}

static void cap_switch_cmd_cb(const char* value) {
    int switch_state;
    const char* switch_value = capSwitch->getValue();
    if(switch_value == NULL) {
        ESP_LOGE("app", "For some reason, capSwitch->getValue returned NULL");
        return;
    }

    // Deal with MCU
    switch_state = !strcmp(switch_value, caps_helper_switch.attr_switch.value_on);
    if(switch_state)
        MLF_Comm_TurnOn();
    else
        MLF_Comm_TurnOff();

    // Ask MCU about current state and update server
    uint8_t on_state = switch_state;
    MLF_Comm_GetOnState(&on_state);
    capSwitch->setValue(on_state);
}

static void cap_color_cmd_cb(const char* value) {
    // Deal with MCU
    MLF_Comm_SetEffect(2, 0, 0b11, capColor->getRGBColor());

    // Tell server that we're ON and current effect is 'static'
    capSwitch->setValue(1);
    capColorEffect->setEffect("static");
}

static void cap_level_cmd_cb(const char* value) {
    // Just deal with MCU
    MLF_Comm_SetBrightness(capLevel->getValue() * 255 / 100);
}

static void cap_effect_cmd_cb(const char* cmd, iot_cap_cmd_data_t* cmd_data) {
    // Handle request to change effect
    if(capColorEffect->getEffect() == 0)
        MLF_Comm_TurnOff();
    else
        MLF_Comm_SetEffect(capColorEffect->getEffect() - 1, capColorEffectSpeed->getSpeed(), 
                        0b11, capColor->getRGBColor());

    // Ask MCU about current state and update server
    uint8_t on_state;
    if(!MLF_Comm_GetOnState(&on_state))
        capSwitch->setValue(on_state);
}

static void cap_effect_speed_cmd_cb(const char* cmd, iot_cap_cmd_data_t* cmd_data) {
    if(capColorEffect->getEffect() > ST_IoT::CapsColorEffect::MLF_EFFECT_NONE) {
        MLF_Comm_SetEffect(capColorEffect->getEffect() - 1, capColorEffectSpeed->getSpeed(), 
                        0b11, capColor->getRGBColor());
    }
}

/********************
 * MCU callbacks
 *  DO NOT INVOKE ANY MLF_Comm_* FUNCTIONS HERE
 ********************/
static int MLF_Recv_TurnOn(uint8_t* data, uint16_t size, uint8_t* resp, uint16_t* resp_size) {
    capSwitch->setValue(1);
    return MLF_RET_OK;
}

static int MLF_RecvTurnOff(uint8_t* data, uint16_t size, uint8_t* resp, uint16_t* resp_size) {
    capSwitch->setValue(0);
    return MLF_RET_OK;
}

static int MLF_RecvSetBrightness(uint8_t* data, uint16_t size, uint8_t* resp, uint16_t* resp_size) {
    struct MLF_req_cmd_set_brightness* bright = NULL;
    
    if(size < sizeof(*bright)) {
        ESP_LOGE("app", "%s: incorrect request size received - %d (expected: %d)", __func__, size, sizeof(*bright));
        return MLF_RET_INVALID_DATA;
    }
    capLevel->setValue(bright->brightness);

    return MLF_RET_OK;
}

static int MLF_RecvSetEffect(uint8_t* data, uint16_t size, uint8_t* resp, uint16_t* resp_size) {
    struct MLF_req_cmd_set_effect* effect = NULL;
    
    if(size < sizeof(*effect)) {
        ESP_LOGE("app", "%s: incorrect request size received - %d (expected: %d)", __func__, size, sizeof(*effect));
        return MLF_RET_INVALID_DATA;
    }
    capColorEffect->setEffect(effect->effect + 1);
    capColorEffectSpeed->setSpeed(effect->speed);
    capSwitch->setValue(1);

    return MLF_RET_OK;
}

static void MLF_SetupCallbacks(void) {
    MLF_Comm_setCallback(MLF_CMD_TURN_ON, MLF_Recv_TurnOn);
    MLF_Comm_setCallback(MLF_CMD_TURN_OFF, MLF_RecvTurnOff);
    MLF_Comm_setCallback(MLF_CMD_SET_BRIGHTNESS, MLF_RecvSetBrightness);
    MLF_Comm_setCallback(MLF_CMD_SET_EFFECT, MLF_RecvSetEffect);
}

/*
 * SmartThings callback handling all kind of stuff
 */
static void iot_status_cb(iot_status_t status,
                          iot_stat_lv_t stat_lv, void *usr_data)
{
    g_iot_status = status;
    g_iot_stat_lv = stat_lv;

    ESP_LOGD("main", "StatusCB: [status: %d], stat: %d\n", g_iot_status, g_iot_stat_lv);
}


static void iot_noti_cb(iot_noti_data_t *noti_data, void *noti_usr_data) {
    ESP_LOGD("main", "Notification message received\n");

    if (noti_data->type == IOT_NOTI_TYPE_DEV_DELETED) {
        ESP_LOGI("main", "[device deleted]\n");
    } else if (noti_data->type == IOT_NOTI_TYPE_RATE_LIMIT) {
        ESP_LOGW("main", "[rate limit] Remaining time:%d, sequence number:%d\n",
               noti_data->raw.rate_limit.remainingTime, noti_data->raw.rate_limit.sequenceNumber);
    }
}

/*
 * App setup
 */
static void capability_init() {
    capSwitch = new ST_IoT::CapsSwitch(iot_ctx, "main");
    capSwitch->attach(ST_IoT::CapsCbInit, (void*) cap_switch_init_cb);
    capSwitch->attach(ST_IoT::CapsCbCmd, (void*) cap_switch_cmd_cb);

    capColor = new ST_IoT::CapsColor(iot_ctx, "main");
    capColor->attach(ST_IoT::CapsCbCmd, (void*) cap_color_cmd_cb);

    capLevel = new ST_IoT::CapsSwitchLevel(iot_ctx, "main");
    capLevel->attach(ST_IoT::CapsCbInit, (void*) cap_level_init_cb);
    capLevel->attach(ST_IoT::CapsCbCmd, (void*) cap_level_cmd_cb);

    capColorEffect = new ST_IoT::CapsColorEffect(iot_ctx, "visualeffects");
    capColorEffect->attach(ST_IoT::CapsCbCmd, (void*) cap_effect_cmd_cb);

    capColorEffectSpeed = new ST_IoT::CapsColorEffectSpeed(iot_ctx, "visualeffects");
    capColorEffectSpeed->attach(ST_IoT::CapsCbInit, (void*) cap_effects_init_cb);
    capColorEffectSpeed->attach(ST_IoT::CapsCbCmd, (void*) cap_effect_speed_cmd_cb);
}

static void coMcuInit(void) {

    MLF_Comm_init();
    MLF_SetupCallbacks();

    // During ESP32 reboot there might have been some noise on UART line
    //  We're going to ask for current state (ignoring result) to
    //  flush data buffers on destination MCU
    MLF_Comm_GetOnState(NULL);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
}

extern "C" void app_main(void) {
    unsigned char *onboarding_config = (unsigned char *) onboarding_config_start;
    unsigned int onboarding_config_len = onboarding_config_end - onboarding_config_start;
    unsigned char *device_info = (unsigned char *) device_info_start;
    unsigned int device_info_len = device_info_end - device_info_start;

    int err;

    iot_ctx = st_conn_init(onboarding_config, onboarding_config_len, device_info, device_info_len);
    if(iot_ctx == NULL) {
        ESP_LOGE("main", "Failed to create iot_context");
        return;
    }

    err = st_conn_set_noti_cb(iot_ctx, iot_noti_cb, NULL);
    if (err)
        ESP_LOGE("main", "failed to set notification callback function\n");

    capability_init();
    coMcuInit();

    err = st_conn_start(iot_ctx, (st_status_cb)&iot_status_cb, IOT_STATUS_ALL, NULL, NULL);
    if (err)
        ESP_LOGE("main", "failed to start connection. err: %d", err);
}
