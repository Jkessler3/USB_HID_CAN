################################################################################
# Manually maintained build entry for shared CAN helpers.
################################################################################

C_SRCS += \
../../../shared/can/src/can_types.c

OBJS += \
./shared/can/src/can_types.o

C_DEPS += \
./shared/can/src/can_types.d


# Each subdirectory must supply rules for building sources it contributes
shared/can/src/%.o shared/can/src/%.su shared/can/src/%.cyclo: ../../../shared/can/src/%.c shared/can/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -c -I../../../shared/can/include -I../../../shared/core/include -I../../../shared/mapping/include -I../../../shared/usb_host_hid/include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-shared-2f-can-2f-src

clean-shared-2f-can-2f-src:
	-$(RM) ./shared/can/src/can_types.cyclo ./shared/can/src/can_types.d ./shared/can/src/can_types.o ./shared/can/src/can_types.su

.PHONY: clean-shared-2f-can-2f-src
