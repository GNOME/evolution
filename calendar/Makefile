# Generated automatically from Makefile.in by configure.
# Makefile.in generated automatically by automake 1.2c from Makefile.am

# Copyright (C) 1994, 1995, 1996, 1997 Free Software Foundation, Inc.
# This Makefile.in is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.


SHELL = /bin/sh

srcdir = .
top_srcdir = ..
prefix = /da/share/gnome-install
exec_prefix = ${prefix}

bindir = ${exec_prefix}/bin
sbindir = ${exec_prefix}/sbin
libexecdir = ${exec_prefix}/libexec
datadir = ${prefix}/share
sysconfdir = ${prefix}/etc
sharedstatedir = ${prefix}/com
localstatedir = ${prefix}/var
libdir = ${exec_prefix}/lib
infodir = ${prefix}/info
mandir = ${prefix}/man
includedir = ${prefix}/include
oldincludedir = /usr/include

pkgdatadir = $(datadir)/gnome-utils
pkglibdir = $(libdir)/gnome-utils
pkgincludedir = $(includedir)/gnome-utils

top_builddir = ..

ACLOCAL = aclocal -I macros
AUTOCONF = autoconf
AUTOMAKE = automake
AUTOHEADER = autoheader

INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL}
INSTALL_DATA = ${INSTALL} -m 644
INSTALL_SCRIPT = ${INSTALL_PROGRAM}
transform = s,x,x,

NORMAL_INSTALL = true
PRE_INSTALL = true
POST_INSTALL = true
NORMAL_UNINSTALL = true
PRE_UNINSTALL = true
POST_UNINSTALL = true
host_alias = alphaev5-unknown-linux-gnu
host_triplet = alphaev5-unknown-linux-gnu
CATALOGS = 
CATOBJEXT = .gmo
CC = gcc
CFLAGS = -g -O2
CPP = gcc -E
CPPFLAGS = 
CXX = c++
DATADIRNAME = share
GENCAT = 
GMOFILES = 
GMSGFMT = /usr/bin/msgfmt
GNOMEUI_LIBS = -lgnomeui -lgtk -lgdk -lglib -lm  -L/usr/X11R6/lib  -lSM -lICE -lX11  -lXext -lSM -lICE -lgnome  
GNOME_LIBS = -lgnome  
GTKXMHTML_LIBS = -lgtkxmhtml -lgtk -lgdk -lglib -lm -lXpm -ljpeg  -lz  -L/usr/X11R6/lib  -lSM -lICE -lX11  -lXext -lSM -lICE
GT_NO = 
GT_YES = #YES#
GUILE_LIBS = -lguile
INCLUDE_LOCALE_H = #include <locale.h>
INSTOBJEXT = .mo
INTLDEPS = 
INTLLIBS = 
INTLOBJS = 
LD = /usr/bin/ld
LDFLAGS = 
LEX = flex
LIBTOOL = $(SHELL) $(top_builddir)/libtool
LN_S = ln -s
MAINT = #M#
MAKEINFO = makeinfo
MKINSTALLDIRS = ./mkinstalldirs
MSGFMT = /usr/bin/msgfmt
NM = /usr/bin/nm -B
PACKAGE = gnome-utils
POFILES = 
POSUB = po
PROGRAMS_GENIUS = 
PROGRAMS_GTOP = gtop
QTTHREADS_LIB = 
RANLIB = ranlib
READLINE_LIB = 
TERMCAP_LIB = 
USE_INCLUDED_LIBINTL = no
USE_NLS = yes
VERSION = 0.12
YACC = 
l = 

INCLUDES = 			\
	-I$(includedir)		\
	$(X_CFLAGS)

bin_PROGRAMS = gncal

gncal_SOURCES = \
gncal_LDADD = \
	$(GNOMEUI_LIBS)

EXTRA_DIST = \
	gncal.desktop

Productivitydir = $(datadir)/apps/Productivity

Productivity_DATA = gncal.desktop
mkinstalldirs = $(SHELL) $(top_srcdir)/mkinstalldirs
CONFIG_HEADER = ../config.h
CONFIG_CLEAN_FILES = 
PROGRAMS =  $(bin_PROGRAMS)


