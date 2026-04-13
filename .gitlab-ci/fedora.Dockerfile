FROM fedora:41

RUN dnf -y install \
	abseil-cpp-devel \
	boost-devel \
	clang \
	clang-analyzer \
	clang-tools-extra \
	cmake \
	gcc \
	gcc-c++ \
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
	ninja-build \
	openldap-devel \
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

# Enable sudo for wheel users
RUN sed -i -e 's/# %wheel/%wheel/' -e '0,/%wheel/{s/%wheel/# %wheel/}' /etc/sudoers

RUN cd /usr/local/bin && \
    curl -L https://github.com/mozilla/grcov/releases/download/v0.10.6/grcov-x86_64-unknown-linux-gnu.tar.bz2 \
    | tar jxvf -

ARG HOST_USER_ID=5555
ENV HOST_USER_ID ${HOST_USER_ID}
RUN useradd -u $HOST_USER_ID -G wheel -ms /bin/bash user

USER user
WORKDIR /home/user

ENV LANG C.UTF-8
