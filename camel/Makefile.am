## Process this file with automake to produce Makefile.in

SUBDIRS = . providers tests

%-$(BASE_VERSION).pc: %.pc
	 cp $< $@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = camel-$(BASE_VERSION).pc

libcamelincludedir = $(privincludedir)/camel
libcamel_providerincludedir = $(privincludedir)/camel

camellibexecdir = $(libexecdir)/camel/$(BASE_VERSION)
camellibexec_PROGRAMS =	camel-lock-helper camel-index-control

lib_LTLIBRARIES = libcamel-1.2.la libcamel-provider-1.2.la

INCLUDES = -I.. -I$(srcdir)/.. 			 	\
	-I$(top_srcdir)/intl			 	\
	-DCAMEL_LIBEXECDIR=\""$(camellibexecdir)"\"	\
	-DCAMEL_PROVIDERDIR=\""$(camel_providerdir)"\" 	\
	-DG_LOG_DOMAIN=\"camel\"			\
	$(CAMEL_CFLAGS)

libcamel_provider_1_2_la_SOURCES = 			\
	camel-cipher-context.c			\
	camel-digest-folder.c			\
	camel-digest-store.c			\
	camel-digest-summary.c			\
	camel-disco-diary.c			\
	camel-disco-folder.c			\
	camel-disco-store.c			\
	camel-filter-driver.c			\
	camel-filter-search.c			\
	camel-folder-search.c			\
	camel-folder-summary.c			\
	camel-folder-thread.c			\
	camel-folder.c				\
	camel-gpg-context.c			\
	camel-http-stream.c			\
	camel-movemail.c			\
	camel-provider.c			\
	camel-sasl-anonymous.c			\
	camel-sasl-cram-md5.c			\
	camel-sasl-digest-md5.c			\
	camel-sasl-gssapi.c			\
	camel-sasl-kerberos4.c			\
	camel-sasl-login.c			\
	camel-sasl-ntlm.c			\
	camel-sasl-plain.c			\
	camel-sasl-popb4smtp.c			\
	camel-sasl.c				\
	camel-search-private.c			\
	camel-service.c				\
	camel-session.c				\
	camel-smime-context.c			\
	camel-store-summary.c			\
	camel-store.c				\
	camel-tcp-stream-openssl.c		\
	camel-tcp-stream-raw.c			\
	camel-tcp-stream-ssl.c			\
	camel-tcp-stream.c			\
	camel-transport.c			\
	camel-uid-cache.c			\
	camel-vee-folder.c			\
	camel-vee-store.c			\
	camel-vee-summary.c			\
	camel-vtrash-folder.c

libcamel_providerinclude_HEADERS =			\
	camel-cipher-context.h			\
	camel-digest-folder.h			\
	camel-digest-store.h			\
	camel-digest-summary.h			\
	camel-disco-diary.h			\
	camel-disco-folder.h			\
	camel-disco-store.h			\
	camel-filter-driver.h			\
	camel-filter-search.h			\
	camel-folder-search.h			\
	camel-folder-summary.h			\
	camel-folder-thread.h			\
	camel-folder.h				\
	camel-gpg-context.h			\
	camel-http-stream.h			\
	camel-movemail.h			\
	camel-provider.h			\
	camel-sasl-anonymous.h			\
	camel-sasl-cram-md5.h			\
	camel-sasl-digest-md5.h			\
	camel-sasl-gssapi.h			\
	camel-sasl-kerberos4.h			\
	camel-sasl-login.h			\
	camel-sasl-ntlm.h			\
	camel-sasl-plain.h			\
	camel-sasl-popb4smtp.h			\
	camel-sasl.h				\
	camel-service.h				\
	camel-session.h				\
	camel-smime-context.h			\
	camel-store-summary.h			\
	camel-store.h				\
	camel-tcp-stream-raw.h			\
	camel-tcp-stream-ssl.h			\
	camel-tcp-stream.h			\
	camel-transport.h			\
	camel-uid-cache.h			\
	camel-vee-folder.h			\
	camel-vee-store.h			\
	camel-vee-summary.h			\
	camel-vtrash-folder.h

