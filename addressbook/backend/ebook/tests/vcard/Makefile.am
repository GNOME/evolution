
CFLAGS=-I$(srcdir)/../..  `pkg-config --cflags gobject-2.0`

noinst_PROGRAMS=dump-vcard

dump_vcard_LDFLAGS=$(top_builddir)/addressbook/backend/ebook/libevcard.la `pkg-config --libs gobject-2.0`
