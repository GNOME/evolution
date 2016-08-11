privsolib_LTLIBRARIES = libevolution-mail-formatter.la

evolution_mail_formatter_includedir = $(privincludedir)/em-format

ENUM_TYPES = e-mail-formatter-enums.h

e-mail-formatter-enumtypes.h: $(top_srcdir)/enumtypes.h.template $(ENUM_TYPES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(top_srcdir)/enumtypes.h.template \
		--fhead "#ifndef E_MAIL_FORMATTER_ENUMTYPES_H\n#define E_MAIL_FORMATTER_ENUMTYPES_H\n" \
		--ftail "#endif /* E_MAIL_FORMATTER_ENUMTYPES_H */\n" \
		$(addprefix $(srcdir)/,$(ENUM_TYPES)) > $@

e-mail-formatter-enumtypes.c: $(top_srcdir)/enumtypes.c.template $(ENUM_TYPES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(top_srcdir)/enumtypes.c.template \
		--fhead "#include \"e-mail-formatter-enumtypes.h\"" \
		$(addprefix $(srcdir)/,$(ENUM_TYPES)) > $@

ENUM_GENERATED = \
	e-mail-formatter-enumtypes.c \
	e-mail-formatter-enumtypes.h \
	$(NULL)

evolution_mail_formatter_include_HEADERS =		\
	e-mail-extension-registry.h			\
	e-mail-formatter-extension.h			\
	e-mail-formatter.h				\
	e-mail-formatter-enums.h			\
	e-mail-formatter-enumtypes.h			\
	e-mail-formatter-print.h			\
	e-mail-formatter-quote.h			\
	e-mail-formatter-utils.h			\
	e-mail-inline-filter.h				\
	e-mail-parser-extension.h			\
	e-mail-parser.h					\
	e-mail-part.h					\
	e-mail-part-attachment.h			\
	e-mail-part-audio.h				\
	e-mail-part-headers.h				\
	e-mail-part-image.h				\
	e-mail-part-list.h				\
	e-mail-part-secure-button.h			\
	e-mail-part-utils.h				\
	e-mail-stripsig-filter.h

libevolution_mail_formatter_la_CPPFLAGS =		\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/em-format			\
	-I$(top_srcdir)/smime/lib			\
	-I$(top_srcdir)/smime/gui			\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"	\
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\"	\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(LIBSOUP_CFLAGS)				\
	$(CODE_COVERAGE_CFLAGS)				\
	$(NULL)

if ENABLE_SMIME
SMIME_EXTENSIONS = e-mail-parser-application-smime.c
endif

libevolution_mail_formatter_la_SOURCES =		\
	$(evolution_mail_formatter_include_HEADERS)	\
	e-mail-extension-registry.c			\
	e-mail-inline-filter.c				\
	e-mail-formatter.c				\
	e-mail-formatter-print.c			\
	e-mail-formatter-quote.c			\
	e-mail-formatter-utils.c			\
	e-mail-formatter-attachment.c			\
	e-mail-formatter-audio.c			\
	e-mail-formatter-enumtypes.c			\
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
	e-mail-formatter-quote-headers.c		\
	e-mail-formatter-quote-message-rfc822.c		\
	e-mail-formatter-quote-text-enriched.c		\
	e-mail-formatter-quote-text-html.c		\
	e-mail-formatter-quote-text-plain.c		\
	e-mail-parser-extension.c			\
	e-mail-parser.c					\
	e-mail-parser-application-mbox.c		\
	e-mail-parser-audio.c				\
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
	e-mail-part-audio.c				\
	e-mail-part-headers.c				\
	e-mail-part-image.c				\
	e-mail-part-list.c				\
	e-mail-part-secure-button.c			\
	e-mail-part-utils.c				\
	e-mail-stripsig-filter.c			\
	$(SMIME_EXTENSIONS)

libevolution_mail_formatter_la_LDFLAGS = -avoid-version $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

if ENABLE_SMIME
SMIME_LIBS =						\
	$(top_builddir)/smime/lib/libessmime.la		\
	$(top_builddir)/smime/gui/libevolution-smime.la
endif

libevolution_mail_formatter_la_LIBADD =			\
	$(top_builddir)/e-util/libevolution-util.la	\
	$(top_builddir)/shell/libevolution-shell.la	\
	$(top_builddir)/libemail-engine/libemail-engine.la \
	$(EVOLUTION_DATA_SERVER_LIBS)			\
	$(GNOME_PLATFORM_LIBS)				\
	$(LIBSOUP_LIBS)					\
	$(SMIME_LIBS)					\
	$(NULL)

BUILT_SOURCES = \
	$(ENUM_GENERATED) \
	$(NULL)

CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
