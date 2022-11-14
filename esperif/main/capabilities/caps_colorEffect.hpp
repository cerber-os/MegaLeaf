/**
 * @file caps_colorEffect.hpp
 * @author Pawel Wieczorek
 * @brief Class supporting "ColorEffect" custom capability
 * 
 */

#include "caps_base.hpp"

namespace ST_IoT {

class CapsColorEffect : public Capability {
public:
    enum MLF_Effects {
        MLF_EFFECT_NONE = 0,
        MLF_EFFECT_RAINBOW,
        MLF_EFFECT_FADING,
        MLF_EFFECT_STATIC,
        MLF_EFFECT_PROGRESS_BAR,

        MLF_EFFECT_MAX
    };

private:
    const char* MLF_Effects_names[MLF_EFFECT_MAX] = {
        "none", "rainbow", "fading", "static", "progress"
    };

    int effect;

    void handler_cmd(IOT_CAP_HANDLE* handle, const char* value, iot_cap_cmd_data_t* cmd_data);
    static void c_handler_cmd_effect(IOT_CAP_HANDLE* handle, iot_cap_cmd_data_t* cmd_data, void* user_data);

    void handler_init(IOT_CAP_HANDLE* handle);
    static void c_handler_init(IOT_CAP_HANDLE* handle, void* user_data);

    void sendEffectToServer(void);

    int effectNameToID(const char* name);
    const char* effectIDToName(int id);

public:
    CapsColorEffect(IOT_CTX* ctx, const char* component = "main");

    int getEffect(void);
    void setEffect(int value);
    void setEffect(const char* name);

    using Capability::attach;
};

};