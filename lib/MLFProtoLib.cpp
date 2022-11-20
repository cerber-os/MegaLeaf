/**
 * @file MLFProtoLib.cpp
 * @author Pawel Wieczorek
 * @brief Userspace library for communicating with MegaLeaf controller
 * @date 2022-06-13
 */
#include "MLFProtoLib.hpp"
#include "MLFProtoLib.h"

#include "uapi/mlf_protocol_uapi.h"

#include <fcntl.h>


/************************************
 * PLATFORM SPECIFIC CODE
 ************************************/
#ifdef __linux__

/* Linux specific includes */
#include <dirent.h>
#include <string.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

/* Binary open mode is only required on Windows */
#define O_BINARY    0

/* Const string identifier used by MLF controller during USB enumeration */
#define SERIAL_ID_PREFIX        "usb-cerber-os_MegaLeaf_CDC_Controller_"

/**
 * @brief Get the path to MLF Controller USB device
 * 
 * @return std::string resolved path to the device or empty string if not found
 */
static std::string GetPathToUSBDevice(void) {
    DIR* dirStream;
    struct dirent* dir;

    dirStream = opendir("/dev/serial/by-id/");
    if(dirStream == NULL)
        return "";

    while((dir = readdir(dirStream)) != NULL) 
    {
        if(!strncmp(dir->d_name, SERIAL_ID_PREFIX, strlen(SERIAL_ID_PREFIX)))
            return std::string("/dev/serial/by-id/") + std::string(dir->d_name);
    }

    return "";
}

static void ConfigureSerialPort(int fd) {
    struct termios tty;
    if(tcgetattr(fd, &tty) != 0)
        throw MLFException("failed to setup usb connection - get attrs", true);

    cfsetospeed(&tty, B1152000);
    cfsetispeed(&tty, B1152000);

    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 8;

    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_cflag |= (CLOCAL | CREAD | CS8);
    tty.c_cflag &= ~(PARENB | PARODD | CSTOPB | CRTSCTS);

    if(tcsetattr(fd, TCSANOW, &tty) != 0)
        throw MLFException("failed to setup usb connection - set attrs", true);
}

#elif _WIN32

/* Windows specific include files */
#include <windows.h>

#include <initguid.h>
#include <devpropdef.h>
#include <devpkey.h>
#include <io.h>
#include <setupapi.h>

/* Const string identifier used by MLF controller during USB enumeration */
#define BUS_NAME_PREFIX        "USB Serial Device ("

