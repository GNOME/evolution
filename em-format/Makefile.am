emformatincludedir = $(privincludedir)/em-format

privsolib_LTLIBRARIES = libemformat.la

emformatinclude_HEADERS =				\
	e-mail-extension-registry.h			\
	e-mail-formatter-extension.h			\
	e-mail-formatter.h				\
	e-mail-formatter-print.h			\
	e-mail-formatter-quote.h			\
	e-mail-formatter-utils.h			\
	e-mail-inline-filter.h				\
	e-mail-parser-extension.h			\
	e-mail-parser.h					\
	e-mail-part.h					\
	e-mail-part-attachment.h			\
	e-mail-part-attachment-bar.h			\
	e-mail-part-list.h				\
	e-mail-part-utils.h				\
	e-mail-stripsig-filter.h

libemformat_la_CPPFLAGS =				\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/em-format			\
	-I$(top_srcdir)/smime/lib			\
	-I$(top_srcdir)/smime/gui			\
	-I$(top_srcdir)/widgets				\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(LIBSOUP_CFLAGS)				\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"	\
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\"

if ENABLE_SMIME
SMIME_EXTENSIONS = e-mail-parser-application-smime.c
endif

libemformat_la_SOURCES =				\
	$(emformatinclude_HEADERS)			\
	e-mail-extension-registry.c			\
	e-mail-inline-filter.c				\
	e-mail-formatter.c				\
	e-mail-formatter-print.c			\
	e-mail-formatter-quote.c			\
	e-mail-formatter-utils.c			\
	e-mail-formatter-attachment.c			\
	e-mail-formatter-attachment-bar.c		\
	e-mail-formatter-error.c			\
	e-mail-formatter-extension.c			\
	e-mail-formatter-headers.c			\
	e-mail-formatter-image.c			\
	e-mail-formatter-message-rfc822.c		\
	e-mail-formatter-secure-button.c		\
	e-mail-formatter-source.c			\
	e-mail-formatter-text-enriched.c		\
	e-mail-formatter-text-html.c			\
	e-mail-formatter-text-plain.c			\
	e-mail-formatter-print-headers.c		\
	e-mail-formatter-quote-attachment.c		\
	e-mail-formatter-quote-headers.c		\
	e-mail-formatter-quote-message-rfc822.c		\
	e-mail-formatter-quote-text-enriched.c		\
	e-mail-formatter-quote-text-html.c		\
	e-mail-formatter-quote-text-plain.c		\
	e-mail-parser-extension.c			\
	e-mail-parser.c					\
	e-mail-parser-application-mbox.c		\
	e-mail-parser-attachment-bar.c			\
	e-mail-parser-headers.c				\
	e-mail-parser-image.c				\
	e-mail-parser-inlinepgp-encrypted.c		\
	e-mail-parser-inlinepgp-signed.c		\
 	e-mail-parser-message.c				\
	e-mail-parser-message-deliverystatus.c		\
	e-mail-parser-message-external.c		\
	e-mail-parser-message-rfc822.c			\
	e-mail-parser-multipart-alternative.c		\
	e-mail-parser-multipart-appledouble.c		\
	e-mail-parser-multipart-digest.c		\
	e-mail-parser-multipart-encrypted.c		\
	e-mail-parser-multipart-mixed.c			\
	e-mail-parser-multipart-related.c		\
	e-mail-parser-multipart-signed.c		\
	e-mail-parser-secure-button.c			\
	e-mail-parser-source.c				\
	e-mail-parser-text-enriched.c			\
	e-mail-parser-text-html.c			\
	e-mail-parser-text-plain.c			\
	e-mail-part.c					\
	e-mail-part-attachment.c			\
	e-mail-part-list.c				\
	e-mail-part-utils.c				\
	e-mail-stripsig-filter.c			\
	$(SMIME_EXTENSIONS)

libemformat_la_LDFLAGS = -avoid-version $(NO_UNDEFINED)


if ENABLE_SMIME
SMIME_LIBS =						\
	$(top_builddir)/smime/lib/libessmime.la		\
	$(top_builddir)/smime/gui/libevolution-smime.la
endif

libemformat_la_LIBADD =					\
	$(top_builddir)/libevolution-utils/libevolution-utils.la \
	$(top_builddir)/e-util/libeutil.la		\
	$(top_builddir)/shell/libeshell.la		\
	$(top_builddir)/libemail-utils/libemail-utils.la \
	$(top_builddir)/libemail-engine/libemail-engine.la \
	$(top_builddir)/widgets/misc/libemiscwidgets.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)			\
	$(GNOME_PLATFORM_LIBS)				\
	$(LIBSOUP_LIBS)					\
	$(SMIME_LIBS)

-include $(top_srcdir)/git.mk
