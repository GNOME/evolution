# Generated automatically from Makefile.in by configure.
# Makefile.in generated automatically by automake 1.4 from Makefile.am

# Copyright (C) 1994, 1995-8, 1999 Free Software Foundation, Inc.
# This Makefile.in is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.

#======================================================================
#  FILE: Makefile.am
#  CREATOR: eric 
#  
#  $Id: Makefile,v 1.1.1.3 2001/04/17 17:23:12 jpr Exp $
#
#
# (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of either: 
#
#    The LGPL as published by the Free Software Foundation, version
#    2.1, available at: http://www.fsf.org/copyleft/lesser.html
#
#  Or:
#
#    The Mozilla Public License Version 1.0. You may obtain a copy of
#    the License at http://www.mozilla.org/MPL/
#
#  The original code is icalcomponent.c
#
#======================================================================


SHELL = /bin/sh

srcdir = .
top_srcdir = ../..
prefix = /opt/gnome
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

DESTDIR =

pkgdatadir = $(datadir)/libical
pkglibdir = $(libdir)/libical
pkgincludedir = $(includedir)/libical

top_builddir = ../..

ACLOCAL = aclocal
AUTOCONF = autoconf
AUTOMAKE = automake
AUTOHEADER = autoheader

INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL} $(AM_INSTALL_PROGRAM_FLAGS)
INSTALL_DATA = ${INSTALL} -m 644
INSTALL_SCRIPT = ${INSTALL_PROGRAM}
transform = s,x,x,

NORMAL_INSTALL = :
PRE_INSTALL = :
POST_INSTALL = :
NORMAL_UNINSTALL = :
PRE_UNINSTALL = :
POST_UNINSTALL = :
host_alias = i686-pc-linux-gnu
host_triplet = i686-pc-linux-gnu
AR = ar
AS = @AS@
CC = gcc
DLLTOOL = @DLLTOOL@
LEX = flex
LIBTOOL = $(SHELL) $(top_builddir)/libtool
LN_S = ln -s
MAINT = #
MAKEINFO = makeinfo
OBJDUMP = @OBJDUMP@
PACKAGE = libical
PERL = perl
PY_CFLAGS = -I/usr/include/python1.5
PY_EXTRA_LIBS =        		-lieee -ldl  -lpthread
PY_LIBS = python1.5
PY_LIB_LOC = -L/usr/lib/python1.5/config
RANLIB = ranlib
VERSION = 0.23a
YACC = bison -y
python_val = true

DESIGNDATA = $(top_srcdir)/design-data
ICALSCRIPTS = $(top_srcdir)/scripts

lib_LTLIBRARIES = libical.la

YFLAGS = -d -v -t -pical_yy
LFLAGS = -Pical_yy
LEX_OUTPUT_ROOT = lex.ical_yy

INCLUDES =  	-I$(top_builddir)		-I$(top_srcdir)/src		-I$(top_builddir)/src		-I$(srcdir)


libical_la_LDFLAGS = -version-info 0:0:0

libical_la_SOURCES =  	icalattendee.h			icalattendee.c			icalcomponent.c			icalcomponent.h			icalenums.c			icalenums.h			icalerror.c			icalerror.h			icallexer.l			icalmemory.c			icalmemory.h			icalmime.c			icalmime.h			icalparameter.c			icalparameter.h			icalparameterimpl.h		icalderivedparameter.c		icalderivedparameter.h		icalparser.c			icalparser.h			icalderivedproperty.c		icalderivedproperty.h		icalproperty.c			icalproperty.h			icalrecur.c			icalrecur.h			icalrestriction.c		icalrestriction.h		icaltime.c			icaltime.h			icalduration.h			icalduration.c			icalperiod.h			icalperiod.c			icaltypes.c			icaltypes.h			icalvalue.c			icalvalue.h			icalvalueimpl.h			icalderivedvalue.c		icalderivedvalue.h		icalyacc.h			icalyacc.y			pvl.c				pvl.h				sspm.c				sspm.h				vsnprintf.c			icallangbind.h			icallangbind.c


