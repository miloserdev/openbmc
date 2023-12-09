SUMMARY = "Button scripts for MCST management system"
DESCRIPTION = "Button scripts for MCST management system"
SECTION = "base"
PR = "r1"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://LICENSE;md5=b234ee4d69f5fce4486a80fdaf4a4263"

inherit systemd
inherit features_check

DEPENDS = "pkgconfig-native libgpiod dbus"

REQUIRED_DISTRO_FEATURES = "systemd"
SYSTEMD_SERVICE_${PN} = "host-poweroff-hard.service host-poweroff.service host-poweron.service host-reset.service"

SRC_URI = " \
            file://LICENSE \
            file://Makefile \
            file://server_ctl.c \
            file://host-poweroff-hard.service \
            file://host-poweroff.service \
            file://host-poweron.service \
            file://host-reset.service \
          "

S = "${WORKDIR}"

do_install() {
  install -d ${D}/usr/bin ${D}/libexec ${D}${systemd_system_unitdir}
  for symlink in pwrbut_s pwrbut_h reset uid pwr_on pwr_off pwr_off_hard watchdog_reset
  do
    ln -s /libexec/server_ctl ${D}/usr/bin/server_${symlink}
  done
  install -m 755 ${S}/server_ctl ${D}/libexec
  for service in poweron poweroff poweroff-hard reset
  do
    install -m 644 ${S}/host-${service}.service ${D}${systemd_system_unitdir}
  done
}

FILES_${PN} = "/usr/bin /libexec ${systemd_system_unitdir}"

RDEPENDS_${PN} += "systemd gpio-funcs reimu-conf"
