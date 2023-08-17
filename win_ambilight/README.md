# Ambient Lightning app for Windows

AmbiLight is a Windows app utilizing DirectX Desktop Duplication API for generating ambient lightning effects on MegaLeaf with minimal CPU and GPU overhead. Application supports both default desktop mode and most of the full-screen DirectX application (though, some games using older DirectX versions or custom DRM may not work).

TODO: GIF preview

## Overview

AmbiLight uses [DDUAPI (Desktop Duplication API)](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/desktop-dup-api) for accessing currently displayed image. DDUAPI creates GPU surface that could be used directly by graphics card without unnecessary memory coping. Every frame, AmbiLights runs simple DirectX shader on this surface to compute vector of size `<image_width> x 2` containing average RGB colors for both halfs of each of the column on screen. Next, computed result is transfered to CPU memory, scaled to match the number of LEDs used by MegaLeaf and processed through basic filters, like hue/saturation tunning for more vibrant colors. Finally, computed colors are sent over USB to MegaLeaf STM32 MCU which displays them on LEDs strip.

TODO: Graph overview

By using GPU accelerated Desktop Duplication API and DirectX shaders, performance impact is neglatable.

## Building

As this project links against DirectX libraries and compiles custom shaders, there's no easy way for doing cross-compilation on Linux. Therefore, you're gonna need to boot your Windows installation and open this project under recent version of Visual Studio with Windows SDK installed.

Once you've copied all the necessary files, simply click `Build solution` for `x86_64/Release` variant. Copy compiled executable and shader file (`.hlsl` file) to a convienient directory and create desktop shortcut for easier start-up (or even add it to autostart if you wish).

## Usage

Ambilight accepts the following cmdline arguments:

```txt
ambilight.exe [-g] [-d] [-b <brightness>]
    -g      Run in GUI mode, which prints all errors in MessageBoxes
            instead of writing them on stdout and renders control icon
            in task bar tray
    -d      Daemonize process on start
    -b      Set initial panel brightness to provided value. By default
            brightness stored in MegaLeaf controller is used
```

Example usage:

```txt
ambilight.exe -g -d -b 255
```

TODO: Image showing taskbar icon and menu

<!--
## Performance

| Benchmark | Normal | With ambilight | Change |
| -- | -- | -- | -- |
| RDR2 | TODO | TODO | ? |
| CIV VI | TODO | TODO | ? |

-->
## Limitations

Desktop Duplication API imposes a few limitations like:
- might not work in games using older DirectX versions or custom DRM/anti-cheat solution,
- does not work in security contexts (Windows Lock Screen for instance),
- might not be able to capture DRM protected content
