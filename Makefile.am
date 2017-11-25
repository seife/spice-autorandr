bin_PROGRAMS = spice-autorandr

AM_CFLAGS = $(CWARNFLAGS) $(AUTORANDR_CFLAGS)
spice_autorandr_LDADD = $(AUTORANDR_LIBS)

spice_autorandr_SOURCES = spice-autorandr.c

autostartdir = $(sysconfdir)/xdg/autostart
autostart_DATA = data/spice-autorandr.desktop

EXTRA_DIST = README.md
EXTRA_DIST += data/spice-autorandr.desktop
