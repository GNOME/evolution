/* e-contact-store.c - Contacts store with GtkTreeModel interface.
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Hans Petter Jansson <hpj@novell.com>
 */

#include "evolution-config.h"

#include <string.h>
#include <glib/gi18n-lib.h>

#include "e-contact-store.h"

#define ITER_IS_VALID(contact_store, iter) \
	((iter)->stamp == (contact_store)->priv->stamp)
#define ITER_GET(iter) \
	GPOINTER_TO_INT (iter->user_data)
#define ITER_SET(contact_store, iter, index) \
	G_STMT_START { \
	(iter)->stamp = (contact_store)->priv->stamp; \
	(iter)->user_data = GINT_TO_POINTER (index); \
	} G_STMT_END

struct _EContactStorePrivate {
	gint stamp;
	EBookQuery *query;
	GArray *contact_sources;
};

/* Signals */

enum {
	START_CLIENT_VIEW,
	STOP_CLIENT_VIEW,
	START_UPDATE,
	STOP_UPDATE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void e_contact_store_tree_model_init (GtkTreeModelIface *iface);

G_DEFINE_TYPE_WITH_CODE (EContactStore, e_contact_store, G_TYPE_OBJECT,
	G_ADD_PRIVATE (EContactStore)
	G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL, e_contact_store_tree_model_init))

static GtkTreeModelFlags e_contact_store_get_flags       (GtkTreeModel       *tree_model);
static gint         e_contact_store_get_n_columns   (GtkTreeModel       *tree_model);
static GType        e_contact_store_get_column_type (GtkTreeModel       *tree_model,
						     gint                index);
static gboolean     e_contact_store_get_iter        (GtkTreeModel       *tree_model,
						     GtkTreeIter        *iter,
						     GtkTreePath        *path);
static GtkTreePath *e_contact_store_get_path        (GtkTreeModel       *tree_model,
						     GtkTreeIter        *iter);
static void         e_contact_store_get_value       (GtkTreeModel       *tree_model,
						     GtkTreeIter        *iter,
						     gint                column,
						     GValue             *value);
static gboolean     e_contact_store_iter_next       (GtkTreeModel       *tree_model,
						     GtkTreeIter        *iter);
static gboolean     e_contact_store_iter_children   (GtkTreeModel       *tree_model,
						     GtkTreeIter        *iter,
						     GtkTreeIter        *parent);
static gboolean     e_contact_store_iter_has_child  (GtkTreeModel       *tree_model,
						     GtkTreeIter        *iter);
static gint         e_contact_store_iter_n_children (GtkTreeModel       *tree_model,
						     GtkTreeIter        *iter);
static gboolean     e_contact_store_iter_nth_child  (GtkTreeModel       *tree_model,
						     GtkTreeIter        *iter,
						     GtkTreeIter        *parent,
						     gint                n);
static gboolean     e_contact_store_iter_parent     (GtkTreeModel       *tree_model,
						     GtkTreeIter        *iter,
						     GtkTreeIter        *child);

typedef struct
{
	EBookClient *book_client;

	EBookClientView *client_view;
	GPtrArray *contacts;

	EBookClientView *client_view_pending;
	GPtrArray *contacts_pending;
}
ContactSource;

static void free_contact_ptrarray (GPtrArray *contacts);
static void clear_contact_source  (EContactStore *contact_store, ContactSource *source);
static void stop_view             (EContactStore *contact_store, EBookClientView *view);

static void
contact_store_dispose (GObject *object)
{
	EContactStore *self = E_CONTACT_STORE (object);
	gint ii;

	/* Free sources and cached contacts */
	for (ii = 0; ii < self->priv->contact_sources->len; ii++) {
		ContactSource *source;

		/* clear from back, because clear_contact_source can later access freed memory */
		source = &g_array_index (self->priv->contact_sources, ContactSource, self->priv->contact_sources->len - ii - 1);

		clear_contact_source (E_CONTACT_STORE (object), source);
		free_contact_ptrarray (source->contacts);
		g_object_unref (source->book_client);
	}
	g_array_set_size (self->priv->contact_sources, 0);

	g_clear_pointer (&self->priv->query, e_book_query_unref);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_contact_store_parent_class)->dispose (object);
}

static void
contact_store_finalize (GObject *object)
{
	EContactStore *self = E_CONTACT_STORE (object);

	g_array_free (self->priv->contact_sources, TRUE);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_contact_store_parent_class)->finalize (object);
}

static void
e_contact_store_class_init (EContactStoreClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = contact_store_dispose;
	object_class->finalize = contact_store_finalize;

	signals[START_CLIENT_VIEW] = g_signal_new (
		"start-client-view",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EContactStoreClass, start_client_view),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_BOOK_CLIENT_VIEW);

	signals[STOP_CLIENT_VIEW] = g_signal_new (
		"stop-client-view",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EContactStoreClass, stop_client_view),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_BOOK_CLIENT_VIEW);

	signals[START_UPDATE] = g_signal_new (
		"start-update",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EContactStoreClass, start_update),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_BOOK_CLIENT_VIEW);

	signals[STOP_UPDATE] = g_signal_new (
		"stop-update",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EContactStoreClass, stop_update),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_BOOK_CLIENT_VIEW);
}

