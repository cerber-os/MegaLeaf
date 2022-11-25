#include "caps_color.hpp"

#include <cstring>
#include "caps/iot_caps_helper_colorControl.h"
#include "external/JSON.h"
#include "esp_log.h"

/*
 * Color conversion
 *  https://stackoverflow.com/a/6930407
 */
typedef struct {
    double r;       // a fraction between 0 and 1
    double g;       // a fraction between 0 and 1
    double b;       // a fraction between 0 and 1
} rgb;
typedef struct {
    double h;       // angle in degrees
    double s;       // a fraction between 0 and 1
    double v;       // a fraction between 0 and 1
} hsv;


hsv rgb2hsv(rgb in)
{
    hsv         out;
    double      min, max, delta;

    min = in.r < in.g ? in.r : in.g;
    min = min  < in.b ? min  : in.b;

    max = in.r > in.g ? in.r : in.g;
    max = max  > in.b ? max  : in.b;

    out.v = max;                                // v
    delta = max - min;
    if (delta < 0.00001)
    {
        out.s = 0;
        out.h = 0; // undefined, maybe nan?
        return out;
    }
    if( max > 0.0 ) { // NOTE: if Max is == 0, this divide would cause a crash
        out.s = (delta / max);                  // s
    } else {
        // if max is 0, then r = g = b = 0              
        // s = 0, h is undefined
        out.s = 0.0;
        out.h = 0.0;                            // its now undefined
        return out;
    }
    if( in.r >= max )                           // > is bogus, just keeps compilor happy
        out.h = ( in.g - in.b ) / delta;        // between yellow & magenta
    else
    if( in.g >= max )
        out.h = 2.0 + ( in.b - in.r ) / delta;  // between cyan & yellow
    else
        out.h = 4.0 + ( in.r - in.g ) / delta;  // between magenta & cyan

    out.h *= 60.0;                              // degrees

    if( out.h < 0.0 )
        out.h += 360.0;

    return out;
}

rgb hsv2rgb(hsv in)
{
    double      hh, p, q, t, ff;
    long        i;
    rgb         out;

    if(in.s <= 0.0) {       // < is bogus, just shuts up warnings
        out.r = in.v;
        out.g = in.v;
        out.b = in.v;
        return out;
    }
    hh = in.h;
    if(hh >= 360.0) hh = 0.0;
    hh /= 60.0;
    i = (long)hh;
    ff = hh - i;
    p = in.v * (1.0 - in.s);
    q = in.v * (1.0 - (in.s * ff));
    t = in.v * (1.0 - (in.s * (1.0 - ff)));

    switch(i) {
    case 0:
        out.r = in.v;
        out.g = t;
        out.b = p;
        break;
    case 1:
        out.r = q;
        out.g = in.v;
        out.b = p;
        break;
    case 2:
        out.r = p;
        out.g = in.v;
        out.b = t;
        break;

    case 3:
        out.r = p;
        out.g = q;
        out.b = in.v;
        break;
    case 4:
        out.r = t;
        out.g = p;
        out.b = in.v;
        break;
    case 5:
    default:
        out.r = in.v;
        out.g = p;
        out.b = q;
        break;
    }
    return out;     
}

/*
 * Capability
 */
