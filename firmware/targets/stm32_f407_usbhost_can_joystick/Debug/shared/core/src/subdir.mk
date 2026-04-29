################################################################################
# Manually maintained build entry for shared core helpers.
################################################################################

C_SRCS += \
../../../shared/core/src/spsc_ring.c

OBJS += \
./shared/core/src/spsc_ring.o

C_DEPS += \
./shared/core/src/spsc_ring.d


# Each subdirectory must supply rules for building sources it contributes
shared/core/src/%.o shared/core/src/%.su shared/core/src/%.cyclo: ../../../shared/core/src/%.c shared/core/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -c -I../../../shared/can/include -I../../../shared/core/include -I../../../shared/mapping/include -I../../../shared/usb_host_hid/include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-shared-2f-core-2f-src

clean-shared-2f-core-2f-src:
	-$(RM) ./shared/core/src/spsc_ring.cyclo ./shared/core/src/spsc_ring.d ./shared/core/src/spsc_ring.o ./shared/core/src/spsc_ring.su

.PHONY: clean-shared-2f-core-2f-src
