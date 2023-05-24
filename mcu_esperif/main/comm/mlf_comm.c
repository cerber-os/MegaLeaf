/**
 * @file mlf_comm.c
 * @author Pawel Wieczorek
 * @brief Compatiblity layer for MLF controller UART protocol
 * @date 2022-07-30
 * 
 */
#include "mlf_comm.h"
#include "mlf_protocol.h"

#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include <esp_log.h>
#include <driver/uart.h>

/*****************************
 * CONFIG MACROS
 *****************************/
#define MLF_UART_NUM        UART_NUM_2
#define R_BUF_SIZE          256
#define MAX_RESP_DELAY      (100 / portTICK_PERIOD_MS)
#define BIT_0	            ( 1 << 0 )

static const char* TAG = "mlf_comm";

/*****************************
 * GLOBAL STRUCTURES
 *****************************/
static struct MLF_ctx mlf_ctx;
static struct packet_buffer* mlf_packet_buffer;

static QueueHandle_t mlf_queue_handler;
static EventGroupHandle_t xEvResponseAvail;


/*****************************
 * UART event task
 *  handles processing of parts of
 *  incomming packets
 *****************************/
static void mlf_uart_event_task(void* args) {
    uart_event_t event;
    const size_t readBufSize = 64;
    uint8_t* readBuf = malloc(readBufSize);

    for(;;) {
        if(!xQueueReceive(mlf_queue_handler, &event, portMAX_DELAY))
            continue;

        switch(event.type) {
            case UART_DATA:
                for(int dataLeft = event.size; dataLeft > 0;) {
                    int size = dataLeft > readBufSize ? readBufSize : dataLeft;

                    uart_read_bytes(MLF_UART_NUM, readBuf, size, portMAX_DELAY);
                    packet_buffer_append(mlf_packet_buffer, readBuf, size);
                    dataLeft -= size;
                }
                break;
            
            default:
                ESP_LOGW(TAG, "unknown event type (%d)", event.type);
                break;
        }

        if(MLF_is_packet_available(&mlf_ctx))
            MLF_process_packet(&mlf_ctx);
    }

    free(readBuf);
    vTaskDelete(NULL);
}


/*****************************
 * RESPONSE PROCESSING
 *****************************/
volatile static void* mlf_response_buffer;
volatile static size_t mlf_response_buffer_size;

static int mlf_response_handler(uint8_t* data, uint16_t size, uint8_t* res0, uint16_t* res1) {
    void* respBuffer = mlf_response_buffer;
    if(respBuffer != NULL)
        return MLF_RET_NOT_READY;

    if(size != 0) {
        respBuffer = malloc(size);
        if(respBuffer == NULL) {
            ESP_LOGE(TAG, "failed to allocate buffer for response data");
            return MLF_RET_NOT_READY;
        }
        memcpy(respBuffer, data ,size);
    }

    mlf_response_buffer = respBuffer;
    mlf_response_buffer_size = size;
    xEventGroupSetBits(xEvResponseAvail, BIT_0);
    ESP_LOGD(TAG, "%s: notified about new message [size=%u]", __func__, size);
    return 0;
}


static bool MLF_Comm_WaitForResponse() {
    EventBits_t uxBits;

    uxBits = xEventGroupWaitBits(xEvResponseAvail, BIT_0, true, false, MAX_RESP_DELAY);
    if(uxBits & BIT_0)
        return true;

    return false;
}


/*****************************
 * FREERTOS COMPATIBILITY LAYER
 *****************************/
static int _MLF_Comm_write_func(uint8_t* data, uint16_t size) {
    uart_write_bytes(MLF_UART_NUM, data, size);
    return 0;
}

void MLF_Comm_init(void) {
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    
    // Setup UART port
    uart_driver_install(MLF_UART_NUM, R_BUF_SIZE, 0, 10, &mlf_queue_handler, 0);
    uart_set_pin(MLF_UART_NUM, 4, 5, 18, 19);
    ESP_ERROR_CHECK(uart_param_config(MLF_UART_NUM, &uart_config));

    // Initialize MLF protocol layer
    if(MLF_init(&mlf_ctx, _MLF_Comm_write_func)) {
        ESP_LOGE(TAG, "failed to initialize MLF protocol lib");
        return;
    }

    mlf_packet_buffer = packet_buffer_init(&mlf_ctx);
    if(mlf_packet_buffer == NULL) {
        ESP_LOGE(TAG, "failed to initalize MLF packet buffer library");
        return;
    }

    xEvResponseAvail = xEventGroupCreate();

    xTaskCreate(mlf_uart_event_task, "mlf_uart_event_task", 6144, NULL, 12, NULL);

    MLF_register_callback(&mlf_ctx, MLF_CMD_HANDLE_RESPONSE, mlf_response_handler);
}