static void
e_contact_store_tree_model_init (GtkTreeModelIface *iface)
{
	iface->get_flags = e_contact_store_get_flags;
	iface->get_n_columns = e_contact_store_get_n_columns;
	iface->get_column_type = e_contact_store_get_column_type;
	iface->get_iter = e_contact_store_get_iter;
	iface->get_path = e_contact_store_get_path;
	iface->get_value = e_contact_store_get_value;
	iface->iter_next = e_contact_store_iter_next;
	iface->iter_children = e_contact_store_iter_children;
	iface->iter_has_child = e_contact_store_iter_has_child;
	iface->iter_n_children = e_contact_store_iter_n_children;
	iface->iter_nth_child = e_contact_store_iter_nth_child;
	iface->iter_parent = e_contact_store_iter_parent;
}

static void
e_contact_store_init (EContactStore *contact_store)
{
	GArray *contact_sources;

	contact_sources = g_array_new (FALSE, FALSE, sizeof (ContactSource));

	contact_store->priv = e_contact_store_get_instance_private (contact_store);
	contact_store->priv->stamp = g_random_int ();
	contact_store->priv->contact_sources = contact_sources;
}

/**
 * e_contact_store_new:
 *
 * Creates a new #EContactStore.
 *
 * Returns: A new #EContactStore.
 **/
EContactStore *
e_contact_store_new (void)
{
	return g_object_new (E_TYPE_CONTACT_STORE, NULL);
}

/* ------------------ *
 * Row update helpers *
 * ------------------ */

static void
row_deleted (EContactStore *contact_store,
             gint n)
{
	GtkTreePath *path;

	path = gtk_tree_path_new ();
	gtk_tree_path_append_index (path, n);
	gtk_tree_model_row_deleted (GTK_TREE_MODEL (contact_store), path);
	gtk_tree_path_free (path);
}

static void
row_inserted (EContactStore *contact_store,
              gint n)
{
	GtkTreePath *path;
	GtkTreeIter  iter;

	path = gtk_tree_path_new ();
	gtk_tree_path_append_index (path, n);

	if (gtk_tree_model_get_iter (GTK_TREE_MODEL (contact_store), &iter, path))
		gtk_tree_model_row_inserted (GTK_TREE_MODEL (contact_store), path, &iter);

	gtk_tree_path_free (path);
}

static void
row_changed (EContactStore *contact_store,
             gint n)
{
	GtkTreePath *path;
	GtkTreeIter  iter;

	path = gtk_tree_path_new ();
	gtk_tree_path_append_index (path, n);

	if (gtk_tree_model_get_iter (GTK_TREE_MODEL (contact_store), &iter, path))
		gtk_tree_model_row_changed (GTK_TREE_MODEL (contact_store), path, &iter);

	gtk_tree_path_free (path);
}

/* ---------------------- *
 * Contact source helpers *
 * ---------------------- */

static gint
find_contact_source_by_client (EContactStore *contact_store,
                               EBookClient *book_client)
{
	GArray *array;
	gint i;

	array = contact_store->priv->contact_sources;

	for (i = 0; i < array->len; i++) {
		ContactSource *source;

		source = &g_array_index (array, ContactSource, i);
		if (source->book_client == book_client)
			return i;
	}

	return -1;
}

static gint
find_contact_source_by_view (EContactStore *contact_store,
                             EBookClientView *client_view)
{
	GArray *array;
	gint i;

	array = contact_store->priv->contact_sources;

	for (i = 0; i < array->len; i++) {
		ContactSource *source;

		source = &g_array_index (array, ContactSource, i);
		if (source->client_view == client_view ||
		    source->client_view_pending == client_view)
			return i;
	}

	return -1;
}

static gint
find_contact_source_by_offset (EContactStore *contact_store,
                               gint offset)
{
	GArray *array;
	gint i;

	array = contact_store->priv->contact_sources;

	for (i = 0; i < array->len; i++) {
		ContactSource *source;

		source = &g_array_index (array, ContactSource, i);
		if (source->contacts->len > offset)
			return i;

		offset -= source->contacts->len;
	}

	return -1;
}

static gint
find_contact_source_by_pointer (EContactStore *contact_store,
                                ContactSource *source)
{
	GArray *array;
	gint i;

	array = contact_store->priv->contact_sources;

	i = ((gchar *) source - (gchar *) array->data) / sizeof (ContactSource);

	if (i < 0 || i >= array->len)
		return -1;

	return i;
}

static gint
get_contact_source_offset (EContactStore *contact_store,
                           gint contact_source_index)
{
	GArray *array;
	gint offset = 0;
	gint i;

	array = contact_store->priv->contact_sources;

	g_return_val_if_fail (contact_source_index < array->len, 0);

	for (i = 0; i < contact_source_index; i++) {
		ContactSource *source;

		source = &g_array_index (array, ContactSource, i);
		offset += source->contacts->len;
	}

	return offset;
}