libcamel_provider_1_2_la_LDFLAGS = -version-info $(LIBCAMEL_CURRENT):$(LIBCAMEL_REVISION):$(LIBCAMEL_AGE)

libcamel_provider_1_2_la_LIBADD =			\
	$(top_builddir)/libedataserver/libedataserver-$(BASE_VERSION).la 	\
	libcamel-$(BASE_VERSION).la				\
	$(CAMEL_LIBS)

libcamel_1_2_la_SOURCES = 				\
	broken-date-parser.c			\
	camel-address.c				\
	camel-arg.c				\
	camel-block-file.c			\
	camel-certdb.c				\
	camel-charset-map.c			\
	camel-data-cache.c			\
	camel-data-wrapper.c			\
	camel-debug.c				\
	camel-exception.c			\
	camel-file-utils.c			\
	camel-html-parser.c			\
	camel-index.c				\
	camel-internet-address.c		\
	camel-junk-plugin.c			\
	camel-list-utils.c			\
	camel-lock-client.c			\
	camel-lock.c				\
	camel-medium.c				\
	camel-mime-filter-basic.c		\
	camel-mime-filter-bestenc.c		\
	camel-mime-filter-canon.c		\
	camel-mime-filter-charset.c		\
	camel-mime-filter-crlf.c		\
	camel-mime-filter-enriched.c		\
	camel-mime-filter-from.c		\
	camel-mime-filter-gzip.c		\
	camel-mime-filter-html.c		\
	camel-mime-filter-index.c		\
	camel-mime-filter-linewrap.c		\
	camel-mime-filter-save.c		\
	camel-mime-filter-tohtml.c		\
	camel-mime-filter-windows.c		\
	camel-mime-filter-yenc.c		\
	camel-mime-filter.c			\
	camel-mime-message.c			\
	camel-mime-parser.c			\
	camel-mime-part-utils.c			\
	camel-mime-part.c			\
	camel-mime-utils.c			\
	camel-multipart-encrypted.c		\
	camel-multipart-signed.c		\
	camel-multipart.c			\
	camel-net-utils.c			\
	camel-object.c				\
	camel-operation.c			\
	camel-partition-table.c			\
	camel-seekable-stream.c			\
	camel-seekable-substream.c		\
	camel-stream-buffer.c			\
	camel-stream-filter.c			\
	camel-stream-fs.c			\
	camel-stream-mem.c			\
	camel-stream-null.c			\
	camel-stream-process.c			\
	camel-stream.c				\
	camel-string-utils.c			\
	camel-text-index.c			\
	camel-url-scanner.c			\
	camel-url.c				\
	camel-utf8.c				\
	camel.c

libcamelinclude_HEADERS =			\
	camel-address.h				\
	camel-arg.h				\
	camel-block-file.h			\
	camel-certdb.h				\
	camel-charset-map.h			\
	camel-data-cache.h			\
	camel-data-wrapper.h			\
	camel-debug.h				\
	camel-exception-list.def		\
	camel-exception.h			\
	camel-file-utils.h			\
	camel-i18n.h				\
	camel-index.h				\
	camel-internet-address.h		\
	camel-junk-plugin.h			\
	camel-list-utils.h			\
	camel-lock-client.h			\
	camel-lock.h				\
	camel-medium.h				\
	camel-mime-filter-basic.h		\
	camel-mime-filter-bestenc.h		\
	camel-mime-filter-canon.h		\
	camel-mime-filter-charset.h		\
	camel-mime-filter-crlf.h		\
	camel-mime-filter-enriched.h		\
	camel-mime-filter-from.h		\
	camel-mime-filter-gzip.h		\
	camel-mime-filter-html.h		\
	camel-mime-filter-index.h		\
	camel-mime-filter-linewrap.h		\
	camel-mime-filter-save.h		\
	camel-mime-filter-tohtml.h		\
	camel-mime-filter-windows.h		\
	camel-mime-filter-yenc.h		\
	camel-mime-filter.h			\
	camel-mime-message.h			\
	camel-mime-parser.h			\
	camel-mime-part-utils.h			\
	camel-mime-part.h			\
	camel-mime-utils.h			\
	camel-multipart-encrypted.h		\
	camel-multipart-signed.h		\
	camel-multipart.h			\
	camel-net-utils.h			\
	camel-object.h				\
	camel-operation.h			\
	camel-partition-table.h			\
	camel-seekable-stream.h			\
	camel-seekable-substream.h		\
	camel-stream-buffer.h			\
	camel-stream-filter.h			\
	camel-stream-fs.h			\
	camel-stream-mem.h			\
	camel-stream-null.h			\
	camel-stream-process.h			\
	camel-stream.h				\
	camel-string-utils.h			\
	camel-text-index.h			\
	camel-types.h				\
	camel-url-scanner.h			\
	camel-url.h				\
	camel-utf8.h				\
	camel.h

