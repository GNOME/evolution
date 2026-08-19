FROM fedora:44

RUN dnf -y install \
	abseil-cpp-devel \
	bison \
	boost-devel \
	clang \
	clang-analyzer \
	clang-tools-extra \
	cmake \
	curl \
	cyrus-imapd \
	cyrus-sasl-plain \
	dovecot \
	flex \
	gcc \
	gcc-c++ \
	gcovr \
	gettext \
	gi-docgen \
	git \
	glibc-langpack-en \
	gperf \
	gtk-doc \
	highlight \
	intltool \
	itstool \
	krb5-devel \
	libasan \
	libubsan \
	libphonenumber-devel \
	llvm \
	make \
	mhash-devel \
	ninja-build \
	openldap-devel \
	openssl \
	patch \
	pkgconfig \
	"pkgconfig(atk)" \
	"pkgconfig(cairo-gobject)" \
	"pkgconfig(enchant-2)" \
	"pkgconfig(gail-3.0)" \
	"pkgconfig(gdk-pixbuf-2.0)" \
	"pkgconfig(geocode-glib-2.0)" \
	"pkgconfig(gio-2.0)" \
	"pkgconfig(gio-unix-2.0)" \
	"pkgconfig(glib-2.0)" \
	"pkgconfig(gmodule-2.0)" \
	"pkgconfig(gnome-autoar-0)" \
	"pkgconfig(gnome-autoar-gtk-0)" \
	"pkgconfig(gnome-desktop-3.0)" \
	"pkgconfig(goa-1.0)" \
	"pkgconfig(gsettings-desktop-schemas)" \
	"pkgconfig(gspell-1)" \
	"pkgconfig(gtk+-3.0)" \
	"pkgconfig(gtk4)" \
	"pkgconfig(gweather4)" \
	"pkgconfig(icu-i18n)" \
	"pkgconfig(iso-codes)" \
	"pkgconfig(json-glib-1.0)" \
	"pkgconfig(libcanberra-gtk3)" \
	"pkgconfig(libcmark)" \
	"pkgconfig(libical)" \
	"pkgconfig(libical-glib)" \
	"pkgconfig(libmspack)" \
	"pkgconfig(libnotify)" \
	"pkgconfig(libpst)" \
	"pkgconfig(libsecret-unstable)" \
	"pkgconfig(libsoup-3.0)" \
	"pkgconfig(libxml-2.0)" \
	"pkgconfig(libytnef)" \
	"pkgconfig(nspr)" \
	"pkgconfig(nss)" \
	"pkgconfig(openssl)" \
	"pkgconfig(shared-mime-info)" \
	"pkgconfig(sqlite3)" \
	"pkgconfig(uuid)" \
	"pkgconfig(webkit2gtk-4.1)" \
	"pkgconfig(webkit2gtk-web-extension-4.1)" \
	"pkgconfig(webkitgtk-6.0)" \
	protobuf-devel \
	systemd \
	vala \
	xorg-x11-server-Xvfb \
	yelp-tools \
	/usr/bin/dbus-daemon \
	/usr/bin/killall \
	&& dnf clean all

# Disable dovecot services
RUN systemctl disable --now dovecot.service dovecot.socket dovecot-init.service 2>/dev/null || true

# Set up Cyrus IMAP for testing
RUN echo 'cyruspass' | saslpasswd2 -f /etc/sasldb2 -p -c -u localhost cyrus && \
    echo 'testpass' | saslpasswd2 -f /etc/sasldb2 -p -c -u localhost testuser && \
    chown cyrus:mail /etc/sasldb2

RUN printf '%s\n' \
	'servername: localhost' \
	'configdirectory: /var/lib/imap' \
	'partition-default: /var/spool/imap' \
	'proc_path: /run/cyrus/proc' \
	'mboxname_lockpath: /run/cyrus/lock' \
	'duplicate_db_path: /run/cyrus/db/deliver.db' \
	'ptscache_db_path: /run/cyrus/db/ptscache.db' \
	'statuscache_db_path: /run/cyrus/db/statuscache.db' \
	'tls_sessions_db_path: /run/cyrus/db/tls_sessions.db' \
	'defaultpartition: default' \
	'sievedir: /var/lib/imap/sieve' \
	'admins: cyrus' \
	'allowplaintext: yes' \
	'sasl_pwcheck_method: auxprop' \
	'sasl_auxprop_plugin: sasldb' \
	'sasl_sasldb_path: /etc/sasldb2' \
	'sasl_auto_transition: no' \
	'virtdomains: off' \
	'hashimapspool: true' \
	'httpmodules: jmap' \
	'conversations: 1' \
	'conversations_db: twoskip' \
	> /etc/imapd.conf

