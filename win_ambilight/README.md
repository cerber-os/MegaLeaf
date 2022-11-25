# Ambient Lightning app for Windows

AmbiLight is a Windows app utilizing DirectX Desktop Duplication API for generating ambient lightning effects on MegaLeaf with minimal CPU and GPU overhead. Application supports both default desktop mode and most of the full-screen DirectX application (though, some games using older DirectX versions or custom DRM blocks usage of AmbiLight).

TODO: Photo preview

## Overview

AmbiLight uses [DDUAPI (Desktop Duplication API)](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/desktop-dup-api) for accessing currently displayed image. As, DDUAPI returns GPU surface that could be used by GPU without coping memory, AmbiLights runs simple DirectX shader on frame to compute vector of size `<image_width> x 2` containing average RGB colors of every half of the column on screen (top and bottom parts of column). Next, computed result is copied to CPU memory, scaled to match the number of LEDs used by MegaLeaf and processed throught basic filters, like hue/saturation tunning for more vibrant colors. Finally, computed colors are sent over USB to MegaLeaf STM32 MCU which displays them on LEDs strip.

TODO: Graph overview

By using GPU accelerated Desktop Duplication API and DirectX shaders, performance impact is neglatable.

## Building

As this project links against DirectX libraries and compiles custom shaders, there's no easy way for doing cross-compilation on Linux. Therefore, you're gonna need to boot your Windows installation and open this project under recent version of Visual Studio with Windows SDK installed. Also, copy `.cpp/.hpp` files from `lib` directory in here, unless you want to spent a few hours dealing with CMake on Windows, and linking VS project against custom libraries.

Once you copy all necessary files, simply click `Build solution` for `x86_64/Release` variant. Copy compiled executable and shader file (`.hlsl` file) to convienient directory and create desktop shortcut for easier starting.

## Preview

## Performance

## Limitations
Desktop Duplication API imposes a few limitations, as briefly introduced at the begining of this README, like:
- might not work in games using older DirectX version or custom DRM/anti-cheat solution,
- does not work in security contexts (Lock Screen for instance),
- might not be able to capture DRM protected content