/**
* @brief Get the path to MLF Controller USB device
*
* @return std::string resolved path to COM port or empty string if not found
*/
static std::string GetPathToUSBDevice(void) {
    std::string result = "";
    DWORD size, ret;
    HDEVINFO devInfoSet;
    SP_DEVINFO_DATA devInfoData = { 0 };
    DEVPROPTYPE PropType;
    wchar_t buffer[128] = { 0 };
    char friendlyName[256] = { 0 };

    devInfoSet = SetupDiGetClassDevs(NULL, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    // Iterate over all available devices
    for (UINT idx = 0; SetupDiEnumDeviceInfo(devInfoSet, idx, &devInfoData); idx++) {

        // Extract Bus Reported Device Description
        ret = SetupDiGetDeviceProperty(devInfoSet, &devInfoData,
            &DEVPKEY_Device_BusReportedDeviceDesc, &PropType, (PBYTE)buffer,
            sizeof(buffer), &size, 0);
        if(!ret || PropType != DEVPROP_TYPE_STRING)
            continue;

        // Ignore all devices except MLF Controller
        if(wcscmp(buffer, L"MegaLeaf CDC Controller") != 0)
            continue;

        // Extract Device Friendly Name
        // The name is of format "USB Serial Device (COMXXX)"
        //  from which COM port number could be extracted
        ret = SetupDiGetDeviceProperty(devInfoSet, &devInfoData,
            &DEVPKEY_Device_FriendlyName, &PropType, (PBYTE)buffer,
            sizeof(buffer), &size, 0);
        if(!ret || PropType != DEVPROP_TYPE_STRING) {
            goto exit;
        }

        size_t size;
        wcstombs_s(&size, friendlyName, buffer, sizeof(friendlyName));
        if(size < sizeof(BUS_NAME_PREFIX) + 2) {
            goto exit;
        }

        friendlyName[size - 2] = '\0';
        result = std::string(&friendlyName[sizeof(BUS_NAME_PREFIX) - 1]) + ":";
    }

exit:
    if(devInfoSet)
        SetupDiDestroyDeviceInfoList(devInfoSet);
    return result;
}

/* On Windows there's no need for additional configuration of serial COM port*/
static void ConfigureSerialPort(int fd) {
    bool ret;
    HANDLE serialPort = (HANDLE)_get_osfhandle(fd);

    DCB dcb;
    ret = GetCommState(serialPort, &dcb);
    if(!ret)
        throw MLFException("failed to setup usb connection - GetCommState", true);

    dcb.BaudRate = CBR_115200;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;
    dcb.fOutX = 0;
    dcb.fInX = 0;
    dcb.fNull = 0;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;

    ret = SetCommState(serialPort, &dcb);
    if(!ret)
        throw MLFException("failed to setup usb connection - SetCommState", true);
    
    // TODO: Set timeouts using SetCommTimeouts
}

/* Nasty fix to overcome "Posix name deprecated" error */
#define open        ::_open
#define read        ::_read
#define write       ::_write
#define close       ::_close
#define strdup      ::_strdup

#else

#error "Unsupported build variant - please use either Linux or Windows"

#endif


/************************************
 * COMMUNICATION WITH CONTROLLER
 ************************************/

MLFException::MLFException(const char* msg, bool storeErrno) {
    message = "MLFException occurred: " + std::string(msg);
    if(storeErrno)
        message += std::string(" [") + strerror(errno) + "]";
}

const char* MLFException::what(void) const noexcept {
    return message.c_str();
}


void MLFProtoLib::_read(void* data, size_t len) {
    int ret;
    size_t alreadyRead = 0;

    while(alreadyRead < len) {
        ret = read(dev, (char*)data + alreadyRead, len - alreadyRead);
        if(ret < 0)
            throw MLFException("failed to read data from MLF Controller", true);
        else if(ret == 0)
            throw MLFException("failed to read data from MLF Controller [Timeout]");

        alreadyRead += ret;
    }
}

void MLFProtoLib::_write(void* data, size_t len) {
    int ret;
    size_t alreadyWritten = 0;

    while(alreadyWritten < len) {
        ret = write(dev, (char*)data + alreadyWritten, len - alreadyWritten);
        if(ret <= 0)
            throw MLFException("failed to write data to MLF Controller", true);
        alreadyWritten += ret;
    }
}

/*
 * Packets from host to controller looks as follow:
 *  |   HEADER   |   BODY   |   FOOTER   |
 *  , where HEADER contains: magic, command no., body size
 *          BODY contains command specific data
 *          FOOTER contains: magic
 */

 /**
  * @brief Send command with optional data from host to controller
  *
  * @param cmd  ID of command to invoke on controller's side
  * @param data (optional) pointer to data sent with command
  * @param len  (optional) size of `data` array
  */
void MLFProtoLib::_sendData(int cmd, void* data, int len) {
    struct MLF_req_packet_header header = {
        .magic = MLF_HEADER_MAGIC,
        .cmd = (uint8_t)cmd,
        .data_size = (uint16_t)len
    };
    struct MLF_packet_footer footer = {
        .magic = MLF_FOOTER_MAGIC
    };

    const size_t packet_size = sizeof header + len + sizeof footer;
    char* packet = new char[packet_size];
    memcpy(packet, &header, sizeof header);
    memcpy(packet + sizeof header, data, len);
    memcpy(packet + sizeof header + len, &footer, sizeof footer);


    _write(packet, packet_size);
    delete[] packet;
}

/*
 * Packets with response from controller looks the same way as packets with
 *  requests, except they contain error code instead of command ID in header.
 */

/**
 * @brief Receive response from controller
 * 
 * @param output    array in which response data will be stored
 * @param outputLen size of `output`
 * @param actualLen number of bytes that would be written to `output` 
 *                   if it had been large enough
 * 
 * @returns error status sent by controller
 */
int MLFProtoLib::_recvData(void* output, int outputLen, int* actualLen) {
    char* data_buffer = NULL;
    struct MLF_resp_packet_header header;
    struct MLF_packet_footer footer;

    // Read header
    _read(&header, sizeof(header));
    if(header.magic != MLF_RESP_HEADER_MAGIC)
        throw MLFException("invalid header magic was received from MLF Controller");

    try {
        // Read data associated with packet
        data_buffer = new char[header.data_size];
        _read(data_buffer, header.data_size);

        // Read footer
        _read(&footer, sizeof(footer));
        if(footer.magic != MLF_FOOTER_MAGIC)
            throw MLFException("invalid footer magic was received from MLF Controller");

        // Copy result
        if(outputLen > header.data_size)
            outputLen = header.data_size;
        if(actualLen != nullptr)
            *actualLen = header.data_size;
        if(output != nullptr)
            memcpy(output, data_buffer, outputLen);
    }
    catch (...) {
        delete[] data_buffer;
        throw;
    }

    delete[] data_buffer;
    return header.error_code;
}

void MLFProtoLib::invokeCmd(int cmd, void* data, int len) {
    int ret;

    _sendData(cmd, data, len);
    ret = _recvData();
    if(ret != MLF_RET_OK)
        errorToException("contoller failed to process command", ret);
}

void MLFProtoLib::invokeCmd(int cmd, void* data, int len, void* resp, int* respLen) {
    int ret;

    _sendData(cmd, data, len);
    ret = _recvData(resp, *respLen, respLen);
    if(ret != MLF_RET_OK)
        errorToException("contoller failed to process command", ret);
}

void MLFProtoLib::errorToException(const char* message, int error) {
    #define CASE_WRAP(X, S)     case X: errorStr = S; break

    const char* errorStr = nullptr;
    switch(error) {
        CASE_WRAP(MLF_RET_INVALID_CMD, "invalid command");
        CASE_WRAP(MLF_RET_INVALID_HEADER, "malformed header");
        CASE_WRAP(MLF_RET_INVALID_DATA, "invalid data");
        CASE_WRAP(MLF_RET_INVALID_FOOTER, "malformed footer");
        CASE_WRAP(MLF_RET_NOT_READY, "device not ready");
        CASE_WRAP(MLF_RET_TIMEOUT, "timeout");
        CASE_WRAP(MLF_RET_DATA_TOO_LARGE, "data too large");
        default:
            errorStr = "unknown error";
            break;
    }

    std::string exceptionMsg = std::string(message) + errorStr;
    throw MLFException(exceptionMsg.c_str());
}


MLFProtoLib::MLFProtoLib(std::string path) {
    if(path == "") {
        path = GetPathToUSBDevice();
        if(path == "")
            throw MLFException("failed to locate MLF Controller");
    }
    device_name = path;

    dev = open(path.c_str(), O_RDWR | O_BINARY);
    if(dev < 0)
        throw MLFException("failed to open MLF Controller device", true);

    try {
        ConfigureSerialPort(dev);

        // Retrieve basic info
        struct MLF_resp_cmd_get_info resp;
        int resp_size = sizeof(resp);

        invokeCmd(MLF_CMD_GET_INFO, nullptr, 0, &resp, &resp_size);
        if(resp_size != sizeof(resp))
            throw MLFException("failed to get info from MLF Controller");

        fw_version = resp.fw_version;
        leds_count_top = resp.leds_count_top;
        leds_count_bottom = resp.leds_count_bottom;
    }
    catch (...) {
        close(dev);
        throw;
    }
}

MLFProtoLib::~MLFProtoLib() {
    close(dev);
}

void MLFProtoLib::getFWVersion(int& version) const {
    version = fw_version;
}

void MLFProtoLib::getLedsCount(int& top, int& bottom) const {
    top = leds_count_top;
    bottom = leds_count_bottom;
}

void MLFProtoLib::turnOn(void) {
    invokeCmd(MLF_CMD_TURN_ON);
}

void MLFProtoLib::turnOff(void) {
    invokeCmd(MLF_CMD_TURN_OFF);
}

int MLFProtoLib::isTurnedOn(void) {
    struct MLF_resp_cmd_get_on_state on_state;
    int respLen = sizeof(on_state);

    invokeCmd(MLF_CMD_GET_ON_STATE, nullptr, 0, &on_state, &respLen);
    if(respLen < sizeof(on_state))
        throw MLFException("Failed to request on_state - invalid response size");
    return !!on_state.is_on;
}

void MLFProtoLib::setBrightness(int brightness) {
    struct MLF_req_cmd_set_brightness data = {
        .brightness = (uint8_t)brightness,
        .strip = 0b11
    };

    invokeCmd(MLF_CMD_SET_BRIGHTNESS, &data, sizeof data);
}

void MLFProtoLib::setColors(int* colors, int len) {
    const int dataLen = sizeof(struct MLF_req_cmd_set_color) + sizeof(int) * len;
    struct MLF_req_cmd_set_color* data = NULL;
    data = (struct MLF_req_cmd_set_color*) new char[dataLen];

    memcpy(data->colors, colors, len * sizeof(int));

    try {
        invokeCmd(MLF_CMD_SET_COLOR, data, dataLen);
    }
    catch (...) {
        delete[] data;
        throw;
    }

    delete[] data;
}

void MLFProtoLib::setEffect(int effect, int speed, int strip, int color) {
    struct MLF_req_cmd_set_effect data = {
        .effect = (uint8_t)effect,
        .speed = (uint8_t)speed,
        .strip = (uint8_t)strip,
        .color = (uint32_t)color,
    };

    invokeCmd(MLF_CMD_SET_EFFECT, &data, sizeof data);
}

int MLFProtoLib::getBrightness(void) {
    struct MLF_resp_cmd_get_brightness data = {0};
    int respLen = sizeof(data);

    invokeCmd(MLF_CMD_GET_BRIGHTNESS, NULL, 0, &data, &respLen);
    if(respLen < sizeof(data))
        throw MLFException("Failed to get brightness - invalid response size");

    return data.brightness;
}

void MLFProtoLib::getEffect(int* effect, int* speed, int* color) {
    struct MLF_resp_cmd_get_effect data = {0};
    int respLen = sizeof(data);

    invokeCmd(MLF_CMD_GET_EFFECT, NULL, 0, &data, &respLen);
    if(respLen < sizeof(data))
        throw MLFException("Failed to get effect - invalid response size");
    
    if(effect)
        *effect = data.effect;
    if(speed)
        *speed = data.speed;
    if(color)
        *color = data.color;
}

/************************************
 * C bindings
 ************************************/
struct MLF_C_Object {
    MLFProtoLib* instance;
    const char* exceptionMessage;
};


MLF_handler MLFProtoLib_Init(char* path) {
    try {
        MLF_handler handle = new MLF_C_Object;
        handle->instance = new MLFProtoLib(path);
        handle->exceptionMessage = NULL;
        return handle;
    }
    catch (std::exception& ex) {
        fprintf(stderr, "[%s:%d] Failed to initialize library - error '%s'\n", 
                __FILE__, __LINE__, ex.what());
        return NULL;
    }
}

void MLFProtoLib_Deinit(MLF_handler handle) {
    if(handle == NULL)
        return;
    
    try {
        free((void*) handle->exceptionMessage);
        delete handle->instance;
        delete handle;
    }
    catch (...) {
        ;
    }
}

void MLFProtoLib_GetFWVersion(MLF_handler handle, int* version) {
    try {
        int ver;
        handle->instance->getFWVersion(ver);
        *version = ver;
    }
    catch (...) {
        ;
    }
}

void MLFProtoLib_GetLedsCount(MLF_handler handle, int* top, int* bottom) {
    try {
        int t, b;
        handle->instance->getLedsCount(t, b);
        *top = t;
        *bottom = b;
    }
    catch (...) {
        ;
    }
}

int MLFProtoLib_TurnOn(MLF_handler handle) {
    try {
        handle->instance->turnOn();
    } catch (std::exception& ex) {
        if(handle->exceptionMessage)
            free((void*) handle->exceptionMessage);
        handle->exceptionMessage = strdup(ex.what());
        return -1;
    }
    return 0;
}

int MLFProtoLib_TurnOff(MLF_handler handle) {
    try {
        handle->instance->turnOff();
        return 0;
    }
    catch (std::exception& ex) {
        if(handle->exceptionMessage)
            free((void*) handle->exceptionMessage);
        handle->exceptionMessage = strdup(ex.what());
        return -1;
    }
}

int MLFProtoLib_IsTurnedOn(MLF_handler handle) {
    int result;

    try {
        result = handle->instance->isTurnedOn();
    } catch (std::exception& ex) {
        if(handle->exceptionMessage)
            free((void*) handle->exceptionMessage);
        handle->exceptionMessage = strdup(ex.what());
        return -1;
    }

    return result;
}

int MLFProtoLib_SetBrightness(MLF_handler handle, int val) {
    try {
        handle->instance->setBrightness(val);
        return 0;
    }
    catch (std::exception& ex) {
        if(handle->exceptionMessage)
            free((void*) handle->exceptionMessage);
        handle->exceptionMessage = strdup(ex.what());
        return -1;
    }
}

int MLFProtoLib_SetColors(MLF_handler handle, int* colors, int len) {
    try {
        handle->instance->setColors(colors, len);
        return 0;
    }
    catch (std::exception& ex) {
        if(handle->exceptionMessage)
            free((void*) handle->exceptionMessage);
        handle->exceptionMessage = strdup(ex.what());
        return -1;
    }
}

int MLFProtoLib_SetEffect(MLF_handler handle, int effect, int speed, int strip, int color) {
    try {
        handle->instance->setEffect(effect, speed, strip, color);
        return 0;
    }
    catch (std::exception& ex) {
        if(handle->exceptionMessage)
            free((void*) handle->exceptionMessage);
        handle->exceptionMessage = strdup(ex.what());
        return -1;
    }
}

int MLFProtoLib_GetBrightness(MLF_handler handle) {
    int result;
    try {
        result = handle->instance->getBrightness();
    } catch (std::exception& ex) {
        if(handle->exceptionMessage)
            free((void*) handle->exceptionMessage);
        handle->exceptionMessage = strdup(ex.what());
        return -1;
    }
    return result;
}

int MLFProtoLib_GetEffect(MLF_handler handle, int* effect, int* speed, int* color) {
    try {
        handle->instance->getEffect(effect, speed, color);
    } catch (std::exception& ex) {
        if(handle->exceptionMessage)
            free((void*) handle->exceptionMessage);
        handle->exceptionMessage = strdup(ex.what());
        return -1;
    }
    return 0;
}

const char* MLFProtoLib_GetError(MLF_handler handle) {
    return handle->exceptionMessage;
}
