FILESEXTRAPATHS_prepend := "${THISDIR}/${PN}:"
SRC_URI += " \
    file://reimu-64.cfg \
    file://aspeed-bmc-mcst-video.dtsi \
    file://aspeed-bmc-mcst-flash64.dtsi \
    file://openbmc-flash-layout-64.dtsi \
    file://0001-usb-gadget-aspeed-fix-dma-map-failure.patch \
"