static gint
count_contacts (EContactStore *contact_store)
{
	GArray *array;
	gint count = 0;
	gint i;

	array = contact_store->priv->contact_sources;

	for (i = 0; i < array->len; i++) {
		ContactSource *source;

		source = &g_array_index (array, ContactSource, i);
		count += source->contacts->len;
	}

	return count;
}

static gint
find_contact_by_view_and_uid (EContactStore *contact_store,
                              EBookClientView *find_view,
                              const gchar *find_uid)
{
	GArray *array;
	ContactSource *source;
	GPtrArray *contacts;
	gint source_index;
	gint i;

	g_return_val_if_fail (find_uid != NULL, -1);

	source_index = find_contact_source_by_view (contact_store, find_view);
	if (source_index < 0)
		return -1;

	array = contact_store->priv->contact_sources;
	source = &g_array_index (array, ContactSource, source_index);

	if (find_view == source->client_view)
		contacts = source->contacts;          /* Current view */
	else
		contacts = source->contacts_pending;  /* Pending view */

	for (i = 0; i < contacts->len; i++) {
		EContact    *contact = g_ptr_array_index (contacts, i);
		const gchar *uid = e_contact_get_const (contact, E_CONTACT_UID);

		if (uid && !strcmp (find_uid, uid))
			return i;
	}

	return -1;
}

static GHashTable *
get_contact_hash (EContactStore *contact_store,
                  EBookClientView *find_view)
{
	GArray *array;
	ContactSource *source;
	GPtrArray *contacts;
	gint source_index;
	gint ii;
	GHashTable *hash;

	source_index = find_contact_source_by_view (contact_store, find_view);
	if (source_index < 0)
		return NULL;

	array = contact_store->priv->contact_sources;
	source = &g_array_index (array, ContactSource, source_index);

	if (find_view == source->client_view)
		contacts = source->contacts;          /* Current view */
	else
		contacts = source->contacts_pending;  /* Pending view */

	hash = g_hash_table_new (g_str_hash, g_str_equal);

	for (ii = 0; ii < contacts->len; ii++) {
		EContact *contact = g_ptr_array_index (contacts, ii);
		const gchar *uid = e_contact_get_const (contact, E_CONTACT_UID);

		if (uid)
			g_hash_table_insert (hash, (gpointer) uid, GINT_TO_POINTER (ii));
	}

	return hash;
}

static gint
find_contact_by_uid (EContactStore *contact_store,
                     const gchar *find_uid)
{
	GArray *array;
	gint i;

	array = contact_store->priv->contact_sources;

	for (i = 0; i < array->len; i++) {
		ContactSource *source = &g_array_index (array, ContactSource, i);
		gint           j;

		for (j = 0; j < source->contacts->len; j++) {
			EContact    *contact = g_ptr_array_index (source->contacts, j);
			const gchar *uid = e_contact_get_const (contact, E_CONTACT_UID);

			if (!strcmp (find_uid, uid))
				return get_contact_source_offset (contact_store, i) + j;
		}
	}

	return -1;
}

static EBookClient *
get_book_at_row (EContactStore *contact_store,
                 gint row)
{
	GArray *array;
	ContactSource *source;
	gint source_index;

	source_index = find_contact_source_by_offset (contact_store, row);
	if (source_index < 0)
		return NULL;

	array = contact_store->priv->contact_sources;
	source = &g_array_index (array, ContactSource, source_index);

	return source->book_client;
}

static EContact *
get_contact_at_row (EContactStore *contact_store,
                    gint row)
{
	GArray *array;
	ContactSource *source;
	gint source_index;
	gint offset;

	source_index = find_contact_source_by_offset (contact_store, row);
	if (source_index < 0)
		return NULL;

	array = contact_store->priv->contact_sources;
	source = &g_array_index (array, ContactSource, source_index);
	offset = get_contact_source_offset (contact_store, source_index);
	row -= offset;

	g_return_val_if_fail (row < source->contacts->len, NULL);

	return g_ptr_array_index (source->contacts, row);
}

static gboolean
find_contact_source_details_by_view (EContactStore *contact_store,
                                     EBookClientView *client_view,
                                     ContactSource **contact_source,
                                     gint *offset)
{
	GArray *array;
	gint source_index;

	source_index = find_contact_source_by_view (contact_store, client_view);
	if (source_index < 0)
		return FALSE;

	array = contact_store->priv->contact_sources;
	*contact_source = &g_array_index (array, ContactSource, source_index);
	*offset = get_contact_source_offset (contact_store, source_index);

	return TRUE;
}

/* ------------------------- *
 * EBookView signal handlers *
 * ------------------------- */

