# Esperif-based WiFi controller

Main MCU responsible for controlling LEDs strip and communicating over USB connector is STM32. In order to support control of MegaLeaf over SmartThings cloud, extra Esperif based MCU has been added to the board. It's sole purpose is to mantain connection with ST cloud over WiFi, monitor for incomming requests and reroute them to STM32 controlling LEDs.

TODO: Photo from smarthings app

## Overview

Project uses generic ESP32C3 subboard, equipped with Xtensa ESP32 microcontroller. 

The communication between Esperif and STM32 is concluded over USART interface. Both microcontrollers shares information with each other - Esperif notifies STM32 about changes comming from SmartThings cloud and STM32 shares events triggered by PC software via USB interface.

Firmware for ESP32 coprocessor uses official Samsung's SmartThing library for intercating with IoT cloud.

## Building

The easiest way to build this project is by embedding it into Samsung's `st-device-sdk-c-ref` library tree and using their buildsystem to trigger compilation and linking. To do this, start by cloning library repository:

```
git clone https://github.com/SmartThingsCommunity/st-device-sdk-c-ref
```

Copy (or create symlink) `mcu_esperif` directory into `apps/esp32c3` subdir of clonned repo. From other projects in `st-device-sdk-c-ref` copy `partitions.2MB.csv` and `sdkconfig` files. Additionaly add config files downloaded from SmartThings workspace (see next section for details). Finally launch build process with `build.py` script present in root directory of `st-device-sdk-c-ref` repository

```
python build.py apps/esp32/mcu_esperif
```

To flash built project issue:

```
python build.py apps/esp32/mcu_esperif flash
```

Alternatively, you could download standalone library for SmartThings connection, built it and link with this project, but the above approach is much simpler and faster as it uses already prebuilt files.

## Creating SmartThings device

TODO: Describe SmartThings device creation process
