all: help

help:
	@echo Creates evolution from sources
	@echo usual call order is:
	@echo "  - dependencies - builds dependencies, including evolution-data-server"
	@echo "  - evolution - builds evolution itself"
	@echo "  - addons - builds 3rd-party connectors, like ews and rss"

env:
	@if test "$(BUILD_ROOT_DEPS)" = "" -o "$(BUILD_ROOT_EVO)" = "" ; then \
		echo "Run 'source setup-env' first" >&2; \
		exit 1; \
	fi
	@if test ! -d src ; then \
		mkdir src; \
	fi
	@if test ! -d downloads ; then \
		mkdir downloads; \
	fi

# use `make V=1` to get verbose build
VERBOSE="$(V)"
DISABLE_GTK_DOC=--disable-gtk-doc --disable-gtk-doc-html --disable-gtk-doc-pdf

# Shortcuts
deps: dependencies
eds: evolution-data-server
evo: evolution
ews: evolution-ews
rss: evolution-rss

dependencies: core-tools zlib fake-gtk-doc gnome-common dbus glib gtk librsvg2 icon-themes gsettings-desktop-schemas shared-mime-info libsoup nss liboauth libgdata libgweather openldap libical iso-codes enchant gtkspell gcr webkitgtk
addons: evolution-ews evolution-rss
evolution-data-server: dependencies evolution-data-server$(EVO_BUILD_SUFFIX)
evolution: evolution$(EVO_BUILD_SUFFIX)
evolution-ews: evolution-ews$(EVO_BUILD_SUFFIX)
evolution-rss: evolution-rss$(EVO_BUILD_SUFFIX)


# !!! TODO !!! FixMe !!! TODO !!!
evolution-data-server-stable: evolution-data-server-master
evolution-stable: evolution-master
evolution-ews-stable: evolution-ews-master
evolution-rss-stable: evolution-rss-master

evolution-data-server-master: env dependencies $(BUILD_ROOT_EVO)/lib/pkgconfig/libedataserver-1.2.pc

$(BUILD_ROOT_EVO)/lib/pkgconfig/libedataserver-1.2.pc:
	@if test ! -d "../../evolution-data-server" ; then \
		cd ../.. && \
		git clone git://git.gnome.org/evolution-data-server; \
	fi
	@if test ! -f "../../evolution-data-server/gtk-doc.make" ; then \
		cp "patches/gtk-doc.make" "../../evolution-data-server/"; \
	fi
	@cd "$(BUILD_ROOT_EVO)/libexec" && export WIN32_SERVICELIBEXECDIR=`pwd -W` && cd - >/dev/null && \
	./build acmmi "$(BUILD_ROOT_EVO)" ../../../evolution-data-server "" "--enable-weather --disable-goa --disable-uoa --disable-examples --without-libdb --with-openldap=$(BUILD_ROOT_DEPS) $(DISABLE_GTK_DOC)" $(VERBOSE)

evolution-master: env evolution-data-server-master $(BUILD_ROOT_EVO)/lib/pkgconfig/evolution-shell-3.0.pc

$(BUILD_ROOT_EVO)/lib/pkgconfig/evolution-shell-3.0.pc:
	@if test ! -d "../../evolution" ; then \
		cd ../.. && \
		git clone git://git.gnome.org/evolution; \
	fi
	@if test ! -f "../../evolution/gtk-doc.make" ; then \
		cp "patches/gtk-doc.make" "../../evolution/"; \
	fi
	@./build acmmi "$(BUILD_ROOT_EVO)" ../../../evolution "" "--enable-plugins=all --enable-weather --disable-canberra --disable-contact-maps --disable-goa --disable-libcryptui --without-bogofilter --without-spamassassin --disable-text-highlight --disable-pst-import --disable-gnome-desktop --disable-autoar --without-clutter --without-help $(DISABLE_GTK_DOC)" $(VERBOSE)

evolution-ews-master: env evolution-master $(BUILD_ROOT_EVO)/lib/libeews-1.2.dll.a

$(BUILD_ROOT_EVO)/lib/libeews-1.2.dll.a:
	@if test ! -d "../../evolution-ews" ; then \
		cd ../.. && \
		git clone git://git.gnome.org/evolution-ews; \
	fi
	@if test ! -f "../../evolution-ews/gtk-doc.make" ; then \
		cp "patches/gtk-doc.make" "../../evolution-ews/"; \
	fi
	@./build acmmi "$(BUILD_ROOT_EVO)" ../../../evolution-ews "" "--with-internal-lzx" $(VERBOSE)

evolution-rss-master: env evolution-master $(BUILD_ROOT_EVO)/share/glib-2.0/schemas/org.gnome.evolution.plugin.evolution-rss.gschema.xml

$(BUILD_ROOT_EVO)/share/glib-2.0/schemas/org.gnome.evolution.plugin.evolution-rss.gschema.xml:
	@if test ! -d "../../evolution-rss" ; then \
		cd ../.. && \
		git clone git://git.gnome.org/evolution-rss; \
	fi
	@if test ! -f "../../evolution-rss/gtk-doc.make" ; then \
		cp "patches/gtk-doc.make" "../../evolution-rss/"; \
	fi
	@./build acmmi "$(BUILD_ROOT_EVO)" ../../../evolution-rss "../evolution/win32/patches/rss.patch" "" $(VERBOSE)

gcr: env gtk dbus $(BUILD_ROOT_DEPS)/lib/pkgconfig/gcr-3.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/gcr-3.pc:
	@./get-src https://download.gnome.org/sources/gcr/3.16 gcr-3.16.0.tar.xz
	@./build acmmi "$(BUILD_ROOT_DEPS)" gcr-3.16.0 "gcr.patch" "$(DISABLE_GTK_DOC)" $(VERBOSE)

shared-mime-info: env $(BUILD_ROOT_DEPS)/share/pkgconfig/shared-mime-info.pc

$(BUILD_ROOT_DEPS)/share/pkgconfig/shared-mime-info.pc:
	@./get-src http://freedesktop.org/~hadess shared-mime-info-1.2.tar.xz
	@./build cmmi "$(BUILD_ROOT_DEPS)" shared-mime-info-1.2 "" "" $(VERBOSE)

glib: env zlib libxml2 libffi fake-gtk-doc $(BUILD_ROOT_DEPS)/lib/pkgconfig/glib-2.0.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/glib-2.0.pc:
	@./get-src https://download.gnome.org/sources/glib/2.44 glib-2.44.1.tar.xz
	@./build cmmi "$(BUILD_ROOT_DEPS)" glib-2.44.1 "" "--disable-static" $(VERBOSE)

glib-networking: env glib gnutls $(BUILD_ROOT_DEPS)/lib/gio/modules/libgiognutls.la

$(BUILD_ROOT_DEPS)/lib/gio/modules/libgiognutls.la:
	@./get-src https://download.gnome.org/sources/glib-networking/2.44 glib-networking-2.44.0.tar.xz
	@./build cmmi "$(BUILD_ROOT_DEPS)" glib-networking-2.44.0 "" "--without-ca-certificates" $(VERBOSE)

