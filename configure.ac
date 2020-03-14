AC_PREREQ([2.10])
AC_INIT([digital-emulator], [1.0.0], [matt@majenko.co.uk])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([subdir-objects no-dependencies])
AC_CONFIG_MACRO_DIR([m4])
LT_INIT
AC_PREFIX_DEFAULT([/usr/local/digital-emulator])

AM_SILENT_RULES([yes])

# Checks for programs.
AC_PROG_CC

AC_CONFIG_FILES([
    Makefile 
    sim/Makefile 
    devices/Makefile
    devices/microcontrollers/Makefile
    devices/microcontrollers/microchip/Makefile
    devices/microcontrollers/microchip/pic10/Makefile
    devices/support/Makefile
    devices/support/clock/Makefile
    devices/support/led/Makefile
])
AC_OUTPUT

