FILESEXTRAPATHS_prepend := "${THISDIR}:"
SRC_URI += " file://0001-enable-Domains-parameter.patch "

EXTRA_OECONF += " --disable-link-local-autoconfiguration "
