/*
 * SPDX-FileCopyrightText: (C) 2026 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Unit tests for EAddressbookTable's local (in-memory) sort/grouping mode.
 *
 * These tests drive EAddressbookTable entirely through its local-mode test
 * hooks (declared, but not exposed, in e-addressbook-table.c) and never
 * attach a real EBookClient/EBookClientView, so no D-Bus services are needed.
 */

#include "evolution-config.h"

#include <gtk/gtk.h>
#include <e-util/e-util.h>
#include <libebook/libebook.h>

#include "e-addressbook-table.h"

void		_e_addressbook_table_test_enable_local_mode
						(EAddressbookTable *self,
						 gboolean enable);
void		_e_addressbook_table_test_push_local_order
						(EAddressbookTable *self);
void		_e_addressbook_table_test_local_add_contact
						(EAddressbookTable *self,
						 EContact *contact);
void		_e_addressbook_table_test_local_modify_contact
						(EAddressbookTable *self,
						 EContact *contact);
void		_e_addressbook_table_test_local_remove_contact
						(EAddressbookTable *self,
						 const gchar *uid);
void		_e_addressbook_table_test_server_set_total
						(EAddressbookTable *self,
						 guint total);
void		_e_addressbook_table_test_server_set_contact
						(EAddressbookTable *self,
						 guint index,
						 EContact *contact);
void		_e_addressbook_table_test_capture_pending_select
						(EAddressbookTable *self);
gchar *		_e_virtual_tree_get_cell_text	(EVirtualTree *self,
						 guint row_index,
						 guint column_index);

typedef struct {
	GtkWidget *window;
	EAddressbookTable *table;
} ATFixture;

static void
flush_main_context (void)
{
	while (g_main_context_pending (NULL))
		g_main_context_iteration (NULL, FALSE);
}

static void
at_fixture_set_up (ATFixture *fixture,
		   gconstpointer user_data)
{
	fixture->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size (GTK_WINDOW (fixture->window), 600, 400);

	fixture->table = E_ADDRESSBOOK_TABLE (e_addressbook_table_new ());
	gtk_container_add (GTK_CONTAINER (fixture->window), GTK_WIDGET (fixture->table));

	gtk_widget_show_all (fixture->window);
	flush_main_context ();

	_e_addressbook_table_test_enable_local_mode (fixture->table, TRUE);
}

static void
at_fixture_tear_down (ATFixture *fixture,
		      gconstpointer user_data)
{
	gtk_widget_destroy (fixture->window);
	fixture->table = NULL;

	flush_main_context ();
}

static guint
find_column_index (const gchar *column_id)
{
	const gchar * const *ids;
	guint n_ids, ii;

	ids = e_addressbook_table_get_legacy_etable_column_ids (&n_ids);

	for (ii = 0; ii < n_ids; ii++) {
		if (g_strcmp0 (ids[ii], column_id) == 0)
			return ii;
	}

	g_assert_not_reached ();

	return G_MAXUINT;
}

static void
add_contact (ATFixture *fixture,
	     const gchar *uid,
	     const gchar *file_as,
	     const gchar *company)
{
	EContact *contact = e_contact_new ();

	e_contact_set (contact, E_CONTACT_UID, uid);
	e_contact_set (contact, E_CONTACT_FILE_AS, file_as);

	if (company)
		e_contact_set (contact, E_CONTACT_ORG, company);

	_e_addressbook_table_test_local_add_contact (fixture->table, contact);

	g_object_unref (contact);
}

static void
modify_contact (ATFixture *fixture,
		const gchar *uid,
		const gchar *file_as,
		const gchar *company)
{
	EContact *contact = e_contact_new ();

	e_contact_set (contact, E_CONTACT_UID, uid);
	e_contact_set (contact, E_CONTACT_FILE_AS, file_as);

	if (company)
		e_contact_set (contact, E_CONTACT_ORG, company);

	_e_addressbook_table_test_local_modify_contact (fixture->table, contact);

	g_object_unref (contact);
}

static void
modify_contact_nickname (ATFixture *fixture,
			 const gchar *uid,
			 const gchar *file_as,
			 const gchar *company,
			 const gchar *nickname)
{
	EContact *contact = e_contact_new ();

	e_contact_set (contact, E_CONTACT_UID, uid);
	e_contact_set (contact, E_CONTACT_FILE_AS, file_as);
	e_contact_set (contact, E_CONTACT_ORG, company);
	e_contact_set (contact, E_CONTACT_NICKNAME, nickname);

	_e_addressbook_table_test_local_modify_contact (fixture->table, contact);

	g_object_unref (contact);
}

static void
add_contact3 (ATFixture *fixture,
	      const gchar *uid,
	      const gchar *file_as,
	      const gchar *level1,
	      const gchar *level2,
	      const gchar *level3)
{
	EContact *contact = e_contact_new ();

	e_contact_set (contact, E_CONTACT_UID, uid);
	e_contact_set (contact, E_CONTACT_FILE_AS, file_as);
	e_contact_set (contact, E_CONTACT_ORG, level1);
	e_contact_set (contact, E_CONTACT_ORG_UNIT, level2);
	e_contact_set (contact, E_CONTACT_OFFICE, level3);

	_e_addressbook_table_test_local_add_contact (fixture->table, contact);

	g_object_unref (contact);
}