static void
view_contacts_added (EContactStore *contact_store,
                     const GSList *contacts,
                     EBookClientView *client_view)
{
	ContactSource *source;
	gint           offset;
	const GSList  *l;

	if (!find_contact_source_details_by_view (contact_store, client_view, &source, &offset)) {
		g_warning ("EContactStore got 'contacts_added' signal from unknown EBookView!");
		return;
	}

	for (l = contacts; l; l = g_slist_next (l)) {
		EContact *contact = l->data;

		g_object_ref (contact);

		if (client_view == source->client_view) {
			/* Current view */
			g_ptr_array_add (source->contacts, contact);
			row_inserted (contact_store, offset + source->contacts->len - 1);
		} else {
			/* Pending view */
			g_ptr_array_add (source->contacts_pending, contact);
		}
	}
}

static void
view_contacts_removed (EContactStore *contact_store,
                       const GSList *uids,
                       EBookClientView *client_view)
{
	ContactSource *source;
	gint           offset;
	const GSList  *l;

	if (!find_contact_source_details_by_view (contact_store, client_view, &source, &offset)) {
		g_warning ("EContactStore got 'contacts_removed' signal from unknown EBookView!");
		return;
	}

	for (l = uids; l; l = g_slist_next (l)) {
		const gchar *uid = l->data;
		gint         n = find_contact_by_view_and_uid (contact_store, client_view, uid);
		EContact    *contact;

		if (n < 0) {
			g_warning ("EContactStore got 'contacts_removed' on unknown contact!");
			continue;
		}

		if (client_view == source->client_view) {
			/* Current view */
			contact = g_ptr_array_index (source->contacts, n);
			g_object_unref (contact);
			g_ptr_array_remove_index (source->contacts, n);
			row_deleted (contact_store, offset + n);
		} else {
			/* Pending view */
			contact = g_ptr_array_index (source->contacts_pending, n);
			g_object_unref (contact);
			g_ptr_array_remove_index (source->contacts_pending, n);
		}
	}
}

static void
view_contacts_modified (EContactStore *contact_store,
                        const GSList *contacts,
                        EBookClientView *client_view)
{
	GPtrArray     *cached_contacts;
	ContactSource *source;
	gint           offset;
	const GSList  *l;

	if (!find_contact_source_details_by_view (contact_store, client_view, &source, &offset)) {
		g_warning ("EContactStore got 'contacts_changed' signal from unknown EBookView!");
		return;
	}

	if (client_view == source->client_view)
		cached_contacts = source->contacts;
	else
		cached_contacts = source->contacts_pending;

	for (l = contacts; l; l = g_slist_next (l)) {
		EContact    *cached_contact;
		EContact    *contact = l->data;
		const gchar *uid = e_contact_get_const (contact, E_CONTACT_UID);
		gint         n = find_contact_by_view_and_uid (contact_store, client_view, uid);

		if (n < 0) {
			g_warning ("EContactStore got change notification on unknown contact!");
			continue;
		}

		cached_contact = g_ptr_array_index (cached_contacts, n);

		/* Update cached contact */
		if (cached_contact != contact) {
			g_object_unref (cached_contact);
			cached_contacts->pdata[n] = g_object_ref (contact);
		}

		/* Emit changes for current view only */
		if (client_view == source->client_view)
			row_changed (contact_store, offset + n);
	}
}

static void
view_complete (EContactStore *contact_store,
               const GError *error,
               EBookClientView *client_view)
{
	ContactSource *source;
	gint           offset;
	gint           i;
	GHashTable *hash;

	if (!find_contact_source_details_by_view (contact_store, client_view, &source, &offset)) {
		g_warning ("EContactStore got 'complete' signal from unknown EBookClientView!");
		return;
	}

	/* If current view finished, do nothing */
	if (client_view == source->client_view) {
		stop_view (contact_store, source->client_view);
		return;
	}

	g_return_if_fail (client_view == source->client_view_pending);

	/* However, if it was a pending view, calculate and emit the differences between that
	 * and the current view, and move the pending view up to current. */

	g_signal_emit (contact_store, signals[START_UPDATE], 0, client_view);

	/* Deletions */
	hash = get_contact_hash (contact_store, source->client_view_pending);
	for (i = 0; i < source->contacts->len; i++) {
		EContact    *old_contact = g_ptr_array_index (source->contacts, i);
		const gchar *old_uid = e_contact_get_const (old_contact, E_CONTACT_UID);

		if (!g_hash_table_contains (hash, old_uid)) {
			/* Contact is not in new view; removed */
			g_object_unref (old_contact);
			g_ptr_array_remove_index (source->contacts, i);
			row_deleted (contact_store, offset + i);
			i--;  /* Stay in place */
		}
	}
	g_hash_table_unref (hash);

	/* Insertions */
	hash = get_contact_hash (contact_store, source->client_view);
	for (i = 0; i < source->contacts_pending->len; i++) {
		EContact    *new_contact = g_ptr_array_index (source->contacts_pending, i);
		const gchar *new_uid = e_contact_get_const (new_contact, E_CONTACT_UID);

		if (!g_hash_table_contains (hash, new_uid)) {
			/* Contact is not in old view; inserted */
			g_ptr_array_add (source->contacts, new_contact);
			row_inserted (contact_store, offset + source->contacts->len - 1);
		} else {
			/* Contact already in old view; drop the new one */
			g_object_unref (new_contact);
		}
	}
	g_hash_table_unref (hash);

	g_signal_emit (contact_store, signals[STOP_UPDATE], 0, client_view);

	/* Move pending view up to current */
	stop_view (contact_store, source->client_view);
	g_object_unref (source->client_view);
	source->client_view = source->client_view_pending;
	source->client_view_pending = NULL;

	/* Free array of pending contacts (members have been either moved or unreffed) */
	g_ptr_array_free (source->contacts_pending, TRUE);
	source->contacts_pending = NULL;
}

