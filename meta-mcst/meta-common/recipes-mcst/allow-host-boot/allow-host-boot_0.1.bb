SUMMARY = "Allow host boot"
DESCRIPTION = "Allow host boot by releasing SPI_CONNECT and pushing power button on if needed"
SECTION = "base"
PR = "r1"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://LICENSE;md5=b234ee4d69f5fce4486a80fdaf4a4263"

inherit systemd
inherit features_check

REQUIRED_DISTRO_FEATURES = "systemd"
RDEPENDS_${PN} += "systemd bash gpio-funcs buttonscripts"
SYSTEMD_SERVICE_${PN} = "allow_host_boot.service"

SRC_URI = " \
            file://LICENSE \
            file://allow_host_boot \
            file://allow_host_boot.service \
            file://auto_power_on \
          "

S = "${WORKDIR}"

do_install() {
  install -d ${D}/libexec
  install -d ${D}${systemd_system_unitdir}
  install -d ${D}/etc
  install -m 755 ${S}/allow_host_boot ${D}/libexec
  install -m 644 ${S}/allow_host_boot.service ${D}${systemd_system_unitdir}
  install -m 644 ${S}/auto_power_on ${D}/etc
}

FILES_${PN} = "/libexec ${systemd_system_unitdir} /etc"
