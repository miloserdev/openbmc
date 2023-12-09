SUMMARY = "Hardware-dependent REIMU configuration"
DESCRIPTION = "Hardware-dependent REIMU configuration"
SECTION = "base"
PR = "r1"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://LICENSE;md5=b234ee4d69f5fce4486a80fdaf4a4263"

SRC_URI = " \
           file://LICENSE \
          "

S = "${WORKDIR}"

do_install() {
  install -d ${D}/etc
  install -m 644 reimu.conf ${D}/etc
}

FILES_${PN} = "/etc"
