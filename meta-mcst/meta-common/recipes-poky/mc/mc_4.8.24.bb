SUMMARY = "Midnight Commander is an ncurses based file manager"
HOMEPAGE = "http://www.midnight-commander.org/"
SECTION = "console/utils"

LICENSE = "GPLv3"
LIC_FILES_CHKSUM = "file://COPYING;md5=270bbafe360e73f9840bd7981621f9c2"

DEPENDS = "ncurses glib-2.0 util-linux libssh2"
RDEPENDS_${PN} = "ncurses-terminfo python3-core perl glibc-localedata-en-us"

SRC_URI = "http://www.midnight-commander.org/downloads/${BPN}-${PV}.tar.bz2"
SRC_URI[md5sum] = "2621de1fa9058a9c41a4248becc969f9"
SRC_URI[sha256sum] = "cfcc4d0546d0c3a88645a8bf71612ed36647ea3264d973b1f28183a0c84bae34"

EXTRA_OECONF = "PERL='/usr/bin/perl' --with-screen=ncurses --without-gpm-mouse --without-x --enable-vfs-smb --enable-vfs-sftp --with-homedir=/mnt/data/mc"

inherit autotools gettext pkgconfig
