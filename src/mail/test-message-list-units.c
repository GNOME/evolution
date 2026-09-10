/*
 * SPDX-FileCopyrightText: (C) 2026 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Unit tests for EMessageList.
 *
 * The TestSession / TestStore / TestFolder infrastructure is copied from
 * evolution-data-server/src/camel/tests/lib/camel-test.c so that we can
 * build a self-contained test binary without linking to a private archive
 * that is not installed.
 */

#include "evolution-config.h"

#include <string.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <camel/camel.h>
#include <e-util/e-util.h>
#include <e-util/test-keyfile-settings-backend.h>

#include "e-message-list.h"

guint _e_virtual_tree_get_first_visible_row (EVirtualTree *self);
guint _e_virtual_tree_get_visible_count (EVirtualTree *self);
gchar * _e_virtual_tree_get_cell_text (EVirtualTree *self, guint row_index, guint column_index);
gchar * _e_message_list_compute_avatar_initials (const gchar *sender);
gboolean _e_message_list_avatar_pixbuf_cache_contains_key (EMessageList *self, const gchar *key);
void _e_message_list_avatar_photo_miss_record (EMessageList *self, const gchar *email);
gboolean _e_message_list_avatar_photo_miss_is_recent (EMessageList *self, const gchar *email);

#define TEST_TYPE_SESSION (test_session_get_type ())
G_DECLARE_FINAL_TYPE (TestSession, test_session, TEST, SESSION, CamelSession)

struct _TestSession {
	CamelSession parent_instance;
};

G_DEFINE_FINAL_TYPE (TestSession, test_session, CAMEL_TYPE_SESSION)

static void
test_session_class_init (TestSessionClass *klass)
{
}

static void
test_session_init (TestSession *self)
{
}

static CamelSession *
test_session_new (void)
{
	CamelSession *session;
	gchar *data_dir, *cache_dir;

	data_dir = g_build_filename (g_get_tmp_dir (), "ml-test", "data-dir", NULL);
	cache_dir = g_build_filename (g_get_tmp_dir (), "ml-test", "cache-dir", NULL);

	session = g_object_new (TEST_TYPE_SESSION,
		"online", TRUE,
		"user-data-dir", data_dir,
		"user-cache-dir", cache_dir,
		NULL);

	g_free (data_dir);
	g_free (cache_dir);

	return session;
}

/* TestStore */

#define TEST_TYPE_STORE (test_store_get_type ())
G_DECLARE_FINAL_TYPE (TestStore, test_store, TEST, STORE, CamelStore)

struct _TestStore {
	CamelStore parent_instance;
	gchar *db_filename;
};

static GInitableIface *store_parent_initable_interface = NULL;

static CamelFolder *test_folder_new (CamelStore *store, const gchar *folder_name);

static CamelFolder *
test_store_get_folder_sync (CamelStore *store,
			    const gchar *folder_name,
			    CamelStoreGetFolderFlags flags,
			    GCancellable *cancellable,
			    GError **error)
{
	CamelStoreDB *sdb = camel_store_get_db (store);

	g_assert_cmpint (camel_store_db_get_folder_id (sdb, folder_name), !=, 0);

	return test_folder_new (store, folder_name);
}

static gboolean
test_store_initable_init (GInitable *initable,
			  GCancellable *cancellable,
			  GError **error)
{
	CamelStore *store = CAMEL_STORE (initable);
	TestStore *self = TEST_STORE (initable);

	camel_store_set_flags (store, camel_store_get_flags (store) | CAMEL_STORE_USE_TEMP_DIR);

	if (!store_parent_initable_interface->init (initable, cancellable, error))
		return FALSE;

	self->db_filename = g_strdup (camel_db_get_filename (CAMEL_DB (camel_store_get_db (store))));

	return TRUE;
}

static void test_store_initable_init_iface (GInitableIface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (TestStore, test_store, CAMEL_TYPE_STORE,
	G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, test_store_initable_init_iface))

static void
test_store_initable_init_iface (GInitableIface *iface)
{
	store_parent_initable_interface = g_type_interface_peek_parent (iface);
	iface->init = test_store_initable_init;
}

static void
test_store_finalize (GObject *object)
{
	TestStore *self = TEST_STORE (object);

	if (self->db_filename) {
		g_unlink (self->db_filename);
		g_free (self->db_filename);
	}

	G_OBJECT_CLASS (test_store_parent_class)->finalize (object);
}

static void
test_store_class_init (TestStoreClass *klass)
{
	CamelStoreClass *store_class;

	store_class = CAMEL_STORE_CLASS (klass);
	store_class->get_folder_sync = test_store_get_folder_sync;

	G_OBJECT_CLASS (klass)->finalize = test_store_finalize;
}

static void
test_store_init (TestStore *self)
{
}

static CamelStore *
test_store_new (CamelSession *session)
{
	static const CamelProvider provider = {
		.protocol = "none",
		.name = "test-none",
		.description = "Test provider",
		.domain = "mail",
		.flags = 0,
	};
	CamelStoreDBFolderRecord record = { 0, };
	CamelStore *store;
	CamelStoreDB *sdb;
	GError *local_error = NULL;
	gboolean success;

	store = g_initable_new (TEST_TYPE_STORE, NULL, &local_error,
		"uid", "test-store",
		"display-name", "Test Store",
		"provider", &provider,
		"session", session,
		"with-proxy-resolver", FALSE,
		NULL);
	g_assert_no_error (local_error);
	g_assert_nonnull (store);

	sdb = camel_store_get_db (store);
	g_assert_nonnull (sdb);

	success = camel_store_db_write_folder (sdb, "inbox", &record, &local_error);
	g_assert_no_error (local_error);
	g_assert_true (success);

	return store;
}

/* TestFolder */

#define TEST_TYPE_FOLDER (test_folder_get_type ())
G_DECLARE_FINAL_TYPE (TestFolder, test_folder, TEST, FOLDER, CamelFolder)

struct _TestFolder {
	CamelFolder parent_instance;
};

G_DEFINE_FINAL_TYPE (TestFolder, test_folder, CAMEL_TYPE_FOLDER)

static gchar *
test_folder_get_filename (CamelFolder *folder,
			  const gchar *uid,
			  GError **error)
{
	return NULL;
}

static CamelMessageInfo *
test_folder_get_message_info (CamelFolder *folder,
			      const gchar *uid)
{
	CamelFolderSummary *summary;
	CamelMessageInfo *nfo = NULL;
	CamelStoreDB *sdb;
	CamelStoreDBMessageRecord record_data = { 0, };
	GError *local_error = NULL;
	gchar *bdata_ptr = NULL;
	gboolean success;

	summary = camel_folder_get_folder_summary (folder);
	if (summary)
		nfo = camel_folder_summary_peek_loaded (summary, uid);

	if (nfo)
		return nfo;

	sdb = camel_store_get_db (camel_folder_get_parent_store (folder));

	success = camel_store_db_read_message (sdb, camel_folder_get_full_name (folder), uid, &record_data, &local_error);
	g_assert_no_error (local_error);
	g_assert_true (success);

	nfo = camel_message_info_new (summary);
	g_assert_true (camel_message_info_load (nfo, &record_data, &bdata_ptr));
	camel_store_db_message_record_clear (&record_data);

	camel_message_info_take_headers (nfo, NULL);

	if (summary)
		camel_folder_summary_add (summary, nfo, TRUE);

	return nfo;
}

static CamelMimeMessage *
test_folder_get_message_sync (CamelFolder *folder,
			      const gchar *message_uid,
			      GCancellable *cancellable,
			      GError **error)
{
	return camel_mime_message_new ();
}

static void
test_folder_class_init (TestFolderClass *klass)
{
	CamelFolderClass *folder_class;

	folder_class = CAMEL_FOLDER_CLASS (klass);
	folder_class->get_filename = test_folder_get_filename;
	folder_class->get_message_info = test_folder_get_message_info;
	folder_class->get_message_sync = test_folder_get_message_sync;
}

static void
test_folder_init (TestFolder *self)
{
	CamelFolder *folder = CAMEL_FOLDER (self);

	camel_folder_take_folder_summary (folder, camel_folder_summary_new (folder));
}

static CamelFolder *
test_folder_new (CamelStore *store,
		 const gchar *folder_name)
{
	const gchar *dash = strrchr (folder_name, '/');

	return g_object_new (TEST_TYPE_FOLDER,
		"parent-store", store,
		"display-name", dash ? dash + 1 : folder_name,
		"full-name", folder_name,
		NULL);
}

/* test_build_part_string / test_add_messages */

static gchar *
test_build_part_string (guint64 message_id,
			const guint64 *references,
			guint n_references)
{
	GString *str;
	CamelSummaryMessageID mid;
	guint ii;

	str = g_string_new (NULL);

	mid.id.id = message_id;
	camel_util_bdata_put_number (str, mid.id.part.hi);
	camel_util_bdata_put_number (str, mid.id.part.lo);
	camel_util_bdata_put_number (str, n_references);

	for (ii = 0; ii < n_references; ii++) {
		mid.id.id = references[ii];
		camel_util_bdata_put_number (str, mid.id.part.hi);
		camel_util_bdata_put_number (str, mid.id.part.lo);
	}

	return g_string_free (str, FALSE);
}

static void
test_add_messages (CamelFolder *folder,
		   ...)
{
	CamelStore *store;
	CamelStoreDB *sdb;
	CamelStoreDBFolderRecord folder_record = { 0, };
	CamelStoreDBMessageRecord record = { 0, };
	GError *local_error = NULL;
	va_list ap;
	guint32 n_added = 0;
	const gchar *folder_name;
	const gchar *tmp;
	gboolean any_set = FALSE;
	gboolean success;

	folder_name = camel_folder_get_full_name (folder);
	store = camel_folder_get_parent_store (folder);
	sdb = camel_store_get_db (store);

	g_assert_cmpint (camel_store_db_get_folder_id (sdb, folder_name), !=, 0);

	success = camel_store_db_read_folder (sdb, folder_name, &folder_record, &local_error);
	g_assert_no_error (local_error);
	g_assert_true (success);

	va_start (ap, folder);

	for (tmp = va_arg (ap, const gchar *); tmp; tmp = va_arg (ap, const gchar *)) {
		if (!*tmp) {
			if (any_set) {
				g_assert_cmpstr (record.uid, !=, NULL);
				record.folder_id = 0;
				success = camel_store_db_write_message (sdb, folder_name, &record, &local_error);
				g_assert_no_error (local_error);
				g_assert_true (success);

				n_added++;

				if (!(record.flags & CAMEL_MESSAGE_SEEN))
					folder_record.unread_count++;
				if ((record.flags & CAMEL_MESSAGE_DELETED) != 0)
					folder_record.deleted_count++;
				if ((record.flags & CAMEL_MESSAGE_JUNK) != 0)
					folder_record.junk_count++;
				if (!(record.flags & CAMEL_MESSAGE_JUNK) && !(record.flags & CAMEL_MESSAGE_DELETED))
					folder_record.visible_count++;
				if ((record.flags & CAMEL_MESSAGE_JUNK) != 0 && !(record.flags & CAMEL_MESSAGE_DELETED))
					folder_record.jnd_count++;
			}

			memset (&record, 0, sizeof (record));
			any_set = FALSE;
			continue;
		}

		any_set = TRUE;

		if (g_str_equal (tmp, "uid"))
			record.uid = va_arg (ap, const gchar *);
		else if (g_str_equal (tmp, "subject"))
			record.subject = va_arg (ap, const gchar *);
		else if (g_str_equal (tmp, "from"))
			record.from = va_arg (ap, const gchar *);
		else if (g_str_equal (tmp, "to"))
			record.to = va_arg (ap, const gchar *);
		else if (g_str_equal (tmp, "cc"))
			record.cc = va_arg (ap, const gchar *);
		else if (g_str_equal (tmp, "mlist"))
			record.mlist = va_arg (ap, const gchar *);
		else if (g_str_equal (tmp, "labels"))
			record.labels = va_arg (ap, gchar *);
		else if (g_str_equal (tmp, "usertags"))
			record.usertags = va_arg (ap, gchar *);
		else if (g_str_equal (tmp, "flags"))
			record.flags = va_arg (ap, guint32);
		else if (g_str_equal (tmp, "dsent"))
			record.dsent = va_arg (ap, gint64);
		else if (g_str_equal (tmp, "dreceived"))
			record.dreceived = va_arg (ap, gint64);
		else if (g_str_equal (tmp, "size"))
			record.size = va_arg (ap, guint32);
		else if (g_str_equal (tmp, "part"))
			record.part = va_arg (ap, gchar *);
		else
			g_error ("%s: Unknown field name '%s'", G_STRFUNC, tmp);
	}

	va_end (ap);

	if (any_set || n_added > 0) {
		if (any_set) {
			g_assert_cmpstr (record.uid, !=, NULL);
			record.folder_id = 0;
			success = camel_store_db_write_message (sdb, folder_name, &record, &local_error);
			g_assert_no_error (local_error);
			g_assert_true (success);

			n_added++;

			if (!(record.flags & CAMEL_MESSAGE_SEEN))
				folder_record.unread_count++;
			if ((record.flags & CAMEL_MESSAGE_DELETED) != 0)
				folder_record.deleted_count++;
			if ((record.flags & CAMEL_MESSAGE_JUNK) != 0)
				folder_record.junk_count++;
			if (!(record.flags & CAMEL_MESSAGE_JUNK) && !(record.flags & CAMEL_MESSAGE_DELETED))
				folder_record.visible_count++;
			if ((record.flags & CAMEL_MESSAGE_JUNK) != 0 && !(record.flags & CAMEL_MESSAGE_DELETED))
				folder_record.jnd_count++;
		}

		folder_record.saved_count += n_added;

		success = camel_store_db_write_folder (sdb, folder_name, &folder_record, &local_error);
		g_assert_no_error (local_error);
		g_assert_true (success);

		camel_store_db_folder_record_clear (&folder_record);

		success = camel_folder_summary_load (camel_folder_get_folder_summary (folder), &local_error);
		g_assert_no_error (local_error);
		g_assert_true (success);
	}
}


