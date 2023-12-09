FILESEXTRAPATHS_prepend := "${THISDIR}/files:"

RDEPENDS_${PN} += "tmux-logger"

SRC_URI += " file://sshd_config \
             file://sshd.socket \
           "

do_install_append() {
  ln -s sshd ${D}/etc/pam.d/dropbear
}

FILES_${PN} += "/etc/pam.d/dropbear"