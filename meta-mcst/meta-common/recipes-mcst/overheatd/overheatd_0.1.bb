SUMMARY = "Overheat check daemon"
DESCRIPTION = "Overheat check daemon"
SECTION = "base"
PR = "r1"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://LICENSE;md5=b234ee4d69f5fce4486a80fdaf4a4263"

inherit systemd
inherit features_check

DEPENDS = "libgpiod i2c-tools"
REQUIRED_DISTRO_FEATURES = "systemd"
RDEPENDS_${PN} += "systemd bash gpio-funcs reimu-conf"
SYSTEMD_SERVICE_${PN} = "overheatd.service"

SRC_URI = " \
            file://LICENSE \
            file://setled \
            file://checkalerts \
            file://Makefile \
            file://overheatd.c \
            file://overheatd.service \
          "

S = "${WORKDIR}"

do_install_append() {
  install -d ${D}/usr/bin
  install -d ${D}/usr/sbin
  install -d ${D}${systemd_system_unitdir}
  install -m 755 setled ${D}/usr/bin
  install -m 755 checkalerts ${D}/usr/bin
  install -m 755 overheatd ${D}/usr/sbin
  install -m 644 overheatd.service ${D}${systemd_system_unitdir}
}

FILES_${PN} = "/usr/bin /usr/sbin ${systemd_system_unitdir}"
