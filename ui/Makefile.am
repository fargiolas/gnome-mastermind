RESOURCES = \
	ui.gresource.xml \
	menus.ui \
	ui.ui

all-local: resources.c
resources.c: $(RESOURCES)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --target=$@ --generate $<

CLEANFILES = resources.c

EXTRA_DIST = $(RESOURCES)