#define MSG_ID_A  100
#define MSG_ID_B  200
#define MSG_ID_C  300
#define MSG_ID_D  400
#define MSG_ID_E  500
#define MSG_ID_F  600
#define MSG_ID_G  700

typedef struct {
	GtkWidget *window;
	CamelSession *session;
	CamelStore *store;
	CamelFolder *folder;
	EMessageList *message_list;
	gboolean list_built;
	guint message_selected_count;
	GString *signal_log;
	gboolean show_deleted;
	gboolean show_junk;
} MLFixture;

static gboolean in_background = FALSE;

static void
flush_main_context (void)
{
	while (g_main_context_pending (NULL)) {
		g_main_context_iteration (NULL, FALSE);
	}
}

static gchar *
get_row_uid_at (EVirtualTree *vtree,
	       guint row_index)
{
	EVirtualTreeModel *model = E_VIRTUAL_TREE_MODEL (e_virtual_tree_get_model (vtree));
	GObject *row_obj = e_virtual_tree_model_dup_row (model, row_index);
	gchar *uid = NULL;

	if (row_obj) {
		gconstpointer key = e_virtual_tree_model_get_row_key (model, row_obj);

		uid = g_strdup ((const gchar *) key);
		g_object_unref (row_obj);
	}

	return uid;
}

static void
on_message_list_built (EMessageList *ml,
		       gpointer user_data)
{
	MLFixture *fixture = user_data;

	fixture->list_built = TRUE;
	g_string_append (fixture->signal_log, "message-list-built;");
}

static void
on_message_selected (EMessageList *ml,
		     const gchar *uid,
		     gpointer user_data)
{
	MLFixture *fixture = user_data;

	fixture->message_selected_count++;

	if (uid)
		g_string_append_printf (fixture->signal_log, "message-selected(%s);", uid);
	else
		g_string_append (fixture->signal_log, "message-selected(null);");
}

static void
on_update_actions (EMessageList *ml,
		   gpointer user_data)
{
	MLFixture *fixture = user_data;

	g_string_append (fixture->signal_log, "update-actions;");
}

static void
wait_for_list_built (MLFixture *fixture)
{
	guint timeout_id;

	if (fixture->list_built)
		return;

	timeout_id = g_timeout_add_seconds (10, (GSourceFunc) g_main_loop_quit, NULL);

	while (!fixture->list_built) {
		g_main_context_iteration (NULL, TRUE);
	}

	g_source_remove (timeout_id);
}

static void
wait_for_message_selected (MLFixture *fixture)
{
	guint prev_count = fixture->message_selected_count;
	guint timeout_id;

	timeout_id = g_timeout_add_seconds (10, (GSourceFunc) g_main_loop_quit, NULL);

	while (fixture->message_selected_count == prev_count) {
		g_main_context_iteration (NULL, TRUE);
	}

	g_source_remove (timeout_id);
}

static void
add_flat_messages (CamelFolder *folder)
{
	gchar *part_a = test_build_part_string (MSG_ID_A, NULL, 0);
	gchar *part_b = test_build_part_string (MSG_ID_B, NULL, 0);
	gchar *part_c = test_build_part_string (MSG_ID_C, NULL, 0);
	gchar *part_d = test_build_part_string (MSG_ID_D, NULL, 0);
	gchar *part_e = test_build_part_string (MSG_ID_E, NULL, 0);

	test_add_messages (folder,
		"uid", "m1", "subject", "First message", "from", "alice@test.com",
		"dsent", (gint64) 1000000, "dreceived", (gint64) 1000010,
		"size", (guint32) 100, "flags", (guint32) 0, "part", part_a, "",
		"uid", "m2", "subject", "Second message", "from", "bob@test.com",
		"dsent", (gint64) 1000100, "dreceived", (gint64) 1000110,
		"size", (guint32) 200, "flags", (guint32) CAMEL_MESSAGE_SEEN, "part", part_b, "",
		"uid", "m3", "subject", "Third message", "from", "charlie@test.com",
		"dsent", (gint64) 1000200, "dreceived", (gint64) 1000210,
		"size", (guint32) 50, "flags", (guint32) 0, "part", part_c, "",
		"uid", "m4", "subject", "Fourth message", "from", "dave@test.com",
		"dsent", (gint64) 1000300, "dreceived", (gint64) 1000310,
		"size", (guint32) 500, "flags", (guint32) (CAMEL_MESSAGE_SEEN | CAMEL_MESSAGE_FLAGGED), "part", part_d, "",
		"uid", "m5", "subject", "Fifth message", "from", "eve@test.com",
		"dsent", (gint64) 1000400, "dreceived", (gint64) 1000410,
		"size", (guint32) 150, "flags", (guint32) CAMEL_MESSAGE_SEEN, "part", part_e, "",
		NULL);

	g_free (part_a);
	g_free (part_b);
	g_free (part_c);
	g_free (part_d);
	g_free (part_e);
}

static void
add_all_read_flat_messages (CamelFolder *folder)
{
	gchar *part_a = test_build_part_string (MSG_ID_A, NULL, 0);
	gchar *part_b = test_build_part_string (MSG_ID_B, NULL, 0);
	gchar *part_c = test_build_part_string (MSG_ID_C, NULL, 0);

	test_add_messages (folder,
		"uid", "m1", "subject", "First message", "from", "alice@test.com",
		"dsent", (gint64) 1000000, "dreceived", (gint64) 1000010,
		"size", (guint32) 100, "flags", (guint32) CAMEL_MESSAGE_SEEN, "part", part_a, "",
		"uid", "m2", "subject", "Second message", "from", "bob@test.com",
		"dsent", (gint64) 1000100, "dreceived", (gint64) 1000110,
		"size", (guint32) 200, "flags", (guint32) CAMEL_MESSAGE_SEEN, "part", part_b, "",
		"uid", "m3", "subject", "Third message", "from", "charlie@test.com",
		"dsent", (gint64) 1000200, "dreceived", (gint64) 1000210,
		"size", (guint32) 50, "flags", (guint32) CAMEL_MESSAGE_SEEN, "part", part_c, "",
		NULL);

	g_free (part_a);
	g_free (part_b);
	g_free (part_c);
}

static void
add_threaded_messages (CamelFolder *folder)
{
	guint64 refs_b[] = { MSG_ID_A };
	guint64 refs_c[] = { MSG_ID_B, MSG_ID_A };
	guint64 refs_e[] = { MSG_ID_D };

	gchar *part_a = test_build_part_string (MSG_ID_A, NULL, 0);
	gchar *part_b = test_build_part_string (MSG_ID_B, refs_b, 1);
	gchar *part_c = test_build_part_string (MSG_ID_C, refs_c, 2);
	gchar *part_d = test_build_part_string (MSG_ID_D, NULL, 0);
	gchar *part_e = test_build_part_string (MSG_ID_E, refs_e, 1);
	gchar *part_f = test_build_part_string (MSG_ID_F, NULL, 0);

	/* Thread 1: A -> B -> C
	 * Thread 2: D -> E
	 * Thread 3: F (standalone) */
	test_add_messages (folder,
		"uid", "t1", "subject", "Thread one", "from", "alice@test.com",
		"dsent", (gint64) 1000000, "dreceived", (gint64) 1000010,
		"flags", (guint32) 0, "part", part_a, "",
		"uid", "t2", "subject", "Re: Thread one", "from", "bob@test.com",
		"dsent", (gint64) 1000100, "dreceived", (gint64) 1000110,
		"flags", (guint32) CAMEL_MESSAGE_SEEN, "part", part_b, "",
		"uid", "t3", "subject", "Re: Thread one", "from", "charlie@test.com",
		"dsent", (gint64) 1000200, "dreceived", (gint64) 1000210,
		"flags", (guint32) 0, "part", part_c, "",
		"uid", "t4", "subject", "Thread two", "from", "dave@test.com",
		"dsent", (gint64) 1000300, "dreceived", (gint64) 1000310,
		"flags", (guint32) CAMEL_MESSAGE_SEEN, "part", part_d, "",
		"uid", "t5", "subject", "Re: Thread two", "from", "eve@test.com",
		"dsent", (gint64) 1000400, "dreceived", (gint64) 1000410,
		"flags", (guint32) 0, "part", part_e, "",
		"uid", "t6", "subject", "Standalone", "from", "frank@test.com",
		"dsent", (gint64) 1000500, "dreceived", (gint64) 1000510,
		"flags", (guint32) CAMEL_MESSAGE_SEEN, "part", part_f, "",
		NULL);

	g_free (part_a);
	g_free (part_b);
	g_free (part_c);
	g_free (part_d);
	g_free (part_e);
	g_free (part_f);
}

#define MANY_MESSAGES_COUNT 30

static void
add_many_flat_messages (CamelFolder *folder)
{
	guint ii;

	for (ii = 0; ii < MANY_MESSAGES_COUNT; ii++) {
		gchar *uid = g_strdup_printf ("m%02u", ii);
		gchar *subject = g_strdup_printf ("Message %02u", ii);
		gchar *from = g_strdup_printf ("user%02u@test.com", ii);
		gchar *part = test_build_part_string (10000 + ii, NULL, 0);
		gint64 dsent = 1000000 + ((gint64) ii) * 1000;

		test_add_messages (folder,
			"uid", uid, "subject", subject, "from", from,
			"dsent", dsent, "dreceived", dsent,
			"size", (guint32) 100, "flags", (guint32) 0, "part", part, "",
			NULL);

		g_free (uid);
		g_free (subject);
		g_free (from);
		g_free (part);
	}
}