/* --------------------- *
 * View/Query management *
 * --------------------- */

static gpointer
contact_store_stop_view_in_thread (gpointer user_data)
{
	EBookClientView *view = user_data;

	g_return_val_if_fail (E_IS_BOOK_CLIENT_VIEW (view), NULL);

	/* this does blocking D-Bus call, thus do it in a dedicated thread */
	e_book_client_view_stop (view, NULL);
	g_object_unref (view);

	return NULL;
}

static void
start_view (EContactStore *contact_store,
            EBookClientView *view)
{
	g_signal_emit (contact_store, signals[START_CLIENT_VIEW], 0, view);

	g_signal_connect_swapped (
		view, "objects-added",
		G_CALLBACK (view_contacts_added), contact_store);
	g_signal_connect_swapped (
		view, "objects-removed",
		G_CALLBACK (view_contacts_removed), contact_store);
	g_signal_connect_swapped (
		view, "objects-modified",
		G_CALLBACK (view_contacts_modified), contact_store);
	g_signal_connect_swapped (
		view, "complete",
		G_CALLBACK (view_complete), contact_store);

	e_book_client_view_start (view, NULL);
}

static void
stop_view (EContactStore *contact_store,
           EBookClientView *view)
{
	GThread *thread;

	thread = g_thread_new (NULL, contact_store_stop_view_in_thread, g_object_ref (view));
	g_thread_unref (thread);

	g_signal_handlers_disconnect_matched (
		view, G_SIGNAL_MATCH_DATA,
		0, 0, NULL, NULL, contact_store);

	g_signal_emit (contact_store, signals[STOP_CLIENT_VIEW], 0, view);
}

static void
clear_contact_ptrarray (GPtrArray *contacts)
{
	gint i;

	for (i = 0; i < contacts->len; i++) {
		EContact *contact = g_ptr_array_index (contacts, i);
		g_object_unref (contact);
	}

	g_ptr_array_set_size (contacts, 0);
}

static void
free_contact_ptrarray (GPtrArray *contacts)
{
	clear_contact_ptrarray (contacts);
	g_ptr_array_free (contacts, TRUE);
}

static void
clear_contact_source (EContactStore *contact_store,
                      ContactSource *source)
{
	gint source_index;
	gint offset;

	source_index = find_contact_source_by_pointer (contact_store, source);
	g_return_if_fail (source_index >= 0);

	offset = get_contact_source_offset (contact_store, source_index);
	g_return_if_fail (offset >= 0);

	/* Inform listeners that contacts went away */

	if (source->contacts && source->contacts->len > 0) {
		GtkTreePath *path = gtk_tree_path_new ();
		gint         i;

		g_signal_emit (contact_store, signals[START_UPDATE], 0, source->client_view);
		gtk_tree_path_append_index (path, source->contacts->len);

		for (i = source->contacts->len - 1; i >= 0; i--) {
			EContact *contact = g_ptr_array_index (source->contacts, i);

			g_object_unref (contact);
			g_ptr_array_remove_index_fast (source->contacts, i);

			gtk_tree_path_prev (path);
			gtk_tree_model_row_deleted (GTK_TREE_MODEL (contact_store), path);
		}

		gtk_tree_path_free (path);
		g_signal_emit (contact_store, signals[STOP_UPDATE], 0, source->client_view);
	}

	/* Free main and pending views, clear cached contacts */

	if (source->client_view) {
		stop_view (contact_store, source->client_view);
		g_object_unref (source->client_view);

		source->client_view = NULL;
	}

	if (source->client_view_pending) {
		stop_view (contact_store, source->client_view_pending);
		g_object_unref (source->client_view_pending);
		free_contact_ptrarray (source->contacts_pending);

		source->client_view_pending = NULL;
		source->contacts_pending = NULL;
	}
}

