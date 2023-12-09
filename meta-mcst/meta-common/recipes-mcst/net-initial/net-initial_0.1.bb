SUMMARY = "Initial network configuration files"
DESCRIPTION = "Initial network configuration files"
SECTION = "base"
PR = "r1"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://LICENSE;md5=b234ee4d69f5fce4486a80fdaf4a4263"

RDEPENDS_${PN} += "bash systemd mcst-fruid xmlstarlet u-boot-fw-utils-aspeed"

inherit systemd
inherit features_check

REQUIRED_DISTRO_FEATURES = "systemd"
SYSTEMD_SERVICE_${PN} = "set_mac.service"

SRC_URI = "file://00-bmc-eth0.network \
           file://00-bmc-eth1.network \
           file://set_mac \
           file://set_mac.service \
           file://LICENSE \
          "

S = "${WORKDIR}"

CONFDIR = "/etc/systemd/network"

do_install() {
  install -d ${D}/libexec
  install -d ${D}${CONFDIR}
  install -d ${D}${systemd_system_unitdir}
  install -m 755 set_mac ${D}/libexec
  install -m 644 00-bmc-eth0.network ${D}${CONFDIR}/00-bmc-eth0.network
  install -m 644 00-bmc-eth1.network ${D}${CONFDIR}/00-bmc-eth1.network
  install -m 644 set_mac.service ${D}${systemd_system_unitdir}
}

FILES_${PN} = "${CONFDIR} /libexec ${systemd_system_unitdir}"