static void
add_many_messages_with_collapsible_thread (CamelFolder *folder)
{
	gchar *part_root;

	add_many_flat_messages (folder);

	/* Sent before every m* message, so with descending date sort it
	 * lands at the very last row, right after them. */
	part_root = test_build_part_string (90000, NULL, 0);
	test_add_messages (folder,
		"uid", "th-root", "subject", "Th root", "from", "root@test.com",
		"dsent", (gint64) 500000, "dreceived", (gint64) 500000,
		"size", (guint32) 100, "flags", (guint32) 0, "part", part_root, "",
		NULL);
	g_free (part_root);
}

static void
add_many_messages_with_far_orphan (CamelFolder *folder)
{
	guint64 refs[] = { 91000 };
	gchar *part_child;

	add_many_flat_messages (folder);

	/* "far-child" is sent before every m* message and references a
	 * not-yet-existing "far-parent" - it lands at the very last row,
	 * far from the viewport used by these tests. */
	part_child = test_build_part_string (91001, refs, 1);
	test_add_messages (folder,
		"uid", "far-child", "subject", "Re: Far parent", "from", "child@test.com",
		"dsent", (gint64) 400000, "dreceived", (gint64) 400000,
		"size", (guint32) 100, "flags", (guint32) 0, "part", part_child, "",
		NULL);
	g_free (part_child);
}

static void
ml_fixture_set_up (MLFixture *fixture,
		   gconstpointer user_data)
{
	GtkWidget *ml_widget;
	GSettings *mail_settings;

	mail_settings = e_util_ref_settings ("org.gnome.evolution.mail");
	fixture->show_deleted = g_settings_get_boolean (mail_settings, "show-deleted");
	fixture->show_junk = g_settings_get_boolean (mail_settings, "show-junk");
	g_settings_set_boolean (mail_settings, "show-deleted", FALSE);
	g_settings_set_boolean (mail_settings, "show-junk", FALSE);
	g_clear_object (&mail_settings);

	fixture->signal_log = g_string_new ("");

	fixture->session = test_session_new ();
	fixture->store = test_store_new (fixture->session);
	fixture->folder = camel_store_get_folder_sync (fixture->store, "inbox", 0, NULL, NULL);
	g_assert_nonnull (fixture->folder);

	if (user_data == add_threaded_messages)
		add_threaded_messages (fixture->folder);
	else if (user_data == add_many_flat_messages)
		add_many_flat_messages (fixture->folder);
	else if (user_data == add_many_messages_with_collapsible_thread)
		add_many_messages_with_collapsible_thread (fixture->folder);
	else if (user_data == add_many_messages_with_far_orphan)
		add_many_messages_with_far_orphan (fixture->folder);
	else if (user_data == add_all_read_flat_messages)
		add_all_read_flat_messages (fixture->folder);
	else
		add_flat_messages (fixture->folder);

	fixture->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size (GTK_WINDOW (fixture->window), 600, 400);

	if (in_background) {
		gtk_window_set_keep_below (GTK_WINDOW (fixture->window), TRUE);
		gtk_window_set_focus_on_map (GTK_WINDOW (fixture->window), FALSE);
	}

	ml_widget = e_message_list_new (NULL);
	fixture->message_list = E_MESSAGE_LIST (ml_widget);
	gtk_container_add (GTK_CONTAINER (fixture->window), ml_widget);

	g_signal_connect (fixture->message_list, "message-list-built",
		G_CALLBACK (on_message_list_built), fixture);
	g_signal_connect (fixture->message_list, "message-selected",
		G_CALLBACK (on_message_selected), fixture);
	g_signal_connect (fixture->message_list, "update-actions",
		G_CALLBACK (on_update_actions), fixture);

	gtk_widget_show_all (fixture->window);
	flush_main_context ();

	fixture->list_built = FALSE;
	e_message_list_set_folder (fixture->message_list, fixture->folder);
	wait_for_list_built (fixture);

	g_string_truncate (fixture->signal_log, 0);
}

static void
ml_fixture_tear_down (MLFixture *fixture,
		      gconstpointer user_data)
{
	GSettings *mail_settings;

	gtk_widget_destroy (fixture->window);
	fixture->message_list = NULL;

	g_clear_object (&fixture->folder);
	g_clear_object (&fixture->store);
	g_clear_object (&fixture->session);

	g_string_free (fixture->signal_log, TRUE);
	fixture->signal_log = NULL;

	mail_settings = e_util_ref_settings ("org.gnome.evolution.mail");
	g_settings_set_boolean (mail_settings, "show-deleted", fixture->show_deleted);
	g_settings_set_boolean (mail_settings, "show-junk", fixture->show_junk);
	g_clear_object (&mail_settings);

	flush_main_context ();
}


static void
test_signals_message_list_built (MLFixture *fixture,
				 gconstpointer user_data)
{
	/* Re-set the folder to verify the signal fires again. */
	fixture->list_built = FALSE;
	g_string_truncate (fixture->signal_log, 0);

	e_message_list_set_folder (fixture->message_list, fixture->folder);
	wait_for_list_built (fixture);

	g_assert_true (fixture->list_built);
	g_assert_nonnull (strstr (fixture->signal_log->str, "message-list-built;"));
}

static void
test_signals_message_selected (MLFixture *fixture,
			       gconstpointer user_data)
{
	e_message_list_select_uid (fixture->message_list, "m3", FALSE);
	wait_for_message_selected (fixture);

	g_assert_nonnull (strstr (fixture->signal_log->str, "message-selected(m3)"));

	g_string_truncate (fixture->signal_log, 0);
	e_virtual_tree_set_selection_mode (
		e_message_list_get_virtual_tree (fixture->message_list),
		GTK_SELECTION_MULTIPLE);
	e_virtual_tree_unselect_all (
		e_message_list_get_virtual_tree (fixture->message_list));
	wait_for_message_selected (fixture);

	g_assert_nonnull (strstr (fixture->signal_log->str, "message-selected(null)"));
}

static void
test_signals_update_actions (MLFixture *fixture,
			     gconstpointer user_data)
{
	e_message_list_select_uid (fixture->message_list, "m2", FALSE);
	wait_for_message_selected (fixture);
	flush_main_context ();

	g_assert_nonnull (strstr (fixture->signal_log->str, "update-actions;"));
}


static void
test_select_uid (MLFixture *fixture,
		 gconstpointer user_data)
{
	const gchar *cursor_uid;

	e_message_list_select_uid (fixture->message_list, "m3", FALSE);

	cursor_uid = e_message_list_get_cursor_uid (fixture->message_list);
	g_assert_cmpstr (cursor_uid, ==, "m3");
}

static void
test_select_uid_fallback_prefers_oldest_unread (MLFixture *fixture,
						gconstpointer user_data)
{
	const gchar *cursor_uid;

	e_message_list_select_uid (fixture->message_list, "does-not-exist", TRUE);

	cursor_uid = e_message_list_get_cursor_uid (fixture->message_list);
	g_assert_cmpstr (cursor_uid, ==, "m1");
}

static void
test_select_uid_fallback_uses_newest_read_when_no_unread (MLFixture *fixture,
							   gconstpointer user_data)
{
	const gchar *cursor_uid;

	e_message_list_select_uid (fixture->message_list, "does-not-exist", TRUE);

	cursor_uid = e_message_list_get_cursor_uid (fixture->message_list);
	g_assert_cmpstr (cursor_uid, ==, "m3");
}

static void
test_select_uid_null_with_fallback_selects_something (MLFixture *fixture,
						      gconstpointer user_data)
{
	const gchar *cursor_uid;

	e_message_list_select_uid (fixture->message_list, NULL, TRUE);

	cursor_uid = e_message_list_get_cursor_uid (fixture->message_list);
	g_assert_cmpstr (cursor_uid, ==, "m1");
}

static void
test_select_uid_null_no_fallback_selects_nothing (MLFixture *fixture,
						  gconstpointer user_data)
{
	const gchar *cursor_uid;

	e_message_list_select_uid (fixture->message_list, NULL, FALSE);

	cursor_uid = e_message_list_get_cursor_uid (fixture->message_list);
	g_assert_null (cursor_uid);
}

static void
test_get_selected (MLFixture *fixture,
		   gconstpointer user_data)
{
	EVirtualTree *vtree;
	GPtrArray *selected;

	vtree = e_message_list_get_virtual_tree (fixture->message_list);
	e_virtual_tree_set_selection_mode (vtree, GTK_SELECTION_MULTIPLE);

	e_virtual_tree_unselect_all (vtree);
	e_virtual_tree_select_row (vtree, 0);
	e_virtual_tree_select_row (vtree, 2);

	selected = e_message_list_get_selected (fixture->message_list);
	g_assert_nonnull (selected);
	g_assert_cmpuint (selected->len, ==, 2);
	g_ptr_array_unref (selected);
}

static void
test_selected_count (MLFixture *fixture,
		     gconstpointer user_data)
{
	EVirtualTree *vtree;

	vtree = e_message_list_get_virtual_tree (fixture->message_list);
	e_virtual_tree_set_selection_mode (vtree, GTK_SELECTION_MULTIPLE);

	e_virtual_tree_unselect_all (vtree);
	g_assert_cmpuint (e_message_list_selected_count (fixture->message_list), ==, 0);

	e_virtual_tree_select_row (vtree, 1);
	e_virtual_tree_select_row (vtree, 3);
	g_assert_cmpuint (e_message_list_selected_count (fixture->message_list), ==, 2);
}

static void
test_count (MLFixture *fixture,
	    gconstpointer user_data)
{
	g_assert_cmpuint (e_message_list_count (fixture->message_list), ==, 5);
}

static void
test_contains_uid (MLFixture *fixture,
		   gconstpointer user_data)
{
	g_assert_true (e_message_list_contains_uid (fixture->message_list, "m1"));
	g_assert_true (e_message_list_contains_uid (fixture->message_list, "m5"));
	g_assert_false (e_message_list_contains_uid (fixture->message_list, "nonexistent"));
}


static void
test_count_threaded (MLFixture *fixture,
		     gconstpointer user_data)
{
	g_assert_cmpuint (e_message_list_count (fixture->message_list), ==, 6);
}

static void
test_contains_uid_threaded (MLFixture *fixture,
			    gconstpointer user_data)
{
	g_assert_true (e_message_list_contains_uid (fixture->message_list, "t1"));
	g_assert_true (e_message_list_contains_uid (fixture->message_list, "t6"));
	g_assert_false (e_message_list_contains_uid (fixture->message_list, "m1"));
}

static void
test_freeze_thaw_basic (MLFixture *fixture,
			gconstpointer user_data)
{
	guint count_before;

	e_message_list_freeze (fixture->message_list);

	fixture->list_built = FALSE;
	e_message_list_set_show_deleted (fixture->message_list, TRUE);
	flush_main_context ();

	g_assert_false (fixture->list_built);

	e_message_list_thaw (fixture->message_list);
	wait_for_list_built (fixture);

	g_assert_true (fixture->list_built);

	count_before = e_message_list_count (fixture->message_list);
	g_assert_cmpuint (count_before, ==, 5);
}

static void
test_freeze_thaw_nested (MLFixture *fixture,
			 gconstpointer user_data)
{
	e_message_list_freeze (fixture->message_list);
	e_message_list_freeze (fixture->message_list);

	fixture->list_built = FALSE;
	e_message_list_set_show_deleted (fixture->message_list, TRUE);
	flush_main_context ();

	g_assert_false (fixture->list_built);

	e_message_list_thaw (fixture->message_list);
	flush_main_context ();
	g_assert_false (fixture->list_built);

	e_message_list_thaw (fixture->message_list);
	wait_for_list_built (fixture);

	g_assert_true (fixture->list_built);
}

