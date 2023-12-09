SUMMARY = "Get info for CPU, AHB & PCLK frequency"
DESCRIPTION = "Get info for CPU, AHB & PCLK frequency"
SECTION = "base"
PR = "r1"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://LICENSE;md5=b234ee4d69f5fce4486a80fdaf4a4263"

SRC_URI = "file://astclkinfo \
           file://LICENSE \
          "

RDEPENDS_${PN} += "bash"

S = "${WORKDIR}"

do_install() {
  install -d ${D}/usr/bin
  install -m 755 astclkinfo ${D}/usr/bin
}

FILES_${PN} = "/usr/bin"
