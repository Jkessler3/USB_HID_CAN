################################################################################
# Manually maintained build entry for shared USB host HID helpers.
################################################################################

C_SRCS += \
../../../shared/usb_host_hid/src/usb_host_hid.c

OBJS += \
./shared/usb_host_hid/src/usb_host_hid.o

C_DEPS += \
./shared/usb_host_hid/src/usb_host_hid.d


# Each subdirectory must supply rules for building sources it contributes
shared/usb_host_hid/src/%.o shared/usb_host_hid/src/%.su shared/usb_host_hid/src/%.cyclo: ../../../shared/usb_host_hid/src/%.c shared/usb_host_hid/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -c -I../../../shared/can/include -I../../../shared/core/include -I../../../shared/mapping/include -I../../../shared/usb_host_hid/include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-shared-2f-usb_host_hid-2f-src

clean-shared-2f-usb_host_hid-2f-src:
	-$(RM) ./shared/usb_host_hid/src/usb_host_hid.cyclo ./shared/usb_host_hid/src/usb_host_hid.d ./shared/usb_host_hid/src/usb_host_hid.o ./shared/usb_host_hid/src/usb_host_hid.su

.PHONY: clean-shared-2f-usb_host_hid-2f-src
