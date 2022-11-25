/**
 * @file caps_base.hpp
 * @author Pawel Wieczorek
 * @brief Abstract class providing unified interface for all used capabilities
 * 
 */
#ifndef _H_CAPS_BASE
#define _H_CAPS_BASE

#include "st_dev.h"

namespace ST_IoT {

enum BaseCb{
    CapsCbInit = 1,
    CapsCbCmd = 2,
};

class Capability {
protected:
    void (*cb_init)(void);
    void (*cb_cmd)(const char* cmd, iot_cap_cmd_data_t* cmd_data);

    IOT_CAP_HANDLE* handle;

public:
    Capability();

    void attach(enum BaseCb, void* callback);
};

};


#endif /* _H_CAPS_BASE */
