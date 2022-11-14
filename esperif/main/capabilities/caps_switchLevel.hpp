/**
 * @file caps_switchLevel.hpp
 * @author Pawel Wieczorek
 * @brief Class supporting "SwitchLevel" capability
 * 
 */

#include "caps_base.hpp"

namespace ST_IoT {

class CapsSwitchLevel : public Capability {
    int level;

    void handler_cmd(IOT_CAP_HANDLE* handle, const char* value, iot_cap_cmd_data_t* cmd_data);
    static void c_handler_cmd_level(IOT_CAP_HANDLE* handle, iot_cap_cmd_data_t* cmd_data, void* user_data);

    void handler_init(IOT_CAP_HANDLE* handle);
    static void c_handler_init(IOT_CAP_HANDLE* handle, void* user_data);

    void sendLevelToServer(void);

public:
    CapsSwitchLevel(IOT_CTX* ctx, const char* component = "main");

    int getValue(void);
    void setValue(int value);

    using Capability::attach;
};

};