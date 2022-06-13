
#include "MLFProtoLib.hpp"

#include <zlib.h>

#include <stdexcept>

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
    if (tcgetattr (fd, &tty) != 0)
        throw std::runtime_error("Failed to get terminal attrs");

    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 5;

    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_cflag |= (CLOCAL | CREAD | CS8);
    tty.c_cflag &= ~(PARENB | PARODD | CSTOPB | CRTSCTS);

    if (tcsetattr (fd, TCSANOW, &tty) != 0)
        throw std::runtime_error("Failed to set terminal attrs");
}

#elif _WIN32

/**
 * @brief Get the path to MLF Controller USB device
 * 
 * @return std::string resolved path to COM port or empty string if not found
 */
static std::string GetPathToUSBDevice(void) {
    // TODO: Add support for windows COM port discovery
    return "";
}

#else

#error "Unsupported build variant - please use either Linux or Windows"

#endif


/************************************
 * COMMUNICATION WITH CONTROLLER
 ************************************/

// TODO: Timeout
void MLFProtoLib::_read(void* data, size_t len) {
    int ret;
    size_t alreadyRead = 0;

    while(alreadyRead < len) {
        ret = read(dev, data + alreadyRead, len - alreadyRead);
        if(ret < 0)
            throw std::runtime_error("Failed to write data");
        alreadyRead += ret;
        usleep(1000 * 300);
    }
}

void MLFProtoLib::_write(void* data, size_t len) {
    int ret;
    size_t alreadyWritten = 0;

    while(alreadyWritten < len) {
        ret = write(dev, data + alreadyWritten, len - alreadyWritten);
        if(ret <= 0)
            throw std::runtime_error("Failed to write data");
        alreadyWritten += ret;
    }
}

int MLFProtoLib::_calcCRC(void* data, int len) {
    long long crc = crc32(0, NULL, 0);
    return crc32(crc, (unsigned char*) data, len);
}

int MLFProtoLib::_appendCRC(int crc, void* data, int len) {
    return crc32(crc, (unsigned char*) data, len);
}

/*
 * Packets from host to controller looks as follow:
 *  |   HEADER   |   BODY   |   FOOTER   |
 *  , where HEADER contains: magic, command no., body size
 *          BODY contains command specific data
 *          FOOTER contains: CRC checksum of HEADER+BODY, magic
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
        .cmd = cmd,
        .data_size = len
    };
    struct MLF_packet_footer footer = {
        .magic = MLF_FOOTER_MAGIC
    };

    const size_t packet_size = sizeof header + len + sizeof footer;
    char* packet = new char[packet_size];
    memcpy(packet, &header, sizeof header);
    memcpy(packet + sizeof header, data, len);
    
    // Calculate CRC of header + body and add it to packet footer
    footer.crc = _calcCRC(packet, sizeof header + len);
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
int MLFProtoLib::_recvData(char* output, int outputLen, int* actualLen) {
    int ret = 0;
    struct MLF_resp_packet_header header;
    struct MLF_packet_footer footer;

    // Read header
    _read(&header, sizeof(header));
    if(header.magic != MLF_HEADER_MAGIC)
        throw std::runtime_error("Invalid header magic was received from MLF Controller");

    printf("DBG: Received HDR %d %d %d\n", header.magic, header.error_code, header.data_size);
    
    // Read data associated with packet
    char* data_buffer = new char[header.data_size];
    _read(data_buffer, header.data_size);
    
    // Read footer
    _read(&footer, sizeof(footer));    
    if(footer.magic != MLF_FOOTER_MAGIC)
        throw std::runtime_error("Invalid footer magic was received from MLF Controller");

    // Validate CRC checksum
    int received_crc = _calcCRC(&header, sizeof header);
    received_crc = _appendCRC(received_crc, data_buffer, header.data_size);
    if(received_crc != footer.crc)
        ;
    // TODO: Fix CRC mismatch
    //    throw std::runtime_error("Incorrect CRC in packet received from MLF Controller");

    // Copy result
    if(outputLen > header.data_size)
        outputLen = header.data_size;
    if(actualLen != nullptr)
        *actualLen = header.data_size;
    if(output != nullptr)
        memcpy(output, data_buffer, outputLen);

    // TODO: Fix memory leak when exception is thrown
    delete[] data_buffer;
    return header.error_code;
}

int MLFProtoLib::invokeCmd(int cmd, void* data, int len) {
    _sendData(cmd, data, len);
    return _recvData();
}

MLFProtoLib::MLFProtoLib(std::string path) {
    if(path == "") {
        path = GetPathToUSBDevice();
        if(path == "")
            throw std::runtime_error("Failed to locate MLF Controller");
    }
    device_name = path;

    dev = open(path.c_str(), O_RDWR);
    if(dev < 0)
        throw std::runtime_error("Failed to open MLF Controller device");

    ConfigureSerialPort(dev);

    // TODO: Retrieve basic info (LEDs count, etc.)
}

MLFProtoLib::~MLFProtoLib() {
    close(dev);
}

void MLFProtoLib::turnOff(void) {
    invokeCmd(MLF_CMD_TURN_OFF);
}

void MLFProtoLib::setBrightness(int brightness) {
    int ret;
    struct MLF_req_cmd_set_brightness data = {
        .brightness = 50,
        .strip = 0b11
    };

    ret = invokeCmd(MLF_CMD_SET_BRIGHTNESS, &data, sizeof data);
    if(ret != MLF_RET_OK)
        throw std::runtime_error("MLF_CMD_SET_BRIGHTNESS failed");
}

void MLFProtoLib::setEffect(int effect, int speed, int strip, int color) {
    int ret;
    struct MLF_req_cmd_set_effect data = {
        .effect = effect,
        .speed = speed,
        .strip = strip,
        .color = color,
    };

    ret = invokeCmd(MLF_CMD_SET_EFFECT, &data, sizeof data);
    if(ret != MLF_RET_OK)
        throw std::runtime_error("MLD_CMD_SET_EFFECT failed");
}



/*************
 * TEMP
 * **************88*/
#include <iostream>

int main() {
    std::cout << "Startup\n";
    
    MLFProtoLib com;
    com.setEffect(2, 1, 0b11, 0x0000ff);
    com.setBrightness(30);
}