static void
test_regen_selects_unread (MLFixture *fixture,
			   gconstpointer user_data)
{
	const gchar *cursor_uid;

	e_message_list_set_regen_selects_unread (fixture->message_list, TRUE);

	fixture->list_built = FALSE;
	e_message_list_set_show_deleted (fixture->message_list,
		!e_message_list_get_show_deleted (fixture->message_list));
	wait_for_list_built (fixture);

	cursor_uid = e_message_list_get_cursor_uid (fixture->message_list);
	g_assert_nonnull (cursor_uid);
}

static void
test_expand_collapse_all (MLFixture *fixture,
			  gconstpointer user_data)
{
	EVirtualTree *vtree;
	EVirtualTreeModel *model;
	guint total, ii;
	gboolean any_expanded, any_collapsed;

	e_message_list_set_threading (fixture->message_list, CAMEL_FOLDER_VIEW_THREADING_FULL);
	fixture->list_built = FALSE;
	wait_for_list_built (fixture);

	vtree = e_message_list_get_virtual_tree (fixture->message_list);
	model = E_VIRTUAL_TREE_MODEL (e_virtual_tree_get_model (vtree));
	total = e_virtual_tree_model_get_row_count (model);

	e_message_list_expand_all_threads (fixture->message_list);
	total = e_virtual_tree_model_get_row_count (model);

	any_collapsed = FALSE;
	for (ii = 0; ii < total; ii++) {
		GObject *row_obj = e_virtual_tree_model_dup_row (model, ii);

		if (row_obj) {
			if (e_virtual_tree_model_is_expandable (model, row_obj) &&
			    !e_virtual_tree_model_get_expanded (model, row_obj))
				any_collapsed = TRUE;
			g_object_unref (row_obj);
		}
	}
	g_assert_false (any_collapsed);

	e_message_list_collapse_all_threads (fixture->message_list);
	total = e_virtual_tree_model_get_row_count (model);

	any_expanded = FALSE;
	for (ii = 0; ii < total; ii++) {
		GObject *row_obj = e_virtual_tree_model_dup_row (model, ii);

		if (row_obj) {
			if (e_virtual_tree_model_is_expandable (model, row_obj) &&
			    e_virtual_tree_model_get_expanded (model, row_obj))
				any_expanded = TRUE;
			g_object_unref (row_obj);
		}
	}
	g_assert_false (any_expanded);
}

static void
test_select_thread (MLFixture *fixture,
		    gconstpointer user_data)
{
	EVirtualTree *vtree;

	e_message_list_set_threading (fixture->message_list, CAMEL_FOLDER_VIEW_THREADING_FULL);
	fixture->list_built = FALSE;
	wait_for_list_built (fixture);

	e_message_list_expand_all_threads (fixture->message_list);

	vtree = e_message_list_get_virtual_tree (fixture->message_list);
	e_virtual_tree_set_selection_mode (vtree, GTK_SELECTION_MULTIPLE);
	e_virtual_tree_unselect_all (vtree);

	e_virtual_tree_set_cursor (vtree, 1);
	e_message_list_select_thread (fixture->message_list);

	g_assert_cmpuint (e_message_list_selected_count (fixture->message_list), >=, 2);
}

static void
test_select_subthread (MLFixture *fixture,
		       gconstpointer user_data)
{
	EVirtualTree *vtree;
	guint selected;

	e_message_list_set_threading (fixture->message_list, CAMEL_FOLDER_VIEW_THREADING_FULL);
	fixture->list_built = FALSE;
	wait_for_list_built (fixture);

	e_message_list_expand_all_threads (fixture->message_list);

	vtree = e_message_list_get_virtual_tree (fixture->message_list);
	e_virtual_tree_set_selection_mode (vtree, GTK_SELECTION_MULTIPLE);
	e_virtual_tree_unselect_all (vtree);

	e_virtual_tree_set_cursor (vtree, 1);
	e_message_list_select_subthread (fixture->message_list);

	selected = e_message_list_selected_count (fixture->message_list);
	g_assert_cmpuint (selected, >=, 1);
}

static void
test_thread_subject_incremental (MLFixture *fixture,
				 gconstpointer user_data)
{
	EVirtualTree *vtree;
	EVirtualTreeModel *model;
	CamelFolderChangeInfo *changes;
	GObject *row_obj;
	gchar *part_c, *part_p;
	guint row_index;

	e_message_list_set_threading (fixture->message_list, CAMEL_FOLDER_VIEW_THREADING_FULL);
	e_message_list_set_thread_subject (fixture->message_list, TRUE);
	fixture->list_built = FALSE;
	wait_for_list_built (fixture);

	vtree = e_message_list_get_virtual_tree (fixture->message_list);
	model = E_VIRTUAL_TREE_MODEL (e_virtual_tree_get_model (vtree));

	/* "c" arrives first: no References headers, nothing to match it
	 * against yet, so it's its own root. */
	part_c = test_build_part_string (96000, NULL, 0);
	test_add_messages (fixture->folder,
		"uid", "c", "subject", "Re: Important topic", "from", "c@test.com",
		"dsent", (gint64) 2000000, "dreceived", (gint64) 2000000,
		"size", (guint32) 100, "flags", (guint32) 0, "part", part_c, "",
		NULL);
	g_free (part_c);

	fixture->list_built = FALSE;
	changes = camel_folder_change_info_new ();
	camel_folder_change_info_add_uid (changes, "c");
	camel_folder_changed (fixture->folder, changes);
	camel_folder_change_info_free (changes);
	wait_for_list_built (fixture);

	/* "p" arrives later, in its own separate change notification, same
	 * normalized subject, still no References headers. */
	part_p = test_build_part_string (96001, NULL, 0);
	test_add_messages (fixture->folder,
		"uid", "p", "subject", "Important topic", "from", "p@test.com",
		"dsent", (gint64) 1900000, "dreceived", (gint64) 1900000,
		"size", (guint32) 100, "flags", (guint32) 0, "part", part_p, "",
		NULL);
	g_free (part_p);

	fixture->list_built = FALSE;
	changes = camel_folder_change_info_new ();
	camel_folder_change_info_add_uid (changes, "p");
	camel_folder_changed (fixture->folder, changes);
	camel_folder_change_info_free (changes);
	wait_for_list_built (fixture);

	/* "c" should now be grouped under "p" by subject, not still sitting
	 * as a separate root at depth 0. */
	e_message_list_select_uid (fixture->message_list, "p", FALSE);
	row_index = (guint) e_virtual_tree_get_cursor (vtree);
	row_obj = e_virtual_tree_model_dup_row (model, row_index);
	g_assert_nonnull (row_obj);
	g_assert_cmpuint (e_virtual_tree_model_get_depth (model, row_obj), ==, 0);
	g_assert_true (e_virtual_tree_model_is_expandable (model, row_obj));
	g_object_unref (row_obj);

	e_message_list_select_uid (fixture->message_list, "c", FALSE);
	row_index = (guint) e_virtual_tree_get_cursor (vtree);
	row_obj = e_virtual_tree_model_dup_row (model, row_index);
	g_assert_nonnull (row_obj);
	g_assert_cmpuint (e_virtual_tree_model_get_depth (model, row_obj), ==, 1);
	g_object_unref (row_obj);
}

static void
test_search_folder_guard (MLFixture *fixture,
			  gconstpointer user_data)
{
	g_assert_false (e_message_list_is_setting_up_search_folder (fixture->message_list));

	e_message_list_inc_setting_up_search_folder (fixture->message_list);
	g_assert_true (e_message_list_is_setting_up_search_folder (fixture->message_list));

	e_message_list_inc_setting_up_search_folder (fixture->message_list);
	g_assert_true (e_message_list_is_setting_up_search_folder (fixture->message_list));

	e_message_list_dec_setting_up_search_folder (fixture->message_list);
	g_assert_true (e_message_list_is_setting_up_search_folder (fixture->message_list));

	e_message_list_dec_setting_up_search_folder (fixture->message_list);
	g_assert_false (e_message_list_is_setting_up_search_folder (fixture->message_list));
}


static void
test_scroll_stability_real_folder_view (MLFixture *fixture,
					gconstpointer user_data)
{
	EVirtualTree *vtree;
	CamelFolderChangeInfo *changes;
	gchar *part_new;
	gchar *cursor_uid_before;
	guint first_visible_before, cursor_row_before;
	guint first_visible_after, cursor_row_after;
	const gchar *cursor_uid_after;

	vtree = e_message_list_get_virtual_tree (fixture->message_list);
	g_assert_nonnull (vtree);

	g_assert_cmpuint (e_message_list_count (fixture->message_list), ==, MANY_MESSAGES_COUNT);

	/* Default sort is Date Sent, descending: row 0 is the newest (m29),
	 * row 29 is the oldest (m00). Scroll into the middle and select a
	 * row there. */
	e_virtual_tree_scroll_to_row (vtree, 10);
	e_virtual_tree_set_cursor (vtree, 13);
	flush_main_context ();

	first_visible_before = _e_virtual_tree_get_first_visible_row (vtree);
	cursor_row_before = (guint) e_virtual_tree_get_cursor (vtree);
	g_assert_cmpuint (first_visible_before, ==, 10);
	g_assert_cmpuint (cursor_row_before, ==, 13);

	cursor_uid_before = g_strdup (e_message_list_get_cursor_uid (fixture->message_list));
	g_assert_nonnull (cursor_uid_before);

	/* Add a new message dated after all existing ones: with descending
	 * date sort it lands at row 0, strictly above the current viewport. */
	part_new = test_build_part_string (99999, NULL, 0);
	test_add_messages (fixture->folder,
		"uid", "new1", "subject", "Newest message", "from", "new@test.com",
		"dsent", (gint64) 2000000, "dreceived", (gint64) 2000000,
		"size", (guint32) 100, "flags", (guint32) 0, "part", part_new, "",
		NULL);
	g_free (part_new);

	fixture->list_built = FALSE;
	changes = camel_folder_change_info_new ();
	camel_folder_change_info_add_uid (changes, "new1");
	camel_folder_changed (fixture->folder, changes);
	camel_folder_change_info_free (changes);

	wait_for_list_built (fixture);

	g_assert_cmpuint (e_message_list_count (fixture->message_list), ==, MANY_MESSAGES_COUNT + 1);

	first_visible_after = _e_virtual_tree_get_first_visible_row (vtree);
	cursor_row_after = (guint) e_virtual_tree_get_cursor (vtree);
	cursor_uid_after = e_message_list_get_cursor_uid (fixture->message_list);

	/* One row was inserted above the viewport: everything shifts down
	 * by exactly one, and the same message stays selected and visible. */
	g_assert_cmpuint (first_visible_after, ==, first_visible_before + 1);
	g_assert_cmpuint (cursor_row_after, ==, cursor_row_before + 1);
	g_assert_cmpstr (cursor_uid_after, ==, cursor_uid_before);

	g_free (cursor_uid_before);
}

