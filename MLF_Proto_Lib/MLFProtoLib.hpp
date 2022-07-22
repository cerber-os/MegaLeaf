#include "uapi/mlf_protocol_uapi.h"

#include <fcntl.h>
#include <cstdio>
#include <stdexcept>
#include <string>

/**
 * @brief 
 * 
 */
class MLFException : std::exception {

    std::string message;

public:
    MLFException(const char* msg, bool storeErrno = false);
    const char* what(void) const noexcept;
};

/**
 * @brief 
 * 
 */
class MLFProtoLib {
    /* Char device used to communicate with device */
    int dev;

    /* Path to file pointed by `dev` member (for debug purpose) */
    std::string device_name;

    int leds_count_top, leds_count_bottom;
    int fw_version;


    void _read(void* data, size_t len);
    void _write(void* data, size_t len);

    int  _calcCRC(void* data, int len);
    int  _appendCRC(int crc, void* data, int len);
    void _sendData(int cmd, void* data = nullptr, int len = 0);
    int  _recvData(void* output = NULL, int outputLen = 0, int* actualLen = 0);
    int  invokeCmd(int cmd, void* data = nullptr, int len = 0);
    int  invokeCmd(int cmd, void* data, int len, void* resp, int* respLen);

    void errorToException(const char* message, int error);

public:
    MLFProtoLib(std::string path = "");
    ~MLFProtoLib();

    void getFWVersion(int& version) const;
    void getLedsCount(int& top, int& bottom) const;

    void turnOff(void);
    void setBrightness(int brightness);
    void setColors(int* colors, int len);
    void setEffect(int effect, int speed, int strip, int color);
};