static void
client_view_ready_cb (GObject *source_object,
                      GAsyncResult *result,
                      gpointer user_data)
{
	EContactStore *contact_store = user_data;
	gint source_idx;
	EBookClient *book_client;
	EBookClientView *client_view = NULL;

	g_return_if_fail (contact_store != NULL);
	g_return_if_fail (source_object != NULL);

	book_client = E_BOOK_CLIENT (source_object);
	g_return_if_fail (book_client != NULL);

	e_book_client_get_view_finish (
		book_client, result, &client_view, NULL);

	source_idx = find_contact_source_by_client (contact_store, book_client);
	if (source_idx >= 0) {
		ContactSource *source;

		source = &g_array_index (contact_store->priv->contact_sources, ContactSource, source_idx);

		if (source->client_view) {
			if (source->client_view_pending) {
				stop_view (contact_store, source->client_view_pending);
				g_object_unref (source->client_view_pending);
				free_contact_ptrarray (source->contacts_pending);
			}

			source->client_view_pending = client_view;

			if (source->client_view_pending) {
				source->contacts_pending = g_ptr_array_new ();
				start_view (contact_store, client_view);
			} else {
				source->contacts_pending = NULL;
			}
		} else {
			source->client_view = client_view;

			if (source->client_view) {
				start_view (contact_store, client_view);
			}
		}
	}

	g_object_unref (contact_store);
}

static void
query_contact_source (EContactStore *contact_store,
                      ContactSource *source)
{
	gchar *query_str;

	g_return_if_fail (source->book_client != NULL);

	if (!contact_store->priv->query) {
		clear_contact_source (contact_store, source);
		return;
	}

	if (source->client_view) {
		if (source->client_view_pending) {
			stop_view (contact_store, source->client_view_pending);
			g_object_unref (source->client_view_pending);
			free_contact_ptrarray (source->contacts_pending);
			source->client_view_pending = NULL;
			source->contacts_pending = NULL;
		}
	}

	query_str = e_book_query_to_string (contact_store->priv->query);
	e_book_client_get_view (source->book_client, query_str, NULL, client_view_ready_cb, g_object_ref (contact_store));
	g_free (query_str);
}

/* ----------------- *
 * EContactStore API *
 * ----------------- */

/**
 * e_contact_store_get_client:
 * @contact_store: an #EContactStore
 * @iter: a #GtkTreeIter from @contact_store
 *
 * Gets the #EBookClient that provided the contact at @iter.
 *
 * Returns: An #EBookClient.
 *
 * Since: 3.2
 **/
EBookClient *
e_contact_store_get_client (EContactStore *contact_store,
                            GtkTreeIter *iter)
{
	gint index;

	g_return_val_if_fail (E_IS_CONTACT_STORE (contact_store), NULL);
	g_return_val_if_fail (ITER_IS_VALID (contact_store, iter), NULL);

	index = ITER_GET (iter);

	return get_book_at_row (contact_store, index);
}

/**
 * e_contact_store_get_contact:
 * @contact_store: an #EContactStore
 * @iter: a #GtkTreeIter from @contact_store
 *
 * Gets the #EContact at @iter.
 *
 * Returns: An #EContact.
 **/
EContact *
e_contact_store_get_contact (EContactStore *contact_store,
                             GtkTreeIter *iter)
{
	gint index;

	g_return_val_if_fail (E_IS_CONTACT_STORE (contact_store), NULL);
	g_return_val_if_fail (ITER_IS_VALID (contact_store, iter), NULL);

	index = ITER_GET (iter);

	return get_contact_at_row (contact_store, index);
}

/**
 * e_contact_store_find_contact:
 * @contact_store: an #EContactStore
 * @uid: a unique contact identifier
 * @iter: a destination #GtkTreeIter to set
 *
 * Sets @iter to point to the contact row matching @uid.
 *
 * Returns: %TRUE if the contact was found, and @iter was set. %FALSE otherwise.
 **/
gboolean
e_contact_store_find_contact (EContactStore *contact_store,
                              const gchar *uid,
                              GtkTreeIter *iter)
{
	gint index;

	g_return_val_if_fail (E_IS_CONTACT_STORE (contact_store), FALSE);
	g_return_val_if_fail (uid != NULL, FALSE);

	index = find_contact_by_uid (contact_store, uid);
	if (index < 0)
		return FALSE;

	ITER_SET (contact_store, iter, index);
	return TRUE;
}

/**
 * e_contact_store_get_clients:
 * @contact_store: an #EContactStore
 *
 * Gets the list of book clients that provide contacts for @contact_store.
 *
 * Returns: A #GSList of pointers to #EBookClient. The caller owns the list,
 * but not the book clients.
 *
 * Since: 3.2
 **/
GSList *
e_contact_store_get_clients (EContactStore *contact_store)
{
	GArray *array;
	GSList *client_list = NULL;
	gint i;

	g_return_val_if_fail (E_IS_CONTACT_STORE (contact_store), NULL);

	array = contact_store->priv->contact_sources;

	for (i = 0; i < array->len; i++) {
		ContactSource *source;

		source = &g_array_index (array, ContactSource, i);
		client_list = g_slist_prepend (client_list, source->book_client);
	}

	return client_list;
}

/**
 * e_contact_store_add_client:
 * @contact_store: an #EContactStore
 * @book_client: an #EBookClient
 *
 * Adds @book_client to the list of clients that provide contacts for
 * @contact_store.  The @contact_store adds a reference to @book_client,
 * if added.
 *
 * Since: 3.2
 **/