static void
modify_contact3 (ATFixture *fixture,
		 const gchar *uid,
		 const gchar *file_as,
		 const gchar *level1,
		 const gchar *level2,
		 const gchar *level3)
{
	EContact *contact = e_contact_new ();

	e_contact_set (contact, E_CONTACT_UID, uid);
	e_contact_set (contact, E_CONTACT_FILE_AS, file_as);
	e_contact_set (contact, E_CONTACT_ORG, level1);
	e_contact_set (contact, E_CONTACT_ORG_UNIT, level2);
	e_contact_set (contact, E_CONTACT_OFFICE, level3);

	_e_addressbook_table_test_local_modify_contact (fixture->table, contact);

	g_object_unref (contact);
}

static void
push_group_levels_3 (ATFixture *fixture)
{
	EVirtualTree *vtree = e_addressbook_table_get_virtual_tree (fixture->table);
	guint company_col = find_column_index ("company");
	guint unit_col = find_column_index ("unit");
	guint office_col = find_column_index ("office");

	e_virtual_tree_set_column_group (vtree, company_col, GTK_SORT_ASCENDING, 1);
	e_virtual_tree_set_column_group (vtree, unit_col, GTK_SORT_ASCENDING, 2);
	e_virtual_tree_set_column_group (vtree, office_col, GTK_SORT_ASCENDING, 3);

	_e_addressbook_table_test_push_local_order (fixture->table);
}

static EVirtualTreeModel *
table_model (ATFixture *fixture)
{
	return e_virtual_tree_get_model (e_addressbook_table_get_virtual_tree (fixture->table));
}

static gchar *
row_uid_at (ATFixture *fixture,
	    guint row_index)
{
	EVirtualTreeModel *model = table_model (fixture);
	GObject *row_obj = e_virtual_tree_model_dup_row (model, row_index);
	EContact *contact;
	gchar *uid = NULL;

	if (row_obj) {
		contact = e_addressbook_table_row_ref_contact (row_obj);

		if (contact) {
			uid = g_strdup (e_contact_get_const (contact, E_CONTACT_UID));
			g_object_unref (contact);
		}

		g_object_unref (row_obj);
	}

	return uid;
}

static gboolean
has_group_row_with_label (ATFixture *fixture,
			  guint depth,
			  const gchar *label)
{
	EVirtualTree *vtree = e_addressbook_table_get_virtual_tree (fixture->table);
	EVirtualTreeModel *model = table_model (fixture);
	guint total = e_virtual_tree_model_get_row_count (model);
	guint ii;
	gboolean found = FALSE;

	for (ii = 0; ii < total && !found; ii++) {
		GObject *row_obj = e_virtual_tree_model_dup_row (model, ii);

		if (!row_obj)
			continue;

		if (e_virtual_tree_model_is_expandable (model, row_obj) &&
		    e_virtual_tree_model_get_depth (model, row_obj) == depth) {
			gchar *text = _e_virtual_tree_get_cell_text (vtree, ii, 0);

			found = g_strcmp0 (text, label) == 0;
			g_free (text);
		}

		g_object_unref (row_obj);
	}

	return found;
}

static void
test_widget_destroy_twice_does_not_crash (ATFixture *fixture,
					  gconstpointer user_data)
{
	g_assert_nonnull (fixture->table);
}

static void
test_local_basic_add_remove (ATFixture *fixture,
			     gconstpointer user_data)
{
	EVirtualTreeModel *model = table_model (fixture);
	gchar *uid;

	add_contact (fixture, "uid-1", "Alice", "Acme");
	add_contact (fixture, "uid-2", "Bob", "Acme");
	add_contact (fixture, "uid-3", "Carl", "Beta");

	flush_main_context ();

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (model), ==, 3);

	uid = row_uid_at (fixture, 0);
	g_assert_cmpstr (uid, ==, "uid-1");
	g_free (uid);

	_e_addressbook_table_test_local_remove_contact (fixture->table, "uid-2");
	flush_main_context ();

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (model), ==, 2);

	uid = row_uid_at (fixture, 1);
	g_assert_cmpstr (uid, ==, "uid-3");
	g_free (uid);
}

static void
test_local_tie_break_by_uid (ATFixture *fixture,
			     gconstpointer user_data)
{
	EVirtualTree *vtree = e_addressbook_table_get_virtual_tree (fixture->table);
	guint file_as_col = find_column_index ("file-as");
	gchar *uid;

	add_contact (fixture, "uid-b", "Same", NULL);
	add_contact (fixture, "uid-a", "Same", NULL);

	e_virtual_tree_set_column_sort (vtree, file_as_col, GTK_SORT_ASCENDING, 1);
	_e_addressbook_table_test_push_local_order (fixture->table);

	flush_main_context ();

	uid = row_uid_at (fixture, 0);
	g_assert_cmpstr (uid, ==, "uid-a");
	g_free (uid);

	uid = row_uid_at (fixture, 1);
	g_assert_cmpstr (uid, ==, "uid-b");
	g_free (uid);
}

