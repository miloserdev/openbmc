FILESEXTRAPATHS_prepend := "${THISDIR}:"
SRC_URI += " \
             file://0001-remove-unused-things.patch \
             file://0002-power-transitions.patch \
             file://0003-add-Domains-support.patch \
             file://0004-fix-DHCPEnabled.patch \
             file://0005-correct-IP-update-mechanism.patch \
             file://0006-alert-on-websocket-failure.patch \
             file://0007-omit-power-parameters.patch \
             file://0008-set-nameservers-correctly.patch \
           "
