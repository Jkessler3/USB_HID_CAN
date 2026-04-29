################################################################################
# Manually maintained build entry for non-generated app glue.
################################################################################

C_SRCS += \
../App/joystick_translator_app.c

OBJS += \
./App/joystick_translator_app.o

C_DEPS += \
./App/joystick_translator_app.d


# Each subdirectory must supply rules for building sources it contributes
App/%.o App/%.su App/%.cyclo: ../App/%.c App/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -c -I../App -I../USB_HOST/App -I../USB_HOST/Target -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Middlewares/ST/STM32_USB_Host_Library/Core/Inc -I../Middlewares/ST/STM32_USB_Host_Library/Class/HID/Inc -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../../../shared/can/include -I../../../shared/core/include -I../../../shared/mapping/include -I../../../shared/usb_host_hid/include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-App

clean-App:
	-$(RM) ./App/joystick_translator_app.cyclo ./App/joystick_translator_app.d ./App/joystick_translator_app.o ./App/joystick_translator_app.su

.PHONY: clean-App
