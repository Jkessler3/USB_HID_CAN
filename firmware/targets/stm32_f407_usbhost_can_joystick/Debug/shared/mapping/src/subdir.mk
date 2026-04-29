################################################################################
# Manually maintained build entry for shared mapping helpers.
################################################################################

C_SRCS += \
../../../shared/mapping/src/joystick_can_mapping.c

OBJS += \
./shared/mapping/src/joystick_can_mapping.o

C_DEPS += \
./shared/mapping/src/joystick_can_mapping.d


# Each subdirectory must supply rules for building sources it contributes
shared/mapping/src/%.o shared/mapping/src/%.su shared/mapping/src/%.cyclo: ../../../shared/mapping/src/%.c shared/mapping/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -c -I../../../shared/can/include -I../../../shared/core/include -I../../../shared/mapping/include -I../../../shared/usb_host_hid/include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-shared-2f-mapping-2f-src

clean-shared-2f-mapping-2f-src:
	-$(RM) ./shared/mapping/src/joystick_can_mapping.cyclo ./shared/mapping/src/joystick_can_mapping.d ./shared/mapping/src/joystick_can_mapping.o ./shared/mapping/src/joystick_can_mapping.su

.PHONY: clean-shared-2f-mapping-2f-src
