noinst_LTLIBRARIES = libeabbookmerging.la

libeabbookmerging_la_CPPFLAGS =				\
	$(AM_CPPFLAGS)					\
	-DG_LOG_DOMAIN=\"eab-contact-merging\"		\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"		\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	-I$(top_srcdir)/addressbook			\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)

libeabbookmerging_la_SOURCES =			\
	eab-contact-compare.c			\
	eab-contact-compare.h			\
	eab-contact-merging.c			\
	eab-contact-merging.h

ui_DATA = \
	eab-contact-duplicate-detected.ui	\
	eab-contact-commit-duplicate-detected.ui

EXTRA_DIST = $(ui_DATA)

-include $(top_srcdir)/git.mk