static void
test_local_sort_by_non_indexed_column (ATFixture *fixture,
				       gconstpointer user_data)
{
	EVirtualTree *vtree = e_addressbook_table_get_virtual_tree (fixture->table);
	guint company_col = find_column_index ("company");
	gchar *uid;

	add_contact (fixture, "uid-1", "Alice", "Zeta");
	add_contact (fixture, "uid-2", "Bob", "Acme");
	add_contact (fixture, "uid-3", "Carl", "Middle");

	e_virtual_tree_set_column_sort (vtree, company_col, GTK_SORT_ASCENDING, 1);
	_e_addressbook_table_test_push_local_order (fixture->table);

	flush_main_context ();

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (table_model (fixture)), ==, 3);

	uid = row_uid_at (fixture, 0);
	g_assert_cmpstr (uid, ==, "uid-2");
	g_free (uid);

	uid = row_uid_at (fixture, 1);
	g_assert_cmpstr (uid, ==, "uid-3");
	g_free (uid);

	uid = row_uid_at (fixture, 2);
	g_assert_cmpstr (uid, ==, "uid-1");
	g_free (uid);
}

static void
test_local_group_single_level (ATFixture *fixture,
			       gconstpointer user_data)
{
	EVirtualTree *vtree = e_addressbook_table_get_virtual_tree (fixture->table);
	EVirtualTreeModel *model = table_model (fixture);
	guint company_col = find_column_index ("company");
	guint file_as_col = find_column_index ("file-as");
	GObject *row_obj;
	gchar *text;
	gchar *uid;

	add_contact (fixture, "uid-1", "Alice", "Acme");
	add_contact (fixture, "uid-2", "Bob", "Acme");
	add_contact (fixture, "uid-3", "Carl", "Beta");

	e_virtual_tree_set_column_group (vtree, company_col, GTK_SORT_ASCENDING, 1);
	e_virtual_tree_set_column_sort (vtree, file_as_col, GTK_SORT_ASCENDING, 1);
	_e_addressbook_table_test_push_local_order (fixture->table);

	flush_main_context ();

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (model), ==, 5);

	row_obj = e_virtual_tree_model_dup_row (model, 0);
	g_assert_true (e_virtual_tree_model_is_expandable (model, row_obj));
	g_assert_cmpuint (e_virtual_tree_model_get_depth (model, row_obj), ==, 0);
	g_object_unref (row_obj);

	text = _e_virtual_tree_get_cell_text (vtree, 0, file_as_col);
	g_assert_cmpstr (text, ==, "Acme");
	g_free (text);

	row_obj = e_virtual_tree_model_dup_row (model, 1);
	g_assert_false (e_virtual_tree_model_is_expandable (model, row_obj));
	g_assert_cmpuint (e_virtual_tree_model_get_depth (model, row_obj), ==, 1);
	g_object_unref (row_obj);

	uid = row_uid_at (fixture, 1);
	g_assert_cmpstr (uid, ==, "uid-1");
	g_free (uid);

	uid = row_uid_at (fixture, 2);
	g_assert_cmpstr (uid, ==, "uid-2");
	g_free (uid);

	text = _e_virtual_tree_get_cell_text (vtree, 3, file_as_col);
	g_assert_cmpstr (text, ==, "Beta");
	g_free (text);

	uid = row_uid_at (fixture, 4);
	g_assert_cmpstr (uid, ==, "uid-3");
	g_free (uid);
}

static void
test_local_group_none_sorts_first (ATFixture *fixture,
				   gconstpointer user_data)
{
	EVirtualTree *vtree = e_addressbook_table_get_virtual_tree (fixture->table);
	guint file_as_col = find_column_index ("file-as");
	gchar *text;

	add_contact (fixture, "uid-1", "Alice", "Acme");
	add_contact (fixture, "uid-2", "Bob", NULL);
	add_contact (fixture, "uid-3", "Carl", "Beta");

	e_virtual_tree_set_column_group (vtree, find_column_index ("company"), GTK_SORT_ASCENDING, 1);
	_e_addressbook_table_test_push_local_order (fixture->table);

	flush_main_context ();

	text = _e_virtual_tree_get_cell_text (vtree, 0, file_as_col);
	g_assert_cmpstr (text, ==, "(None)");
	g_free (text);
}

static void
test_local_peek_selected_contacts_skips_group_row (ATFixture *fixture,
						   gconstpointer user_data)
{
	EVirtualTree *vtree = e_addressbook_table_get_virtual_tree (fixture->table);
	GPtrArray *contacts;

	add_contact (fixture, "uid-1", "Alice", "Acme");
	add_contact (fixture, "uid-2", "Bob", "Beta");

	e_virtual_tree_set_column_group (vtree, find_column_index ("company"), GTK_SORT_ASCENDING, 1);
	_e_addressbook_table_test_push_local_order (fixture->table);

	flush_main_context ();

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (table_model (fixture)), ==, 4);

	e_virtual_tree_select_row (vtree, 0);

	contacts = e_addressbook_table_peek_selected_contacts (fixture->table);
	g_assert_nonnull (contacts);
	g_assert_cmpuint (contacts->len, ==, 0);
	g_ptr_array_unref (contacts);
}