libicalincludedir = $(includedir)

libicalinclude_HEADERS = ical.h 

# ORDERING OF HEADERS IS SIGNIFICANT. Don't change this ordering. It
# is required to make the combined header ical.h properly
COMBINEDHEADERS =  	$(top_builddir)/src/libical/icalversion.h		$(top_srcdir)/src/libical/icaltime.h			$(top_srcdir)/src/libical/icalduration.h		$(top_srcdir)/src/libical/icalperiod.h			$(top_srcdir)/src/libical/icalenums.h			$(top_srcdir)/src/libical/icaltypes.h			$(top_srcdir)/src/libical/icalrecur.h			$(top_builddir)/src/libical/icalderivedvalue.h		$(top_builddir)/src/libical/icalderivedparameter.h 	$(top_srcdir)/src/libical/icalvalue.h			$(top_srcdir)/src/libical/icalparameter.h		$(top_builddir)/src/libical/icalderivedproperty.h		$(top_srcdir)/src/libical/icalproperty.h		$(top_srcdir)/src/libical/icalattendee.h		$(top_srcdir)/src/libical/pvl.h				$(top_srcdir)/src/libical/icalcomponent.h		$(top_srcdir)/src/libical/icalparser.h			$(top_srcdir)/src/libical/icalmemory.h			$(top_srcdir)/src/libical/icalerror.h			$(top_srcdir)/src/libical/icalrestriction.h		$(top_srcdir)/src/libical/sspm.h			$(top_srcdir)/src/libical/icalmime.h 			$(top_srcdir)/src/libical/icallangbind.h	


BUILT_SOURCES =  	ical.h				icalderivedparameter.c		icalderivedparameter.h		icalderivedproperty.c		icalderivedproperty.h		icalrestriction.c		icalderivedvalue.c		icalderivedvalue.h


# parameters

PARAMETERDEPS =  	$(ICALSCRIPTS)/mkderivedparameters.pl 	$(DESIGNDATA)/parameters.csv		icalderivedparameter.c.in 	icalderivedparameter.h.in


# properties

PROPERTYDEPS =  	$(ICALSCRIPTS)/mkderivedproperties.pl		$(DESIGNDATA)/properties.csv			$(DESIGNDATA)/value-types.csv			icalderivedproperty.c.in			icalderivedproperty.h.in


# restrictions

RESTRICTIONDEPS =  	$(ICALSCRIPTS)/mkrestrictiontable.pl		$(DESIGNDATA)/restrictions.csv			icalrestriction.c.in


# values

VALUEDEPS =  	$(ICALSCRIPTS)/mkderivedvalues.pl  		$(DESIGNDATA)/value-types.csv			icalderivedvalue.c.in					icalderivedvalue.h.in


# housekeeping
CONFIG_CLEAN_FILES = y.output

CLEANFILES =  $(BUILT_SOURCES)

EXTRA_DIST =  	icalderivedparameter.c.in 	icalderivedparameter.h.in 	icalderivedproperty.c.in 	icalderivedproperty.h.in 	icalrestriction.c.in		icalderivedvalue.c.in		icalderivedvalue.h.in		icalversion.h.in		icallexer.c			icalyacc.c

mkinstalldirs = $(SHELL) $(top_srcdir)/mkinstalldirs
CONFIG_HEADER = ../../config.h
LTLIBRARIES =  $(lib_LTLIBRARIES)


