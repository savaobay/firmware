################################################################################
#
# serial
#
################################################################################

SERIAL_SITE_METHOD = local
SERIAL_SITE = $(SERIAL_PKGDIR)

SERIAL_LICENSE = MIT
SERIAL_LICENSE_FILES = LICENSE
SERIAL_DEPENDENCIES += libcurl-openipc mbedtls-openipc

SERIAL_TARGET = serial
SERIAL_FAMILY = star6b0
SERIAL_OSDRV = $(SIGMASTAR_OSDRV_INFINITY6B0_PKGDIR)/files/lib

define SERIAL_BUILD_CMDS
$(MAKE) CC=$(TARGET_CC) DRV=$(SERIAL_OSDRV) TARGET=$(SERIAL_TARGET) $(SERIAL_FAMILY) -C $(@D)/src
	# $(TARGET_CC) DRV=$(OSD_OPENIPC_OSDRV) $(@D)/* -lcurl -lmbedtls -lmbedcrypto -o $(@D)/serial -s
endef

define SERIAL_INSTALL_TARGET_CMDS
	$(INSTALL) -m 755 -d $(TARGET_DIR)/etc
	$(INSTALL) -m 644 -t $(TARGET_DIR)/etc $(@D)/serial.yaml

	$(INSTALL) -m 755 -d $(TARGET_DIR)/usr/bin
	$(INSTALL) -m 0755 -t $(TARGET_DIR)/usr/bin $(@D)/src/serial
endef

$(eval $(generic-package))