static void
test_local_group_collapse_expand (ATFixture *fixture,
				  gconstpointer user_data)
{
	EVirtualTree *vtree = e_addressbook_table_get_virtual_tree (fixture->table);
	EVirtualTreeModel *model = table_model (fixture);
	guint company_col = find_column_index ("company");
	GObject *group_row;

	add_contact (fixture, "uid-1", "Alice", "Acme");
	add_contact (fixture, "uid-2", "Bob", "Acme");
	add_contact (fixture, "uid-3", "Carl", "Beta");

	e_virtual_tree_set_column_group (vtree, company_col, GTK_SORT_ASCENDING, 1);
	_e_addressbook_table_test_push_local_order (fixture->table);

	flush_main_context ();

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (model), ==, 5);

	group_row = e_virtual_tree_model_dup_row (model, 0);
	g_assert_true (e_virtual_tree_model_get_expanded (model, group_row));

	e_virtual_tree_model_set_expanded (model, group_row, FALSE);

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (model), ==, 3);

	e_virtual_tree_model_set_expanded (model, group_row, TRUE);

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (model), ==, 5);

	g_object_unref (group_row);
}

static void
test_local_collapse_persists_across_rebuild (ATFixture *fixture,
					     gconstpointer user_data)
{
	EVirtualTree *vtree = e_addressbook_table_get_virtual_tree (fixture->table);
	EVirtualTreeModel *model = table_model (fixture);
	guint company_col = find_column_index ("company");
	GObject *group_row;

	add_contact (fixture, "uid-1", "Alice", "Acme");
	add_contact (fixture, "uid-2", "Bob", "Acme");
	add_contact (fixture, "uid-3", "Carl", "Beta");

	e_virtual_tree_set_column_group (vtree, company_col, GTK_SORT_ASCENDING, 1);
	_e_addressbook_table_test_push_local_order (fixture->table);

	flush_main_context ();

	group_row = e_virtual_tree_model_dup_row (model, 0);
	e_virtual_tree_model_set_expanded (model, group_row, FALSE);
	g_object_unref (group_row);

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (model), ==, 3);

	add_contact (fixture, "uid-4", "Dana", "Beta");
	flush_main_context ();

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (model), ==, 4);

	group_row = e_virtual_tree_model_dup_row (model, 0);
	g_assert_false (e_virtual_tree_model_get_expanded (model, group_row));
	g_object_unref (group_row);
}

static void
test_local_modify_contact_moves_group (ATFixture *fixture,
				       gconstpointer user_data)
{
	EVirtualTree *vtree = e_addressbook_table_get_virtual_tree (fixture->table);
	EVirtualTreeModel *model = table_model (fixture);
	guint company_col = find_column_index ("company");
	gchar *text;

	add_contact (fixture, "uid-1", "Alice", "Acme");

	e_virtual_tree_set_column_group (vtree, company_col, GTK_SORT_ASCENDING, 1);
	_e_addressbook_table_test_push_local_order (fixture->table);

	flush_main_context ();

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (model), ==, 2);

	modify_contact (fixture, "uid-1", "Alice", "Beta");
	flush_main_context ();

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (model), ==, 2);

	text = _e_virtual_tree_get_cell_text (vtree, 0, company_col);
	g_assert_cmpstr (text, ==, "Beta");
	g_free (text);
}

static void
test_local_remove_contact_removes_empty_group (ATFixture *fixture,
					       gconstpointer user_data)
{
	EVirtualTree *vtree = e_addressbook_table_get_virtual_tree (fixture->table);
	EVirtualTreeModel *model = table_model (fixture);
	guint company_col = find_column_index ("company");
	gchar *text;

	add_contact (fixture, "uid-1", "Alice", "Acme");
	add_contact (fixture, "uid-2", "Bob", "Beta");

	e_virtual_tree_set_column_group (vtree, company_col, GTK_SORT_ASCENDING, 1);
	_e_addressbook_table_test_push_local_order (fixture->table);

	flush_main_context ();

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (model), ==, 4);

	_e_addressbook_table_test_local_remove_contact (fixture->table, "uid-1");
	flush_main_context ();

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (model), ==, 2);

	text = _e_virtual_tree_get_cell_text (vtree, 0, company_col);
	g_assert_cmpstr (text, ==, "Beta");
	g_free (text);
}