DEFS = -DHAVE_CONFIG_H -I. -I$(srcdir) -I../..
CPPFLAGS = 
LDFLAGS = 
LIBS = 
libical_la_LIBADD = 
libical_la_OBJECTS =  icalattendee.lo icalcomponent.lo icalenums.lo \
icalerror.lo icallexer.lo icalmemory.lo icalmime.lo icalparameter.lo \
icalderivedparameter.lo icalparser.lo icalderivedproperty.lo \
icalproperty.lo icalrecur.lo icalrestriction.lo icaltime.lo \
icalduration.lo icalperiod.lo icaltypes.lo icalvalue.lo \
icalderivedvalue.lo icalyacc.lo pvl.lo sspm.lo vsnprintf.lo \
icallangbind.lo
LEXLIB = -lfl
CFLAGS = -g -O2
COMPILE = $(CC) $(DEFS) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS)
LTCOMPILE = $(LIBTOOL) --mode=compile $(CC) $(DEFS) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS)
CCLD = $(CC)
LINK = $(LIBTOOL) --mode=link $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@
HEADERS =  $(libicalinclude_HEADERS)

DIST_COMMON =  Makefile.am Makefile.in icallexer.c icalversion.h.in \
icalyacc.c


DISTFILES = $(DIST_COMMON) $(SOURCES) $(HEADERS) $(TEXINFOS) $(EXTRA_DIST)

TAR = gtar
GZIP_ENV = --best
DEP_FILES =  .deps/icalattendee.P .deps/icalcomponent.P \
.deps/icalderivedparameter.P .deps/icalderivedproperty.P \
.deps/icalderivedvalue.P .deps/icalduration.P .deps/icalenums.P \
.deps/icalerror.P .deps/icallangbind.P .deps/icallexer.P \
.deps/icalmemory.P .deps/icalmime.P .deps/icalparameter.P \
.deps/icalparser.P .deps/icalperiod.P .deps/icalproperty.P \
.deps/icalrecur.P .deps/icalrestriction.P .deps/icaltime.P \
.deps/icaltypes.P .deps/icalvalue.P .deps/icalyacc.P .deps/pvl.P \
.deps/sspm.P .deps/vsnprintf.P
SOURCES = $(libical_la_SOURCES)
OBJECTS = $(libical_la_OBJECTS)

all: all-redirect
.SUFFIXES:
.SUFFIXES: .S .c .l .lo .o .s .y
$(srcdir)/Makefile.in: # Makefile.am $(top_srcdir)/configure.in $(ACLOCAL_M4) 
	cd $(top_srcdir) && $(AUTOMAKE) --gnu src/libical/Makefile

Makefile: $(srcdir)/Makefile.in  $(top_builddir)/config.status $(BUILT_SOURCES)
	cd $(top_builddir) \
	  && CONFIG_FILES=$(subdir)/$@ CONFIG_HEADERS= $(SHELL) ./config.status

icalversion.h: $(top_builddir)/config.status icalversion.h.in
	cd $(top_builddir) && CONFIG_FILES=$(subdir)/$@ CONFIG_HEADERS= $(SHELL) ./config.status

mostlyclean-libLTLIBRARIES:

clean-libLTLIBRARIES:
	-test -z "$(lib_LTLIBRARIES)" || rm -f $(lib_LTLIBRARIES)

distclean-libLTLIBRARIES:

maintainer-clean-libLTLIBRARIES:

install-libLTLIBRARIES: $(lib_LTLIBRARIES)
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(libdir)
	@list='$(lib_LTLIBRARIES)'; for p in $$list; do \
	  if test -f $$p; then \
	    echo "$(LIBTOOL)  --mode=install $(INSTALL) $$p $(DESTDIR)$(libdir)/$$p"; \
	    $(LIBTOOL)  --mode=install $(INSTALL) $$p $(DESTDIR)$(libdir)/$$p; \
	  else :; fi; \
	done

uninstall-libLTLIBRARIES:
	@$(NORMAL_UNINSTALL)
	list='$(lib_LTLIBRARIES)'; for p in $$list; do \
	  $(LIBTOOL)  --mode=uninstall rm -f $(DESTDIR)$(libdir)/$$p; \
	done

.s.o:
	$(COMPILE) -c $<

.S.o:
	$(COMPILE) -c $<

mostlyclean-compile:
	-rm -f *.o core *.core

clean-compile:

distclean-compile:
	-rm -f *.tab.c

