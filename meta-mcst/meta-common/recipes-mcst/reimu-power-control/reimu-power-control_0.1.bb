SUMMARY = "REIMU power control daemon"
DESCRIPTION = "REIMU power control daemon"
SECTION = "base"
PR = "r1"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://LICENSE;md5=b234ee4d69f5fce4486a80fdaf4a4263"

inherit systemd
inherit features_check

DEPENDS = "pkgconfig-native libgpiod dbus"
REQUIRED_DISTRO_FEATURES = "systemd"
RDEPENDS_${PN} += "systemd gpio-funcs reimu-conf"
SYSTEMD_SERVICE_${PN} = "reimupwr.service"

SRC_URI = " \
            file://LICENSE \
            file://Makefile \
            file://reimupwr.c \
            file://reimupwr.service \
          "

S = "${WORKDIR}"

do_install_append() {
  install -d ${D}/usr/sbin
  install -d ${D}/var/lib/reimu
  install -d ${D}${systemd_system_unitdir}
  install -m 755 reimupwr ${D}/usr/sbin
  install -m 644 reimupwr.service ${D}${systemd_system_unitdir}
}

FILES_${PN} = "/usr/sbin /var/lib/reimu ${systemd_system_unitdir}"
