# MegaLeaf schematic and PCB

Hardware base of MegaLeaf project is custom-designed and home-made PCB board, equiped with both STM32F4 and Esperif ESP32-C3 microcontrollers.

## Schematic

The main goal of custom-designed board was to support communication between LEDs strip and STM32F4 MCU (via voltage level shifter). Furthermore, it connects MCU with both Esperif ESP32 Wifi module (for IoT stuff) and USB type C connector (used in old-school USB2.0-FS mode). Finally, there's a 3.3V voltage source from linear stabilizer powered from the LEDs power supply and USB ESD protection.

Schematic preview: 
![MegaLeaf schematic view](renders/MegaLeafPCB.svg)

**Note**: Full schematic exported to PDF is available [here](renders/MegaLeafPCB.pdf).

## PCB views

The design of PCB has been stricly adapted for the process of home-made DIY PCB board. Therefore, vias are huge (1.2mm diameter) and limited to the very minimum (as every single one of them has to be drilled manually), copper tracks are extra spaced when possible to prevent problems during itching process and all components are far from each other to ease soldering on board without soldermask.

But since, there aren't any very high frequency signals (48MHz from USB2.0 is our fastets lines), all the above flaws does not affect final performance and stability of the board.

| Front | Back |
| -- | -- |
| ![Board front image](renders/Board_Render.jpg) | ![Board back image](renders/Board_Render_Back.jpg) |

## Sources

1. Getting started with STM32F4 hardware development - [AN4488](https://www.st.com/resource/en/application_note/dm00115714-getting-started-with-stm32f4xxxx-mcu-hardware-development-stmicroelectronics.pdf)

2. Esperif hardware reference - [docs.esperif.com](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/hw-reference/index.html)