maintainer-clean-compile:

.s.lo:
	$(LIBTOOL) --mode=compile $(COMPILE) -c $<

.S.lo:
	$(LIBTOOL) --mode=compile $(COMPILE) -c $<

mostlyclean-libtool:
	-rm -f *.lo

clean-libtool:
	-rm -rf .libs _libs

distclean-libtool:

maintainer-clean-libtool:

libical.la: $(libical_la_OBJECTS) $(libical_la_DEPENDENCIES)
	$(LINK) -rpath $(libdir) $(libical_la_LDFLAGS) $(libical_la_OBJECTS) $(libical_la_LIBADD) $(LIBS)
.l.c:
	$(LEX) $(AM_LFLAGS) $(LFLAGS) $< && mv $(LEX_OUTPUT_ROOT).c $@
.y.c:
	$(YACC) $(AM_YFLAGS) $(YFLAGS) $< && mv y.tab.c $*.c
	if test -f y.tab.h; then \
	if cmp -s y.tab.h $*.h; then rm -f y.tab.h; else mv y.tab.h $*.h; fi; \
	else :; fi
icalyacc.h: icalyacc.c


install-libicalincludeHEADERS: $(libicalinclude_HEADERS)
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(libicalincludedir)
	@list='$(libicalinclude_HEADERS)'; for p in $$list; do \
	  if test -f "$$p"; then d= ; else d="$(srcdir)/"; fi; \
	  echo " $(INSTALL_DATA) $$d$$p $(DESTDIR)$(libicalincludedir)/$$p"; \
	  $(INSTALL_DATA) $$d$$p $(DESTDIR)$(libicalincludedir)/$$p; \
	done

uninstall-libicalincludeHEADERS:
	@$(NORMAL_UNINSTALL)
	list='$(libicalinclude_HEADERS)'; for p in $$list; do \
	  rm -f $(DESTDIR)$(libicalincludedir)/$$p; \
	done

tags: TAGS

ID: $(HEADERS) $(SOURCES) $(LISP)
	list='$(SOURCES) $(HEADERS)'; \
	unique=`for i in $$list; do echo $$i; done | \
	  awk '    { files[$$0] = 1; } \
	       END { for (i in files) print i; }'`; \
	here=`pwd` && cd $(srcdir) \
	  && mkid -f$$here/ID $$unique $(LISP)

TAGS:  $(HEADERS) $(SOURCES)  $(TAGS_DEPENDENCIES) $(LISP)
	tags=; \
	here=`pwd`; \
	list='$(SOURCES) $(HEADERS)'; \
	unique=`for i in $$list; do echo $$i; done | \
	  awk '    { files[$$0] = 1; } \
	       END { for (i in files) print i; }'`; \
	test -z "$(ETAGS_ARGS)$$unique$(LISP)$$tags" \
	  || (cd $(srcdir) && etags $(ETAGS_ARGS) $$tags  $$unique $(LISP) -o $$here/TAGS)

mostlyclean-tags:

clean-tags:

distclean-tags:
	-rm -f TAGS ID

maintainer-clean-tags:

distdir = $(top_builddir)/$(PACKAGE)-$(VERSION)/$(subdir)

subdir = src/libical

distdir: $(DISTFILES)
	here=`cd $(top_builddir) && pwd`; \
	top_distdir=`cd $(top_distdir) && pwd`; \
	distdir=`cd $(distdir) && pwd`; \
	cd $(top_srcdir) \
	  && $(AUTOMAKE) --include-deps --build-dir=$$here --srcdir-name=$(top_srcdir) --output-dir=$$top_distdir --gnu src/libical/Makefile
	@for file in $(DISTFILES); do \
	  d=$(srcdir); \
	  if test -d $$d/$$file; then \
	    cp -pr $$d/$$file $(distdir)/$$file; \
	  else \
	    test -f $(distdir)/$$file \
	    || ln $$d/$$file $(distdir)/$$file 2> /dev/null \
	    || cp -p $$d/$$file $(distdir)/$$file || :; \
	  fi; \
	done
	$(MAKE) $(AM_MAKEFLAGS) top_distdir="$(top_distdir)" distdir="$(distdir)" dist-hook

