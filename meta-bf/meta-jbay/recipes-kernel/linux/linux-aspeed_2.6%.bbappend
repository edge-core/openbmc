LINUX_VERSION_EXTENSION = "-jbay"

COMPATIBLE_MACHINE = "jbay"
FILESEXTRAPATHS_prepend := "${THISDIR}/files:"

KERNEL_MODULE_AUTOLOAD += " \
    ltc4151 \
"

SRC_URI += "file://patch-2.6.28.9/0035-Create-Jbay-OpenBMC.patch \
            file://defconfig \
           "
