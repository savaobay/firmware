################################################################################
#
# autonight
#
################################################################################

SERIAL_SITE_METHOD = local
SERIAL_SITE = $(SERIAL_PKGDIR)/src

SERIAL_LICENSE = MIT
SERIAL_LICENSE_FILES = LICENSE
SERIAL_DEPENDENCIES += libcurl-openipc

define SERIAL_BUILD_CMDS
	$(TARGET_CC) $(@D)/serial.c -lcurl -o $(@D)/serial -s
endef

define SERIAL_INSTALL_TARGET_CMDS
	$(INSTALL) -m 0755 -t $(TARGET_DIR)/usr/bin $(@D)/serial
endef

$(eval $(generic-package))
