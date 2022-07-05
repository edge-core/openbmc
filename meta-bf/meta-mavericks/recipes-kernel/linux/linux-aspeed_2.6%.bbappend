LINUX_VERSION_EXTENSION = "-mavericks"

COMPATIBLE_MACHINE = "mavericks"
FILESEXTRAPATHS_prepend := "${THISDIR}/files:"

KERNEL_MODULE_AUTOLOAD += " \
    ltc4151 \
"

SRC_URI += "file://patch-2.6.28.9/0035-Create-Mavericks-OpenBMC.patch \
            file://patch-2.6.28.9/0036-register-i2c-device-for-tmp75-0x4d.patch \
            file://patch-2.6.28.9/add-sensors-support-65x.patch \
            file://patch-2.6.28.9/0037-i2c-device-for-24c64-0x50.patch \
            file://patch-2.6.28.9/0038-32x-switch-eeprom-i2c-bus.patch \
            file://defconfig \
           "
