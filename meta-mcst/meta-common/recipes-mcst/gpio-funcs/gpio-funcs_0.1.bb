SUMMARY = "GPIO number decoder for MUS-A and REIMU"
DESCRIPTION = "GPIO number decoder for MUS-A and REIMU"
SECTION = "base"
PR = "r1"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://LICENSE;md5=b234ee4d69f5fce4486a80fdaf4a4263"

RDEPENDS_${PN} += "bash"

SRC_URI = "file://gpio-funcs \
           file://gpiotab \
           file://LICENSE \
          "

S = "${WORKDIR}"

do_install() {
  install -d ${D}/libexec
  install -d ${D}/etc
  install -m 755 gpio-funcs ${D}/libexec/gpio-funcs
  install -m 644 gpiotab ${D}/etc/gpiotab
}

FILES_${PN} = " /libexec /etc "
