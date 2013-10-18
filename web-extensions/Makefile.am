webextension_LTLIBRARIES = libevolutionwebextension.la

libevolutionwebextension_la_SOURCES = \
	evolution-web-extension.c \
	evolution-web-extension.h \
	$(top_srcdir)/e-utils/e-dom-utils.c \
	$(top_srcdir)/e-utils/e-dom-utils.h

libevolutionwebextension_la_CPPFLAGS = \
	-I$(top_srcdir)/e-util \
	$(AM_CPPFLAGS)

libevolutionwebextension_la_CFLAGS = \
	$(WEB_EXTENSIONS_CFLAGS)

libevolutionwebextension_la_LIBADD = \
	$(WEB_EXTENSIONS_LIBS)

libevolutionwebextension_la_LDFLAGS = \
	-module -avoid-version -no-undefined