int MLF_Comm_sendCmd(enum MLF_commands cmd, void* data, uint16_t size, void* resp, size_t* resp_size) {
    size_t copy_size;

    mlf_response_buffer = NULL;
    
    MLF_SendCmd(&mlf_ctx, cmd, data, size);
    
    // Wait for command response
    if(!MLF_Comm_WaitForResponse()) {
        ESP_LOGE(TAG, "failed to collect response from MLF controller");
        return -1;
    }
    
    // Copy response
    if(resp != NULL && resp_size != NULL) {
        if(mlf_response_buffer_size > *resp_size)
            copy_size = *resp_size;
        else
            copy_size = mlf_response_buffer_size;
        
        memcpy(resp, mlf_response_buffer, copy_size);
        *resp_size = mlf_response_buffer_size;
    }

    free(mlf_response_buffer);
    mlf_response_buffer = NULL;
    mlf_response_buffer_size = 0;

    return 0;
}

void MLF_Comm_setCallback(enum MLF_commands cmd, MLF_command_handler cb) {
    MLF_register_callback(&mlf_ctx, cmd, cb);
}

/********************
 * COMMANDS SENDERS
 ********************/
int MLF_Comm_SetBrightness(uint16_t brightness) {
    ESP_LOGI(TAG, "MLF_COMM - setting brightness to %d", brightness);

    struct MLF_req_cmd_set_brightness bright = {
        .brightness = brightness,
        .strip = 0b11
    };

    MLF_Comm_sendCmd(MLF_CMD_SET_BRIGHTNESS, &bright, sizeof(bright), NULL, NULL);
    return 0;
}

int MLF_Comm_SetEffect(uint8_t mode, uint8_t speed, uint8_t strip, uint32_t color) {
    ESP_LOGI(TAG, "MLF_COMM - setting effect to %d-%d-%d", mode, speed, color);

    struct MLF_req_cmd_set_effect effect = {
        .effect = mode,
        .speed = speed,
        .strip = strip,
        .color = color
    };

    MLF_Comm_sendCmd(MLF_CMD_SET_EFFECT, &effect, sizeof(effect), NULL, NULL);
    return 0;
}

int MLF_Comm_TurnOff(void) {
    ESP_LOGI(TAG, "MLF_COMM - turning panel off ...");

    MLF_Comm_sendCmd(MLF_CMD_TURN_OFF, NULL, 0, NULL, NULL);
    return 0;
}

int MLF_Comm_TurnOn(void) {
    ESP_LOGI(TAG, "MLF_COMM - turning panel on ...");

    MLF_Comm_sendCmd(MLF_CMD_TURN_ON, NULL, 0, NULL, NULL);
    return 0; 
}


int MLF_Comm_GetBrightness(uint16_t* brightness) {
    uint8_t resp[MLF_RESP_CMD_GET_BRIGHTNESS_LEN];
    size_t resp_size = sizeof(resp);
    uint16_t tmp;

    MLF_Comm_sendCmd(MLF_CMD_GET_BRIGHTNESS, NULL, 0, resp, &resp_size);
    if(resp_size < sizeof(resp)) {
        ESP_LOGE(TAG, "%s: invalid response size received - %d (expected: %d)", __func__, resp_size, sizeof(resp));
        return MLF_RET_INVALID_HEADER;
    }

    struct MLF_resp_cmd_get_brightness* resp_bright = (struct MLF_resp_cmd_get_brightness*) resp;
    tmp = resp_bright->brightness;

    // Scale brightness to range 0-100
    *brightness = tmp * 100 / 255;
    return 0;
}

int MLF_Comm_GetEffect(uint16_t* effect, uint16_t* speed, uint32_t* color) {
    uint8_t resp[MLF_RESP_CMD_GET_EFFECT_LEN];
    size_t resp_size = sizeof(resp);

    MLF_Comm_sendCmd(MLF_CMD_GET_EFFECT, NULL, 0, resp, &resp_size);
    if(resp_size < sizeof(resp)) {
        ESP_LOGE(TAG, "%s: invalid response size received - %d (expected: %d)", __func__, resp_size, sizeof(resp));
        return MLF_RET_INVALID_HEADER;
    }

    struct MLF_resp_cmd_get_effect* resp_effect = (struct MLF_resp_cmd_get_effect*) resp;
    if(effect) *effect = resp_effect->effect;
    if(speed) *speed = resp_effect->speed;
    if(color) *color = resp_effect->color;
    return 0;
}

int MLF_Comm_GetOnState(uint8_t* on_state) {
    uint8_t resp[MLF_RESP_CMD_GET_ON_STATE_LEN];
    size_t resp_size = sizeof(resp);

    MLF_Comm_sendCmd(MLF_CMD_GET_ON_STATE, NULL, 0, resp, &resp_size);
    if(resp_size < sizeof(resp)) {
        ESP_LOGE(TAG, "%s: invalid response size received - %d (expected: %d)", __func__, resp_size, sizeof(resp));
        return MLF_RET_INVALID_HEADER;
    }

    struct MLF_resp_cmd_get_on_state* resp_state = (struct MLF_resp_cmd_get_on_state*) resp;
    if(on_state)
        *on_state = !!resp_state->is_on;    
    return 0;
}
