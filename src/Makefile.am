bin_PROGRAMS = gnome-mastermind

gnome_mastermind_LDADD = \
	$(GLIB_LIBS)             \
	$(GTK_LIBS)              \
	$(GTK_DEP_LIBS)		 \
        $(GCONF_LIBS)	

gnome_mastermind_CFLAGS = \
	-I$(top_srcdir)                              \
	-I$(top_srcdir)/include                      \
	$(GLIB_CFLAGS)                               \
	$(GTK_CFLAGS) \
	$(GTK_DEP_CFLAGS) \
	$(GCONF_CFLAGS)             \
	$(GM_DEFINES)			     \
	-DGMLOCALEDIR=\""$(gmlocaledir)"\"
	-D_XOPEN_SOURCE=600

gnome_mastermind_SOURCES = \
	gnome-mastermind.c 