libmspack: env $(BUILD_ROOT_DEPS)/lib/pkgconfig/libmspack.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/libmspack.pc:
	@./get-src http://www.cabextract.org.uk/libmspack libmspack-0.4alpha.tar.gz
	@./build cmmi "$(BUILD_ROOT_DEPS)" libmspack-0.4alpha "" "" $(VERBOSE)

libsoup: env glib-networking sqlite $(BUILD_ROOT_DEPS)/lib/pkgconfig/libsoup-2.4.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/libsoup-2.4.pc:
	@./get-src https://download.gnome.org/sources/libsoup/2.50 libsoup-2.50.0.tar.xz
	@./build cmmi "$(BUILD_ROOT_DEPS)" libsoup-2.50.0 "" "--disable-tls-check $(DISABLE_GTK_DOC)" $(VERBOSE)

libical: env tzdata cmake $(BUILD_ROOT_DEPS)/lib/pkgconfig/libical.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/libical.pc:
	@./get-src "https://github.com/libical/libical/archive/v1.0.1/libical-1.0.1.tar.gz" "0" libical-1.0.1.tar.gz || (mv downloads/v1.0.1 downloads/libical-1.0.1.tar.gz && (tar -xf downloads/libical-1.0.1.tar.gz --directory=src/ || echo "Error ignored."))
	@cd src/libical-1.0.1 && \
	cmake -G "MSYS Makefiles" -DCMAKE_PREFIX_PATH=$(BUILD_ROOT_DEPS) -DCMAKE_INSTALL_PREFIX=$(BUILD_ROOT_DEPS) .
	@./build mmi "$(BUILD_ROOT_DEPS)" libical-1.0.1 "libical.patch" "" $(VERBOSE)

libgweather: env libsoup tzdata $(BUILD_ROOT_DEPS)/lib/pkgconfig/gweather-3.0.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/gweather-3.0.pc:
	@./get-src https://download.gnome.org/sources/libgweather/3.10 libgweather-3.10.2.tar.xz
	@./build cmmi "$(BUILD_ROOT_DEPS)" libgweather-3.10.2 "libgweather.patch" "--with-zoneinfo-dir=$(BUILD_ROOT_DEPS)/share/zoneinfo $(DISABLE_GTK_DOC)" $(VERBOSE)

tzdata: env $(BUILD_ROOT_DEPS)/share/zoneinfo/zone.tab