DEFS = -DHAVE_CONFIG_H -I. -I$(srcdir) -I..
LIBS = 
gncal_OBJECTS = 
gncal_LDADD = $(LDADD)
gncal_DEPENDENCIES = 
gncal_LDFLAGS = 
DATA =  $(Productivity_DATA)

DIST_COMMON =  ChangeLog Makefile.am Makefile.in


DISTFILES = $(DIST_COMMON) $(SOURCES) $(HEADERS) $(TEXINFOS) $(EXTRA_DIST)

TAR = tar
GZIP = --best
SOURCES = $(gncal_SOURCES)
OBJECTS = $(gncal_OBJECTS)

default: all

.SUFFIXES:
.SUFFIXES: .c .lo .o
$(srcdir)/Makefile.in: #M# Makefile.am $(top_srcdir)/configure.in $(ACLOCAL_M4) 
	cd $(top_srcdir) && $(AUTOMAKE) --gnu gncal/Makefile

Makefile: $(srcdir)/Makefile.in $(top_builddir)/config.status $(BUILT_SOURCES)
	cd $(top_builddir) \
	  && CONFIG_FILES=$(subdir)/$@ CONFIG_HEADERS= $(SHELL) ./config.status


mostlyclean-binPROGRAMS:

clean-binPROGRAMS:
	-test -z "$(bin_PROGRAMS)" || rm -f $(bin_PROGRAMS)

distclean-binPROGRAMS:

maintainer-clean-binPROGRAMS:

install-binPROGRAMS: $(bin_PROGRAMS)
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(bindir)
	@list='$(bin_PROGRAMS)'; for p in $$list; do \
	  if test -f $$p; then \
	    echo " $(LIBTOOL)  --mode=install $(INSTALL_PROGRAM) $$p $(bindir)/`echo $$p|sed '$(transform)'`"; \
	    $(LIBTOOL)  --mode=install $(INSTALL_PROGRAM) $$p $(bindir)/`echo $$p|sed '$(transform)'`; \
	  else :; fi; \
	done

uninstall-binPROGRAMS:
	$(NORMAL_UNINSTALL)
	list='$(bin_PROGRAMS)'; for p in $$list; do \
	  rm -f $(bindir)/`echo $$p|sed '$(transform)'`; \
	done

.c.o:
	$(COMPILE) -c $<

mostlyclean-compile:
	-rm -f *.o core

clean-compile:

distclean-compile:
	-rm -f *.tab.c

maintainer-clean-compile:

.c.lo:
	$(LIBTOOL) --mode=compile $(COMPILE) -c $<

mostlyclean-libtool:
	-rm -f *.lo

clean-libtool:
	-rm -rf .libs

distclean-libtool:

maintainer-clean-libtool:

gncal: $(gncal_OBJECTS) $(gncal_DEPENDENCIES)
	@rm -f gncal
	$(LINK) $(gncal_LDFLAGS) $(gncal_OBJECTS) $(gncal_LDADD) $(LIBS)

install-ProductivityDATA: $(Productivity_DATA)
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(Productivitydir)
	@list='$(Productivity_DATA)'; for p in $$list; do \
	  if test -f $(srcdir)/$$p; then \
	    echo " $(INSTALL_DATA) $(srcdir)/$$p $(Productivitydir)/$$p"; \
	    $(INSTALL_DATA) $(srcdir)/$$p $(Productivitydir)/$$p; \
	  else if test -f $$p; then \
	    echo " $(INSTALL_DATA) $$p $(Productivitydir)/$$p"; \
	    $(INSTALL_DATA) $$p $(Productivitydir)/$$p; \
	  fi; fi; \
	done

uninstall-ProductivityDATA:
	$(NORMAL_UNINSTALL)
	list='$(Productivity_DATA)'; for p in $$list; do \
	  rm -f $(Productivitydir)/$$p; \
	done

tags: TAGS

ID: $(HEADERS) $(SOURCES)
	here=`pwd` && cd $(srcdir) && mkid -f$$here/ID $(SOURCES) $(HEADERS)

TAGS:  $(HEADERS) $(SOURCES)  $(TAGS_DEPENDENCIES)
	tags=; \
	here=`pwd`; \
	test -z "$(ETAGS_ARGS)$(SOURCES)$(HEADERS)$$tags" \
	  || (cd $(srcdir) && etags $(ETAGS_ARGS) $$tags  $(SOURCES) $(HEADERS) -o $$here/TAGS)

