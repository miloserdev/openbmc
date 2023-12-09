do_install_append() {
    for lang_hlp in es hu it pl sr
    do
        rm ${D}/usr/share/mc/help/mc.hlp.${lang_hlp}
    done
}