static void
test_scroll_stability_insert_after_viewport (MLFixture *fixture,
					     gconstpointer user_data)
{
	EVirtualTree *vtree;
	CamelFolderChangeInfo *changes;
	gchar *part_new;
	gchar *cursor_uid_before;
	guint first_visible_before, cursor_row_before;
	guint first_visible_after, cursor_row_after;
	const gchar *cursor_uid_after;

	vtree = e_message_list_get_virtual_tree (fixture->message_list);
	g_assert_nonnull (vtree);

	/* Same viewport/selection as the "before" test. */
	e_virtual_tree_scroll_to_row (vtree, 10);
	e_virtual_tree_set_cursor (vtree, 13);
	flush_main_context ();

	first_visible_before = _e_virtual_tree_get_first_visible_row (vtree);
	cursor_row_before = (guint) e_virtual_tree_get_cursor (vtree);
	g_assert_cmpuint (first_visible_before, ==, 10);
	g_assert_cmpuint (cursor_row_before, ==, 13);

	cursor_uid_before = g_strdup (e_message_list_get_cursor_uid (fixture->message_list));
	g_assert_nonnull (cursor_uid_before);

	/* Add a message dated before all existing ones: with descending
	 * date sort it lands at the very end, strictly below the viewport
	 * and below the cursor - nothing on screen should move at all. */
	part_new = test_build_part_string (99998, NULL, 0);
	test_add_messages (fixture->folder,
		"uid", "old1", "subject", "Oldest message", "from", "old@test.com",
		"dsent", (gint64) 500000, "dreceived", (gint64) 500000,
		"size", (guint32) 100, "flags", (guint32) 0, "part", part_new, "",
		NULL);
	g_free (part_new);

	fixture->list_built = FALSE;
	changes = camel_folder_change_info_new ();
	camel_folder_change_info_add_uid (changes, "old1");
	camel_folder_changed (fixture->folder, changes);
	camel_folder_change_info_free (changes);

	wait_for_list_built (fixture);

	g_assert_cmpuint (e_message_list_count (fixture->message_list), ==, MANY_MESSAGES_COUNT + 1);

	first_visible_after = _e_virtual_tree_get_first_visible_row (vtree);
	cursor_row_after = (guint) e_virtual_tree_get_cursor (vtree);
	cursor_uid_after = e_message_list_get_cursor_uid (fixture->message_list);

	/* Insertion below the viewport/cursor: no shift at all. */
	g_assert_cmpuint (first_visible_after, ==, first_visible_before);
	g_assert_cmpuint (cursor_row_after, ==, cursor_row_before);
	g_assert_cmpstr (cursor_uid_after, ==, cursor_uid_before);

	g_free (cursor_uid_before);
}

static void
test_scroll_stability_remove_selected_row (MLFixture *fixture,
					   gconstpointer user_data)
{
	EVirtualTree *vtree;
	CamelFolderChangeInfo *changes;
	gchar *cursor_uid_before;
	guint first_visible_before, cursor_row_before;
	guint first_visible_after, cursor_row_after;
	const gchar *cursor_uid_after;

	vtree = e_message_list_get_virtual_tree (fixture->message_list);
	g_assert_nonnull (vtree);

	e_virtual_tree_scroll_to_row (vtree, 10);
	e_virtual_tree_set_cursor (vtree, 13);
	flush_main_context ();

	first_visible_before = _e_virtual_tree_get_first_visible_row (vtree);
	cursor_row_before = (guint) e_virtual_tree_get_cursor (vtree);
	g_assert_cmpuint (first_visible_before, ==, 10);
	g_assert_cmpuint (cursor_row_before, ==, 13);

	/* Descending date sort: row 13 is m16 (29 - 13). */
	cursor_uid_before = g_strdup (e_message_list_get_cursor_uid (fixture->message_list));
	g_assert_cmpstr (cursor_uid_before, ==, "m16");

	/* Remove the selected message itself. */
	camel_folder_summary_remove_uid (camel_folder_get_folder_summary (fixture->folder), "m16");

	fixture->list_built = FALSE;
	changes = camel_folder_change_info_new ();
	camel_folder_change_info_remove_uid (changes, "m16");
	camel_folder_changed (fixture->folder, changes);
	camel_folder_change_info_free (changes);

	wait_for_list_built (fixture);

	g_assert_cmpuint (e_message_list_count (fixture->message_list), ==, MANY_MESSAGES_COUNT - 1);

	first_visible_after = _e_virtual_tree_get_first_visible_row (vtree);
	cursor_row_after = (guint) e_virtual_tree_get_cursor (vtree);
	cursor_uid_after = e_message_list_get_cursor_uid (fixture->message_list);

	/* The viewport itself doesn't move (the removal is inside it, not
	 * above it); the cursor "sticks" to whatever now occupies its old
	 * screen row - the message that used to be one row below (m15). */
	g_assert_cmpuint (first_visible_after, ==, first_visible_before);
	g_assert_cmpuint (cursor_row_after, ==, cursor_row_before);
	g_assert_cmpstr (cursor_uid_after, ==, "m15");

	g_free (cursor_uid_before);
}

static void
test_scroll_stability_mixed_before_and_after (MLFixture *fixture,
					      gconstpointer user_data)
{
	EVirtualTree *vtree;
	CamelFolderChangeInfo *changes;
	gchar *part_new1, *part_new2;
	gchar *cursor_uid_before;
	guint first_visible_before, cursor_row_before;
	guint first_visible_after, cursor_row_after;
	const gchar *cursor_uid_after;

	vtree = e_message_list_get_virtual_tree (fixture->message_list);
	g_assert_nonnull (vtree);

	e_virtual_tree_scroll_to_row (vtree, 10);
	e_virtual_tree_set_cursor (vtree, 13);
	flush_main_context ();

	first_visible_before = _e_virtual_tree_get_first_visible_row (vtree);
	cursor_row_before = (guint) e_virtual_tree_get_cursor (vtree);
	g_assert_cmpuint (first_visible_before, ==, 10);
	g_assert_cmpuint (cursor_row_before, ==, 13);

	cursor_uid_before = g_strdup (e_message_list_get_cursor_uid (fixture->message_list));
	g_assert_nonnull (cursor_uid_before);

	/* One batch: a message dated after all existing ones (lands at row
	 * 0, above the viewport) and one dated before all existing ones
	 * (lands at the very end, below the viewport/cursor). */
	part_new1 = test_build_part_string (99997, NULL, 0);
	part_new2 = test_build_part_string (99996, NULL, 0);
	test_add_messages (fixture->folder,
		"uid", "new1", "subject", "Newest message", "from", "new@test.com",
		"dsent", (gint64) 2000000, "dreceived", (gint64) 2000000,
		"size", (guint32) 100, "flags", (guint32) 0, "part", part_new1, "",
		"uid", "old1", "subject", "Oldest message", "from", "old@test.com",
		"dsent", (gint64) 500000, "dreceived", (gint64) 500000,
		"size", (guint32) 100, "flags", (guint32) 0, "part", part_new2, "",
		NULL);
	g_free (part_new1);
	g_free (part_new2);

	fixture->list_built = FALSE;
	changes = camel_folder_change_info_new ();
	camel_folder_change_info_add_uid (changes, "new1");
	camel_folder_change_info_add_uid (changes, "old1");
	camel_folder_changed (fixture->folder, changes);
	camel_folder_change_info_free (changes);

	wait_for_list_built (fixture);

	g_assert_cmpuint (e_message_list_count (fixture->message_list), ==, MANY_MESSAGES_COUNT + 2);

	first_visible_after = _e_virtual_tree_get_first_visible_row (vtree);
	cursor_row_after = (guint) e_virtual_tree_get_cursor (vtree);
	cursor_uid_after = e_message_list_get_cursor_uid (fixture->message_list);

	/* Only the insertion above the viewport shifts anything; the one
	 * below has no effect on the viewport/cursor position. */
	g_assert_cmpuint (first_visible_after, ==, first_visible_before + 1);
	g_assert_cmpuint (cursor_row_after, ==, cursor_row_before + 1);
	g_assert_cmpstr (cursor_uid_after, ==, cursor_uid_before);

	g_free (cursor_uid_before);
}

static void
test_scroll_stability_cursor_follows_big_reposition (MLFixture *fixture,
						     gconstpointer user_data)
{
	EVirtualTree *vtree;
	CamelFolderChangeInfo *changes;
	GString *uids = g_string_new (NULL);
	gchar *cursor_uid_before;
	guint first_visible_before, cursor_row_before;
	guint first_visible_after, cursor_row_after;
	const gchar *cursor_uid_after;
	guint ii;

	vtree = e_message_list_get_virtual_tree (fixture->message_list);
	g_assert_nonnull (vtree);

	e_virtual_tree_scroll_to_row (vtree, 5);
	e_virtual_tree_set_cursor (vtree, 10);
	flush_main_context ();

	first_visible_before = _e_virtual_tree_get_first_visible_row (vtree);
	cursor_row_before = (guint) e_virtual_tree_get_cursor (vtree);
	g_assert_cmpuint (first_visible_before, ==, 5);
	g_assert_cmpuint (cursor_row_before, ==, 10);

	cursor_uid_before = g_strdup (e_message_list_get_cursor_uid (fixture->message_list));
	g_assert_nonnull (cursor_uid_before);

	/* 20 messages dated between row 6 (m23) and row 7 (m22): they all
	 * land as one contiguous block at row 7 - after the viewport's top
	 * (5), so the viewport itself would not auto-shift, but before the
	 * cursor (10), which does shift - by 20, well past the viewport.
	 * The view must scroll to keep the cursor visible. */
	changes = camel_folder_change_info_new ();
	for (ii = 0; ii < 20; ii++) {
		gchar *uid = g_strdup_printf ("new%02u", ii);
		gchar *subject = g_strdup_printf ("New message %02u", ii);
		gchar *part = test_build_part_string (99900 + ii, NULL, 0);

		g_string_append_printf (uids, "%s%s", uids->len ? ", " : "", uid);

		test_add_messages (fixture->folder,
			"uid", uid, "subject", subject, "from", "new@test.com",
			"dsent", (gint64) (1022500 + ii), "dreceived", (gint64) (1022500 + ii),
			"size", (guint32) 100, "flags", (guint32) 0, "part", part, "",
			NULL);
		camel_folder_change_info_add_uid (changes, uid);

		g_free (uid);
		g_free (subject);
		g_free (part);
	}
	g_string_free (uids, TRUE);

	fixture->list_built = FALSE;
	camel_folder_changed (fixture->folder, changes);
	camel_folder_change_info_free (changes);

	wait_for_list_built (fixture);

	g_assert_cmpuint (e_message_list_count (fixture->message_list), ==, MANY_MESSAGES_COUNT + 20);

	first_visible_after = _e_virtual_tree_get_first_visible_row (vtree);
	cursor_row_after = (guint) e_virtual_tree_get_cursor (vtree);
	cursor_uid_after = e_message_list_get_cursor_uid (fixture->message_list);

	g_assert_cmpstr (cursor_uid_after, ==, cursor_uid_before);
	g_assert_cmpuint (cursor_row_after, ==, cursor_row_before + 20);

	/* The cursor's message must still be on screen. */
	g_assert_cmpuint (cursor_row_after, >=, first_visible_after);
	g_assert_cmpuint (cursor_row_after, <, first_visible_after + _e_virtual_tree_get_visible_count (vtree));

	g_free (cursor_uid_before);
}

