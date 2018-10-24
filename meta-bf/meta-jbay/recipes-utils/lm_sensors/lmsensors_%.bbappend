
FILESEXTRAPATHS_prepend := "${THISDIR}/files:"

SRC_URI += "file://jbay.conf \
           "

do_install_board_config() {
    install -d ${D}${sysconfdir}/sensors.d
    install -m 644 ../jbay.conf ${D}${sysconfdir}/sensors.d/jbay.conf
}
