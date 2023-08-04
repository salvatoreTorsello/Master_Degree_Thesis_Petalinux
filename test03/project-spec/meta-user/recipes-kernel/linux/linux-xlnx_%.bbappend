FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

SRC_URI:append = " file://bsp.cfg"
KERNEL_FEATURES:append = " bsp.cfg"
SRC_URI += "file://user_2023-07-29-16-45-00.cfg \
            file://user_2023-07-29-18-34-00.cfg \
            file://user_2023-07-29-21-27-00.cfg \
            file://user_2023-08-01-11-09-00.cfg \
            file://user_2023-08-03-13-41-00.cfg \
            "

