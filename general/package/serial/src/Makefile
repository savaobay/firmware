SRCS := app_config.c config.c schrift.c bitmap.c region.c text.c compat.c tools.c utils.c watchdog.c serial.c main.c
BUILD = $(CC) $(SRCS) -I $(SDK)/include -L $(DRV) $(LIB) -Os -s -o $(or $(TARGET),$@)

star6b0:

	$(eval SDK = ../sdk/infinity6)
	$(eval LIB = -D__SIGMASTAR__ -D__INFINITY6__ -D__INFINITY6B0__ -lcurl -lmbedtls -lmbedcrypto -lcam_os_wrapper -lm -lmi_rgn -lmi_sys)
	$(BUILD)