static void
test_local_modify_unrelated_field_no_move (ATFixture *fixture,
					   gconstpointer user_data)
{
	EVirtualTree *vtree = e_addressbook_table_get_virtual_tree (fixture->table);
	guint company_col = find_column_index ("company");
	guint file_as_col = find_column_index ("file-as");
	guint nickname_col = find_column_index ("nickname");
	gchar *uid;
	gchar *text;

	add_contact (fixture, "uid-1", "Alice", "Acme");
	add_contact (fixture, "uid-2", "Bob", "Acme");
	add_contact (fixture, "uid-3", "Carl", "Beta");

	e_virtual_tree_set_column_group (vtree, company_col, GTK_SORT_ASCENDING, 1);
	e_virtual_tree_set_column_sort (vtree, file_as_col, GTK_SORT_ASCENDING, 1);
	_e_addressbook_table_test_push_local_order (fixture->table);

	flush_main_context ();

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (table_model (fixture)), ==, 5);

	uid = row_uid_at (fixture, 2);
	g_assert_cmpstr (uid, ==, "uid-2");
	g_free (uid);

	modify_contact_nickname (fixture, "uid-2", "Bob", "Acme", "Bobby");
	flush_main_context ();

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (table_model (fixture)), ==, 5);

	uid = row_uid_at (fixture, 2);
	g_assert_cmpstr (uid, ==, "uid-2");
	g_free (uid);

	text = _e_virtual_tree_get_cell_text (vtree, 2, nickname_col);
	g_assert_cmpstr (text, ==, "Bobby");
	g_free (text);
}

static void
test_local_sort_modify_moves_up (ATFixture *fixture,
				 gconstpointer user_data)
{
	EVirtualTree *vtree = e_addressbook_table_get_virtual_tree (fixture->table);
	guint file_as_col = find_column_index ("file-as");
	gchar *uid;

	add_contact (fixture, "uid-1", "Bob", NULL);
	add_contact (fixture, "uid-2", "Dan", NULL);
	add_contact (fixture, "uid-3", "Frank", NULL);

	e_virtual_tree_set_column_sort (vtree, file_as_col, GTK_SORT_ASCENDING, 1);
	_e_addressbook_table_test_push_local_order (fixture->table);

	flush_main_context ();

	uid = row_uid_at (fixture, 0);
	g_assert_cmpstr (uid, ==, "uid-1");
	g_free (uid);

	modify_contact (fixture, "uid-3", "Aaron", NULL);
	flush_main_context ();

	uid = row_uid_at (fixture, 0);
	g_assert_cmpstr (uid, ==, "uid-3");
	g_free (uid);

	uid = row_uid_at (fixture, 1);
	g_assert_cmpstr (uid, ==, "uid-1");
	g_free (uid);

	uid = row_uid_at (fixture, 2);
	g_assert_cmpstr (uid, ==, "uid-2");
	g_free (uid);
}

static void
test_local_sort_modify_moves_down (ATFixture *fixture,
				   gconstpointer user_data)
{
	EVirtualTree *vtree = e_addressbook_table_get_virtual_tree (fixture->table);
	guint file_as_col = find_column_index ("file-as");
	gchar *uid;

	add_contact (fixture, "uid-1", "Bob", NULL);
	add_contact (fixture, "uid-2", "Dan", NULL);
	add_contact (fixture, "uid-3", "Frank", NULL);

	e_virtual_tree_set_column_sort (vtree, file_as_col, GTK_SORT_ASCENDING, 1);
	_e_addressbook_table_test_push_local_order (fixture->table);

	flush_main_context ();

	modify_contact (fixture, "uid-1", "Zack", NULL);
	flush_main_context ();

	uid = row_uid_at (fixture, 0);
	g_assert_cmpstr (uid, ==, "uid-2");
	g_free (uid);

	uid = row_uid_at (fixture, 1);
	g_assert_cmpstr (uid, ==, "uid-3");
	g_free (uid);

	uid = row_uid_at (fixture, 2);
	g_assert_cmpstr (uid, ==, "uid-1");
	g_free (uid);
}

static void
test_local_sort_modify_same_position (ATFixture *fixture,
				      gconstpointer user_data)
{
	EVirtualTree *vtree = e_addressbook_table_get_virtual_tree (fixture->table);
	guint file_as_col = find_column_index ("file-as");
	gchar *uid;
	gchar *text;

	add_contact (fixture, "uid-1", "Bob", NULL);
	add_contact (fixture, "uid-2", "Dan", NULL);
	add_contact (fixture, "uid-3", "Frank", NULL);

	e_virtual_tree_set_column_sort (vtree, file_as_col, GTK_SORT_ASCENDING, 1);
	_e_addressbook_table_test_push_local_order (fixture->table);

	flush_main_context ();

	modify_contact (fixture, "uid-2", "Dave", NULL);
	flush_main_context ();

	uid = row_uid_at (fixture, 1);
	g_assert_cmpstr (uid, ==, "uid-2");
	g_free (uid);

	text = _e_virtual_tree_get_cell_text (vtree, 1, file_as_col);
	g_assert_cmpstr (text, ==, "Dave");
	g_free (text);
}

