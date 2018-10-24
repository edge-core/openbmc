# No OOB NIC on jbay. Just copy the i2craw
do_install() {
  install -d ${D}${sbindir}
  install -m 755 i2craw ${D}${sbindir}/i2craw
}