void
e_contact_store_add_client (EContactStore *contact_store,
                            EBookClient *book_client)
{
	GArray *array;
	ContactSource source;
	ContactSource *indexed_source;

	g_return_if_fail (E_IS_CONTACT_STORE (contact_store));
	g_return_if_fail (E_IS_BOOK_CLIENT (book_client));

	/* Return silently if we already have this EBookClient. */
	if (find_contact_source_by_client (contact_store, book_client) >= 0)
		return;

	array = contact_store->priv->contact_sources;

	memset (&source, 0, sizeof (ContactSource));
	source.book_client = g_object_ref (book_client);
	source.contacts = g_ptr_array_new ();
	g_array_append_val (array, source);

	indexed_source = &g_array_index (array, ContactSource, array->len - 1);

	query_contact_source (contact_store, indexed_source);
}

/**
 * e_contact_store_remove_client:
 * @contact_store: an #EContactStore
 * @book_client: an #EBookClient
 *
 * Removes @book_client from the list of clients that provide contacts for
 * @contact_store.
 *
 * Returns: whether @book_client was found and removed
 *
 * Since: 3.2
 **/
gboolean
e_contact_store_remove_client (EContactStore *contact_store,
                               EBookClient *book_client)
{
	GArray *array;
	ContactSource *source;
	gint source_index;

	g_return_val_if_fail (E_IS_CONTACT_STORE (contact_store), FALSE);
	g_return_val_if_fail (E_IS_BOOK_CLIENT (book_client), FALSE);

	source_index = find_contact_source_by_client (contact_store, book_client);
	if (source_index < 0)
		return FALSE;

	array = contact_store->priv->contact_sources;

	source = &g_array_index (array, ContactSource, source_index);
	clear_contact_source (contact_store, source);
	free_contact_ptrarray (source->contacts);
	g_object_unref (book_client);

	g_array_remove_index (array, source_index);  /* Preserve order */

	return TRUE;
}

/**
 * e_contact_store_set_query:
 * @contact_store: an #EContactStore
 * @book_query: an #EBookQuery
 *
 * Sets @book_query to be the query used to fetch contacts from the books
 * assigned to @contact_store.
 **/
void
e_contact_store_set_query (EContactStore *contact_store,
                           EBookQuery *book_query)
{
	GArray *array;
	gint i;

	g_return_if_fail (E_IS_CONTACT_STORE (contact_store));

	if (book_query == contact_store->priv->query)
		return;

	if (contact_store->priv->query)
		e_book_query_unref (contact_store->priv->query);

	contact_store->priv->query = book_query;
	if (book_query)
		e_book_query_ref (book_query);

	/* Query books */
	array = contact_store->priv->contact_sources;
	for (i = 0; i < array->len; i++) {
		ContactSource *contact_source;

		contact_source = &g_array_index (array, ContactSource, i);
		query_contact_source (contact_store, contact_source);
	}
}

/**
 * e_contact_store_peek_query:
 * @contact_store: an #EContactStore
 *
 * Gets the query that's being used to fetch contacts from the books
 * assigned to @contact_store.
 *
 * Returns: The #EBookQuery being used.
 **/
EBookQuery *
e_contact_store_peek_query (EContactStore *contact_store)
{
	g_return_val_if_fail (E_IS_CONTACT_STORE (contact_store), NULL);

	return contact_store->priv->query;
}

/* ---------------- *
 * GtkTreeModel API *
 * ---------------- */

static GtkTreeModelFlags
e_contact_store_get_flags (GtkTreeModel *tree_model)
{
	g_return_val_if_fail (E_IS_CONTACT_STORE (tree_model), 0);

	return GTK_TREE_MODEL_LIST_ONLY;
}

static gint
e_contact_store_get_n_columns (GtkTreeModel *tree_model)
{
	g_return_val_if_fail (E_IS_CONTACT_STORE (tree_model), 0);

	return E_CONTACT_FIELD_LAST;
}

static GType
get_column_type (EContactStore *contact_store,
                 gint column)
{
	const gchar  *field_name;
	GObjectClass *contact_class;
	GParamSpec   *param_spec;
	GType         value_type;

	/* Silently suppress requests for columns lower than the first EContactField.
	 * GtkTreeView automatically queries the type of all columns up to the maximum
	 * provided, and we have to return a valid value type, so let it be a generic
	 * pointer. */
	if (column < E_CONTACT_FIELD_FIRST) {
		return G_TYPE_POINTER;
	}

	field_name = e_contact_field_name (column);
	contact_class = g_type_class_ref (E_TYPE_CONTACT);
	param_spec = g_object_class_find_property (contact_class, field_name);
	value_type = G_PARAM_SPEC_VALUE_TYPE (param_spec);
	g_type_class_unref (contact_class);

	return value_type;
}

