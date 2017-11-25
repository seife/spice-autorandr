dnl Process this file with autoconf to create configure.

# Initialize Autoconf
AC_PREREQ([2.60])
AC_INIT([spice-autorandr], [0.0.2])
AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_HEADERS([config.h])
AC_USE_SYSTEM_EXTENSIONS

# Initialize Automake
AM_INIT_AUTOMAKE([foreign dist-xz])

XORG_DEFAULT_OPTIONS

# Checks for pkg-config packages
PKG_CHECK_MODULES(AUTORANDR, [xrandr x11 xproto >= 7.0.17])

AC_CONFIG_FILES([
	Makefile
	data/spice-autorandr.spec
	])
AC_OUTPUT
