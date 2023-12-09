FILESEXTRAPATHS_prepend := "${THISDIR}:"
SRC_URI += " file://0004-wait-online-any-interface.patch "

do_install_append() {
    rm ${D}/usr/lib/sysctl.d/50-coredump.conf
}
