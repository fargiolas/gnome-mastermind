RESOURCES = \
	ui.gresource.xml \
	menus.ui \
	ui.ui

all: resources.c
resources.c: $(RESOURCES)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --generate $<

CLEANFILES = resources.c

EXTRA_DIST = $(RESOURCES)