namespace ST_IoT {

static const char* MOD_TAG = "Caps_Color";

CapsColor::CapsColor(IOT_CTX* ctx, const char* component) 
    : Capability() {
    int err;

    hue = 0;
    saturation = 100;
    memset(color, 0, sizeof(color));
    
    handle = st_cap_handle_init(ctx, component, caps_helper_colorControl.id, CapsColor::c_handler_init, this);
    if(handle == nullptr) {
        ESP_LOGE(MOD_TAG, "failed to initialize color compatibility handler");
        return;
    }

    err = st_cap_cmd_set_cb(handle, caps_helper_colorControl.cmd_setHue.name, CapsColor::c_handler_cmd_hue, this);
    if(err)
        ESP_LOGE(MOD_TAG, "failed to set cmd_cb for color:hue");
    
    err = st_cap_cmd_set_cb(handle, caps_helper_colorControl.cmd_setSaturation.name, CapsColor::c_handler_cmd_saturation, this);
    if(err)
        ESP_LOGE(MOD_TAG, "failed to set cmd_cb for color:saturation");

    err = st_cap_cmd_set_cb(handle, caps_helper_colorControl.cmd_setColor.name, CapsColor::c_handler_cmd_color, this);
    if(err)
        ESP_LOGE(MOD_TAG, "failed to set cmd_cb for color:color");
}

CapsColor::~CapsColor(void) {

} 

double CapsColor::getHue(void) const { return hue; }
double CapsColor::getSaturation(void) const { return saturation; }
const char* CapsColor::getColor(void) const{ return color; }
int CapsColor::getRGBColor(void) const {
    hsv input = {
        .h = hue * 360.0 / 100.0,
        .s = saturation / 100.0,
        .v = 1.0
    };
    rgb output = hsv2rgb(input);
    int r = (int)(output.r * 255.0);
    int g = (int)(output.g * 255.0);
    int b = (int)(output.b * 255.0);
    return r | (g << 8) | (b << 16);
}

void CapsColor::sendHueToServer(void) const {
    int sequence_no = -1;

    if(!handle) {
        ESP_LOGE(MOD_TAG, "failed to aquire handle");
        return;
    }

    ST_CAP_SEND_ATTR_NUMBER(handle, 
            caps_helper_colorControl.attr_hue.name, 
            hue, NULL, NULL, sequence_no);
    
    if(sequence_no < 0)
        ESP_LOGE(MOD_TAG, "failed to send hue value");
}

void CapsColor::sendSaturationToServer(void) const {
    int sequence_no = -1;

    if(!handle) {
        ESP_LOGE(MOD_TAG, "failed to aquire handle");
        return;
    }

    ST_CAP_SEND_ATTR_NUMBER(handle, 
            caps_helper_colorControl.attr_saturation.name, 
            saturation, NULL, NULL, sequence_no);
    
    if(sequence_no < 0)
        ESP_LOGE(MOD_TAG, "failed to send satturation value");
}

void CapsColor::sendColorToServer(void) const {
    int sequence_no = -1;

    if(!handle) {
        ESP_LOGE(MOD_TAG, "failed to aquire handle");
        return;
    }

    ST_CAP_SEND_ATTR_STRING(handle, 
                caps_helper_colorControl.attr_color.name, 
                (char*) color, NULL, NULL, sequence_no)
    
    if(sequence_no < 0)
        ESP_LOGE(MOD_TAG, "failed to send hue value");
}


void CapsColor::setHue(double hue) {
    this->hue = hue;
    sendHueToServer();
}

void CapsColor::setSaturation(double saturation) {
    this->saturation = saturation;
    sendSaturationToServer();
}

void CapsColor::setColor(const char* color) {
    strncpy(this->color, color, sizeof(this->color) - 1);
    sendColorToServer();
}

void CapsColor::setColorInt(int color) {
    rgb _color = {
        .r = (color & 0xff) / 255.0,
        .g = ((color >> 8) & 0xff) / 255.0,
        .b = ((color >> 16) & 0xff) / 255.0
    };

    hsv ret = rgb2hsv(_color);
    this->hue = ret.h;
    this->saturation = ret.s;
    sendHueToServer();
    sendSaturationToServer();
}


void CapsColor::handler_cmd(IOT_CAP_HANDLE* handle, const char* cmd, iot_cap_cmd_data_t* cmd_data) {
    if(!strcmp(cmd, "hue")) {
        if(cmd_data->num_args != 1 || cmd_data->cmd_data[0].type != IOT_CAP_VAL_TYPE_INT_OR_NUM) {
            ESP_LOGE(MOD_TAG, "received malformed request from server [num_args=%d|type=%d]", 
                cmd_data->num_args, cmd_data->cmd_data[0].type);
            return;
        }

        this->hue = cmd_data->cmd_data[0].number;
    } else if(!strcmp(cmd, "saturation")) {
        if(cmd_data->num_args != 1 || cmd_data->cmd_data[0].type != IOT_CAP_VAL_TYPE_INT_OR_NUM) {
            ESP_LOGE(MOD_TAG, "received malformed request from server [num_args=%d|type=%d]", 
                cmd_data->num_args, cmd_data->cmd_data[0].type);
            return;
        }

        this->saturation = cmd_data->cmd_data[0].number;
    } else if(!strcmp(cmd, "color")) {
        // Well... in this case, server can sent us:
        //  {"hue":96.11,"saturation":34.1}
        if(cmd_data->num_args != 1 || cmd_data->cmd_data[0].type != IOT_CAP_VAL_TYPE_JSON_OBJECT) {
            ESP_LOGE(MOD_TAG, "received malformed request from server [num_args=%d|type=%d]", 
                cmd_data->num_args, cmd_data->cmd_data[0].type);
            return;
        }

        // Panie, w dupe se Pan wsadź tego JSONa tutaj
        JSON_H* json = JSON_PARSE(cmd_data->cmd_data[0].json_object);
        JSON_H* item = JSON_GET_OBJECT_ITEM(json, "hue");
		if (item == NULL)
			ESP_LOGE(MOD_TAG, "missing hue in setColor JSON");
		else
            this->hue = item->valueint;

        item = JSON_GET_OBJECT_ITEM(json, "saturation");
		if (item == NULL)
			ESP_LOGE(MOD_TAG, "missing saturation in setColor JSON");
		else
            this->saturation = item->valueint;

        JSON_DELETE(json);
    } else {
        abort();
    }

    if(cb_cmd)
        cb_cmd(cmd, cmd_data);
    sendHueToServer();
    sendSaturationToServer();

    ESP_LOGI(MOD_TAG, "Set HUE to %lf", this->hue);
    ESP_LOGI(MOD_TAG, "Set Saturation to %lf", this->saturation);
}

void CapsColor::c_handler_cmd_hue(IOT_CAP_HANDLE* handle, iot_cap_cmd_data_t* cmd_data, void* user_data) {
    CapsColor* pCaps = (CapsColor*) user_data;
    pCaps->handler_cmd(handle, "hue", cmd_data);
}

void CapsColor::c_handler_cmd_saturation(IOT_CAP_HANDLE* handle, iot_cap_cmd_data_t* cmd_data, void* user_data) {
    CapsColor* pCaps = (CapsColor*) user_data;
    pCaps->handler_cmd(handle, "saturation", cmd_data);
}

void CapsColor::c_handler_cmd_color(IOT_CAP_HANDLE* handle, iot_cap_cmd_data_t* cmd_data, void* user_data) {
    CapsColor* pCaps = (CapsColor*) user_data;
    pCaps->handler_cmd(handle, "color", cmd_data);
}

void CapsColor::handler_init(IOT_CAP_HANDLE* handle) {
    ESP_LOGD(MOD_TAG, "called init handler");

    if(cb_init)
        cb_init();
    
    sendHueToServer();
    sendSaturationToServer();
}

void CapsColor::c_handler_init(IOT_CAP_HANDLE* handle, void* user_data) {
    CapsColor* pCaps = (CapsColor*) user_data;
    pCaps->handler_init(handle);
}

}