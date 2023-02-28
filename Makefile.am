SUBDIRS = ui src themes po desktop icons help

EXTRA_DIST = \
	    AUTHORS	\
	    BUGS	\
	    COPYING	\
	    copyright   \
	    copyright-artwork \
	    INSTALL	\
	    NEWS	\
	    README	\
	    TODO	\
	    autogen.sh  \
	    configure.ac\
	    intltool-extract.in\
	    intltool-merge.in\
	    intltool-update.in

DISTCLEANFILES = \
	         intltool-extract \
	         intltool-merge \
	         intltool-update \
		 po/.intltool-merge-cache.lock

doc_DATA = TODO

ACLOCAL_AMFLAGS = -I m4

gsettings_SCHEMAS = org.fargiolas.gnome-mastermind.gschema.xml
EXTRA_DIST += $(gsettings_SCHEMAS)
@GSETTINGS_RULES@

