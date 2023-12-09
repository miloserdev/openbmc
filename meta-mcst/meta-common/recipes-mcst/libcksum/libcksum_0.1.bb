SUMMARY = "A tiny library implementing POSIX cksum algorithm"
DESCRIPTION = "A tiny library implementing POSIX cksum algorithm"
SECTION = "base"
PR = "r1"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://LICENSE;md5=b234ee4d69f5fce4486a80fdaf4a4263"

SRC_URI = "git://github.com/makise-homura/libcksum.git;protocol=https"
SRCREV = "${AUTOREV}"

S = "${WORKDIR}/git"

prefix = "/usr"
libdir = "${prefix}/lib"
includedir = "${prefix}/include"
mandir = "${prefix}/share/man/man3"

PV_MAJOR = "${@d.getVar('PV',d,1).split('.')[0]}"

do_install() {
  install -d ${D}${libdir}
  install -d ${D}${includedir}
  install -d ${D}${mandir}
  install -m 755 libcksum.so.${PV} ${D}${libdir}
  install -m 644 libcksum.a ${D}${libdir}
  install -m 644 cksum.h ${D}${includedir}
  install -m 644 cksum.3 ${D}${mandir}
  ln -s libcksum.so.${PV} ${D}${libdir}/libcksum.so
  ln -s libcksum.so.${PV} ${D}${libdir}/libcksum.so.${PV_MAJOR}
}

FILES_${PN} = "${prefix}/lib"