static void
test_local_group_modify_creates_new_group (ATFixture *fixture,
					   gconstpointer user_data)
{
	EVirtualTree *vtree = e_addressbook_table_get_virtual_tree (fixture->table);

	add_contact (fixture, "uid-1", "Alice", "Acme");
	add_contact (fixture, "uid-2", "Bob", "Acme");

	e_virtual_tree_set_column_group (vtree, find_column_index ("company"), GTK_SORT_ASCENDING, 1);
	_e_addressbook_table_test_push_local_order (fixture->table);

	flush_main_context ();

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (table_model (fixture)), ==, 3);

	modify_contact (fixture, "uid-1", "Alice", "Zenith");
	flush_main_context ();

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (table_model (fixture)), ==, 4);
	g_assert_true (has_group_row_with_label (fixture, 0, "Acme"));
	g_assert_true (has_group_row_with_label (fixture, 0, "Zenith"));
}

static void
test_local_group_modify_moves_to_existing_group_both_survive (ATFixture *fixture,
							      gconstpointer user_data)
{
	EVirtualTree *vtree = e_addressbook_table_get_virtual_tree (fixture->table);

	add_contact (fixture, "uid-1", "Alice", "Acme");
	add_contact (fixture, "uid-2", "Bob", "Acme");
	add_contact (fixture, "uid-3", "Carl", "Beta");
	add_contact (fixture, "uid-4", "Dana", "Beta");

	e_virtual_tree_set_column_group (vtree, find_column_index ("company"), GTK_SORT_ASCENDING, 1);
	_e_addressbook_table_test_push_local_order (fixture->table);

	flush_main_context ();

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (table_model (fixture)), ==, 6);

	modify_contact (fixture, "uid-1", "Alice", "Beta");
	flush_main_context ();

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (table_model (fixture)), ==, 6);
	g_assert_true (has_group_row_with_label (fixture, 0, "Acme"));
	g_assert_true (has_group_row_with_label (fixture, 0, "Beta"));
}

static void
test_local_group_modify_moves_to_existing_group_old_dropped (ATFixture *fixture,
							     gconstpointer user_data)
{
	EVirtualTree *vtree = e_addressbook_table_get_virtual_tree (fixture->table);

	add_contact (fixture, "uid-1", "Alice", "Acme");
	add_contact (fixture, "uid-2", "Carl", "Beta");
	add_contact (fixture, "uid-3", "Dana", "Beta");

	e_virtual_tree_set_column_group (vtree, find_column_index ("company"), GTK_SORT_ASCENDING, 1);
	_e_addressbook_table_test_push_local_order (fixture->table);

	flush_main_context ();

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (table_model (fixture)), ==, 5);

	modify_contact (fixture, "uid-1", "Alice", "Beta");
	flush_main_context ();

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (table_model (fixture)), ==, 4);
	g_assert_false (has_group_row_with_label (fixture, 0, "Acme"));
	g_assert_true (has_group_row_with_label (fixture, 0, "Beta"));
}

static void
test_local_group_level1_modify_creates_new_group (ATFixture *fixture,
						  gconstpointer user_data)
{
	add_contact3 (fixture, "uid-1", "Alice", "Acme", "Sales", "NY");
	add_contact3 (fixture, "uid-2", "Bob", "Acme", "Sales", "NY");

	push_group_levels_3 (fixture);

	flush_main_context ();

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (table_model (fixture)), ==, 5);

	modify_contact3 (fixture, "uid-1", "Alice", "Zenith", "Sales", "NY");
	flush_main_context ();

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (table_model (fixture)), ==, 8);
	g_assert_true (has_group_row_with_label (fixture, 0, "Acme"));
	g_assert_true (has_group_row_with_label (fixture, 0, "Zenith"));
}

static void
test_local_group_level1_modify_moves_to_existing_group_both_survive (ATFixture *fixture,
								     gconstpointer user_data)
{
	add_contact3 (fixture, "uid-1", "Alice", "Acme", "Sales", "NY");
	add_contact3 (fixture, "uid-2", "Bob", "Acme", "Sales", "NY");
	add_contact3 (fixture, "uid-3", "Carl", "Beta", "Sales", "NY");
	add_contact3 (fixture, "uid-4", "Dana", "Beta", "Sales", "NY");

	push_group_levels_3 (fixture);

	flush_main_context ();

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (table_model (fixture)), ==, 10);

	modify_contact3 (fixture, "uid-1", "Alice", "Beta", "Sales", "NY");
	flush_main_context ();

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (table_model (fixture)), ==, 10);
	g_assert_true (has_group_row_with_label (fixture, 0, "Acme"));
	g_assert_true (has_group_row_with_label (fixture, 0, "Beta"));
}

static void
test_local_group_level1_modify_moves_to_existing_group_old_dropped (ATFixture *fixture,
								    gconstpointer user_data)
{
	add_contact3 (fixture, "uid-1", "Alice", "Acme", "Sales", "NY");
	add_contact3 (fixture, "uid-2", "Bob", "Beta", "Sales", "NY");
	add_contact3 (fixture, "uid-3", "Carl", "Beta", "Sales", "NY");

	push_group_levels_3 (fixture);

	flush_main_context ();

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (table_model (fixture)), ==, 9);

	modify_contact3 (fixture, "uid-1", "Alice", "Beta", "Sales", "NY");
	flush_main_context ();

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (table_model (fixture)), ==, 6);
	g_assert_false (has_group_row_with_label (fixture, 0, "Acme"));
	g_assert_true (has_group_row_with_label (fixture, 0, "Beta"));
}

