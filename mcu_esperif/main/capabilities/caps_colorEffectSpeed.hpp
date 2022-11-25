/**
 * @file caps_colorEffectSpeed.hpp
 * @author Pawel Wieczorek
 * @brief Class supporting "ColorEffectSpeed" custom capability
 * 
 */

#include "caps_base.hpp"

namespace ST_IoT {

class CapsColorEffectSpeed : public Capability {
private:
    int speed;

    void handler_cmd(IOT_CAP_HANDLE* handle, const char* value, iot_cap_cmd_data_t* cmd_data);
    static void c_handler_cmd_speed(IOT_CAP_HANDLE* handle, iot_cap_cmd_data_t* cmd_data, void* user_data);

    void handler_init(IOT_CAP_HANDLE* handle);
    static void c_handler_init(IOT_CAP_HANDLE* handle, void* user_data);

    void sendEffectSpeedToServer(void);

    int effectNameToID(const char* name);
    const char* effectIDToName(int id);

public:
    CapsColorEffectSpeed(IOT_CTX* ctx, const char* component = "main");

    int getSpeed(void);
    void setSpeed(int speed);

    using Capability::attach;
};

};