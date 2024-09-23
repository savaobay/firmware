################################################################################
#
# autonight
#
################################################################################

SERIAL_SITE_METHOD = local
SERIAL_SITE = $(SERIAL_PKGDIR)/src

SERIAL_LICENSE = MIT
SERIAL_LICENSE_FILES = LICENSE
SERIAL_DEPENDENCIES += libcurl-openipc mbedtls-openipc

define SERIAL_BUILD_CMDS
	$(TARGET_CC) $(@D)/* -lcurl -lmbedtls -lmbedcrypto -o $(@D)/serial -s
	cp $(SERIAL_PKGDIR)/serial.yaml $(@D)/
endef

define SERIAL_INSTALL_TARGET_CMDS
	$(INSTALL) -m 755 -d $(TARGET_DIR)/etc
	$(INSTALL) -m 644 -t $(TARGET_DIR)/etc $(@D)/serial.yaml

	$(INSTALL) -m 755 -d $(TARGET_DIR)/usr/bin
	$(INSTALL) -m 0755 -t $(TARGET_DIR)/usr/bin $(@D)/serial
endef

$(eval $(generic-package))
