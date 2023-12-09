SUMMARY = "Raw I2C connectivity tool"
DESCRIPTION = "A tool for sending SEND and RECV I2C requests"
SECTION = "base"
PR = "r1"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://LICENSE;md5=b234ee4d69f5fce4486a80fdaf4a4263"

DEPENDS += "i2c-tools"

SRC_URI = "file://rawi2ctool.c \
           file://Makefile \
           file://LICENSE \
          "

S = "${WORKDIR}"

pkgdir = "rawi2ctool"

do_install() {
  bin="${D}/usr/bin"
  install -d $bin
  install -m 755 rawi2ctool ${bin}/rawi2ctool
}

FILES_${PN} = "/usr/bin"