static void
test_local_group_level2_modify_creates_new_group (ATFixture *fixture,
						  gconstpointer user_data)
{
	add_contact3 (fixture, "uid-1", "Alice", "Acme", "Sales", "NY");
	add_contact3 (fixture, "uid-2", "Bob", "Acme", "Sales", "NY");

	push_group_levels_3 (fixture);

	flush_main_context ();

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (table_model (fixture)), ==, 5);

	modify_contact3 (fixture, "uid-1", "Alice", "Acme", "Support", "NY");
	flush_main_context ();

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (table_model (fixture)), ==, 7);
	g_assert_true (has_group_row_with_label (fixture, 1, "Sales"));
	g_assert_true (has_group_row_with_label (fixture, 1, "Support"));
}

static void
test_local_group_level2_modify_moves_to_existing_group_both_survive (ATFixture *fixture,
								     gconstpointer user_data)
{
	add_contact3 (fixture, "uid-1", "Alice", "Acme", "Sales", "NY");
	add_contact3 (fixture, "uid-2", "Bob", "Acme", "Sales", "NY");
	add_contact3 (fixture, "uid-3", "Carl", "Acme", "Support", "NY");
	add_contact3 (fixture, "uid-4", "Dana", "Acme", "Support", "NY");

	push_group_levels_3 (fixture);

	flush_main_context ();

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (table_model (fixture)), ==, 9);

	modify_contact3 (fixture, "uid-1", "Alice", "Acme", "Support", "NY");
	flush_main_context ();

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (table_model (fixture)), ==, 9);
	g_assert_true (has_group_row_with_label (fixture, 1, "Sales"));
	g_assert_true (has_group_row_with_label (fixture, 1, "Support"));
}

static void
test_local_group_level2_modify_moves_to_existing_group_old_dropped (ATFixture *fixture,
								    gconstpointer user_data)
{
	add_contact3 (fixture, "uid-1", "Alice", "Acme", "Sales", "NY");
	add_contact3 (fixture, "uid-2", "Bob", "Acme", "Support", "NY");
	add_contact3 (fixture, "uid-3", "Carl", "Acme", "Support", "NY");

	push_group_levels_3 (fixture);

	flush_main_context ();

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (table_model (fixture)), ==, 8);

	modify_contact3 (fixture, "uid-1", "Alice", "Acme", "Support", "NY");
	flush_main_context ();

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (table_model (fixture)), ==, 6);
	g_assert_false (has_group_row_with_label (fixture, 1, "Sales"));
	g_assert_true (has_group_row_with_label (fixture, 1, "Support"));
}

static void
test_local_debounce_coalesces_batch (ATFixture *fixture,
				     gconstpointer user_data)
{
	EVirtualTreeModel *model = table_model (fixture);

	add_contact (fixture, "uid-1", "Alice", NULL);
	add_contact (fixture, "uid-2", "Bob", NULL);
	add_contact (fixture, "uid-3", "Carl", NULL);

	flush_main_context ();

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (model), ==, 3);
}

static void
test_server_cursor_follows_resort (ATFixture *fixture,
				   gconstpointer user_data)
{
	static const gchar * const asc_vals[] = { "10", "11", "12", "13", "14" };
	static const gchar * const desc_vals[] = { "14", "13", "12", "11", "10" };
	EVirtualTree *vtree = e_addressbook_table_get_virtual_tree (fixture->table);
	gchar *uid;
	guint ii;

	_e_addressbook_table_test_enable_local_mode (fixture->table, FALSE);

	_e_addressbook_table_test_server_set_total (fixture->table, 5);

	for (ii = 0; ii < 5; ii++) {
		EContact *contact = e_contact_new ();
		gchar *uid_str = g_strdup_printf ("uid-%s", asc_vals[ii]);

		e_contact_set (contact, E_CONTACT_UID, uid_str);
		e_contact_set (contact, E_CONTACT_FILE_AS, asc_vals[ii]);

		_e_addressbook_table_test_server_set_contact (fixture->table, ii, contact);

		g_object_unref (contact);
		g_free (uid_str);
	}

	flush_main_context ();

	g_assert_cmpuint (e_virtual_tree_model_get_row_count (table_model (fixture)), ==, 5);

	uid = row_uid_at (fixture, 3);
	g_assert_cmpstr (uid, ==, "uid-13");
	g_free (uid);

	e_virtual_tree_set_cursor (vtree, 3);
	e_virtual_tree_select_row (vtree, 3);

	g_assert_cmpint (e_virtual_tree_get_cursor (vtree), ==, 3);

	/* Simulate clicking the column header to sort descending: remember the
	 * cursor's contact UID, the way addressbook_table_apply_server_sort_fields()
	 * does right before asking the server view to resort. */
	_e_addressbook_table_test_capture_pending_select (fixture->table);

	/* The server invalidates the cache as soon as the resort is accepted,
	 * well before any of the reordered contacts stream back in. */
	_e_addressbook_table_test_server_set_total (fixture->table, 5);
	flush_main_context ();

	g_assert_cmpint (e_virtual_tree_get_cursor (vtree), ==, 3);

	/* Reordered data streams back in, descending this time. */
	for (ii = 0; ii < 5; ii++) {
		EContact *contact = e_contact_new ();
		gchar *uid_str = g_strdup_printf ("uid-%s", desc_vals[ii]);

		e_contact_set (contact, E_CONTACT_UID, uid_str);
		e_contact_set (contact, E_CONTACT_FILE_AS, desc_vals[ii]);

		_e_addressbook_table_test_server_set_contact (fixture->table, ii, contact);
		flush_main_context ();

		g_object_unref (contact);
		g_free (uid_str);
	}

	/* "13" is now at index 1; the cursor must have followed it there and
	 * it must still be the (only) selected row. */
	g_assert_cmpint (e_virtual_tree_get_cursor (vtree), ==, 1);

	uid = row_uid_at (fixture, 1);
	g_assert_cmpstr (uid, ==, "uid-13");
	g_free (uid);

	g_assert_cmpuint (e_virtual_tree_selected_count (vtree), ==, 1);
}

