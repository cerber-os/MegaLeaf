# MegaLeaf protocol library

This C++ library provides easy to use interface for interacting with MegaLeaf Controller over USB CDC protocol. Bindings exists for both C and Python.

# Building

Just use provided cmake configuration to build and install library

```sh
mkdir -p output
cd output
cmake ..
cmake --build -j8 .
cmake --install -j8 .
```


# API

## C++ API

## C API
### Library initialization
```c
/**
 * @brief Initializes MLFProtoLib object
 * 
 * @param path path to MLF Controller device or empty to auto detect
 * @return MLF_handler library handler or NULL if an error occurred
 */
MLF_handler MLFProtoLib_Init(char* path);

/**
 * @brief Deinitializes MLFProtoLib object
 * 
 * @param handle MLFProtoLib handler
 */
void MLFProtoLib_Deinit(MLF_handler handle);
```

### Basic information retrieval
```c
/**
 * @brief Retrieve current FW version
 * 
 * @param handle MLFProtoLib handler
 * @param version pointer to memory in which FW version will be stored
 */
void MLFProtoLib_GetFWVersion(MLF_handler handle, int* version);

/**
 * @brief Retrieve number of LEDS on both strips
 * 
 * @param handle MLFProtoLib handler
 * @param top    number of leds stored on top strip
 * @param bottom number of leds stored on bottom strip
 */
void MLFProtoLib_GetLedsCount(MLF_handler handle, int* top, int* bottom);
```

### LED panel controls:
```c
/**
 * @brief Bring back all LEDs to the state from before calling
 *          MLFProtoLib_TurnOff
 * 
 * @param handle MLFProtoLib handler
 * @return int  0 on success, -1 otherwise
 */
int MLFProtoLib_TurnOn(MLF_handler handle);

/**
 * @brief Turn off all LEDs
 * 
 * @param handle MLFProtoLib handler
 * @return int   0 on success, -1 otherwise
 */
int MLFProtoLib_TurnOff(MLF_handler handle);

/**
 * @brief Return true if LEDs aren't in OFF mode now
 * 
 * @param handle MLFProtoLib handler
 * @return int   0/1 - LEDs off/on, -1 in case of an error
 */
int MLFProtoLib_IsTurnedOn(MLF_handler handle);

/**
 * @brief Set brightness of all LEDS
 * 
 * @param handle MLFProtoLib handler
 * @param val    target brightness to be set
 * @return int   0 on success, -1 otherwise
 */
int MLFProtoLib_SetBrightness(MLF_handler handle, int val);

/**
 * @brief Set color of all LEDS
 * 
 * @param handle MLFProtoLib handler
 * @param colors array of integers representing color of each LED
 * @param len    number of elements in `colors`
 * @return int   0 on success, -1 otherwise
 */
int MLFProtoLib_SetColors(MLF_handler handle, int* colors, int len);

/**
 * @brief Run one of the predefined effects on selected LED strip
 * 
 * @param handle MLFProtoLib handler
 * @param effect ID of predefined effect
 * @param speed  how fast the effect will be running (lowest: 0)
 * @param strip  bitfield indicating target strip (1 - bottom, 2 - top)
 * @param color  optional color parameter used by selected effect
 * @return int   0 on success, -1 otherwise
 */
int MLFProtoLib_SetEffect(MLF_handler handle, int effect, int speed, int strip, int color);

/**
 * @brief Acquire currently set brightness from MegaLeaf controller
 * 
 * @param handle MLFProtoLib handler
 * @return int   [0:255] current brightness, -1 in case of an error
 */
int MLFProtoLib_GetBrightness(MLF_handler handle);

/**
 * @brief Acquire currently set effect from MegaLeaf controller
 * 
 * @param handle MLFProtoLib handler
 * @param effect place to store ID of current effect
 * @param speed  place to store the speed of current effect
 * @param color  place to store the color associated with present effect
 */
int MLFProtoLib_GetEffect(MLF_handler handle, int* effect, int* speed, int* color);

```

## Python API

# Examples
Simple Python application enabling rainbow effect on LED panel and setting brightness to 50%

```python
import MLFProtoLib

def main():
    try:
        mlf = MLFProtoLib()
        mlf.setBrightness(127)
        mlf.setEffect(2, 0)
    except MLFException:
        # Handle transmission errors
        print("An error occurred during communication with MLF controller")
        raise

if __name__ == '__main__':
    main()
```

C++ code achieving the same effect as the above Python code

```cpp
#include <iostream>
#include "MLFProtoLib.hpp"

int main() {
    try {
        MLFProtoLib controller;
        controller.setBrightness(255);
        controller.setEffect(2, 0, 0b11, 0);
    } except(MLFException& ex) {
        std::cerr << "Failed to communicate with MLF controller";
        std::cerr << ex.what() << std::endl;
    }
}
```