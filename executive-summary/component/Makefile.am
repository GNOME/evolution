bin_PROGRAMS =	evolution-executive-summary

INCLUDES = \
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/widgets/e-text			\
	-I$(top_srcdir)					\
	-I$(top_builddir)/shell				\
	-I$(top_srcdir)/shell				\
	-I$(top_builddir)/executive-summary/evolution-services	\
	-I$(top_srcdir)/executive-summary/widgets	\
	$(GNOME_VFS_CFLAGS)				\
	$(UNICODE_CFLAGS)				\
	$(GTKHTML_CFLAGS)				\
	-DEVOLUTION_VERSION=\""$(VERSION)"\"		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_ICONSDIR=\""$(iconsdir)"\"		\
	-DEVOLUTION_LOCALEDIR=\""$(datadir)/locale"\"	\
	-DEVOLUTION_DATADIR=\""$(datadir)"\"            \
	-DG_LOG_DOMAIN=\"evolution-executive-summary\"

CLEANFILES = $(IDL_GENERATED)

IDL_GENERATED = \
	Executive-Summary.h		\
	Executive-Summary-common.c	\
	Executive-Summary-skels.c	\
	Executive-Summary-stubs.c

Executive-Summary-impl.o: Executive-Summary.h

$(IDL_GENERATED):
	$(ORBIT_IDL) -I $(datadir)/idl `$(GNOME_CONFIG) --cflags idl` \
		-I$(srcdir) $(srcdir)/../idl/Executive-Summary.idl

evolution_executive_summary_SOURCES =		\
	component-factory.c		\
	component-factory.h		\
	e-summary.c			\
	e-summary.h			\
	e-summary-factory.c		\
	e-summary-factory.h		\
	main.c

evolution_executive_summary_LDADD = 					\
	$(top_builddir)/shell/libeshell.a		\
	$(top_builddir)/widgets/misc/libemiscwidgets.a	\
	$(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/executive-summary/evolution-services/libevolution-services.la	\
	$(top_builddir)/executive-summary/widgets/libesummary-widgets.a	\
	$(BONOBO_VFS_GNOME_LIBS)			\
	$(EXTRA_GNOME_LIBS)				\
	$(GTKHTML_LIBS)					\
	$(UNICODE_LIBS)

if ENABLE_PURIFY
PLINK = $(LIBTOOL) --mode=link $(PURIFY) $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@

all-local: evolution-mail.pure

evolution-mail.pure: evolution-mail
	@rm -f evolution-mail.pure
	$(PLINK) $(evolution_mail_LDFLAGS) $(evolution_mail_OBJECTS) $(evolution_mail_LDADD) $(LIBS)

endif
