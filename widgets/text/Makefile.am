if OS_WIN32
WIN32_BOOTSTRAP_LIBS = $(top_builddir)/win32/libemiscwidgets.la
endif

INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/widgets			\
	$(EXTRA_GNOME_CFLAGS)			\
	-DG_LOG_DOMAIN=\"e-text\"


privsolib_LTLIBRARIES = libetext.la

libetext_la_SOURCES =				\
	e-entry.c				\
	e-completion-match.c			\
	e-completion.c				\
	e-completion-callbacks.c		\
	e-completion-view.c			\
	e-table-text-model.c			\
	e-text-model-repos.c			\
	e-text-model.c				\
	e-text-model-uri.c                      \
	e-text.c

libetextincludedir = $(privincludedir)/text

libetextinclude_HEADERS = 			\
	e-entry.h				\
	e-completion-match.h			\
	e-completion.h				\
	e-completion-callbacks.h		\
	e-completion-view.h			\
	e-table-text-model.h			\
	e-text-model-repos.h			\
	e-text-model.h				\
	e-text-model-uri.h			\
	e-text.h

libetext_la_LDFLAGS = $(NO_UNDEFINED)

libetext_la_LIBADD = 					\
	$(WIN32_BOOTSTRAP_LIBS)				\
	$(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/a11y/libevolution-a11y.la	\
	$(top_builddir)/widgets/table/libetable.la	\
	$(EXTRA_GNOME_LIBS)				\
	$(REGEX_LIBS)
