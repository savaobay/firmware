LIBMP4V2_OPENIPC_VERSION = HEAD
LIBMP4V2_OPENIPC_SITE = $(call github,enzo1982,mp4v2,$(LIBMP4V2_OPENIPC_VERSION))

LIBMP4V2_OPENIPC_INSTALL_STAGING = YES
LIBMP4V2_OPENIPC_LICENSE = MOZILLA PUBLIC LICENSE
LIBMP4V2_OPENIPC_LICENSE_FILES = README

LIBMP4V2_OPENIPC_DEPENDENCIES = host-pkgconf zlib
LIBMP4V2_OPENIPC_CONF_OPTS += -DBUILD_UTILS=OFF

# define LIBMP4V2_OPENIPC_CONFIGURE_CMDS
#   (cd $(LIBMP4V2_OPENIPC_SRCDIR) \
# 	autoreconf -i)
# endef

$(eval $(cmake-package))