static void
test_scroll_stability_offscreen_cursor_not_followed (MLFixture *fixture,
						     gconstpointer user_data)
{
	EVirtualTree *vtree;
	CamelFolderChangeInfo *changes;
	gchar *top_uid_before, *top_uid_after;
	guint first_visible_before, cursor_row_before;
	guint first_visible_after, cursor_row_after;
	guint ii;

	vtree = e_message_list_get_virtual_tree (fixture->message_list);
	g_assert_nonnull (vtree);

	/* Cursor far below the viewport - the user scrolled away from it. */
	e_virtual_tree_set_cursor (vtree, 25);
	e_virtual_tree_scroll_to_row (vtree, 5);
	flush_main_context ();

	first_visible_before = _e_virtual_tree_get_first_visible_row (vtree);
	cursor_row_before = (guint) e_virtual_tree_get_cursor (vtree);
	g_assert_cmpuint (first_visible_before, ==, 5);
	g_assert_cmpuint (cursor_row_before, ==, 25);
	g_assert_cmpuint (cursor_row_before, >=, first_visible_before + _e_virtual_tree_get_visible_count (vtree));

	top_uid_before = get_row_uid_at (vtree, first_visible_before);
	g_assert_nonnull (top_uid_before);

	/* 20 new messages landing as one block before the viewport: both the
	 * viewport's top row and the (off-screen) cursor shift down by 20. */
	changes = camel_folder_change_info_new ();
	for (ii = 0; ii < 20; ii++) {
		gchar *uid = g_strdup_printf ("new%02u", ii);
		gchar *subject = g_strdup_printf ("New message %02u", ii);
		gchar *part = test_build_part_string (99700 + ii, NULL, 0);

		test_add_messages (fixture->folder,
			"uid", uid, "subject", subject, "from", "new@test.com",
			"dsent", (gint64) (1026500 + ii), "dreceived", (gint64) (1026500 + ii),
			"size", (guint32) 100, "flags", (guint32) 0, "part", part, "",
			NULL);
		camel_folder_change_info_add_uid (changes, uid);

		g_free (uid);
		g_free (subject);
		g_free (part);
	}

	fixture->list_built = FALSE;
	camel_folder_changed (fixture->folder, changes);
	camel_folder_change_info_free (changes);

	wait_for_list_built (fixture);

	g_assert_cmpuint (e_message_list_count (fixture->message_list), ==, MANY_MESSAGES_COUNT + 20);

	first_visible_after = _e_virtual_tree_get_first_visible_row (vtree);
	cursor_row_after = (guint) e_virtual_tree_get_cursor (vtree);
	top_uid_after = get_row_uid_at (vtree, first_visible_after);

	/* The viewport keeps showing the same message it did before - it
	 * does not jump to the cursor, which was never on screen. */
	g_assert_cmpuint (first_visible_after, ==, first_visible_before + 20);
	g_assert_cmpstr (top_uid_after, ==, top_uid_before);

	/* The cursor still tracks its own message correctly, but stays
	 * off-screen rather than pulling the view to it. */
	g_assert_cmpuint (cursor_row_after, ==, cursor_row_before + 20);
	g_assert_cmpuint (cursor_row_after, >=, first_visible_after + _e_virtual_tree_get_visible_count (vtree));

	g_free (top_uid_before);
	g_free (top_uid_after);
}

static void
test_scroll_stability_cursor_follows_unrelated_thread_arrival (MLFixture *fixture,
							       gconstpointer user_data)
{
	EVirtualTree *vtree;
	CamelFolderChangeInfo *changes;
	gchar *cursor_uid_before;
	guint first_visible_before, cursor_row_before;
	guint first_visible_after, cursor_row_after;
	const gchar *cursor_uid_after;
	guint ii;

	e_message_list_set_threading (fixture->message_list, CAMEL_FOLDER_VIEW_THREADING_FULL);
	fixture->list_built = FALSE;
	wait_for_list_built (fixture);

	vtree = e_message_list_get_virtual_tree (fixture->message_list);
	g_assert_nonnull (vtree);

	e_virtual_tree_scroll_to_row (vtree, 5);
	e_virtual_tree_set_cursor (vtree, 10);
	flush_main_context ();

	first_visible_before = _e_virtual_tree_get_first_visible_row (vtree);
	cursor_row_before = (guint) e_virtual_tree_get_cursor (vtree);
	g_assert_cmpuint (first_visible_before, ==, 5);
	g_assert_cmpuint (cursor_row_before, ==, 10);

	cursor_uid_before = g_strdup (e_message_list_get_cursor_uid (fixture->message_list));
	g_assert_nonnull (cursor_uid_before);

	/* 20 brand new, unrelated messages (no references to anything
	 * existing) dated to land as one block right before the cursor's
	 * row. None of them is a parent/child of the cursor's message, so
	 * the bounded reposition diff never lists the cursor's own UID as
	 * a candidate - its index shifts without any rows-removed/inserted
	 * signal ever mentioning it directly. */
	changes = camel_folder_change_info_new ();
	for (ii = 0; ii < 20; ii++) {
		gchar *uid = g_strdup_printf ("newthread%02u", ii);
		gchar *subject = g_strdup_printf ("Unrelated new thread %02u", ii);
		gchar *part = test_build_part_string (99800 + ii, NULL, 0);

		test_add_messages (fixture->folder,
			"uid", uid, "subject", subject, "from", "new@test.com",
			"dsent", (gint64) (1022500 + ii), "dreceived", (gint64) (1022500 + ii),
			"size", (guint32) 100, "flags", (guint32) 0, "part", part, "",
			NULL);
		camel_folder_change_info_add_uid (changes, uid);

		g_free (uid);
		g_free (subject);
		g_free (part);
	}

	fixture->list_built = FALSE;
	camel_folder_changed (fixture->folder, changes);
	camel_folder_change_info_free (changes);

	wait_for_list_built (fixture);

	g_assert_cmpuint (e_message_list_count (fixture->message_list), ==, MANY_MESSAGES_COUNT + 20);

	first_visible_after = _e_virtual_tree_get_first_visible_row (vtree);
	cursor_row_after = (guint) e_virtual_tree_get_cursor (vtree);
	cursor_uid_after = e_message_list_get_cursor_uid (fixture->message_list);

	g_assert_cmpstr (cursor_uid_after, ==, cursor_uid_before);
	g_assert_cmpuint (cursor_row_after, ==, cursor_row_before + 20);

	/* The cursor's message must still be on screen. */
	g_assert_cmpuint (cursor_row_after, >=, first_visible_after);
	g_assert_cmpuint (cursor_row_after, <, first_visible_after + _e_virtual_tree_get_visible_count (vtree));

	g_free (cursor_uid_before);
}

static void
test_scroll_stability_collapsed_thread_no_shift (MLFixture *fixture,
						 gconstpointer user_data)
{
	EVirtualTree *vtree;
	EVirtualTreeModel *model;
	CamelFolderChangeInfo *changes;
	GObject *root_row_obj;
	guint64 refs_reply[] = { 90000 };
	gchar *part_reply;
	gchar *cursor_uid_before;
	guint first_visible_before, cursor_row_before;
	guint first_visible_after, cursor_row_after;
	const gchar *cursor_uid_after;

	e_message_list_set_threading (fixture->message_list, CAMEL_FOLDER_VIEW_THREADING_FULL);
	fixture->list_built = FALSE;
	wait_for_list_built (fixture);

	vtree = e_message_list_get_virtual_tree (fixture->message_list);
	g_assert_nonnull (vtree);
	model = E_VIRTUAL_TREE_MODEL (e_virtual_tree_get_model (vtree));

	g_assert_cmpuint (e_message_list_count (fixture->message_list), ==, MANY_MESSAGES_COUNT + 1);

	/* "th-root" is older than every m* message, so with descending
	 * date sort it lands at the very last row. Collapse it before it
	 * has any children. */
	root_row_obj = e_virtual_tree_model_dup_row (model, MANY_MESSAGES_COUNT);
	g_assert_nonnull (root_row_obj);
	e_virtual_tree_model_set_expanded (model, root_row_obj, FALSE);
	g_object_unref (root_row_obj);

	e_virtual_tree_scroll_to_row (vtree, 10);
	e_virtual_tree_set_cursor (vtree, 13);
	flush_main_context ();

	first_visible_before = _e_virtual_tree_get_first_visible_row (vtree);
	cursor_row_before = (guint) e_virtual_tree_get_cursor (vtree);
	g_assert_cmpuint (first_visible_before, ==, 10);
	g_assert_cmpuint (cursor_row_before, ==, 13);

	cursor_uid_before = g_strdup (e_message_list_get_cursor_uid (fixture->message_list));
	g_assert_nonnull (cursor_uid_before);

	/* Add a reply threaded under the collapsed root: it stays hidden,
	 * so nothing on screen should move and the row count should not
	 * grow until the thread is expanded again. */
	part_reply = test_build_part_string (90001, refs_reply, 1);
	test_add_messages (fixture->folder,
		"uid", "th-reply", "subject", "Re: Th root", "from", "reply@test.com",
		"dsent", (gint64) 500100, "dreceived", (gint64) 500100,
		"size", (guint32) 100, "flags", (guint32) 0, "part", part_reply, "",
		NULL);
	g_free (part_reply);

	fixture->list_built = FALSE;
	changes = camel_folder_change_info_new ();
	camel_folder_change_info_add_uid (changes, "th-reply");
	camel_folder_changed (fixture->folder, changes);
	camel_folder_change_info_free (changes);

	wait_for_list_built (fixture);

	g_assert_cmpuint (e_message_list_count (fixture->message_list), ==, MANY_MESSAGES_COUNT + 1);

	first_visible_after = _e_virtual_tree_get_first_visible_row (vtree);
	cursor_row_after = (guint) e_virtual_tree_get_cursor (vtree);
	cursor_uid_after = e_message_list_get_cursor_uid (fixture->message_list);

	g_assert_cmpuint (first_visible_after, ==, first_visible_before);
	g_assert_cmpuint (cursor_row_after, ==, cursor_row_before);
	g_assert_cmpstr (cursor_uid_after, ==, cursor_uid_before);

	/* Expanding the thread now reveals the reply that arrived while collapsed. */
	root_row_obj = e_virtual_tree_model_dup_row (model, MANY_MESSAGES_COUNT);
	g_assert_nonnull (root_row_obj);
	e_virtual_tree_model_set_expanded (model, root_row_obj, TRUE);
	g_object_unref (root_row_obj);

	g_assert_cmpuint (e_message_list_count (fixture->message_list), ==, MANY_MESSAGES_COUNT + 2);

	g_free (cursor_uid_before);
}

static void
test_scroll_stability_far_reposition_no_shift (MLFixture *fixture,
					       gconstpointer user_data)
{
	EVirtualTree *vtree;
	CamelFolderChangeInfo *changes;
	gchar *part_parent;
	gchar *cursor_uid_before;
	guint first_visible_before, cursor_row_before;
	guint first_visible_after, cursor_row_after;
	const gchar *cursor_uid_after;

	vtree = e_message_list_get_virtual_tree (fixture->message_list);
	g_assert_nonnull (vtree);

	e_message_list_set_threading (fixture->message_list, CAMEL_FOLDER_VIEW_THREADING_FULL);
	fixture->list_built = FALSE;
	wait_for_list_built (fixture);

	g_assert_cmpuint (e_message_list_count (fixture->message_list), ==, MANY_MESSAGES_COUNT + 1);

	e_virtual_tree_scroll_to_row (vtree, 10);
	e_virtual_tree_set_cursor (vtree, 13);
	flush_main_context ();

	first_visible_before = _e_virtual_tree_get_first_visible_row (vtree);
	cursor_row_before = (guint) e_virtual_tree_get_cursor (vtree);
	g_assert_cmpuint (first_visible_before, ==, 10);
	g_assert_cmpuint (cursor_row_before, ==, 13);

	cursor_uid_before = g_strdup (e_message_list_get_cursor_uid (fixture->message_list));
	g_assert_nonnull (cursor_uid_before);

	/* "far-parent" arrives, dated between "far-child" and all m*
	 * messages, so with descending date sort it lands right before
	 * far-child at the very end of the list - far below the viewport.
	 * far-child, already visible as its own root, reparents under it:
	 * a genuine reposition of an existing row, but nowhere near the
	 * viewport, so nothing on screen should move. */
	part_parent = test_build_part_string (91000, NULL, 0);
	test_add_messages (fixture->folder,
		"uid", "far-parent", "subject", "Far parent", "from", "parent@test.com",
		"dsent", (gint64) 450000, "dreceived", (gint64) 450000,
		"size", (guint32) 100, "flags", (guint32) 0, "part", part_parent, "",
		NULL);
	g_free (part_parent);

	fixture->list_built = FALSE;
	changes = camel_folder_change_info_new ();
	camel_folder_change_info_add_uid (changes, "far-parent");
	camel_folder_changed (fixture->folder, changes);
	camel_folder_change_info_free (changes);

	wait_for_list_built (fixture);

	g_assert_cmpuint (e_message_list_count (fixture->message_list), ==, MANY_MESSAGES_COUNT + 2);

	first_visible_after = _e_virtual_tree_get_first_visible_row (vtree);
	cursor_row_after = (guint) e_virtual_tree_get_cursor (vtree);
	cursor_uid_after = e_message_list_get_cursor_uid (fixture->message_list);

	g_assert_cmpuint (first_visible_after, ==, first_visible_before);
	g_assert_cmpuint (cursor_row_after, ==, cursor_row_before);
	g_assert_cmpstr (cursor_uid_after, ==, cursor_uid_before);

	g_free (cursor_uid_before);
}

