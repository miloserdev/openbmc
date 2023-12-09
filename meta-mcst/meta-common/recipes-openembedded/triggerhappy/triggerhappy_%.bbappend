FILESEXTRAPATHS_prepend := "${THISDIR}/files:"

SRC_URI += " \
             file://0001-set-root-user.patch \
             file://uid.conf \
             "

do_install_append() {
  install -m 644 ${WORKDIR}/uid.conf ${D}/etc/triggerhappy/triggers.d/00-uid.conf
}

FILES_${PN} += "/etc/triggerhappy/triggers.d"
