INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/widgets			\
	$(GNOME_INCLUDEDIR)			\
	$(ICONV_CFLAGS)				\
	$(EXTRA_GNOME_CFLAGS)			\
        $(GNOME_FULL_CFLAGS)			\
	-DG_LOG_DOMAIN=\"e-text\"


privlib_LTLIBRARIES = libetext.la

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