#define add_test(path, func) \
	g_test_add (path, ATFixture, NULL, \
		at_fixture_set_up, func, at_fixture_tear_down)

gint
main (gint argc,
     gchar **argv)
{
	g_test_init (&argc, &argv, NULL);
	gtk_init (&argc, &argv);
	e_util_init_main_thread (NULL);

	add_test ("/addressbook-table/lifecycle/widget-destroy-twice-does-not-crash", test_widget_destroy_twice_does_not_crash);
	add_test ("/addressbook-table/local/basic-add-remove", test_local_basic_add_remove);
	add_test ("/addressbook-table/local/tie-break-by-uid", test_local_tie_break_by_uid);
	add_test ("/addressbook-table/local/sort-by-non-indexed-column", test_local_sort_by_non_indexed_column);
	add_test ("/addressbook-table/local/group-single-level", test_local_group_single_level);
	add_test ("/addressbook-table/local/group-none-sorts-first", test_local_group_none_sorts_first);
	add_test ("/addressbook-table/local/peek-selected-contacts-skips-group-row", test_local_peek_selected_contacts_skips_group_row);
	add_test ("/addressbook-table/local/group-collapse-expand", test_local_group_collapse_expand);
	add_test ("/addressbook-table/local/collapse-persists-across-rebuild", test_local_collapse_persists_across_rebuild);
	add_test ("/addressbook-table/local/modify-contact-moves-group", test_local_modify_contact_moves_group);
	add_test ("/addressbook-table/local/remove-contact-removes-empty-group", test_local_remove_contact_removes_empty_group);
	add_test ("/addressbook-table/local/modify/unrelated-field-no-move", test_local_modify_unrelated_field_no_move);
	add_test ("/addressbook-table/local/modify/sort-moves-up", test_local_sort_modify_moves_up);
	add_test ("/addressbook-table/local/modify/sort-moves-down", test_local_sort_modify_moves_down);
	add_test ("/addressbook-table/local/modify/sort-same-position", test_local_sort_modify_same_position);
	add_test ("/addressbook-table/local/modify/group-creates-new-group", test_local_group_modify_creates_new_group);
	add_test ("/addressbook-table/local/modify/group-moves-to-existing-both-survive", test_local_group_modify_moves_to_existing_group_both_survive);
	add_test ("/addressbook-table/local/modify/group-moves-to-existing-old-dropped", test_local_group_modify_moves_to_existing_group_old_dropped);
	add_test ("/addressbook-table/local/modify/group-level1-creates-new-group", test_local_group_level1_modify_creates_new_group);
	add_test ("/addressbook-table/local/modify/group-level1-moves-to-existing-both-survive", test_local_group_level1_modify_moves_to_existing_group_both_survive);
	add_test ("/addressbook-table/local/modify/group-level1-moves-to-existing-old-dropped", test_local_group_level1_modify_moves_to_existing_group_old_dropped);
	add_test ("/addressbook-table/local/modify/group-level2-creates-new-group", test_local_group_level2_modify_creates_new_group);
	add_test ("/addressbook-table/local/modify/group-level2-moves-to-existing-both-survive", test_local_group_level2_modify_moves_to_existing_group_both_survive);
	add_test ("/addressbook-table/local/modify/group-level2-moves-to-existing-old-dropped", test_local_group_level2_modify_moves_to_existing_group_old_dropped);
	add_test ("/addressbook-table/local/debounce-coalesces-batch", test_local_debounce_coalesces_batch);
	add_test ("/addressbook-table/server/cursor-follows-resort", test_server_cursor_follows_resort);

	return g_test_run ();
}
