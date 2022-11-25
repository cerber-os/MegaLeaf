# MegaLeaf protocol library

C++ library providing easy to use interface for interacting with MegaLeaf Controller over USB CDC protocol. Bindings exists for both C and Python as well.

# Building

Just use provided cmake configuration to build and install library

```sh
mkdir -p output
cd output
cmake ..
cmake --build -j8 .
cmake --install -j8 .
```

After executing this commands, you should end up with `libMLFProtoLib.so` file.

# Examples
Simple Python application enabling rainbow effect on LED panel and setting brightness to 50%

```python
from MLFProtoLib import MLFProto, MLFEffect

def main():
    try:
        mlf = MLFProto()
        mlf.setBrightness(127)
        mlf.setEffect(effect=MLFEffect.RAINBOW, speed=2)
    except MLFException as e:
        # Handle transmission errors
        print("An error occurred during communication with MLF controller:")
        print(repr(e))
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

Plain C example:

```c
#include <stdio.h>
#include "MLFProtoLib.h"

int main() {
    int ret;
    MLF_handler handler = MLFProtoLib_Init(NULL);
    if(handler == NULL)
        return 1;
    
    ret = MLFProtoLib_SetBrightness(handler, 127);
    if(ret) {
        fprintf(stderr, "Failed to set brightness: %s", MLFProtoLib_GetError(handler));
        ret = 1;
        goto exit;
    }

    ret = MLFProtoLib_SetEffect(handler, 2, 0, 0b11, 0);
    if(ret) {
        fprintf(stderr, "Failed to set effect: %s", MLFProtoLib_GetError(handler));
        ret = 1;
        goto exit;
    }

exit:
    MLFProtoLib_Deinit(handler);
    return ret;
}

```
