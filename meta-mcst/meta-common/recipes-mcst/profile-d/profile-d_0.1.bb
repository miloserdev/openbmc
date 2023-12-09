SUMMARY = "Export some variables for common TTY support"
DESCRIPTION = "Export some variables for common TTY support"
SECTION = "base"
PR = "r1"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://LICENSE;md5=b234ee4d69f5fce4486a80fdaf4a4263"

RDEPENDS_${PN} += "bash"

SRC_URI = "file://exportvars \
           file://LICENSE \
          "

S = "${WORKDIR}"

do_install() {
  install -d ${D}/etc/profile.d
  install -m 644 exportvars ${D}${CONFDIR}/etc/profile.d/00-exportvars.sh
}

FILES_${PN} = "/etc/profile.d"
