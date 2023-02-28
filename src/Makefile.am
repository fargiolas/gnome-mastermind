bin_PROGRAMS = gnome-mastermind

gnome_mastermind_LDADD = \
	$(GTK_LIBS)

gnome_mastermind_CFLAGS = \
	-I$(top_srcdir)                              \
	-I$(top_srcdir)/include                      \
	$(GTK_CFLAGS) \
	$(GM_DEFINES)			     \
	-DGMLOCALEDIR=\""$(gmlocaledir)"\"
	-D_XOPEN_SOURCE=600

gnome_mastermind_SOURCES = \
	gnome-mastermind.h \
	gnome-mastermind.c

nodist_gnome_mastermind_SOURCES = $(top_srcdir)/ui/resources.c
