
FILESEXTRAPATHS_prepend := "${THISDIR}/files:"
SRC_URI += "file://mavericks32.conf \
            file://mavericks65.conf \
           "

do_install_board_config() {
    install -d ${D}${sysconfdir}/sensors.d
    install -m 644 ../mavericks32.conf ${D}${sysconfdir}/sensors.d/mavericks32.conf
    install -m 644 ../mavericks65.conf ${D}${sysconfdir}/sensors.d/mavericks65.conf
}