RUN printf '%s\n' \
	'START {' \
	'  recover cmd="ctl_cyrusdb -r"' \
	'}' \
	'SERVICES {' \
	'  imap cmd="imapd" listen="10143" prefork=1' \
	'  http cmd="httpd" listen="8080" prefork=1' \
	'}' \
	'EVENTS {' \
	'  checkpoint cmd="ctl_cyrusdb -c" period=30' \
	'}' \
	> /etc/cyrus.conf

RUN install -d -o cyrus -g mail /var/lib/imap /var/spool/imap \
	/var/lib/imap/db /var/lib/imap/sieve /var/lib/imap/socket \
	/var/lib/imap/proc /var/lib/imap/log \
	/run/cyrus /run/cyrus/proc /run/cyrus/lock /run/cyrus/db \
	/run/cyrus/socket

COPY wendzelnntpd.patch /tmp/wendzelnntpd.patch

# Enable sudo for wheel users
RUN sed -i -e 's/# %wheel/%wheel/' -e '0,/%wheel/{s/%wheel/# %wheel/}' /etc/sudoers

ARG HOST_USER_ID=5555
ENV HOST_USER_ID=${HOST_USER_ID}
RUN useradd -u $HOST_USER_ID -G wheel -ms /bin/bash user

USER user
WORKDIR /home/user

ARG WENDZELNNTPD_VERSION=v2.2.0-alpha
ARG WENDZELNNTPD_SHA256=457896ef5a0422f0aaccc8da98553d7a95613b29a87ed1a0512b07abbed38a37
RUN curl -sSL "https://sourceforge.net/projects/wendzelnntpd/files/${WENDZELNNTPD_VERSION}/${WENDZELNNTPD_VERSION}.tar.gz/download" -o /tmp/wendzelnntpd.tar.gz && \
	echo "${WENDZELNNTPD_SHA256}  /tmp/wendzelnntpd.tar.gz" | sha256sum -c - && \
	mkdir /tmp/wendzelnntpd-src && \
	tar -xzf /tmp/wendzelnntpd.tar.gz --strip-components=1 -C /tmp/wendzelnntpd-src && \
	cd /tmp/wendzelnntpd-src && \
	patch -p1 < /tmp/wendzelnntpd.patch && \
	./configure --disable-mysql --prefix=$HOME/_wendzel && \
	make -j$(nproc) && \
	make install && \
	printf '%s\n' \
		'database-engine sqlite3' \
		'' \
		'<connector>' \
		'    port        11119' \
		'    listen      127.0.0.1' \
		'    enable-starttls' \
		'    tls-server-certificate "/home/user/_wendzel/etc/wendzelnntpd/ssl/server.crt"' \
		'    tls-server-key "/home/user/_wendzel/etc/wendzelnntpd/ssl/server.key"' \
		'    tls-ca-certificate "/home/user/_wendzel/etc/wendzelnntpd/ssl/ca.crt"' \
		'</connector>' \
		'' \
		'<connector>' \
		'    port        11129' \
		'    listen      127.0.0.1' \
		'    enable-tls' \
		'    tls-server-certificate "/home/user/_wendzel/etc/wendzelnntpd/ssl/server.crt"' \
		'    tls-server-key "/home/user/_wendzel/etc/wendzelnntpd/ssl/server.key"' \
		'    tls-ca-certificate "/home/user/_wendzel/etc/wendzelnntpd/ssl/ca.crt"' \
		'</connector>' \
		'' \
		'verbose-mode' \
		'max-size-of-postings 20971520' \
		'hash-salt 0.hG4//3baA-::_\' \
		> $HOME/_wendzel/etc/wendzelnntpd/wendzelnntpd.conf && \
	$HOME/_wendzel/sbin/wendzelnntpadm addgroup camel.test.group y && \
	$HOME/_wendzel/sbin/wendzelnntpadm addgroup camel.test.group2 y && \
	$HOME/_wendzel/sbin/wendzelnntpadm addgroup camel.test.readonly n && \
	cd /home/user && \
	rm -rf /tmp/wendzelnntpd.tar.gz /tmp/wendzelnntpd-src

ENV LANG=C.UTF-8
