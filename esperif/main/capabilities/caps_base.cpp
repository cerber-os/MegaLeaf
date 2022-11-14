#include "caps_base.hpp"

namespace ST_IoT {

Capability::Capability() {
    cb_init = nullptr;
    cb_cmd = nullptr;
}

void Capability::attach(enum BaseCb target, void* cb) {
    switch(target) {
        case CapsCbInit:
            cb_init = (void (*)()) cb;
            break;
        case CapsCbCmd:
            cb_cmd = (void (*)(const char*, iot_cap_cmd_data_t* cmd_data)) cb;
            break;
        default:
            break;
    }
}

/*void Capability::c_handler_cmd(IOT_CAP_HANDLE* handle, iot_cap_cmd_data_t* cmd_data, void* user_data) {
    Capability* pCaps = (Capability*) user_data;
    pCaps->handler_cmd(handle, cmd_data->command_id, cmd_data);
}*/

}