static GType
e_contact_store_get_column_type (GtkTreeModel *tree_model,
                                 gint index)
{
	EContactStore *contact_store = E_CONTACT_STORE (tree_model);

	g_return_val_if_fail (E_IS_CONTACT_STORE (tree_model), G_TYPE_INVALID);
	g_return_val_if_fail (index >= 0 && index < E_CONTACT_FIELD_LAST, G_TYPE_INVALID);

	return get_column_type (contact_store, index);
}

static gboolean
e_contact_store_get_iter (GtkTreeModel *tree_model,
                          GtkTreeIter *iter,
                          GtkTreePath *path)
{
	EContactStore *contact_store = E_CONTACT_STORE (tree_model);
	gint           index;

	g_return_val_if_fail (E_IS_CONTACT_STORE (tree_model), FALSE);
	g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

	index = gtk_tree_path_get_indices (path)[0];
	if (index >= count_contacts (contact_store))
		return FALSE;

	ITER_SET (contact_store, iter, index);
	return TRUE;
}

static GtkTreePath *
e_contact_store_get_path (GtkTreeModel *tree_model,
                          GtkTreeIter *iter)
{
	EContactStore *contact_store = E_CONTACT_STORE (tree_model);
	GtkTreePath   *path;
	gint           index;

	g_return_val_if_fail (E_IS_CONTACT_STORE (tree_model), NULL);
	g_return_val_if_fail (ITER_IS_VALID (contact_store, iter), NULL);

	index = ITER_GET (iter);
	path = gtk_tree_path_new ();
	gtk_tree_path_append_index (path, index);

	return path;
}

static gboolean
e_contact_store_iter_next (GtkTreeModel *tree_model,
                           GtkTreeIter *iter)
{
	EContactStore *contact_store = E_CONTACT_STORE (tree_model);
	gint           index;

	g_return_val_if_fail (E_IS_CONTACT_STORE (tree_model), FALSE);
	g_return_val_if_fail (ITER_IS_VALID (contact_store, iter), FALSE);

	index = ITER_GET (iter);

	if (index + 1 < count_contacts (contact_store)) {
		ITER_SET (contact_store, iter, index + 1);
		return TRUE;
	}

	return FALSE;
}

static gboolean
e_contact_store_iter_children (GtkTreeModel *tree_model,
                               GtkTreeIter *iter,
                               GtkTreeIter *parent)
{
	EContactStore *contact_store = E_CONTACT_STORE (tree_model);

	g_return_val_if_fail (E_IS_CONTACT_STORE (tree_model), FALSE);

	/* This is a list, nodes have no children. */
	if (parent)
		return FALSE;

	/* But if parent == NULL we return the list itself as children of the root. */
	if (count_contacts (contact_store) <= 0)
		return FALSE;

	ITER_SET (contact_store, iter, 0);
	return TRUE;
}

static gboolean
e_contact_store_iter_has_child (GtkTreeModel *tree_model,
                                GtkTreeIter *iter)
{
	g_return_val_if_fail (E_IS_CONTACT_STORE (tree_model), FALSE);

	if (iter == NULL)
		return TRUE;

	return FALSE;
}

static gint
e_contact_store_iter_n_children (GtkTreeModel *tree_model,
                                 GtkTreeIter *iter)
{
	EContactStore *contact_store = E_CONTACT_STORE (tree_model);

	g_return_val_if_fail (E_IS_CONTACT_STORE (tree_model), -1);

	if (iter == NULL)
		return count_contacts (contact_store);

	g_return_val_if_fail (ITER_IS_VALID (contact_store, iter), -1);
	return 0;
}

static gboolean
e_contact_store_iter_nth_child (GtkTreeModel *tree_model,
                                GtkTreeIter *iter,
                                GtkTreeIter *parent,
                                gint n)
{
	EContactStore *contact_store = E_CONTACT_STORE (tree_model);

	g_return_val_if_fail (E_IS_CONTACT_STORE (tree_model), FALSE);

	if (parent)
		return FALSE;

	if (n < count_contacts (contact_store)) {
		ITER_SET (contact_store, iter, n);
		return TRUE;
	}

	return FALSE;
}

static gboolean
e_contact_store_iter_parent (GtkTreeModel *tree_model,
                             GtkTreeIter *iter,
                             GtkTreeIter *child)
{
	return FALSE;
}

static void
e_contact_store_get_value (GtkTreeModel *tree_model,
                           GtkTreeIter *iter,
                           gint column,
                           GValue *value)
{
	EContactStore *contact_store = E_CONTACT_STORE (tree_model);
	EContact      *contact;
	const gchar   *field_name;
	gint           row;

	g_return_if_fail (E_IS_CONTACT_STORE (tree_model));
	g_return_if_fail (column < E_CONTACT_FIELD_LAST);
	g_return_if_fail (ITER_IS_VALID (contact_store, iter));

	g_value_init (value, get_column_type (contact_store, column));

	row = ITER_GET (iter);
	contact = get_contact_at_row (contact_store, row);
	if (!contact || column < E_CONTACT_FIELD_FIRST)
		return;

	field_name = e_contact_field_name (column);
	g_object_get_property (G_OBJECT (contact), field_name, value);
}
