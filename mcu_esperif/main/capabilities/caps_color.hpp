/**
 * @file caps_color.hpp
 * @author Pawel Wieczorek
 * @brief Class supporting "Color" compatibility
 * 
 */

#include "caps_base.hpp"

namespace ST_IoT {

class CapsColor : public Capability {
    double hue, saturation;
    char color[256];

    void handler_init(IOT_CAP_HANDLE* handle);
    void handler_cmd(IOT_CAP_HANDLE* handle, const char* cmd, iot_cap_cmd_data_t* cmd_data);

    static void c_handler_init(IOT_CAP_HANDLE* handle, void* user_data);
    static void c_handler_cmd_hue(IOT_CAP_HANDLE* handle, iot_cap_cmd_data_t* cmd_data, void* user_data);
    static void c_handler_cmd_saturation(IOT_CAP_HANDLE* handle, iot_cap_cmd_data_t* cmd_data, void* user_data);
    static void c_handler_cmd_color(IOT_CAP_HANDLE* handle, iot_cap_cmd_data_t* cmd_data, void* user_data);

    void sendHueToServer(void) const;
    void sendSaturationToServer(void) const;
    void sendColorToServer(void) const;

public:
    CapsColor(IOT_CTX* ctx, const char* component = "main");
    ~CapsColor();

    double getHue(void) const;
    double getSaturation(void) const;
    const char* getColor(void) const;
    int getRGBColor(void) const;

    void setHue(double hue);
    void setSaturation(double saturation);
    void setColor(const char* value);
    void setColorInt(int color);

    using Capability::attach;
};

}