libcamel_1_2_la_LDFLAGS = -version-info $(LIBCAMEL_CURRENT):$(LIBCAMEL_REVISION):$(LIBCAMEL_AGE)

libcamel_1_2_la_LIBADD =				\
	$(top_builddir)/libedataserver/libedataserver-$(BASE_VERSION).la 	\
	$(CAMEL_LIBS)

camel_lock_helper_SOURCES = 			\
	camel-lock-helper.c 			\
	camel-lock-helper.h

# NOTE: This needs to be cleaned up, it shouldn't link with all this crap,
# because it might be setuid.
camel_lock_helper_LDADD =			\
	camel-lock.o				\
	$(libcamel_la_LIBADD)

camel_index_control_SOURCES =			\
	camel-index-control.c

camel_index_control_LDADD =			\
	libcamel-$(BASE_VERSION).la				\
	$(libcamel_la_LIBADD)

install-exec-hook:
	@if test -n "$(CAMEL_LOCK_HELPER_USER)"; then \
	    if test `whoami` = root ; then \
	 	chown $(CAMEL_LOCK_HELPER_USER) $(DESTDIR)$(camellibexecdir)/camel-lock-helper ; \
		chmod u+s $(DESTDIR)$(camellibexecdir)/camel-lock-helper ; \
	    else \
	    	echo '*** WARNING ***' ; \
		    echo "Camel will not be able to open mbox files until you perform the following steps:" ; \
		    echo "    1. Become root" ; \
		    echo "    2. chown $(CAMEL_LOCK_HELPER_USER) $(DESTDIR)$(sbindir)/camel-lock-helper" ; \
		    echo "    3. chmod u+s $(DESTDIR)$(sbindir)/camel-lock-helper" ; \
		    echo '*** WARNING ***' ; \
	    fi \
	else \
	    echo "No user to chown to"; \
	fi
	@if test -n "$(CAMEL_LOCK_HELPER_GROUP)"; then \
	    if test `whoami` = root ; then \
	    	chgrp $(CAMEL_LOCK_HELPER_GROUP) $(DESTDIR)$(camellibexecdir)/camel-lock-helper ; \
		    chmod g+s $(DESTDIR)$(camellibexecdir)/camel-lock-helper ; \
	    else \
	    	echo '*** WARNING ***' ; \
		    echo "Camel will not be able to open mbox files until you perform the following steps:" ; \
		    echo "    1. Become root" ; \
		    echo "    2. chgrp $(CAMEL_LOCK_HELPER_GROUP) $(DESTDIR)$(camellibexecdir)/camel-lock-helper" ; \
		    echo "    3. chmod g+s $(DESTDIR)$(camellibexecdir)/camel-lock-helper" ; \
		    echo '*** WARNING ***' ; \
	    fi \
	else \
	    echo "No user to chown to"; \
	fi

noinst_HEADERS =				\
	broken-date-parser.h			\
	camel-charset-map-private.h		\
	camel-private.h				\
	camel-search-private.h			\
	camel-html-parser.h

EXTRA_DIST =					\
	$(pkgconfig_DATA:.pc=.pc.in) 		\
	ChangeLog.pre-1-4			\
	README
