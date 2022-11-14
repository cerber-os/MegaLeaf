################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: Local ARM compiler
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../App/Src/app.c \
../App/Src/mlf_effects.c \
../App/Src/mlf_protocol.c \
../App/Src/ws2812b.c 

OBJS += \
./App/Src/app.o \
./App/Src/mlf_effects.o \
./App/Src/mlf_protocol.o \
./App/Src/ws2812b.o 

C_DEPS += \
./App/Src/app.d \
./App/Src/mlf_effects.d \
./App/Src/mlf_protocol.d \
./App/Src/ws2812b.d 


# Each subdirectory must supply rules for building sources it contributes
App/Src/%.o App/Src/%.su: ../App/Src/%.c App/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -DUSE_HAL_DRIVER -DSTM32F401xC -c -I../Core/Inc -I"/home/someone/Projects/STM32IDE/MegaLeaf_STM32F4/App/Inc" -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../USB_DEVICE/App -I../USB_DEVICE/Target -I../Middlewares/ST/STM32_USB_Device_Library/Core/Inc -I../Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-App-2f-Src

clean-App-2f-Src:
	-$(RM) ./App/Src/app.d ./App/Src/app.o ./App/Src/app.su ./App/Src/mlf_effects.d ./App/Src/mlf_effects.o ./App/Src/mlf_effects.su ./App/Src/mlf_protocol.d ./App/Src/mlf_protocol.o ./App/Src/mlf_protocol.su ./App/Src/ws2812b.d ./App/Src/ws2812b.o ./App/Src/ws2812b.su

.PHONY: clean-App-2f-Src