$(BUILD_ROOT_DEPS)/share/zoneinfo/zone.tab:
	@./get-src https://www.iana.org/time-zones/repository/releases tzdata2014b.tar.gz "" src/tzdata2014b
	@./get-src https://www.iana.org/time-zones/repository/releases tzcode2014b.tar.gz "" src/tzcode2014b
	@cp src/tzcode2014b/* src/tzdata2014b/
	@./build mmi "$(BUILD_ROOT_DEPS)" tzdata2014b "tzdata.patch" "TOPDIR=$(BUILD_ROOT_DEPS) CC=gcc" $(VERBOSE)

iso-codes: env $(BUILD_ROOT_DEPS)/share/pkgconfig/iso-codes.pc

$(BUILD_ROOT_DEPS)/share/pkgconfig/iso-codes.pc:
	@./get-src https://pkg-isocodes.alioth.debian.org/downloads iso-codes-3.51.tar.xz
	@./build cmmi "$(BUILD_ROOT_DEPS)" iso-codes-3.51 "" "" $(VERBOSE)

enchant: env $(BUILD_ROOT_DEPS)/lib/pkgconfig/enchant.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/enchant.pc:
	@./get-src http://www.abisource.com/downloads/enchant/1.6.0 enchant-1.6.0.tar.gz
	@export CFLAGS="$(CFLAGS) -DNO_STRICT=1" && ./build cmmi "$(BUILD_ROOT_DEPS)" enchant-1.6.0 "" "" $(VERBOSE)

nss: env moztools $(BUILD_ROOT_DEPS)/lib/pkgconfig/nss.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/nss.pc:
	@./get-src ftp://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_15_4_RTM/src nss-3.15.4-with-nspr-4.10.2.tar.gz "" "" "nss-3.15.4"
	@cd src/nss-3.15.4/ && \
	(patch -p1 -f <../../patches/nss.patch || echo a > /dev/null) && \
	export NSPR_USE_GCC=1 && \
	export NSPR_PREFIX=`pwd`/dist/WIN954.0_gcc_OPT.OBJ && \
	export BUILD_OPT=1 && \
	export NS_USE_GCC=1 && \
	../../build cmmi "$(BUILD_ROOT_DEPS)" nspr "" "--prefix=$(BUILD_ROOT_DEPS) --enable-win32-target=WIN95 --enable-debug=no --with-dist-prefix=`pwd`/dist/WIN954.0_gcc_OPT.OBJ --with-dist-includedir=`pwd`/dist/WIN954.0_gcc_OPT.OBJ/include " "$(VERBOSE)" "nspr" "'-DRELEASE_OBJDIR_NAME=WIN954.0_gcc_OPT.OBJ'" && \
	../../build m "$(BUILD_ROOT_DEPS)" nss "" "all" "$(VERBOSE)" "nss" && \
	cp -rf dist/public/* "$(BUILD_ROOT_DEPS)/include/" && \
	mv $(BUILD_ROOT_DEPS)/lib/nspr4.dll $(BUILD_ROOT_DEPS)/bin/libnspr4.dll && \
	mv $(BUILD_ROOT_DEPS)/lib/plc4.dll $(BUILD_ROOT_DEPS)/bin/libplc4.dll && \
	mv $(BUILD_ROOT_DEPS)/lib/plds4.dll $(BUILD_ROOT_DEPS)/bin/libplds4.dll && \
	mv $(BUILD_ROOT_DEPS)/lib/nspr4.a $(BUILD_ROOT_DEPS)/lib/libnspr4.a && \
	mv $(BUILD_ROOT_DEPS)/lib/plc4.a $(BUILD_ROOT_DEPS)/lib/libplc4.a && \
	mv $(BUILD_ROOT_DEPS)/lib/plds4.a $(BUILD_ROOT_DEPS)/lib/libplds4.a && \
	mv dist/WIN954.0_gcc_OPT.OBJ/lib/*.dll "$(BUILD_ROOT_DEPS)/bin/" && \
	mv dist/WIN954.0_gcc_OPT.OBJ/lib/* "$(BUILD_ROOT_DEPS)/lib/" && \
	echo "prefix=$(BUILD_ROOT_DEPS)" >$(BUILD_ROOT_DEPS)/lib/pkgconfig/nss.pc && \
	cat nss/nss.pc >>$(BUILD_ROOT_DEPS)/lib/pkgconfig/nss.pc

moztools: env $(BUILD_ROOT_DEPS)/bin/nsinstall.exe

$(BUILD_ROOT_DEPS)/bin/nsinstall.exe:
	@./get-src http://ftp.mozilla.org/pub/mozilla.org/mozilla/libraries/win32 moztools-static.zip "" "" "moztools"
	@cp src/moztools/bin/nsinstall.exe $(BUILD_ROOT_DEPS)/bin/

gnutls: env zlib gmp nettle p11-kit $(BUILD_ROOT_DEPS)/lib/pkgconfig/gnutls.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/gnutls.pc:
	@./get-src ftp://ftp.gnutls.org/gcrypt/gnutls/v3.4 gnutls-3.4.2.tar.xz "" "" "gnutls-3.4.2"
	@./build cmmi "$(BUILD_ROOT_DEPS)" gnutls-3.4.2 "@gnutls.patch" "--disable-guile --disable-doc --disable-libdane --disable-crywrap" $(VERBOSE)

gnupg: env gnutls pth libassuan libksba $(BUILD_ROOT_DEPS)/bin/gpg2.exe

$(BUILD_ROOT_DEPS)/bin/gpg2.exe:
	@./get-src ftp://ftp.gnupg.org/gcrypt/gnupg gnupg-2.0.22.tar.bz2
	@./build cmmi "$(BUILD_ROOT_DEPS)" gnupg-2.0.22 "" "" $(VERBOSE)

pth: env $(BUILD_ROOT_DEPS)/lib/libw32pth.dll.a

$(BUILD_ROOT_DEPS)/lib/libw32pth.dll.a:
	@./get-src ftp://ftp.g10code.com/g10code/w32pth w32pth-2.0.5.tar.bz2
	@cd src/w32pth-2.0.5 && \
	mv libw32pth.def libw32pth.def.orig && \
	echo EXPORTS >libw32pth.def && \
	cat libw32pth.def.orig | grep -v EXPORTS >>libw32pth.def
	@./build cmmi "$(BUILD_ROOT_DEPS)" w32pth-2.0.5 "" "" $(VERBOSE)

libassuan: env $(BUILD_ROOT_DEPS)/lib/libassuan.dll.a

$(BUILD_ROOT_DEPS)/lib/libassuan.dll.a:
	@./get-src ftp://ftp.gnupg.org/gcrypt/libassuan libassuan-2.1.1.tar.bz2
	@cd src/libassuan-2.1.1/src && \
	mv libassuan.def libassuan.def.orig && \
	echo EXPORTS >libassuan.def && \
	cat libassuan.def.orig | grep -v EXPORTS >>libassuan.def
	@./build cmmi "$(BUILD_ROOT_DEPS)" libassuan-2.1.1 "" "" $(VERBOSE)

libksba: env $(BUILD_ROOT_DEPS)/lib/libksba.dll.a

$(BUILD_ROOT_DEPS)/lib/libksba.dll.a:
	@./get-src ftp://ftp.gnupg.org/gcrypt/libksba libksba-1.3.0.tar.bz2
	@cd src/libksba-1.3.0/src && \
	mv libksba.def libksba.def.orig && \
	echo EXPORTS >libksba.def.def && \
	cat libksba.def.orig | grep -v EXPORTS >>libksba.def
	@./build cmmi "$(BUILD_ROOT_DEPS)" libksba-1.3.0 "" "" $(VERBOSE)

sqlite: env $(BUILD_ROOT_DEPS)/lib/pkgconfig/sqlite3.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/sqlite3.pc:
	@./get-src http://sqlite.org/2014 sqlite-autoconf-3080300.tar.gz
	@./build cmmi "$(BUILD_ROOT_DEPS)" sqlite-autoconf-3080300 "" "" $(VERBOSE)

nettle: env $(BUILD_ROOT_DEPS)/lib/pkgconfig/nettle.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/nettle.pc:
	@./get-src http://www.lysator.liu.se/~nisse/archive nettle-3.1.tar.gz
	@./build cmmi "$(BUILD_ROOT_DEPS)" nettle-3.1 "nettle.patch" "" $(VERBOSE)

gmp: env $(BUILD_ROOT_DEPS)/lib/libgmp.la

$(BUILD_ROOT_DEPS)/lib/libgmp.la:
	@./get-src https://gmplib.org/download/gmp gmp-4.3.2.tar.bz2
	@./build cmmi "$(BUILD_ROOT_DEPS)" gmp-4.3.2 "" "--disable-assembly --disable-fft --without-readline --enable-shared --disable-static --disable-rpath" $(VERBOSE)

p11-kit: env libtasn1 $(BUILD_ROOT_DEPS)/lib/pkgconfig/p11-kit-1.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/p11-kit-1.pc:
	@./get-src http://cgit.freedesktop.org/p11-glue/p11-kit/snapshot p11-kit-ec9e2450ba.tar.gz
	@./build acmmi "$(BUILD_ROOT_DEPS)" p11-kit-ec9e2450ba "p11-kit.patch" "--disable-rpath --disable-trust-module --without-trust-paths" $(VERBOSE)

trousers: env openssl $(BUILD_ROOT_DEPS)/lib/pkgconfig/trousers.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/trousers.pc:
	@./get-src "http://downloads.sourceforge.net/project/trousers/trousers/0.3.11/trousers-0.3.11.2.tar.gz?r=http%3A%2F%2Fsourceforge.net%2Fprojects%2Ftrousers%2Ffiles%2Ftrousers%2F0.3.11%2F&ts=1390773464&use_mirror=skylink" "0" trousers-0.3.11.2.tar.gz
	@./build cmmi "$(BUILD_ROOT_DEPS)" trousers-0.3.11.2 "" "" $(VERBOSE)

libunbound: env openssl ldns $(BUILD_ROOT_DEPS)/lib/pkgconfig/libunbound.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/libunbound.pc:
	@./get-src http://unbound.net/downloads unbound-1.4.21.tar.gz
	@./build cmmi "$(BUILD_ROOT_DEPS)" unbound-1.4.21 "" "--with-ssl=$(BUILD_ROOT_DEPS) --with-libexpat=$(BUILD_ROOT_DEPS) --with-libunbound-only --disable-ecdsa" $(VERBOSE)

ldns: env $(BUILD_ROOT_DEPS)/lib/pkgconfig/ldns.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/ldns.pc:
	@./get-src http://www.nlnetlabs.nl/downloads/ldns ldns-1.6.16.tar.gz
	@mkdir src/ldns-1.6.16/lib
	@mkdir src/ldns-1.6.16/lib/.libs
	@./build cmmi "$(BUILD_ROOT_DEPS)" ldns-1.6.16 "" "--with-ssl=$(BUILD_ROOT_DEPS) " $(VERBOSE)

liboauth: env nss $(BUILD_ROOT_DEPS)/lib/pkgconfig/oauth.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/oauth.pc:
	@./get-src "http://downloads.sourceforge.net/project/liboauth/liboauth-1.0.1.tar.gz?r=http%3A%2F%2Fliboauth.sourceforge.net%2F&ts=1394912907&use_mirror=heanet" "0" liboauth-1.0.1.tar.gz
	@./build cmmi "$(BUILD_ROOT_DEPS)" liboauth-1.0.1 "@liboauth.patch" "--enable-nss" $(VERBOSE)
	@rm $(BUILD_ROOT_DEPS)/lib/liboauth.la

curl: env nss openldap $(BUILD_ROOT_DEPS)/lib/pkgconfig/curl.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/curl.pc:
	@./get-src http://curl.haxx.se/download curl-7.35.0.tar.bz2
	@./build cmmi "$(BUILD_ROOT_DEPS)" curl-7.35.0 "" "--with-nss" $(VERBOSE)

heimdal: env nss ncurses openssl openldap $(BUILD_ROOT_DEPS)/lib/pkgconfig/heimdal.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/heimdal.pc:
	@./get-src http://www.h5l.org/dist/src heimdal-1.5.2.tar.gz
	@./build cmmi "$(BUILD_ROOT_DEPS)" heimdal-1.5.2 "" "" $(VERBOSE)

ncurses: env $(BUILD_ROOT_DEPS)/lib/libncurses.a

$(BUILD_ROOT_DEPS)/lib/libncurses.a:
	@./get-src ftp://invisible-island.net/ncurses ncurses-5.9.tgz
	@./build cmmi "$(BUILD_ROOT_DEPS)" ncurses-5.9 "ncurses.patch" "--without-ada --without-manpages --without-progs --without-tests" $(VERBOSE)

openldap: env regex gnutls libgpg-error libgcrypt $(BUILD_ROOT_DEPS)/lib/libldap.la

$(BUILD_ROOT_DEPS)/lib/libldap.la:
	@./get-src ftp://ftp.openldap.org/pub/OpenLDAP/openldap-release openldap-2.4.39.tgz
	@./build cmmi "$(BUILD_ROOT_DEPS)" openldap-2.4.39 "@openldap.patch" "--disable-backends --disable-overlays --disable-slapd --disable-debug --enable-dynamic --without-threads --with-tls=gnutls" $(VERBOSE)

libgpg-error: env gnutls $(BUILD_ROOT_DEPS)/lib/libgpg-error.la

$(BUILD_ROOT_DEPS)/lib/libgpg-error.la:
	@./get-src ftp://ftp.gnutls.org/gcrypt/libgpg-error libgpg-error-1.12.tar.bz2
	@./build cmmi "$(BUILD_ROOT_DEPS)" libgpg-error-1.12 "libgpg-error.patch" "" $(VERBOSE)

libgcrypt: env libgpg-error $(BUILD_ROOT_DEPS)/lib/libgcrypt.la

$(BUILD_ROOT_DEPS)/lib/libgcrypt.la:
	@./get-src ftp://ftp.gnutls.org/gcrypt/libgcrypt libgcrypt-1.6.1.tar.bz2
	@export w32root=$(BUILD_ROOT_DEPS) && \
	./build acmmi "$(BUILD_ROOT_DEPS)" libgcrypt-1.6.1 "" "--build-w32^" $(VERBOSE)

regex: env $(BUILD_ROOT_DEPS)/include/regex.h

$(BUILD_ROOT_DEPS)/include/regex.h:
	@./get-src "http://downloads.sourceforge.net/project/mingw/MSYS/Base/regex/regex-1.20090805-2/regex-1.20090805-2-msys-1.0.13-src.tar.lzma?r=http%3A%2F%2Fsourceforge.net%2Fprojects%2Fmingw%2Ffiles%2FMSYS%2FBase%2Fregex%2Fregex-1.20090805-2%2Fregex-1.20090805-2-msys-1.0.13-src.tar.lzma%2Fdownload&ts=1395255273&use_mirror=cznic" "0" regex-1.20090805-2-msys-1.0.13-src.tar.lzma
	@tar -xf src/regex-20090805.tar.xz --directory=src
	@./build cmmi "$(BUILD_ROOT_DEPS)" regex-20090805 "regex.patch" "" $(VERBOSE)

catgets: env $(BUILD_ROOT_DEPS)/include/langinfo.h

$(BUILD_ROOT_DEPS)/include/langinfo.h:
	@./get-src "http://downloads.sourceforge.net/project/mingw/MinGW/Extension/catgets/mingw-catgets-1.0.1/mingw-catgets-1.0.1-src.tar.gz?r=http%3A%2F%2Fsourceforge.net%2Fprojects%2Fmingw%2Ffiles%2FMinGW%2FExtension%2Fcatgets%2Fmingw-catgets-1.0.1%2F&ts=1396202721&use_mirror=freefr" "0" mingw-catgets-1.0.1-src.tar.gz "mingw-catgets-1.0.1"
	@./build cmmi "$(BUILD_ROOT_DEPS)" mingw-catgets-1.0.1 "" "" $(VERBOSE)

libdb: env $(BUILD_ROOT_DEPS)/lib/libdb.a

$(BUILD_ROOT_DEPS)/lib/libdb.a:
	@./get-src http://download.oracle.com/berkeley-db db-5.1.29.tar.gz
	@./build cmmi "$(BUILD_ROOT_DEPS)" db-5.1.29/build_unix "@libdb.patch" "../dist/configure --enable-smallbuild --disable-cxx --disable-java --disable-replication --enable-mingw" $(VERBOSE)

libgdata: env glib libsoup liboauth json-glib $(BUILD_ROOT_DEPS)/lib/pkgconfig/libgdata.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/libgdata.pc:
	@./get-src https://download.gnome.org/sources/libgdata/0.15 libgdata-0.15.1.tar.xz
	@./build cmmi "$(BUILD_ROOT_DEPS)" libgdata-0.15.1 "libgdata.patch" "--disable-gnome --disable-goa $(DISABLE_GTK_DOC)" $(VERBOSE)

gsettings-desktop-schemas: env gtk $(BUILD_ROOT_DEPS)/share/pkgconfig/gsettings-desktop-schemas.pc

$(BUILD_ROOT_DEPS)/share/pkgconfig/gsettings-desktop-schemas.pc:
	@./get-src https://download.gnome.org/sources/gsettings-desktop-schemas/3.10 gsettings-desktop-schemas-3.10.1.tar.xz
	@./build cmmi "$(BUILD_ROOT_DEPS)" gsettings-desktop-schemas-3.10.1 "" "" $(VERBOSE)

gtkspell: env gtk enchant $(BUILD_ROOT_DEPS)/lib/pkgconfig/gtkspell3-3.0.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/gtkspell3-3.0.pc:
	@./get-src "http://downloads.sourceforge.net/project/gtkspell/3.0.4/gtkspell3-3.0.4.tar.gz?r=http%3A%2F%2Fgtkspell.sourceforge.net%2F&ts=1395867752&use_mirror=skylink" "0" gtkspell3-3.0.4.tar.gz
	@./build cmmi "$(BUILD_ROOT_DEPS)" gtkspell3-3.0.4 "" "$(DISABLE_GTK_DOC)" $(VERBOSE)

gnome-common: env $(BUILD_ROOT_DEPS)/share/aclocal/gnome-common.m4

$(BUILD_ROOT_DEPS)/share/aclocal/gnome-common.m4:
	@./get-src https://download.gnome.org/sources/gnome-common/3.10 gnome-common-3.10.0.tar.xz
	@./build cmmi "$(BUILD_ROOT_DEPS)" gnome-common-3.10.0 "" "" $(VERBOSE)

webkitgtk: env gtk enchant libsoup ruby gperf libwebp icu libsecret libxslt $(BUILD_ROOT_DEPS)/lib/pkgconfig/webkitgtk-3.0.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/webkitgtk-3.0.pc:
	@./get-src http://webkitgtk.org/releases webkitgtk-2.4.9.tar.xz
	@./build cmmi "$(BUILD_ROOT_DEPS)" webkitgtk-2.4.9 "" "--enable-win32-target --enable-spellcheck --enable-jit --disable-geolocation --disable-video --disable-web-audio --disable-webgl --disable-accelerated-compositing --disable-glx --disable-egl --disable-gles2 --disable-webkit2 $(DISABLE_GTK_DOC)" $(VERBOSE)

ruby: env 7zip $(BUILD_ROOT_DEPS)/bin/ruby.exe

$(BUILD_ROOT_DEPS)/bin/ruby.exe:
	@./get-src http://dl.bintray.com/oneclick/rubyinstaller ruby-1.9.3-p545-i386-mingw32.7z
	@cp -r src/ruby-1.9.3-p545-i386-mingw32/bin/* $(BUILD_ROOT_DEPS)/bin/
	@cp -r src/ruby-1.9.3-p545-i386-mingw32/include/* $(BUILD_ROOT_DEPS)/include/
	@cp -r src/ruby-1.9.3-p545-i386-mingw32/lib/* $(BUILD_ROOT_DEPS)/lib/

gperf: env $(BUILD_ROOT_DEPS)/bin/gperf.exe

$(BUILD_ROOT_DEPS)/bin/gperf.exe:
	@./get-src http://ftp.gnu.org/pub/gnu/gperf gperf-3.0.4.tar.gz
	@./build cmmi "$(BUILD_ROOT_DEPS)" gperf-3.0.4 "" "" $(VERBOSE)

libwebp: env giflib $(BUILD_ROOT_DEPS)/lib/pkgconfig/libwebp.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/libwebp.pc:
	@./get-src https://webp.googlecode.com/files libwebp-0.4.0.tar.gz
	@./build cmmi "$(BUILD_ROOT_DEPS)" libwebp-0.4.0 "" "" $(VERBOSE)

icu: env $(BUILD_ROOT_DEPS)/lib/pkgconfig/icu-uc.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/icu-uc.pc:
	@./get-src http://download.icu-project.org/files/icu4c/53.1 icu4c-53_1-src.tgz "" "" icu
	@./build cmmi "$(BUILD_ROOT_DEPS)" icu/source "" "--disable-rpath" $(VERBOSE)
	@mv $(BUILD_ROOT_DEPS)/lib/icu*.dll $(BUILD_ROOT_DEPS)/bin/
	@mv $(BUILD_ROOT_DEPS)/lib/icudt.dll.a $(BUILD_ROOT_DEPS)/lib/libicudt.dll.a
	@mv $(BUILD_ROOT_DEPS)/lib/icuin.dll.a $(BUILD_ROOT_DEPS)/lib/libicuin.dll.a
	@mv $(BUILD_ROOT_DEPS)/lib/icuio.dll.a $(BUILD_ROOT_DEPS)/lib/libicuio.dll.a
	@mv $(BUILD_ROOT_DEPS)/lib/icule.dll.a $(BUILD_ROOT_DEPS)/lib/libicule.dll.a
	@mv $(BUILD_ROOT_DEPS)/lib/iculx.dll.a $(BUILD_ROOT_DEPS)/lib/libiculx.dll.a
	@mv $(BUILD_ROOT_DEPS)/lib/icutest.dll.a $(BUILD_ROOT_DEPS)/lib/libicutest.dll.a
	@mv $(BUILD_ROOT_DEPS)/lib/icutu.dll.a $(BUILD_ROOT_DEPS)/lib/libicutu.dll.a
	@mv $(BUILD_ROOT_DEPS)/lib/icuuc.dll.a $(BUILD_ROOT_DEPS)/lib/libicuuc.dll.a
	@mv $(BUILD_ROOT_DEPS)/lib/pkgconfig/icu-i18n.pc $(BUILD_ROOT_DEPS)/lib/pkgconfig/icu-i18n.pc.bak && \
	cat $(BUILD_ROOT_DEPS)/lib/pkgconfig/icu-i18n.pc.bak | grep -v "Libs: -licuin53" >$(BUILD_ROOT_DEPS)/lib/pkgconfig/icu-i18n.pc && \
	echo "Libs: -licuin" >>$(BUILD_ROOT_DEPS)/lib/pkgconfig/icu-i18n.pc && \
	rm $(BUILD_ROOT_DEPS)/lib/pkgconfig/icu-i18n.pc.bak
	@mv $(BUILD_ROOT_DEPS)/lib/pkgconfig/icu-io.pc $(BUILD_ROOT_DEPS)/lib/pkgconfig/icu-io.pc.bak && \
	cat $(BUILD_ROOT_DEPS)/lib/pkgconfig/icu-io.pc.bak | grep -v "Libs: -licuio53" >$(BUILD_ROOT_DEPS)/lib/pkgconfig/icu-io.pc && \
	echo "Libs: -licuio" >>$(BUILD_ROOT_DEPS)/lib/pkgconfig/icu-io.pc && \
	rm $(BUILD_ROOT_DEPS)/lib/pkgconfig/icu-io.pc.bak
	@mv $(BUILD_ROOT_DEPS)/lib/pkgconfig/icu-le.pc $(BUILD_ROOT_DEPS)/lib/pkgconfig/icu-le.pc.bak && \
	cat $(BUILD_ROOT_DEPS)/lib/pkgconfig/icu-le.pc.bak | grep -v "Libs: -licule53" >$(BUILD_ROOT_DEPS)/lib/pkgconfig/icu-le.pc && \
	echo "Libs: -licule" >>$(BUILD_ROOT_DEPS)/lib/pkgconfig/icu-le.pc && \
	rm $(BUILD_ROOT_DEPS)/lib/pkgconfig/icu-le.pc.bak
	@mv $(BUILD_ROOT_DEPS)/lib/pkgconfig/icu-lx.pc $(BUILD_ROOT_DEPS)/lib/pkgconfig/icu-lx.pc.bak && \
	cat $(BUILD_ROOT_DEPS)/lib/pkgconfig/icu-lx.pc.bak | grep -v "Libs: -liculx53" >$(BUILD_ROOT_DEPS)/lib/pkgconfig/icu-lx.pc && \
	echo "Libs: -liculx" >>$(BUILD_ROOT_DEPS)/lib/pkgconfig/icu-lx.pc && \
	rm $(BUILD_ROOT_DEPS)/lib/pkgconfig/icu-lx.pc.bak
	@mv $(BUILD_ROOT_DEPS)/lib/pkgconfig/icu-uc.pc $(BUILD_ROOT_DEPS)/lib/pkgconfig/icu-uc.pc.bak && \
	cat $(BUILD_ROOT_DEPS)/lib/pkgconfig/icu-uc.pc.bak | grep -v "} -licuuc53 -licudt53" >$(BUILD_ROOT_DEPS)/lib/pkgconfig/icu-uc.pc && \
	echo "Libs: -L$(BUILD_ROOT_DEPS)/lib/ -licuuc -licudt" >>$(BUILD_ROOT_DEPS)/lib/pkgconfig/icu-uc.pc && \
	rm $(BUILD_ROOT_DEPS)/lib/pkgconfig/icu-uc.pc.bak

libsecret: env $(BUILD_ROOT_DEPS)/lib/pkgconfig/libsecret-1.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/libsecret-1.pc:
	@./get-src https://download.gnome.org/sources/libsecret/0.16 libsecret-0.16.tar.xz
	@./build acmmi "$(BUILD_ROOT_DEPS)" libsecret-0.16 "libsecret.patch" "--disable-manpages" $(VERBOSE)

libxslt: env libxml2 $(BUILD_ROOT_DEPS)/lib/pkgconfig/libxslt.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/libxslt.pc:
	@./get-src https://git.gnome.org/browse/libxslt/snapshot libxslt-1.1.28.tar.gz
	@./build acmmi "$(BUILD_ROOT_DEPS)" libxslt-1.1.28 "libxslt.patch" "" $(VERBOSE)

json-glib: env glib $(BUILD_ROOT_DEPS)/lib/pkgconfig/json-glib-1.0.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/json-glib-1.0.pc:
	@./get-src https://download.gnome.org/sources/json-glib/0.99 json-glib-0.99.2.tar.xz
	@mv $(BUILD_ROOT_DEPS)/bin/msgmerge.exe $(BUILD_ROOT_DEPS)/bin/msgmerge.ex && \
	./build cmmi "$(BUILD_ROOT_DEPS)" json-glib-0.99.2 "" "$(DISABLE_GTK_DOC)" $(VERBOSE) && \
	mv $(BUILD_ROOT_DEPS)/bin/msgmerge.ex $(BUILD_ROOT_DEPS)/bin/msgmerge.exe

libunistring: env $(BUILD_ROOT_DEPS)/lib/pkgconfig/libunistring.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/libunistring.pc:
	@./get-src ftp://ftp.gnu.org/pub/gnu/libunistring libunistring-0.9.tar.gz
	@./build cmmi "$(BUILD_ROOT_DEPS)" libunistring-0.9 "" "" $(VERBOSE)

libtasn1: env $(BUILD_ROOT_DEPS)/lib/pkgconfig/libtasn1.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/libtasn1.pc:
	@./get-src http://ftp.gnu.org/gnu/libtasn1 libtasn1-4.5.tar.gz
	@./build cmmi "$(BUILD_ROOT_DEPS)" libtasn1-4.5 "" "" $(VERBOSE)

openssl: env $(BUILD_ROOT_DEPS)/lib/pkgconfig/openssl.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/openssl.pc:
	@./get-src http://www.openssl.org/source openssl-1.0.1g.tar.gz
	@./build cmmi "$(BUILD_ROOT_DEPS)" openssl-1.0.1g "" "mingw" $(VERBOSE)

gtk: env glib intltool atk gdk-pixbuf cairo pango libepoxy $(BUILD_ROOT_DEPS)/lib/pkgconfig/gtk+-3.0.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/gtk+-3.0.pc:
	@./get-src https://download.gnome.org/sources/gtk+/3.16 gtk+-3.16.4.tar.xz
	@./build cmmi "$(BUILD_ROOT_DEPS)" gtk+-3.16.4 "gtk.patch" "--disable-static $(DISABLE_GTK_DOC) --disable-man" $(VERBOSE)

libepoxy: env glib intltool cairo pango $(BUILD_ROOT_DEPS)/lib/pkgconfig/epoxy.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/epoxy.pc:
	@./get-src https://github.com/anholt/libepoxy/archive/v1.2.tar.gz "0" "libepoxy-1.2.tar.gz" || (mv downloads/v1.2 downloads/libepoxy-1.2.tar.gz && tar -xf downloads/libepoxy-1.2.tar.gz --directory=src/)
	@./build acmmi "$(BUILD_ROOT_DEPS)" libepoxy-1.2 "libepoxy.patch" "" $(VERBOSE)

atk: env glib $(BUILD_ROOT_DEPS)/lib/pkgconfig/atk.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/atk.pc:
	@./get-src https://download.gnome.org/sources/atk/2.16 atk-2.16.0.tar.xz
	@./build cmmi "$(BUILD_ROOT_DEPS)" atk-2.16.0 "atk.patch" "--disable-static" $(VERBOSE)

cairo: env glib pixman libpng freetype fontconfig $(BUILD_ROOT_DEPS)/lib/pkgconfig/cairo.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/cairo.pc:
	@./get-src http://cairographics.org/releases cairo-1.14.2.tar.xz
	@./build cmmi "$(BUILD_ROOT_DEPS)" cairo-1.14.2 "" "--disable-static $(DISABLE_GTK_DOC)" $(VERBOSE)

pixman: env $(BUILD_ROOT_DEPS)/lib/pkgconfig/pixman-1.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/pixman-1.pc:
	@./get-src http://cairographics.org/releases pixman-0.32.6.tar.gz
	@./build cmmi "$(BUILD_ROOT_DEPS)" pixman-0.32.6 "" "--disable-static" $(VERBOSE)

libpng: env zlib $(BUILD_ROOT_DEPS)/lib/pkgconfig/libpng.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/libpng.pc:
	@./get-src http://download.sourceforge.net/libpng libpng-1.6.7.tar.xz
	@./build mmi "$(BUILD_ROOT_DEPS)" libpng-1.6.7 "libpng.patch" "-f scripts/makefile.msys prefix=$(BUILD_ROOT_DEPS)" $(VERBOSE)
	@cp $(BUILD_ROOT_DEPS)/lib/libpng16.dll.a $(BUILD_ROOT_DEPS)/lib/libpng.dll.a

fontconfig: env freetype expat $(BUILD_ROOT_DEPS)/lib/pkgconfig/fontconfig.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/fontconfig.pc:
	@./get-src http://www.freedesktop.org/software/fontconfig/release fontconfig-2.11.0.tar.gz
	@./build cmmi "$(BUILD_ROOT_DEPS)" fontconfig-2.11.0 "@fontconfig.patch" "--disable-docs" $(VERBOSE)

freetype: env zlib giflib libpng libjpeg libtiff jasper $(BUILD_ROOT_DEPS)/lib/pkgconfig/freetype2.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/freetype2.pc:
	@./get-src http://download.savannah.gnu.org/releases/freetype freetype-2.5.2.tar.gz
	@./build cmmi "$(BUILD_ROOT_DEPS)" freetype-2.5.2 "freetype.patch" "" $(VERBOSE)

harfbuzz: env cairo freetype glib icu $(BUILD_ROOT_DEPS)/lib/pkgconfig/harfbuzz.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/harfbuzz.pc:
	@./get-src http://www.freedesktop.org/software/harfbuzz/release harfbuzz-0.9.27.tar.bz2 || test -d src/harfbuzz-0.9.27
	@./build cmmi "$(BUILD_ROOT_DEPS)" harfbuzz-0.9.27 "harfbuzz.patch" "--with-icu=yes --disable-static" $(VERBOSE)

librsvg2: env zlib cairo gdk-pixbuf libcroco $(BUILD_ROOT_DEPS)/lib/pkgconfig/librsvg-2.0.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/librsvg-2.0.pc:
	@./get-src http://ftp.acc.umu.se/pub/gnome/sources/librsvg/2.36 librsvg-2.36.4.tar.xz
	@export PATH="$(PATH):`pwd`/src/librsvg-2.36.4/.libs" && \
	./build cmmi "$(BUILD_ROOT_DEPS)" librsvg-2.36.4 "" "--disable-gtk-theme --enable-pixbuf-loader --enable-introspection=no $(DISABLE_GTK_DOC)" $(VERBOSE)

pango: env glib cairo fontconfig freetype harfbuzz $(BUILD_ROOT_DEPS)/lib/pkgconfig/pango.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/pango.pc:
	@./get-src http://ftp.gnome.org/pub/GNOME/sources/pango/1.36 pango-1.36.8.tar.xz
	@./build cmmi "$(BUILD_ROOT_DEPS)" pango-1.36.8 "" "" $(VERBOSE)

icon-themes: hicolor-icon-theme icon-naming-utils adwaita-icon-theme

hicolor-icon-theme: env $(BUILD_ROOT_DEPS)/share/icons/hicolor/index.theme

$(BUILD_ROOT_DEPS)/share/icons/hicolor/index.theme:
	@./get-src http://icon-theme.freedesktop.org/releases hicolor-icon-theme-0.13.tar.gz
	@./build cmmi "$(BUILD_ROOT_DEPS)" hicolor-icon-theme-0.13 "" "" $(VERBOSE)

perl-xml-simple: /lib/perl5/5.8/XML

/lib/perl5/5.8/XML:
	@./get-src http://pkgs.fedoraproject.org/repo/pkgs/perl-XML-Simple/XML-Simple-2.18.tar.gz/593aa8001e5c301cdcdb4bb3b63abc33 XML-Simple-2.18.tar.gz
	@cp -r src/XML-Simple-2.18/lib/XML /lib/perl5/5.8/

icon-naming-utils: env perl-xml-simple $(BUILD_ROOT_DEPS)/share/pkgconfig/icon-naming-utils.pc

$(BUILD_ROOT_DEPS)/share/pkgconfig/icon-naming-utils.pc:
	@./get-src http://tango.freedesktop.org/releases icon-naming-utils-0.8.90.tar.gz
	@./build cmmi "$(BUILD_ROOT_DEPS)" icon-naming-utils-0.8.90 "" "" $(VERBOSE)

adwaita-icon-theme: env icon-naming-utils gtk $(BUILD_ROOT_DEPS)/share/pkgconfig/adwaita-icon-theme.pc

$(BUILD_ROOT_DEPS)/share/pkgconfig/adwaita-icon-theme.pc:
	@./get-src https://download.gnome.org/sources/adwaita-icon-theme/3.16 adwaita-icon-theme-3.16.2.tar.xz
	@./build cmmi "$(BUILD_ROOT_DEPS)" adwaita-icon-theme-3.16.2 "" "" $(VERBOSE)

gdk-pixbuf: env giflib libpng libjpeg libtiff jasper glib $(BUILD_ROOT_DEPS)/lib/pkgconfig/gdk-pixbuf-2.0.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/gdk-pixbuf-2.0.pc:
	@./get-src https://download.gnome.org/sources/gdk-pixbuf/2.30 gdk-pixbuf-2.30.8.tar.xz
	@./build cmmi "$(BUILD_ROOT_DEPS)" gdk-pixbuf-2.30.8 "" "--disable-static --with-libjasper" $(VERBOSE)

libcroco: env glib $(BUILD_ROOT_DEPS)/lib/pkgconfig/libcroco-0.6.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/libcroco-0.6.pc:
	@./get-src http://ftp.acc.umu.se/pub/gnome/sources/libcroco/0.6 libcroco-0.6.8.tar.xz
	@./build cmmi "$(BUILD_ROOT_DEPS)" libcroco-0.6.8 "" "$(DISABLE_GTK_DOC)" $(VERBOSE)

giflib: env $(BUILD_ROOT_DEPS)/include/gif_lib.h

$(BUILD_ROOT_DEPS)/include/gif_lib.h:
	@./get-src "http://downloads.sourceforge.net/project/giflib/giflib-5.0.6.tar.bz2?r=http%3A%2F%2Fsourceforge.net%2Fprojects%2Fgiflib%2F&ts=1398011957&use_mirror=kent" "0" giflib-5.0.6.tar.bz2
	@./build cmmi "$(BUILD_ROOT_DEPS)" giflib-5.0.6 "" "" $(VERBOSE)

libjpeg: env zlib $(BUILD_ROOT_DEPS)/bin/libjpeg-9.dll

$(BUILD_ROOT_DEPS)/bin/libjpeg-9.dll:
	@./get-src http://www.ijg.org/files jpegsrc.v9.tar.gz
	@./build cmmi "$(BUILD_ROOT_DEPS)" jpeg-9 "" "" $(VERBOSE)

libtiff: env zlib $(BUILD_ROOT_DEPS)/lib/pkgconfig/libtiff-4.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/libtiff-4.pc:
	@./get-src ftp://ftp.remotesensing.org/pub/libtiff tiff-4.0.3.tar.gz
	@./build cmmi "$(BUILD_ROOT_DEPS)" tiff-4.0.3 "" "" $(VERBOSE)

jasper: env zlib $(BUILD_ROOT_DEPS)/include/jasper/jasper.h

$(BUILD_ROOT_DEPS)/include/jasper/jasper.h:
	@./get-src http://www.ece.uvic.ca/~frodo/jasper/software jasper-1.900.1.zip
	@./build cmmi "$(BUILD_ROOT_DEPS)" jasper-1.900.1 "jasper.patch" "" $(VERBOSE)

libffi: env $(BUILD_ROOT_DEPS)/lib/pkgconfig/libffi.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/libffi.pc:
	@./get-src ftp://sourceware.org/pub/libffi/ libffi-3.0.9.tar.gz
	@./build cmmi "$(BUILD_ROOT_DEPS)" libffi-3.0.9 "" "" $(VERBOSE)

dbus: env expat $(BUILD_ROOT_DEPS)/lib/pkgconfig/dbus-1.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/dbus-1.pc:
	@./get-src http://dbus.freedesktop.org/releases/dbus dbus-1.8.0.tar.gz
	@./build rcmmi "$(BUILD_ROOT_DEPS)" dbus-1.8.0 "dbus.patch" "--build=mingw32" $(VERBOSE)

expat: env libxml2 $(BUILD_ROOT_DEPS)/lib/pkgconfig/expat.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/expat.pc:
	@./get-src "http://downloads.sourceforge.net/project/expat/expat/2.1.0/expat-2.1.0.tar.gz?r=http%3A%2F%2Fsourceforge.net%2Fprojects%2Fexpat%2Ffiles%2Fexpat%2F2.1.0%2F&ts=1388081569&use_mirror=heanet" "0" expat-2.1.0.tar.gz
	@./build cmmi "$(BUILD_ROOT_DEPS)" expat-2.1.0 "" "" $(VERBOSE)

krb5: env $(BUILD_ROOT_DEPS)/include/krb5.h

$(BUILD_ROOT_DEPS)/include/krb5.h:
	@./get-src http://web.mit.edu/kerberos/dist/krb5/1.12 krb5-1.12-signed.tar
	@./build cmmi "$(BUILD_ROOT_DEPS)" krb5-1.12-signed "" "" $(VERBOSE)

# to be able to call ./autogen.sh
fake-gtk-doc: env $(BUILD_ROOT_DEPS)/bin/gtkdocize

$(BUILD_ROOT_DEPS)/bin/gtkdocize:
	@echo "#!/bin/bash" >$(BUILD_ROOT_DEPS)/bin/gtkdocize
	@echo "echo fake gtkdocize binary >&2" >>$(BUILD_ROOT_DEPS)/bin/gtkdocize
	@echo "#if [ x$-1 = x--version ] ; then" >>$(BUILD_ROOT_DEPS)/bin/gtkdocize
	@echo "	echo 1.20.1" >>$(BUILD_ROOT_DEPS)/bin/gtkdocize
	@echo "	exit 0" >>$(BUILD_ROOT_DEPS)/bin/gtkdocize
	@echo "#fi" >>$(BUILD_ROOT_DEPS)/bin/gtkdocize
	@echo "exit 1" >>$(BUILD_ROOT_DEPS)/bin/gtkdocize
	@cp patches/gtk-doc.m4 $(BUILD_ROOT_DEPS)/share/aclocal/
	@cp patches/yelp.m4 $(BUILD_ROOT_DEPS)/share/aclocal/

libxml2: env libiconv $(BUILD_ROOT_DEPS)/lib/pkgconfig/libxml-2.0.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/libxml-2.0.pc:
	@./get-src ftp://xmlsoft.org/libxml2 libxml2-2.8.0.tar.gz
	@./build cmmi "$(BUILD_ROOT_DEPS)" libxml2-2.8.0 "" "" $(VERBOSE)

zlib: env pkg-config $(BUILD_ROOT_DEPS)/lib/pkgconfig/zlib.pc

$(BUILD_ROOT_DEPS)/lib/pkgconfig/zlib.pc:
	@./get-src http://zlib.net/ zlib-1.2.8.tar.xz
	@./build mmi "$(BUILD_ROOT_DEPS)" zlib-1.2.8 "" "-f win32/Makefile.gcc NO_MSCVER_FILE64_FUNC=1" $(VERBOSE)
	@cd src/zlib-1.2.8 && \
	cp libz.dll.a $(BUILD_ROOT_DEPS)/lib/ && \
	cp zlib1.dll $(BUILD_ROOT_DEPS)/bin/ && \
	cd ../..

cmake: env $(BUILD_ROOT_DEPS)/bin/cmake.exe

$(BUILD_ROOT_DEPS)/bin/cmake.exe:
	@./get-src http://www.cmake.org/files/v2.8 cmake-2.8.12.2-win32-x86.zip
	@cp -r src/cmake-2.8.12.2-win32-x86/share/* $(BUILD_ROOT_DEPS)/share/
	@cp -r src/cmake-2.8.12.2-win32-x86/bin/* $(BUILD_ROOT_DEPS)/bin/

7zip: env $(BUILD_ROOT_DEPS)/bin/7za.exe

$(BUILD_ROOT_DEPS)/bin/7za.exe:
	@./get-src http://downloads.sourceforge.net/sevenzip 7za920.zip
	@cp src/7za.exe $(BUILD_ROOT_DEPS)/bin/7za.exe

intltool: env $(BUILD_ROOT_DEPS)/bin/intltoolize

$(BUILD_ROOT_DEPS)/bin/intltoolize:
	@./get-src https://launchpad.net/intltool/trunk/0.50.2/+download intltool-0.50.2.tar.gz
	@./build cmmi "$(BUILD_ROOT_DEPS)" intltool-0.50.2 "" "" $(VERBOSE)

core-tools: regex libtool autoconf automake pkg-config intltool libiconv gettext

libtool: env $(BUILD_ROOT_DEPS)/bin/libtool

$(BUILD_ROOT_DEPS)/bin/libtool:
	@./get-src http://ftp.gnu.org/gnu/libtool libtool-2.4.tar.xz
	@./build cmmi "$(BUILD_ROOT_DEPS)" libtool-2.4 "" "" $(VERBOSE)

autoconf: env $(BUILD_ROOT_DEPS)/bin/autoconf

$(BUILD_ROOT_DEPS)/bin/autoconf:
	@./get-src http://ftp.gnu.org/gnu/autoconf autoconf-2.69.tar.xz
	@./build cmmi "$(BUILD_ROOT_DEPS)" autoconf-2.69 "" "" $(VERBOSE)

automake: env $(BUILD_ROOT_DEPS)/bin/automake

$(BUILD_ROOT_DEPS)/bin/automake:
	@./get-src http://ftp.gnu.org/gnu/automake automake-1.14.tar.xz
	@./build cmmi "$(BUILD_ROOT_DEPS)" automake-1.14 "" "" $(VERBOSE)

libiconv: env $(BUILD_ROOT_DEPS)/bin/iconv.exe

$(BUILD_ROOT_DEPS)/bin/iconv.exe:
	@./get-src http://ftp.gnu.org/gnu/libiconv libiconv-1.14.tar.gz
	@./build cmmi "$(BUILD_ROOT_DEPS)" libiconv-1.14 "" "" $(VERBOSE)

gettext: env expat $(BUILD_ROOT_DEPS)/bin/gettext.exe

$(BUILD_ROOT_DEPS)/bin/gettext.exe:
	@./get-src http://ftp.gnu.org/pub/gnu/gettext gettext-0.19.4.tar.gz
	@./build cmmi "$(BUILD_ROOT_DEPS)" gettext-0.19.4 "@gettext.patch" "" $(VERBOSE)

bison: env $(BUILD_ROOT_DEPS)/bin/bison.exe

$(BUILD_ROOT_DEPS)/bin/bison.exe:
	@./get-src http://ftp.gnu.org/gnu/bison bison-3.0.2.tar.xz
	@./build acmmi "$(BUILD_ROOT_DEPS)" bison-3.0.2 "bison.patch" "" $(VERBOSE)

pkg-config: env $(BUILD_ROOT_DEPS)/bin/pkg-config.exe

$(BUILD_ROOT_DEPS)/bin/pkg-config.exe:
	@./get-src "http://downloads.sourceforge.net/project/pkgconfiglite/0.28-1/pkg-config-lite-0.28-1.tar.gz?r=http%3A%2F%2Fsourceforge.net%2Fprojects%2Fpkgconfiglite%2Ffiles%2F0.28-1%2F&ts=1391208353&use_mirror=garr" "0" pkg-config-lite-0.28-1.tar.gz
	@./build cmmi "$(BUILD_ROOT_DEPS)" pkg-config-lite-0.28-1 "" "" $(VERBOSE)
