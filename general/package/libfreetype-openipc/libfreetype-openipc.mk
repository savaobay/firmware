################################################################################
#
# libfreetype-openipc
#
################################################################################

LIBFREETYPE_OPENIPC_VERSION = 2.13.2
LIBFREETYPE_OPENIPC_SITE = https://download.savannah.gnu.org/releases/freetype
LIBFREETYPE_OPENIPC_SOURCE = freetype-$(LIBFREETYPE_OPENIPC_VERSION).tar.xz

LIBFREETYPE_OPENIPC_INSTALL_STAGING = YES
LIBFREETYPE_OPENIPC_LICENSE = GPLv2
LIBFREETYPE_OPENIPC_LICENSE_FILES = README

$(eval $(autotools-package))
