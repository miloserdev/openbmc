FILESEXTRAPATHS_prepend := "${THISDIR}/files:"
SRC_URI += "file://rtc.conf"

do_install_append() {
    install -d ${D}/etc/reimu.d
    install -m 644 rtc.conf ${D}/etc/reimu.d/rtc.conf
}
