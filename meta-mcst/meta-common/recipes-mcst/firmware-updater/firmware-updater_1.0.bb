# Copyright 2019 MCST.
#
# This program file is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program in a file named COPYING; if not, write to the
# Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor,
# Boston, MA 02110-1301 USA

SUMMARY = "Updater of BMC firmware"
DESCRIPTION = "The tool to update BMC firmware from various sources"
SECTION = "utils"
PR = "r1"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://LICENSE;md5=b234ee4d69f5fce4486a80fdaf4a4263"

RDEPENDS_${PN} += "bash pv coreutils wget gpio-funcs"

S = "${WORKDIR}"

SRC_URI = "file://LICENSE \
           file://libfirmware-updater \
           file://firmware-updater.plain \
           file://firmware-updater \
           file://update-host-flash \
          "

do_install() {
  install -d ${D}/libexec
  install -d ${D}/usr/sbin
  install -m 755 libfirmware-updater ${D}/libexec/
  install -m 755 firmware-updater.plain ${D}/libexec/
  install -m 755 firmware-updater ${D}/usr/sbin/
  install -m 755 update-host-flash ${D}/usr/sbin/
}

FILES_${PN} = "/usr/sbin/ /libexec"