static void
test_scroll_stability_delete_cursor_causes_reposition (MLFixture *fixture,
						       gconstpointer user_data)
{
	EVirtualTree *vtree;
	GtkTreeView *tree_view;
	CamelFolderChangeInfo *changes;
	GtkTreePath *path = NULL;
	gchar *part_root, *part_reply;
	guint64 refs_reply[] = { 90200 };
	guint cursor_row_before;
	guint first_visible_after, cursor_row_after;
	guint gtk_cursor_row;
	const gchar *cursor_uid_after;
	guint ii, reply_row = G_MAXUINT;

	vtree = e_message_list_get_virtual_tree (fixture->message_list);
	g_assert_nonnull (vtree);
	tree_view = e_virtual_tree_get_tree_view (vtree);

	e_message_list_set_threading (fixture->message_list, CAMEL_FOLDER_VIEW_THREADING_FULL);
	fixture->list_built = FALSE;
	wait_for_list_built (fixture);

	part_root = test_build_part_string (90200, NULL, 0);
	part_reply = test_build_part_string (90201, refs_reply, 1);
	test_add_messages (fixture->folder,
		"uid", "thr-root", "subject", "Th root", "from", "root@test.com",
		"dsent", (gint64) 1014500, "dreceived", (gint64) 1014500,
		"size", (guint32) 100, "flags", (guint32) 0, "part", part_root, "",
		"uid", "thr-reply", "subject", "Re: Th root", "from", "reply@test.com",
		"dsent", (gint64) 1030000, "dreceived", (gint64) 1030000,
		"size", (guint32) 100, "flags", (guint32) 0, "part", part_reply, "",
		NULL);
	g_free (part_root);
	g_free (part_reply);

	fixture->list_built = FALSE;
	changes = camel_folder_change_info_new ();
	camel_folder_change_info_add_uid (changes, "thr-root");
	camel_folder_change_info_add_uid (changes, "thr-reply");
	camel_folder_changed (fixture->folder, changes);
	camel_folder_change_info_free (changes);

	wait_for_list_built (fixture);

	g_assert_cmpuint (e_message_list_count (fixture->message_list), ==, MANY_MESSAGES_COUNT + 2);

	for (ii = 0; ii < 2; ii++) {
		gchar *uid = get_row_uid_at (vtree, ii);

		if (g_strcmp0 (uid, "thr-reply") == 0)
			reply_row = ii;

		g_free (uid);
	}
	g_assert_cmpuint (reply_row, !=, G_MAXUINT);

	e_virtual_tree_scroll_to_row (vtree, 0);
	e_virtual_tree_set_cursor (vtree, reply_row);
	flush_main_context ();

	cursor_row_before = (guint) e_virtual_tree_get_cursor (vtree);
	g_assert_cmpuint (cursor_row_before, ==, reply_row);
	g_assert_cmpstr (e_message_list_get_cursor_uid (fixture->message_list), ==, "thr-reply");

	camel_folder_summary_remove_uid (camel_folder_get_folder_summary (fixture->folder), "thr-reply");

	fixture->list_built = FALSE;
	changes = camel_folder_change_info_new ();
	camel_folder_change_info_remove_uid (changes, "thr-reply");
	camel_folder_changed (fixture->folder, changes);
	camel_folder_change_info_free (changes);

	wait_for_list_built (fixture);

	g_assert_cmpuint (e_message_list_count (fixture->message_list), ==, MANY_MESSAGES_COUNT + 1);

	first_visible_after = _e_virtual_tree_get_first_visible_row (vtree);
	cursor_row_after = (guint) e_virtual_tree_get_cursor (vtree);
	cursor_uid_after = e_message_list_get_cursor_uid (fixture->message_list);

	g_assert_nonnull (cursor_uid_after);
	g_assert_cmpuint (cursor_row_after, >=, first_visible_after);
	g_assert_cmpuint (cursor_row_after, <, first_visible_after + _e_virtual_tree_get_visible_count (vtree));

	for (ii = 0; ii < e_message_list_count (fixture->message_list); ii++) {
		gchar *uid = get_row_uid_at (vtree, ii);
		gboolean is_root = g_strcmp0 (uid, "thr-root") == 0;

		g_free (uid);

		if (is_root) {
			g_assert_cmpuint (ii, >, reply_row + 5);
			break;
		}
	}

	gtk_tree_view_get_cursor (tree_view, &path, NULL);
	g_assert_nonnull (path);
	g_assert_cmpint (gtk_tree_path_get_depth (path), ==, 1);

	gtk_cursor_row = first_visible_after + (guint) gtk_tree_path_get_indices (path)[0];
	g_assert_cmpuint (gtk_cursor_row, ==, cursor_row_after);

	gtk_tree_path_free (path);
}

static void
test_avatar_initials (void)
{
	struct {
		const gchar *sender;
		const gchar *expected;
	} cases[] = {
		{ "John Doe", "JD" },
		{ "John Doe <john.doe@no.where>", "JD" },
		{ "\"Doe, Jane\" <jane.doe@no.where>", "JD" },
		{ "Doe, Jane", "JD" },
		{ "John M. Doe III.", "JD" },
		{ "John M. Doe III", "JD" },
		{ "John M. Doe Jr.", "JD" },
		{ "Jane", "J" },
		{ "Jane Doe (abc) <jane.doe@no.where>", "JD" },
		{ "Jane Doe (abc)", "JD" },
		{ "\"Jane Doe (abc)\" <jane.doe@no.where>", "JD" },
		{ "", "?" },
		{ NULL, "?" }
	};
	guint ii;

	for (ii = 0; ii < G_N_ELEMENTS (cases); ii++) {
		gchar *initials = _e_message_list_compute_avatar_initials (cases[ii].sender);

		g_assert_cmpstr (initials, ==, cases[ii].expected);

		g_free (initials);
	}
}

static void
test_avatar_photo_miss_recorded (MLFixture *fixture,
				 gconstpointer user_data)
{
	const gchar *email = "nobody@nowhere.test";

	g_assert_false (_e_message_list_avatar_photo_miss_is_recent (fixture->message_list, email));

	_e_message_list_avatar_photo_miss_record (fixture->message_list, email);

	g_assert_true (_e_message_list_avatar_photo_miss_is_recent (fixture->message_list, email));
	g_assert_false (_e_message_list_avatar_photo_miss_is_recent (fixture->message_list, "someone.else@nowhere.test"));
}

static void
test_composite_columns_show_from_and_to (MLFixture *fixture,
					 gconstpointer user_data)
{
	EVirtualTree *vtree;
	CamelFolderChangeInfo *changes;
	gchar *part;
	gchar *composite_text;
	gchar *composite_to_text;
	gint row;

	part = test_build_part_string (97000, NULL, 0);
	test_add_messages (fixture->folder,
		"uid", "composite-test", "subject", "Composite test",
		"from", "jane@no.where", "to", "john@no.where",
		"dsent", (gint64) 3000000, "dreceived", (gint64) 3000000,
		"size", (guint32) 100, "flags", (guint32) 0, "part", part, "",
		NULL);
	g_free (part);

	fixture->list_built = FALSE;
	changes = camel_folder_change_info_new ();
	camel_folder_change_info_add_uid (changes, "composite-test");
	camel_folder_changed (fixture->folder, changes);
	camel_folder_change_info_free (changes);
	wait_for_list_built (fixture);

	vtree = e_message_list_get_virtual_tree (fixture->message_list);
	g_assert_nonnull (vtree);

	e_message_list_select_uid (fixture->message_list, "composite-test", FALSE);
	row = e_virtual_tree_get_cursor (vtree);
	g_assert_cmpint (row, >=, 0);

	g_assert_false (gtk_tree_view_column_get_visible (e_message_list_get_column (fixture->message_list, E_MESSAGE_LIST_COLUMN_COMPOSITE_TO)));

	composite_text = _e_virtual_tree_get_cell_text (vtree, (guint) row, E_MESSAGE_LIST_COLUMN_COMPOSITE);
	composite_to_text = _e_virtual_tree_get_cell_text (vtree, (guint) row, E_MESSAGE_LIST_COLUMN_COMPOSITE_TO);

	g_assert_nonnull (composite_text);
	g_assert_nonnull (composite_to_text);

	g_assert_nonnull (strstr (composite_text, "jane"));
	g_assert_null (strstr (composite_text, "john"));

	g_assert_nonnull (strstr (composite_to_text, "john"));
	g_assert_null (strstr (composite_to_text, "jane"));

	g_free (composite_text);
	g_free (composite_to_text);
}

static void
test_avatar_initials_shared_address (MLFixture *fixture,
				     gconstpointer user_data)
{
	EVirtualTree *vtree;
	CamelFolderChangeInfo *changes;
	gchar *part;
	gchar *text;
	gint row;

	part = test_build_part_string (97000, NULL, 0);
	test_add_messages (fixture->folder,
		"uid", "shared-1", "subject", "First",
		"from", "Jane Doe <shared@no.where>", "to", "someone@no.where",
		"dsent", (gint64) 3000000, "dreceived", (gint64) 3000000,
		"size", (guint32) 100, "flags", (guint32) 0, "part", part, "",
		"uid", "shared-2", "subject", "Second",
		"from", "John Doe <shared@no.where>", "to", "someone@no.where",
		"dsent", (gint64) 3000001, "dreceived", (gint64) 3000001,
		"size", (guint32) 100, "flags", (guint32) 0, "part", part, "",
		NULL);
	g_free (part);

	fixture->list_built = FALSE;
	changes = camel_folder_change_info_new ();
	camel_folder_change_info_add_uid (changes, "shared-1");
	camel_folder_change_info_add_uid (changes, "shared-2");
	camel_folder_changed (fixture->folder, changes);
	camel_folder_change_info_free (changes);
	wait_for_list_built (fixture);

	vtree = e_message_list_get_virtual_tree (fixture->message_list);
	g_assert_nonnull (vtree);

	e_message_list_select_uid (fixture->message_list, "shared-1", FALSE);
	row = e_virtual_tree_get_cursor (vtree);
	g_assert_cmpint (row, >=, 0);
	text = _e_virtual_tree_get_cell_text (vtree, (guint) row, E_MESSAGE_LIST_COLUMN_COMPOSITE);
	g_free (text);

	e_message_list_select_uid (fixture->message_list, "shared-2", FALSE);
	row = e_virtual_tree_get_cursor (vtree);
	g_assert_cmpint (row, >=, 0);
	text = _e_virtual_tree_get_cell_text (vtree, (guint) row, E_MESSAGE_LIST_COLUMN_COMPOSITE);
	g_free (text);

	g_assert_true (_e_message_list_avatar_pixbuf_cache_contains_key (fixture->message_list, "Jane Doe <shared@no.where>"));
	g_assert_true (_e_message_list_avatar_pixbuf_cache_contains_key (fixture->message_list, "John Doe <shared@no.where>"));
}