DEPS_MAGIC := $(shell mkdir .deps > /dev/null 2>&1 || :)

-include $(DEP_FILES)

mostlyclean-depend:

clean-depend:

distclean-depend:
	-rm -rf .deps

maintainer-clean-depend:

%.o: %.c
	@echo '$(COMPILE) -c $<'; \
	$(COMPILE) -Wp,-MD,.deps/$(*F).pp -c $<
	@-cp .deps/$(*F).pp .deps/$(*F).P; \
	tr ' ' '\012' < .deps/$(*F).pp \
	  | sed -e 's/^\\$$//' -e '/^$$/ d' -e '/:$$/ d' -e 's/$$/ :/' \
	    >> .deps/$(*F).P; \
	rm .deps/$(*F).pp

%.lo: %.c
	@echo '$(LTCOMPILE) -c $<'; \
	$(LTCOMPILE) -Wp,-MD,.deps/$(*F).pp -c $<
	@-sed -e 's/^\([^:]*\)\.o[ 	]*:/\1.lo \1.o :/' \
	  < .deps/$(*F).pp > .deps/$(*F).P; \
	tr ' ' '\012' < .deps/$(*F).pp \
	  | sed -e 's/^\\$$//' -e '/^$$/ d' -e '/:$$/ d' -e 's/$$/ :/' \
	    >> .deps/$(*F).P; \
	rm -f .deps/$(*F).pp
info-am:
info: info-am
dvi-am:
dvi: dvi-am
check-am: all-am
check: check-am
installcheck-am:
installcheck: installcheck-am
install-exec-am: install-libLTLIBRARIES
install-exec: install-exec-am

install-data-am: install-libicalincludeHEADERS
install-data: install-data-am

install-am: all-am
	@$(MAKE) $(AM_MAKEFLAGS) install-exec-am install-data-am
install: install-am
uninstall-am: uninstall-libLTLIBRARIES uninstall-libicalincludeHEADERS
uninstall: uninstall-am
all-am: Makefile $(LTLIBRARIES) $(HEADERS)
all-redirect: all-am
install-strip:
	$(MAKE) $(AM_MAKEFLAGS) AM_INSTALL_PROGRAM_FLAGS=-s install
installdirs:
	$(mkinstalldirs)  $(DESTDIR)$(libdir) $(DESTDIR)$(libicalincludedir)


mostlyclean-generic:

clean-generic:
	-test -z "$(CLEANFILES)" || rm -f $(CLEANFILES)

distclean-generic:
	-rm -f Makefile $(CONFIG_CLEAN_FILES)
	-rm -f config.cache config.log stamp-h stamp-h[0-9]*

maintainer-clean-generic:
	-test -z "icallexerlicalyacchicalyaccc$(BUILT_SOURCES)" || rm -f icallexerl icalyacch icalyaccc $(BUILT_SOURCES)
mostlyclean-am:  mostlyclean-libLTLIBRARIES mostlyclean-compile \
		mostlyclean-libtool mostlyclean-tags mostlyclean-depend \
		mostlyclean-generic

mostlyclean: mostlyclean-am

clean-am:  clean-libLTLIBRARIES clean-compile clean-libtool clean-tags \
		clean-depend clean-generic mostlyclean-am

clean: clean-am

distclean-am:  distclean-libLTLIBRARIES distclean-compile \
		distclean-libtool distclean-tags distclean-depend \
		distclean-generic clean-am
	-rm -f libtool

distclean: distclean-am

maintainer-clean-am:  maintainer-clean-libLTLIBRARIES \
		maintainer-clean-compile maintainer-clean-libtool \
		maintainer-clean-tags maintainer-clean-depend \
		maintainer-clean-generic distclean-am
	@echo "This command is intended for maintainers to use;"
	@echo "it deletes files that may require special tools to rebuild."

