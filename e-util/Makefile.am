SUBDIRS = . ename

eutilincludedir = $(privincludedir)/e-util
econdincludedir = $(privincludedir)/e-conduit
edb3includedir  = $(privincludedir)/e-db3util

INCLUDES =								\
	-I$(top_srcdir)							\
	-DEVOLUTION_IMAGES=\""$(imagesdir)"\"				\
	-DEVOLUTION_CATEGORY_ICONS=\""$(imagesdir)/categories"\"	\
	-DG_LOG_DOMAIN=\"e-utils\"					\
	-DG_DISABLE_DEPRECATED						\
	-DGTK_DISABLE_DEPRECATED					\
	$(DB3_CFLAGS)							\
	$(GNOME_PILOT_CFLAGS)						\
	$(E_UTIL_CFLAGS)

privlib_LTLIBRARIES = libeutil.la libedb3util.la libeconduit.la
noinst_LTLIBRARIES = libeutil-static.la libeconduit-static.la

eutilinclude_HEADERS = 				\
	e-account-list.h			\
	e-account.h				\
	e-categories-config.h			\
	e-categories-master-list-wombat.h	\
	e-component-listener.h			\
	e-config-listener.h			\
	e-corba-utils.h				\
	e-dialog-utils.h			\
	e-dialog-widgets.h			\
	e-gtk-utils.h				\
	e-gui-utils.h				\
	e-host-utils.h				\
	e-html-utils.h				\
	e-iterator.h				\
	e-lang-utils.h				\
	e-list-iterator.h			\
	e-list.h				\
	e-memory.h				\
	e-meta.h				\
	e-mktemp.h				\
	e-msgport.h				\
	e-passwords.h				\
	e-path.h				\
	e-proxy.h				\
	e-request.h				\
	e-sexp.h				\
	e-time-utils.h				\
	e-trie.h				\
	e-url.h					\
	e-xml-hash-utils.h			\
	md5-utils.h

libeutil_la_SOURCES =				\
	$(eutilinclude_HEADERS)			\
	e-account-list.c			\
	e-account.c				\
	e-categories-config.c			\
	e-categories-master-list-wombat.c	\
	e-component-listener.c			\
	e-config-listener.c			\
	e-corba-utils.c				\
	e-dialog-utils.c			\
	e-dialog-widgets.c			\
	e-gtk-utils.c				\
	e-gui-utils.c				\
	e-host-utils.c				\
	e-html-utils.c				\
	e-iterator.c				\
	e-lang-utils.c				\
	e-list-iterator.c			\
	e-list.c				\
	e-memory.c				\
	e-meta.c				\
	e-mktemp.c				\
	e-msgport.c				\
	e-passwords.c				\
	e-path.c				\
	e-proxy.c				\
	e-request.c				\
	e-sexp.c				\
	e-time-utils.c				\
	e-trie.c				\
	e-url.c					\
	e-util-marshal.c			\
	e-xml-hash-utils.c			\
	eggtrayicon.c				\
	eggtrayicon.h				\
	md5-utils.c

MARSHAL_GENERATED = e-util-marshal.c e-util-marshal.h
@EVO_MARSHAL_RULE@

libeutil_la_LIBADD = $(E_UTIL_LIBS)

libeutil_static_la_SOURCES = $(libeutil_la_SOURCES)
libeutil_static_la_LIBADD  = $(libeutil_la_LIBADD)
libeutil_static_la_LDFLAGS = -all-static
# Make sure libeutil.la and libeutil-static.la are not built in
# parallel when using a parallel make; libtool can't handle that.
libeutil_static_la_DEPENDENCIES = libeutil.la

econdinclude_HEADERS =		\
	e-pilot-map.h		\
	e-pilot-settings.h	\
	e-pilot-util.h

pilot_sources = \
	$(econdinclude_HEADERS)	\
	e-pilot-map.c		\
	e-pilot-settings.c	\
	e-pilot-util.c

if ENABLE_PILOT_CONDUITS
pilot_compile = $(pilot_sources)
else
pilot_compile = 
endif

libeconduit_la_SOURCES = $(pilot_compile)

libeconduit_static_la_SOURCES = $(libeconduit_la_SOURCES)
libeconduit_static_la_LDFLAGS = -all-static

EXTRA_DIST =				\
	$(pilot_sources)		\
	ChangeLog.pre-1-4		\
	e-util-marshal.list

edb3include_HEADERS =			\
	e-db3-utils.h			\
	e-dbhash.h

libedb3util_la_SOURCES =		\
	$(edb3include_HEADERS)		\
	e-db3-utils.c			\
	e-dbhash.c

BUILT_SOURCES = $(MARSHAL_GENERATED)
CLEANFILES    = $(BUILT_SOURCES)
