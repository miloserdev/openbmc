SUMMARY = "Activate/deactivate ACTIVE# gpio"
DESCRIPTION = "Activate/deactivate ACTIVE# gpio line, which indicates that manager is ready"
SECTION = "base"
PR = "r1"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://LICENSE;md5=b234ee4d69f5fce4486a80fdaf4a4263"

inherit systemd
inherit features_check

REQUIRED_DISTRO_FEATURES = "systemd"
RDEPENDS_${PN} += "systemd bash gpio-funcs"
SYSTEMD_SERVICE_${PN} = "active-gpio-on.service"

SRC_URI = " \
            file://LICENSE \
            file://active-gpio-on \
            file://active-gpio-on.service \
          "

S = "${WORKDIR}"

do_install() {
  install -d ${D}/libexec
  install -d ${D}${systemd_system_unitdir}
  install -m 755 active-gpio-on ${D}/libexec
  install -m 644 active-gpio-on.service ${D}${systemd_system_unitdir}
}

FILES_${PN} = "/libexec ${systemd_system_unitdir}"
