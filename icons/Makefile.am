gm16icondir = $(datadir)/icons/hicolor/16x16/apps
gm16icon_DATA = 16x16/gnome-mastermind.png \
	16x16/gnome-mastermind.svg
gm22icondir = $(datadir)/icons/hicolor/22x22/apps
gm22icon_DATA = 22x22/gnome-mastermind.png \
	16x16/gnome-mastermind.svg
gm32icondir = $(datadir)/icons/hicolor/32x32/apps
gm32icon_DATA = 32x32/gnome-mastermind.png \
	32x32/gnome-mastermind.svg
gm48icondir = $(datadir)/icons/hicolor/48x48/apps
gm48icon_DATA = scalable/gnome-mastermind.svg

scalableicondir = $(datadir)/icons/hicolor/scalable/apps
scalableicon_DATA = scalable/gnome-mastermind.svg

pixmapsdir = $(datadir)/pixmaps
pixmaps_DATA = scalable/gnome-mastermind.svg

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

install-data-hook: update-icon-cache
uninstall-hook: update-icon-cache

update-icon-cache:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After install, run this:"; \
		echo "***   $(gtk_update_icon_cache)"; \
	fi

EXTRA_DIST = \
	$(gm16icon_DATA) \
	$(gm22icon_DATA) \
	$(gm32icon_DATA) \
	$(gm48icon_DATA) \
	$(scalableicon_DATA)\
	$(pixmaps_DATA)
