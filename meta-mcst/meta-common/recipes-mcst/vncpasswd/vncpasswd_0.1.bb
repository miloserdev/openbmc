SUMMARY = "A simple libvncserver-based implementation of vncpasswd"
DESCRIPTION = "A simple libvncserver-based implementation of vncpasswd"
SECTION = "base"
PR = "r1"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://LICENSE;md5=b234ee4d69f5fce4486a80fdaf4a4263"

DEPENDS = "libvncserver"

SRC_URI = " \
            file://LICENSE \
            file://Makefile \
            file://vncpasswd.c \
          "

# Default password is "OpenBmc"
SRC_URI += " file://passwd "

S = "${WORKDIR}"

do_install() {
  install -d ${D}/home/root/.vnc
  install -d ${D}/usr/bin
  install -m 755 vncpasswd ${D}/usr/bin/vncpasswd
  install -m 600 passwd ${D}/home/root/.vnc/passwd
}

FILES_${PN} = "/usr/bin /home/root/.vnc"
