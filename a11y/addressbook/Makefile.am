privlib_LTLIBRARIES = libevolution-addressbook-a11y.la

INCLUDES =						\
	-DG_LOG_DOMAIN=\"evolution-a11y\"		\
	-I$(top_srcdir)		                        \
	-I$(top_srcdir)/widgets	                        \
	-I$(top_srcdir)/a11y				\
	-I$(top_srcdir)/a11y/addressbook		\
	-I$(top_srcdir)/shell				\
	-I$(top_srcdir)/addressbook			\
	-I$(top_srcdir)/addressbook/gui			\
	-I$(top_srcdir)/addressbook/gui/widgets		\
	-I$(top_srcdir)/addressbook/gui/contact-editor	\
	-I$(top_srcdir)/addressbook/backend             \
	-I$(top_srcdir)/addressbook/backend/ebook       \
	-DEVOLUTION_DATADIR=\""$(datadir)"\"		\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_ETSPECDIR=\""$(etspecdir)"\"	\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"	\
	-DEVOLUTION_GALVIEWSDIR=\""$(viewsdir)"\"	\
	-DEVOLUTION_UIDIR=\""$(evolutionuidir)"\"	\
	-DG_DISABLE_DEPRECATED				\
	-DPREFIX=\""$(prefix)"\"			\
	$(A11Y_CFLAGS)					\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)

libevolution_addressbook_a11y_la_SOURCES =		\
			ea-minicard.c  			\
			ea-minicard.h			\
			ea-minicard-view.c		\
			ea-minicard-view.h		\
			ea-addressbook-view.c		\
			ea-addressbook-view.h		\
			ea-addressbook.c		\
			ea-addressbook.h