mostlyclean-tags:

clean-tags:

distclean-tags:
	-rm -f TAGS ID

maintainer-clean-tags:

distdir = $(top_builddir)/$(PACKAGE)-$(VERSION)/$(subdir)

subdir = gncal

distdir: $(DISTFILES)
	here=`cd $(top_builddir) && pwd`; \
	top_distdir=`cd $(top_distdir) && pwd`; \
	distdir=`cd $(distdir) && pwd`; \
	cd $(top_srcdir) \
	  && $(AUTOMAKE) --include-deps --build-dir=$$here --srcdir-name=$(top_srcdir) --output-dir=$$top_distdir --gnu gncal/Makefile
	@for file in $(DISTFILES); do \
	  d=$(srcdir); \
	  test -f $(distdir)/$$file \
	  || ln $$d/$$file $(distdir)/$$file 2> /dev/null \
	  || cp -p $$d/$$file $(distdir)/$$file; \
	done
info:
dvi:
check: all
	$(MAKE)
installcheck:
install-exec: install-binPROGRAMS
	@$(NORMAL_INSTALL)

install-data: install-ProductivityDATA
	@$(NORMAL_INSTALL)

install: install-exec install-data all
	@:

uninstall: uninstall-binPROGRAMS uninstall-ProductivityDATA

all: Makefile $(PROGRAMS) $(DATA)

install-strip:
	$(MAKE) INSTALL_PROGRAM='$(INSTALL_PROGRAM) -s' INSTALL_SCRIPT='$(INSTALL_PROGRAM)' install
installdirs:
	$(mkinstalldirs)  $(bindir) $(Productivitydir)


mostlyclean-generic:
	-test -z "$(MOSTLYCLEANFILES)" || rm -f $(MOSTLYCLEANFILES)

clean-generic:
	-test -z "$(CLEANFILES)" || rm -f $(CLEANFILES)

distclean-generic:
	-rm -f Makefile $(DISTCLEANFILES)
	-rm -f config.cache config.log stamp-h stamp-h[0-9]*
	-test -z "$(CONFIG_CLEAN_FILES)" || rm -f $(CONFIG_CLEAN_FILES)

maintainer-clean-generic:
	-test -z "$(MAINTAINERCLEANFILES)" || rm -f $(MAINTAINERCLEANFILES)
	-test -z "$(BUILT_SOURCES)" || rm -f $(BUILT_SOURCES)
mostlyclean:  mostlyclean-binPROGRAMS mostlyclean-compile \
		mostlyclean-libtool mostlyclean-tags \
		mostlyclean-generic

clean:  clean-binPROGRAMS clean-compile clean-libtool clean-tags \
		clean-generic mostlyclean

distclean:  distclean-binPROGRAMS distclean-compile distclean-libtool \
		distclean-tags distclean-generic clean
	-rm -f config.status
	-rm -f libtool

maintainer-clean:  maintainer-clean-binPROGRAMS maintainer-clean-compile \
		maintainer-clean-libtool maintainer-clean-tags \
		maintainer-clean-generic distclean
	@echo "This command is intended for maintainers to use;"
	@echo "it deletes files that may require special tools to rebuild."

.PHONY: default mostlyclean-binPROGRAMS distclean-binPROGRAMS \
clean-binPROGRAMS maintainer-clean-binPROGRAMS uninstall-binPROGRAMS \
install-binPROGRAMS mostlyclean-compile distclean-compile clean-compile \
maintainer-clean-compile mostlyclean-libtool distclean-libtool \
clean-libtool maintainer-clean-libtool uninstall-ProductivityDATA \
install-ProductivityDATA tags mostlyclean-tags distclean-tags \
clean-tags maintainer-clean-tags distdir info dvi installcheck \
install-exec install-data install uninstall all installdirs \
mostlyclean-generic distclean-generic clean-generic \
maintainer-clean-generic clean mostlyclean distclean maintainer-clean


# Tell versions [3.59,3.63) of GNU make to not export all variables.
# Otherwise a system limit (for SysV at least) may be exceeded.
.NOEXPORT:
