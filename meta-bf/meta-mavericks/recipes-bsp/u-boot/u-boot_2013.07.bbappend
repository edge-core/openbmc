FILESEXTRAPATHS_prepend := "${THISDIR}/files:"

SRC_URI += "file://patch-2017.02/0001-u-boot-openbmc-mavericks_base.patch \
            file://patch-2017.02/0002-modify-2440lable-1250.patch \
            file://patch-2017.02/0003-modify-console-loglevel.patch \
          "