static void
test_click_status_cell_toggles_without_selecting (MLFixture *fixture,
						  gconstpointer user_data)
{
	EVirtualTree *vtree;
	GtkTreeView *tree_view;
	GtkTreeViewColumn *status_column;
	GtkTreePath *path;
	GdkRectangle cell_rect;
	GdkWindow *bin_window;
	GdkEvent *event;
	CamelMessageInfo *info;
	gchar *clicked_uid;
	gint cursor_before, cursor_after;
	guint32 flags_before, flags_after;

	vtree = e_message_list_get_virtual_tree (fixture->message_list);
	g_assert_nonnull (vtree);

	tree_view = e_virtual_tree_get_tree_view (vtree);
	g_assert_nonnull (tree_view);

	/* Cursor sits on row 0; the click below targets row 2 instead. */
	e_virtual_tree_set_cursor (vtree, 0);
	flush_main_context ();

	cursor_before = e_virtual_tree_get_cursor (vtree);
	g_assert_cmpint (cursor_before, ==, 0);

	clicked_uid = get_row_uid_at (vtree, 2);
	g_assert_nonnull (clicked_uid);

	info = camel_folder_get_message_info (fixture->folder, clicked_uid);
	g_assert_nonnull (info);
	flags_before = camel_message_info_get_flags (info);
	g_clear_object (&info);

	status_column = e_message_list_get_column (fixture->message_list, E_MESSAGE_LIST_COLUMN_STATUS);
	g_assert_nonnull (status_column);

	path = gtk_tree_path_new_from_indices (2, -1);
	gtk_tree_view_get_cell_area (tree_view, path, status_column, &cell_rect);
	gtk_tree_path_free (path);

	bin_window = gtk_tree_view_get_bin_window (tree_view);
	g_assert_nonnull (bin_window);

	event = gdk_event_new (GDK_BUTTON_PRESS);
	event->button.window = g_object_ref (bin_window);
	event->button.send_event = TRUE;
	event->button.time = GDK_CURRENT_TIME;
	event->button.x = cell_rect.x + cell_rect.width / 2.0;
	event->button.y = cell_rect.y + cell_rect.height / 2.0;
	event->button.button = 1;
	event->button.state = 0;
	gdk_event_set_device (event, gdk_seat_get_pointer (gdk_display_get_default_seat (gdk_display_get_default ())));

	gtk_widget_event (GTK_WIDGET (tree_view), event);
	gdk_event_free (event);

	flush_main_context ();

	/* The click toggled the message's read/unread flag... */
	info = camel_folder_get_message_info (fixture->folder, clicked_uid);
	g_assert_nonnull (info);
	flags_after = camel_message_info_get_flags (info);
	g_clear_object (&info);

	g_assert_cmpuint (flags_after & CAMEL_MESSAGE_SEEN, !=, flags_before & CAMEL_MESSAGE_SEEN);

	/* ...but did not move the cursor/selection to the clicked row. */
	cursor_after = e_virtual_tree_get_cursor (vtree);
	g_assert_cmpint (cursor_after, ==, cursor_before);

	g_free (clicked_uid);
}

static void
test_navigation_updates_shift_click_anchor (MLFixture *fixture,
					    gconstpointer user_data)
{
	EVirtualTree *vtree;
	GtkTreeView *tree_view;
	GtkTreeViewColumn *subject_column;
	GtkTreePath *path;
	GdkRectangle cell_rect;
	GdkWindow *bin_window;
	GdkEvent *event;
	guint ii;

	vtree = e_message_list_get_virtual_tree (fixture->message_list);
	g_assert_nonnull (vtree);

	tree_view = e_virtual_tree_get_tree_view (vtree);
	g_assert_nonnull (tree_view);

	/* Simulate a plain click on row 0, anchoring the selection there. */
	e_virtual_tree_set_cursor (vtree, 0);
	e_virtual_tree_unselect_all (vtree);
	e_virtual_tree_select_row (vtree, 0);
	flush_main_context ();

	/* Simulate pressing ']' (Next Message) five times, like the mail
	 * reader's keyboard shortcuts do; each hop should re-anchor the
	 * selection on the newly cursored row, just like a click would. */
	for (ii = 0; ii < 5; ii++) {
		g_assert_true (e_message_list_select (fixture->message_list, E_MESSAGE_LIST_SELECT_NEXT, 0, 0));
	}

	g_assert_cmpint (e_virtual_tree_get_cursor (vtree), ==, 5);
	flush_main_context ();

	subject_column = e_message_list_get_column (fixture->message_list, E_MESSAGE_LIST_COLUMN_SUBJECT);
	g_assert_nonnull (subject_column);

	path = gtk_tree_path_new_from_indices (10, -1);
	gtk_tree_view_get_cell_area (tree_view, path, subject_column, &cell_rect);
	gtk_tree_path_free (path);

	bin_window = gtk_tree_view_get_bin_window (tree_view);
	g_assert_nonnull (bin_window);

	event = gdk_event_new (GDK_BUTTON_PRESS);
	event->button.window = g_object_ref (bin_window);
	event->button.send_event = TRUE;
	event->button.time = GDK_CURRENT_TIME;
	event->button.x = cell_rect.x + cell_rect.width / 2.0;
	event->button.y = cell_rect.y + cell_rect.height / 2.0;
	event->button.button = 1;
	event->button.state = GDK_SHIFT_MASK;
	gdk_event_set_device (event, gdk_seat_get_pointer (gdk_display_get_default_seat (gdk_display_get_default ())));

	gtk_widget_event (GTK_WIDGET (tree_view), event);
	gdk_event_free (event);

	flush_main_context ();

	/* The shift+click range must run from the re-anchored row 5 to the
	 * clicked row 10, not from the stale row 0 anchor. */
	for (ii = 0; ii < MANY_MESSAGES_COUNT; ii++) {
		gboolean expect_selected = ii >= 5 && ii <= 10;

		g_assert_cmpint (e_virtual_tree_row_is_selected (vtree, ii), ==, expect_selected);
	}
}

#define add_flat_test(path, func) \
	g_test_add (path, MLFixture, NULL, \
		ml_fixture_set_up, func, ml_fixture_tear_down)

#define add_threaded_test(path, func) \
	g_test_add (path, MLFixture, add_threaded_messages, \
		ml_fixture_set_up, func, ml_fixture_tear_down)

#define add_many_test(path, func) \
	g_test_add (path, MLFixture, add_many_flat_messages, \
		ml_fixture_set_up, func, ml_fixture_tear_down)

#define add_collapsible_thread_test(path, func) \
	g_test_add (path, MLFixture, add_many_messages_with_collapsible_thread, \
		ml_fixture_set_up, func, ml_fixture_tear_down)

#define add_far_orphan_test(path, func) \
	g_test_add (path, MLFixture, add_many_messages_with_far_orphan, \
		ml_fixture_set_up, func, ml_fixture_tear_down)

#define add_all_read_test(path, func) \
	g_test_add (path, MLFixture, add_all_read_flat_messages, \
		ml_fixture_set_up, func, ml_fixture_tear_down)

int
main (int argc,
      char **argv)
{
	gchar *test_keyfile_filename;
	gint ii, res;

	test_keyfile_filename = e_mktemp ("evolution-XXXXXX.settings");
	g_return_val_if_fail (test_keyfile_filename != NULL, -1);

	/* Start with clean settings file, to run with default settings. */
	g_unlink (test_keyfile_filename);

	/* Force the Evolution's test-keyfile GSettings backend, to not depend on
	   a running dconf/D-Bus session and to not overwrite user settings. */
	g_setenv ("GIO_EXTRA_MODULES", EVOLUTION_TESTGIOMODULESDIR, TRUE);
	g_setenv ("GSETTINGS_BACKEND", TEST_KEYFILE_SETTINGS_BACKEND_NAME, TRUE);
	g_setenv (TEST_KEYFILE_SETTINGS_FILENAME_ENVVAR, test_keyfile_filename, TRUE);

	g_test_init (&argc, &argv, NULL);
	gtk_init (&argc, &argv);
	e_util_init_main_thread (NULL);
	camel_init (g_get_tmp_dir (), FALSE);

	for (ii = 1; ii < argc; ii++) {
		if (g_strcmp0 (argv[ii], "--background") == 0)
			in_background = TRUE;
	}

	add_flat_test ("/message-list/signals/message-list-built", test_signals_message_list_built);
	add_flat_test ("/message-list/signals/message-selected", test_signals_message_selected);
	add_flat_test ("/message-list/signals/update-actions", test_signals_update_actions);
	add_flat_test ("/message-list/select/uid", test_select_uid);
	add_flat_test ("/message-list/select/uid-fallback-prefers-oldest-unread", test_select_uid_fallback_prefers_oldest_unread);
	add_all_read_test ("/message-list/select/uid-fallback-uses-newest-read-when-no-unread", test_select_uid_fallback_uses_newest_read_when_no_unread);
	add_flat_test ("/message-list/select/uid-null-with-fallback-selects-something", test_select_uid_null_with_fallback_selects_something);
	add_flat_test ("/message-list/select/uid-null-no-fallback-selects-nothing", test_select_uid_null_no_fallback_selects_nothing);
	add_flat_test ("/message-list/select/get-selected", test_get_selected);
	add_flat_test ("/message-list/select/selected-count", test_selected_count);
	add_flat_test ("/message-list/query/count", test_count);
	add_flat_test ("/message-list/query/contains-uid", test_contains_uid);
	add_threaded_test ("/message-list/threaded/count", test_count_threaded);
	add_threaded_test ("/message-list/threaded/contains-uid", test_contains_uid_threaded);
	add_flat_test ("/message-list/freeze-thaw/basic", test_freeze_thaw_basic);
	add_flat_test ("/message-list/freeze-thaw/nested", test_freeze_thaw_nested);
	add_flat_test ("/message-list/regen-selects-unread", test_regen_selects_unread);
	add_threaded_test ("/message-list/expand-collapse/all", test_expand_collapse_all);
	add_threaded_test ("/message-list/select/thread", test_select_thread);
	add_threaded_test ("/message-list/select/subthread", test_select_subthread);
	add_flat_test ("/message-list/threaded/thread-subject-incremental", test_thread_subject_incremental);
	add_flat_test ("/message-list/search-folder-guard", test_search_folder_guard);
	add_many_test ("/message-list/scroll-stability/real-folder-view", test_scroll_stability_real_folder_view);
	add_many_test ("/message-list/scroll-stability/insert-after-viewport", test_scroll_stability_insert_after_viewport);
	add_many_test ("/message-list/scroll-stability/remove-selected-row", test_scroll_stability_remove_selected_row);
	add_many_test ("/message-list/scroll-stability/mixed-before-and-after", test_scroll_stability_mixed_before_and_after);
	add_many_test ("/message-list/scroll-stability/cursor-follows-big-reposition", test_scroll_stability_cursor_follows_big_reposition);
	add_many_test ("/message-list/scroll-stability/offscreen-cursor-not-followed", test_scroll_stability_offscreen_cursor_not_followed);
	add_many_test ("/message-list/scroll-stability/cursor-follows-unrelated-thread-arrival", test_scroll_stability_cursor_follows_unrelated_thread_arrival);
	add_collapsible_thread_test ("/message-list/scroll-stability/collapsed-thread-no-shift", test_scroll_stability_collapsed_thread_no_shift);
	add_far_orphan_test ("/message-list/scroll-stability/far-reposition-no-shift", test_scroll_stability_far_reposition_no_shift);
	add_many_test ("/message-list/scroll-stability/delete-cursor-causes-reposition", test_scroll_stability_delete_cursor_causes_reposition);
	add_many_test ("/message-list/cell-click/status-toggles-without-selecting", test_click_status_cell_toggles_without_selecting);
	add_many_test ("/message-list/select/navigation-updates-shift-click-anchor", test_navigation_updates_shift_click_anchor);
	add_many_test ("/message-list/composite/columns-show-from-and-to", test_composite_columns_show_from_and_to);
	add_many_test ("/message-list/avatar/initials-shared-address", test_avatar_initials_shared_address);
	g_test_add_func ("/message-list/avatar/initials", test_avatar_initials);
	add_flat_test ("/message-list/avatar/photo-miss-recorded", test_avatar_photo_miss_recorded);

	res = g_test_run ();

	g_unlink (test_keyfile_filename);
	g_free (test_keyfile_filename);

	return res;
}
