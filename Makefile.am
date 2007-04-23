SUBDIRS = src themes ui po desktop icons help

EXTRA_DIST = config.rpath 	\
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
	    intltool-update.in \
	    gnome-doc-utils.make 
	
DISTCLEANFILES = \
	         intltool-extract \
	         intltool-merge \
	         intltool-update \
		 gnome-doc-utils.make 

DISTCHECK_CONFIGURE_FLAGS = --disable-scrollkeeper

pkgdata_DATA =  AUTHORS    \
	   	BUGS       \
	COPYING    \
	copyright  \
	copyright-artwork \
	NEWS       \
	README     \
	TODO       

ACLOCAL_AMFLAGS = -I m4
