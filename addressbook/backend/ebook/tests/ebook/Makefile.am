
TEST_LIBS=$(top_builddir)/addressbook/backend/ebook/libebook.la

INCLUDES=-I$(srcdir)/../../.. @EVOLUTION_ADDRESSBOOK_CFLAGS@

noinst_PROGRAMS= test-changes test-date test-ebook test-photo test-string

test_date_LDFLAGS=$(TEST_LIBS)
test_ebook_LDFLAGS=$(TEST_LIBS)
test_changes_LDFLAGS=$(TEST_LIBS)
test_photo_LDFLAGS=$(TEST_LIBS)
test_string_LDFLAGS=$(TEST_LIBS)
