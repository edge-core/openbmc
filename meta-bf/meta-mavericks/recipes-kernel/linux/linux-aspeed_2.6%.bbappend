LINUX_VERSION_EXTENSION = "-mavericks"

COMPATIBLE_MACHINE = "mavericks"
FILESEXTRAPATHS_prepend := "${THISDIR}/files:"

KERNEL_MODULE_AUTOLOAD += " \
    ltc4151 \
"

SRC_URI += "file://patch-2.6.28.9/0035-Create-Mavericks-OpenBMC.patch \
            file://defconfig \
           "
