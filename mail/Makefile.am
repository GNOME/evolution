bin_PROGRAMS =	evolution-mail

noinst_PROGRAMS = test-mail test-thread

providerdir = $(libdir)/evolution/camel-providers/$(VERSION)

INCLUDES = \
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/widgets/e-text			\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/camel				\
	-I$(top_builddir)/shell				\
	-I$(top_srcdir)/shell				\
	$(BONOBO_HTML_GNOME_CFLAGS)			\
	$(GNOME_VFS_CFLAGS)				\
	-DEVOLUTION_VERSION=\""$(VERSION)"\"		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_ICONSDIR=\""$(iconsdir)"\"		\
	-DEVOLUTION_LOCALEDIR=\""$(datadir)/locale"\"	\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"            \
	-DCAMEL_PROVIDERDIR=\""$(providerdir)"\"	\
	-DG_LOG_DOMAIN=\"evolution-mail\"		\
	$(THREADS_CFLAGS)

EVOLUTION_MAIL_CORBA_GENERATED = \
	Mail.h			\
	Mail-common.c		\
	Mail-skels.c		\
	Mail-stubs.c

# FIXME Is there any way around having to do this?
CAMEL_OBJS_EXTRA =					\
	$(top_builddir)/camel/providers/vee/libcamelvee.la

evolution_mail_SOURCES =		\
	$(EVOLUTION_MAIL_CORBA_GENERATED)	\
	component-factory.c		\
	component-factory.h		\
	folder-browser.c		\
	folder-browser.h		\
	folder-browser-factory.c	\
	mail-config.c			\
	mail-display.c			\
	mail-display.h			\
	mail-format.c			\
	mail-identify.c			\
	mail-ops.c			\
	mail-threads.c			\
	mail-threads.h			\
	mail-types.h			\
	main.c				\
	message-list.c			\
	message-list.h			\
	session.c			\
	mail.h


evolution_mail_LDADD = 					\
	$(top_builddir)/shell/libeshell.a		\
	$(top_builddir)/composer/libcomposer.la		\
	$(top_builddir)/widgets/e-paned/libepaned.a	\
	$(top_builddir)/widgets/misc/libemiscwidgets.a	\
	$(top_builddir)/widgets/e-table/libetable.a	\
	$(top_builddir)/widgets/e-text/libetext.a	\
	$(CAMEL_OBJS_EXTRA)				\
	$(top_builddir)/camel/libcamel.la		\
	$(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/libibex/libibex.la		\
	$(top_builddir)/filter/libfilter.la		\
	$(BONOBO_VFS_GNOME_LIBS)			\
	$(GTKHTML_LIBS)					\
	$(THREADS_LIBS)					\
	$(UNICODE_LIBS)

test_mail_SOURCES = \
	test-mail.c

test_mail_LDADD = \
	$(BONOBO_HTML_GNOME_LIBS)

test_thread_SOURCES =	\
	mail-threads.c	\
	mail-threads.h	\
	test-thread.c

test_thread_LDADD = 			\
	$(BONOBO_HTML_GNOME_LIBS)	\
	$(THREADS_LIBS)

test_thread_CFLAGS = -g $(THREADS_CFLAGS)

GOAD_FILES = evolution-mail.gnorba
OAF_FILES = evolution-mail.oafinfo

if USING_OAF
oafdir = $(datadir)/oaf
oaf_DATA = $(OAF_FILES)
else
gnorbadir = $(sysconfdir)/CORBA/servers
gnorba_DATA = $(GOAD_FILES)
endif

$(EVOLUTION_MAIL_CORBA_GENERATED): Mail.idl 
	$(ORBIT_IDL) -I$(datadir)/idl -I`$(GNOME_CONFIG) --datadir`/idl -I$(srcdir) $(srcdir)/Mail.idl

EXTRA_DIST = Mail.idl $(GOAD_FILES) $(OAF_FILES)

if ENABLE_PURIFY
PLINK = $(LIBTOOL) --mode=link $(PURIFY) $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@

all-local: evolution-mail.pure

evolution-mail.pure: evolution-mail
	@rm -f evolution-mail.pure
	$(PLINK) $(evolution_mail_LDFLAGS) $(evolution_mail_OBJECTS) $(evolution_mail_LDADD) $(LIBS)

endif

