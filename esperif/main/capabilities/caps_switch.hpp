/**
 * @file caps_switch.hpp
 * @author Pawel Wieczorek
 * @brief Class supporting "Switch" capability
 * 
 */

#include "caps_base.hpp"

namespace ST_IoT {

class CapsSwitch : public Capability {
    bool update_required = false;
    char switch_value[16];

    void handler_cmd(IOT_CAP_HANDLE* handle, const char* value, iot_cap_cmd_data_t* cmd_data);
    static void c_handler_cmd_on(IOT_CAP_HANDLE* handle, iot_cap_cmd_data_t* cmd_data, void* user_data);
    static void c_handler_cmd_off(IOT_CAP_HANDLE* handle, iot_cap_cmd_data_t* cmd_data, void* user_data);

    void handler_init(IOT_CAP_HANDLE* handle);
    static void c_handler_init(IOT_CAP_HANDLE* handle, void* user_data);

    void sendStateToServer(void);

public:
    CapsSwitch(IOT_CTX* ctx, const char* component = "main");

    const char* getValue(void);
    void setValue(const char* value);
    void setValue(int value);

    using Capability::attach;
};

};
