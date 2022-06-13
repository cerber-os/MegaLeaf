#include "mlf_protocol_uapi.h"

#include <fcntl.h>
#include <cstdio>
#include <string>


class MLFProtoLib {
    /* Char device used to communicate with device */
    int dev;

    /* Path to file pointed by `dev` member (for debug purpose) */
    std::string device_name;

    void _read(void* data, size_t len);
    void _write(void* data, size_t len);

    int  _calcCRC(void* data, int len);
    int  _appendCRC(int crc, void* data, int len);
    void _sendData(int cmd, void* data = nullptr, int len = 0);
    int  _recvData(char* output = NULL, int outputLen = 0, int* actualLen = 0);
    int  invokeCmd(int cmd, void* data = nullptr, int len = 0);

public:
    MLFProtoLib(std::string path = "");
    ~MLFProtoLib();

    void getInfo(void);

    void turnOff(void);
    void setBrightness(int brightness);
    void setColors(int* colors, int len);
    void setEffect(int effect, int speed, int strip, int color);
};