maintainer-clean: maintainer-clean-am

.PHONY: mostlyclean-libLTLIBRARIES distclean-libLTLIBRARIES \
clean-libLTLIBRARIES maintainer-clean-libLTLIBRARIES \
uninstall-libLTLIBRARIES install-libLTLIBRARIES mostlyclean-compile \
distclean-compile clean-compile maintainer-clean-compile \
mostlyclean-libtool distclean-libtool clean-libtool \
maintainer-clean-libtool uninstall-libicalincludeHEADERS \
install-libicalincludeHEADERS tags mostlyclean-tags distclean-tags \
clean-tags maintainer-clean-tags distdir mostlyclean-depend \
distclean-depend clean-depend maintainer-clean-depend info-am info \
dvi-am dvi check check-am installcheck-am installcheck install-exec-am \
install-exec install-data-am install-data install-am install \
uninstall-am uninstall all-redirect all-am all installdirs \
mostlyclean-generic distclean-generic clean-generic \
maintainer-clean-generic clean mostlyclean distclean maintainer-clean


ical.h: $(BUILT_SOURCES)
	cat $(COMBINEDHEADERS)				\
	| egrep -v "#include.*\"ical"			\
	| egrep -v "#include.*\"pvl\.h\"" > ical.h

icallexer.c : icalyacc.h

icalderivedparameter.h: $(PARAMETERDEPS) 
	$(PERL) -I $(ICALSCRIPTS) $(ICALSCRIPTS)/mkderivedparameters.pl -i $(srcdir)/icalderivedparameter.h.in -h $(DESIGNDATA)/parameters.csv > icalderivedparameter.h 

icalderivedparameter.c: $(PARAMETERDEPS) icalparameter.h
	$(PERL) -I $(ICALSCRIPTS) $(ICALSCRIPTS)/mkderivedparameters.pl -i $(srcdir)/icalderivedparameter.c.in -c $(DESIGNDATA)/parameters.csv > icalderivedparameter.c

icalderivedproperty.h: $(PROPERTYDEPS)
	$(PERL)  -I$(ICALSCRIPTS) $(ICALSCRIPTS)/mkderivedproperties.pl \
		-i $(srcdir)/icalderivedproperty.h.in -h $(DESIGNDATA)/properties.csv\
		${DESIGNDATA}/value-types.csv > icalderivedproperty.h 

icalderivedproperty.c: $(PROPERTYDEPS) icalproperty.h 
	$(PERL)  -I$(ICALSCRIPTS) $(ICALSCRIPTS)/mkderivedproperties.pl \
		-i $(srcdir)/icalderivedproperty.c.in -c $(DESIGNDATA)/properties.csv \
		${DESIGNDATA}/value-types.csv > icalderivedproperty.c

icalrestriction.c: $(RESTRICTIONDEPS)
	$(PERL) $(ICALSCRIPTS)/mkrestrictiontable.pl  -i $(srcdir)/icalrestriction.c.in \
		$(DESIGNDATA)/restrictions.csv > icalrestriction.c

icalderivedvalue.h: $(VALUEDEPS)
	$(PERL) -I$(ICALSCRIPTS)  $(ICALSCRIPTS)/mkderivedvalues.pl \
	         -i $(srcdir)/icalderivedvalue.h.in -h $(DESIGNDATA)/value-types.csv > icalderivedvalue.h

icalderivedvalue.c: $(VALUEDEPS) icalderivedvalue.h
	$(PERL)  -I$(ICALSCRIPTS) $(ICALSCRIPTS)/mkderivedvalues.pl \
		-i $(srcdir)/icalderivedvalue.c.in -c $(DESIGNDATA)/value-types.csv > icalderivedvalue.c

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

# Tell versions [3.59,3.63) of GNU make to not export all variables.
# Otherwise a system limit (for SysV at least) may be exceeded.
.NOEXPORT:
