/*
 * Copyright (C) 2017 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION: e-accounts-window
 * @include: e-util/e-util.h
 * @short_description: Accounts Window
 *
 * #EAccountsWindow shows all configured accounts in evolution-data-server
 * and allows also create new, modify or remove existing accounts as well.
 * It's extensible through #EExtension, thus it can be taught how to work
 * with particular account types as well.
 **/

#include "evolution-config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include <libedataserver/libedataserver.h>
#include <libedataserverui/libedataserverui.h>

#include "e-alert-dialog.h"
#include "e-dialog-widgets.h"
#include "e-misc-utils.h"

#include "e-accounts-window.h"

#define ADD_POPUP_KEY_KIND	"add-popup-key-kind"

#define UNKNOWN_SORT_HINT	-1
#define COLLECTIONS_SORT_HINT	0
#define MAIL_ACCOUNTS_SORT_HINT	1
#define ADDRESS_BOOKS_SORT_HINT	2
#define CALENDARS_SORT_HINT	3
#define MEMO_LISTS_SORT_HINT	4
#define TASK_LISTS_SORT_HINT	5

struct _EAccountsWindowPrivate {
	ESourceRegistry *registry;

	GtkWidget *notebook;		/* not referenced */
	GtkWidget *button_box;		/* not referenced */
	GtkWidget *tree_view;		/* not referenced */
	GtkWidget *add_button;		/* not referenced */
	GtkWidget *edit_button;		/* not referenced */
	GtkWidget *delete_button;	/* not referenced */
	GtkWidget *refresh_backend_button;	/* not referenced */

	GHashTable *references; /* gchar *UID ~> GtkTreeRowReference * */
	gchar *select_source_uid; /* Which source to select, or NULL */

	gulong source_enabled_handler_id;
	gulong source_disabled_handler_id;
	gulong source_added_handler_id;
	gulong source_removed_handler_id;
	gulong source_changed_handler_id;
};

enum {
	PROP_0,
	PROP_REGISTRY
};

enum {
	GET_EDITING_FLAGS,
	ADD_SOURCE,
	EDIT_SOURCE,
	DELETE_SOURCE,
	ENABLED_TOGGLED,
	POPULATE_ADD_POPUP,
	SELECTION_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_CODE (EAccountsWindow, e_accounts_window, GTK_TYPE_WINDOW,
	G_ADD_PRIVATE (EAccountsWindow)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

enum {
	COLUMN_BOOL_ENABLED,
	COLUMN_BOOL_ENABLED_VISIBLE,
	COLUMN_STRING_DISPLAY_NAME,
	COLUMN_STRING_ICON_NAME,
	COLUMN_BOOL_ICON_VISIBLE,
	COLUMN_RGBA_COLOR,
	COLUMN_BOOL_COLOR_VISIBLE,
	COLUMN_STRING_TYPE,
	COLUMN_OBJECT_SOURCE,
	COLUMN_INT_SORT_HINT,
	COLUMN_UINT_EDITING_FLAGS,
	COLUMN_BOOL_DELETE_WHEN_NO_CHILDREN,
	N_COLUMNS
};

static gint
accounts_window_get_sort_hint_for_source (ESource *source)
{
	g_return_val_if_fail (E_IS_SOURCE (source), UNKNOWN_SORT_HINT);

	if (e_source_has_extension (source, E_SOURCE_EXTENSION_COLLECTION))
		return COLLECTIONS_SORT_HINT;

	if (e_source_has_extension (source, E_SOURCE_EXTENSION_MAIL_ACCOUNT))
		return MAIL_ACCOUNTS_SORT_HINT;

	if (e_source_has_extension (source, E_SOURCE_EXTENSION_ADDRESS_BOOK))
		return ADDRESS_BOOKS_SORT_HINT;

	if (e_source_has_extension (source, E_SOURCE_EXTENSION_CALENDAR))
		return CALENDARS_SORT_HINT;

	if (e_source_has_extension (source, E_SOURCE_EXTENSION_MEMO_LIST))
		return MEMO_LISTS_SORT_HINT;

	if (e_source_has_extension (source, E_SOURCE_EXTENSION_TASK_LIST))
		return TASK_LISTS_SORT_HINT;

	return UNKNOWN_SORT_HINT;
}

static gboolean
accounts_window_emit_get_editing_flags (EAccountsWindow *accounts_window,
					ESource *source,
					guint *out_flags)
{
	gboolean handled = FALSE;

	g_signal_emit (accounts_window, signals[GET_EDITING_FLAGS], 0, source, out_flags, &handled);

	return handled;
}

static void
accounts_window_emit_edit_source (EAccountsWindow *accounts_window)
{
	ESource *source;

	source = e_accounts_window_ref_selected_source (accounts_window);

	if (source) {
		gboolean handled = FALSE;

		g_signal_emit (accounts_window, signals[EDIT_SOURCE], 0, source, &handled);

		g_object_unref (source);
	}
}

static void
accounts_window_emit_delete_source (EAccountsWindow *accounts_window)
{
	ESource *source;

	source = e_accounts_window_ref_selected_source (accounts_window);

	if (source) {
		gboolean handled = FALSE;

		g_signal_emit (accounts_window, signals[DELETE_SOURCE], 0, source, &handled);

		g_object_unref (source);
	}
}

static void
accounts_window_refresh_backend_done_cb (GObject *source_object,
					 GAsyncResult *result,
					 gpointer user_data)
{
	GError *error = NULL;

	if (!e_source_registry_refresh_backend_finish (E_SOURCE_REGISTRY (source_object), result, &error)) {
		g_warning ("%s: Failed to refresh backend: %s", G_STRFUNC, error ? error->message : "Unknown error");
	}

	g_clear_error (&error);
}

static void
accounts_window_refresh_backend_cb (EAccountsWindow *accounts_window)
{
	ESource *source;

	source = e_accounts_window_ref_selected_source (accounts_window);

	if (source) {
		ESourceRegistry *registry;

		registry = e_accounts_window_get_registry (accounts_window);

		e_source_registry_refresh_backend (registry, e_source_get_uid (source), NULL,
			accounts_window_refresh_backend_done_cb, accounts_window);

		g_object_unref (source);
	}
}

static gboolean
accounts_window_find_source_uid_iter (EAccountsWindow *accounts_window,
				      const gchar *uid,
				      GtkTreeIter *out_iter,
				      GtkTreeModel **out_model)
{
	GtkTreeRowReference *reference;
	GtkTreePath *path;
	GtkTreeModel *model;
	gboolean valid;

	g_return_val_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window), FALSE);
	g_return_val_if_fail (uid != NULL, FALSE);
	g_return_val_if_fail (out_iter != NULL, FALSE);

	reference = g_hash_table_lookup (accounts_window->priv->references, uid);
	if (!reference ||
	    !gtk_tree_row_reference_valid (reference)) {
		g_hash_table_remove (accounts_window->priv->references, uid);

		return FALSE;
	}

	path = gtk_tree_row_reference_get_path (reference);
	if (!path)
		return FALSE;

	model = gtk_tree_row_reference_get_model (reference);
	valid = gtk_tree_model_get_iter (model, out_iter, path);

	gtk_tree_path_free (path);

	if (out_model)
		*out_model = model;

	return valid;
}

static gboolean
accounts_window_find_source_iter (EAccountsWindow *accounts_window,
				  ESource *source,
				  GtkTreeIter *out_iter,
				  GtkTreeModel **out_model)
{
	g_return_val_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window), FALSE);
	g_return_val_if_fail (E_IS_SOURCE (source), FALSE);
	g_return_val_if_fail (out_iter != NULL, FALSE);

	return accounts_window_find_source_uid_iter (accounts_window, e_source_get_uid (source), out_iter, out_model);
}

static gboolean
accounts_window_find_child_with_sort_hint (EAccountsWindow *accounts_window,
					   GtkTreeStore *tree_store,
					   GtkTreeIter *parent,
					   gint in_sort_hint,
					   GtkTreeIter *out_iter)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gint row_sort_hint = -1;

	g_return_val_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window), FALSE);
	g_return_val_if_fail (GTK_IS_TREE_STORE (tree_store), FALSE);
	g_return_val_if_fail (out_iter != NULL, FALSE);

	model = GTK_TREE_MODEL (tree_store);

	if (!gtk_tree_model_iter_nth_child (model, &iter, parent, 0))
		return FALSE;

	do {
		gtk_tree_model_get (model, &iter, COLUMN_INT_SORT_HINT, &row_sort_hint, -1);

		if (in_sort_hint == row_sort_hint) {
			*out_iter = iter;

			return TRUE;
		}
	} while (gtk_tree_model_iter_next (model, &iter));

	return FALSE;
}

static gboolean
accounts_window_find_child_with_source_uid (EAccountsWindow *accounts_window,
					    GtkTreeStore *tree_store,
					    GtkTreeIter *parent,
					    const gchar *source_uid,
					    GtkTreeIter *out_iter)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_return_val_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window), FALSE);
	g_return_val_if_fail (GTK_IS_TREE_STORE (tree_store), FALSE);
	g_return_val_if_fail (source_uid != NULL, FALSE);
	g_return_val_if_fail (out_iter != NULL, FALSE);

	model = GTK_TREE_MODEL (tree_store);

	if (!gtk_tree_model_iter_nth_child (model, &iter, parent, 0))
		return FALSE;

	do {
		ESource *source = NULL;

		gtk_tree_model_get (model, &iter, COLUMN_OBJECT_SOURCE, &source, -1);

		if (source && g_strcmp0 (source_uid, e_source_get_uid (source)) == 0) {
			g_clear_object (&source);

			*out_iter = iter;

			return TRUE;
		}

		g_clear_object (&source);
	} while (gtk_tree_model_iter_next (model, &iter));

	return FALSE;
}

static void
accounts_window_fill_row_virtual (EAccountsWindow *accounts_window,
				  GtkTreeStore *tree_store,
				  GtkTreeIter *iter,
				  const gchar *display_name,
				  const gchar *icon_name,
				  gint sort_hint)
{
	g_return_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window));
	g_return_if_fail (GTK_IS_TREE_STORE (tree_store));
	g_return_if_fail (iter != NULL);
	g_return_if_fail (display_name != NULL);

	gtk_tree_store_set (tree_store, iter,
		COLUMN_BOOL_ENABLED_VISIBLE, FALSE,
		COLUMN_STRING_DISPLAY_NAME, display_name,
		COLUMN_STRING_ICON_NAME, icon_name,
		COLUMN_BOOL_ICON_VISIBLE, icon_name != NULL,
		COLUMN_INT_SORT_HINT, sort_hint,
		COLUMN_UINT_EDITING_FLAGS, E_SOURCE_EDITING_FLAG_NONE,
		COLUMN_BOOL_DELETE_WHEN_NO_CHILDREN, TRUE,
		-1);
}

static void
accounts_window_fill_row_with_source (EAccountsWindow *accounts_window,
				      GtkTreeStore *tree_store,
				      GtkTreeIter *iter,
				      ESource *source,
				      const GSList *mail_account_slaves,
				      gboolean can_show_enabled)
{
	GtkTreePath *path;
	gpointer extension;
	gchar *use_type = NULL;
	const gchar *icon_name = NULL;
	GdkRGBA rgba;
	gboolean rgba_set = FALSE;
	guint editing_flags = E_SOURCE_EDITING_FLAG_NONE;

	g_return_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window));
	g_return_if_fail (GTK_IS_TREE_STORE (tree_store));
	g_return_if_fail (iter != NULL);
	g_return_if_fail (E_IS_SOURCE (source));

	if (e_source_has_extension (source, E_SOURCE_EXTENSION_COLLECTION)) {
		gchar *backend_name;

		extension = e_source_get_extension (source, E_SOURCE_EXTENSION_COLLECTION);
		backend_name = e_source_backend_dup_backend_name (extension);

		if (e_source_has_extension (source, E_SOURCE_EXTENSION_GOA)) {
			use_type = g_strconcat ("GOA:", backend_name, NULL);
			icon_name = "goa-panel";
		} else if (e_source_has_extension (source, E_SOURCE_EXTENSION_UOA)) {
			use_type = g_strconcat ("UOA:", backend_name, NULL);
			icon_name = "credentials-preferences";
		} else if (g_strcmp0 (backend_name, "none") != 0) {
			use_type = backend_name;
			backend_name = NULL;
		}

		g_free (backend_name);
	} else if (e_source_has_extension (source, E_SOURCE_EXTENSION_MAIL_ACCOUNT)) {
		gchar *receive_backend_name, *transport_backend_name = NULL;
		gchar *identity_uid, *transport_uid = NULL;
		GSList *link;

		extension = e_source_get_extension (source, E_SOURCE_EXTENSION_MAIL_ACCOUNT);
		receive_backend_name = e_source_backend_dup_backend_name (extension);
		identity_uid = e_source_mail_account_dup_identity_uid (extension);
		if (identity_uid) {
			for (link = (GSList *) mail_account_slaves; link; link = g_slist_next (link)) {
				ESource *subsource = link->data;

				if (g_strcmp0 (e_source_get_uid (subsource), identity_uid) == 0) {
					if (e_source_has_extension (subsource, E_SOURCE_EXTENSION_MAIL_SUBMISSION)) {
						extension = e_source_get_extension (subsource, E_SOURCE_EXTENSION_MAIL_SUBMISSION);
						transport_uid = e_source_mail_submission_dup_transport_uid (extension);
					}
					break;
				}
			}
		}

		if (transport_uid) {
			for (link = (GSList *) mail_account_slaves; link; link = g_slist_next (link)) {
				ESource *subsource = link->data;

				if (g_strcmp0 (e_source_get_uid (subsource), transport_uid) == 0) {
					if (e_source_has_extension (subsource, E_SOURCE_EXTENSION_MAIL_TRANSPORT)) {
						extension = e_source_get_extension (subsource, E_SOURCE_EXTENSION_MAIL_TRANSPORT);
						transport_backend_name = e_source_backend_dup_backend_name (extension);

					}
					break;
				}
			}
		}

		if ((receive_backend_name && !*receive_backend_name) ||
		    g_strcmp0 (receive_backend_name, "none") == 0) {
			g_free (receive_backend_name);
			receive_backend_name = NULL;
		}

		if ((transport_backend_name && !*transport_backend_name) ||
		    g_strcmp0 (transport_backend_name, "none") == 0) {
			g_free (transport_backend_name);
			transport_backend_name = NULL;
		}

		if (g_strcmp0 (receive_backend_name, transport_backend_name) == 0) {
			g_free (transport_backend_name);
			transport_backend_name = NULL;
		}

		if (receive_backend_name && transport_backend_name) {
			use_type = g_strconcat (receive_backend_name, "+", transport_backend_name, NULL);
		} else if (receive_backend_name) {
			use_type = receive_backend_name;
			receive_backend_name = NULL;
		} else {
			use_type = transport_backend_name;
			transport_backend_name = NULL;
		}

		g_free (receive_backend_name);
		g_free (transport_backend_name);
		g_free (identity_uid);
		g_free (transport_uid);
	} else if (e_source_has_extension (source, E_SOURCE_EXTENSION_ADDRESS_BOOK)) {
		extension = e_source_get_extension (source, E_SOURCE_EXTENSION_ADDRESS_BOOK);
		use_type = e_source_backend_dup_backend_name (extension);
	} else {
		extension = NULL;

		if (e_source_has_extension (source, E_SOURCE_EXTENSION_CALENDAR))
			extension = e_source_get_extension (source, E_SOURCE_EXTENSION_CALENDAR);
		else if (e_source_has_extension (source, E_SOURCE_EXTENSION_MEMO_LIST))
			extension = e_source_get_extension (source, E_SOURCE_EXTENSION_MEMO_LIST);
		else if (e_source_has_extension (source, E_SOURCE_EXTENSION_TASK_LIST))
			extension = e_source_get_extension (source, E_SOURCE_EXTENSION_TASK_LIST);

		if (extension) {
			gchar *color;

			use_type = e_source_backend_dup_backend_name (extension);
			color = e_source_selectable_dup_color (extension);
			rgba_set = color && gdk_rgba_parse (&rgba, color);

			g_free (color);
		}
	}

	accounts_window_emit_get_editing_flags (accounts_window, source, &editing_flags);

	if ((editing_flags & E_SOURCE_EDITING_FLAG_CAN_EDIT) != 0 &&
	    !e_source_get_writable (source))
		editing_flags = editing_flags & ~E_SOURCE_EDITING_FLAG_CAN_EDIT;

	if ((editing_flags & E_SOURCE_EDITING_FLAG_CAN_DELETE) != 0 &&
	    !e_source_get_removable (source))
		editing_flags = editing_flags & ~E_SOURCE_EDITING_FLAG_CAN_DELETE;

	gtk_tree_store_set (tree_store, iter,
		COLUMN_BOOL_ENABLED, e_source_get_enabled (source),
		COLUMN_BOOL_ENABLED_VISIBLE, can_show_enabled && (editing_flags & E_SOURCE_EDITING_FLAG_CAN_ENABLE) != 0,
		COLUMN_STRING_DISPLAY_NAME, e_source_get_display_name (source),
		COLUMN_STRING_ICON_NAME, icon_name,
		COLUMN_BOOL_ICON_VISIBLE, icon_name != NULL,
		COLUMN_RGBA_COLOR, rgba_set ? &rgba : NULL,
		COLUMN_BOOL_COLOR_VISIBLE, rgba_set,
		COLUMN_STRING_TYPE, use_type,
		COLUMN_OBJECT_SOURCE, source,
		COLUMN_INT_SORT_HINT, accounts_window_get_sort_hint_for_source (source),
		COLUMN_UINT_EDITING_FLAGS, editing_flags,
		COLUMN_BOOL_DELETE_WHEN_NO_CHILDREN, !can_show_enabled,
		-1);

	g_free (use_type);

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (tree_store), iter);
	g_hash_table_insert (accounts_window->priv->references, e_source_dup_uid (source),
		gtk_tree_row_reference_new (GTK_TREE_MODEL (tree_store), path));
	gtk_tree_path_free (path);
}

static void
accounts_window_fill_children (EAccountsWindow *accounts_window,
			       GtkTreeStore *tree_store,
			       GtkTreeIter *root,
			       gboolean is_managed_collection,
			       gboolean lookup_subroot,
			       const GSList *children)
{
	GtkTreeIter mails_iter, books_iter, calendars_iter, memos_iter, tasks_iter;
	gboolean mails_iter_set = FALSE, books_iter_set = FALSE, calendars_iter_set = FALSE, memos_iter_set = FALSE, tasks_iter_set = FALSE;
	GSList *link;

	g_return_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window));
	g_return_if_fail (GTK_IS_TREE_STORE (tree_store));
	g_return_if_fail (root != NULL);

	for (link = (GSList *) children; link; link = g_slist_next (link)) {
		ESource *source = link->data;
		const GSList *mail_account_slaves = NULL;
		const gchar *subroot_display_name;
		const gchar *subroot_icon_name;
		gint subroot_sort_hint;
		gboolean *subroot_set;
		GtkTreeIter iter, *subroot;

		if (accounts_window_get_sort_hint_for_source (source) == UNKNOWN_SORT_HINT)
			continue;

		if (e_source_has_extension (source, E_SOURCE_EXTENSION_MAIL_ACCOUNT)) {
			subroot_display_name = _("Mail Accounts");
			subroot_icon_name = "evolution-mail";
			subroot_sort_hint = MAIL_ACCOUNTS_SORT_HINT;
			subroot_set = &mails_iter_set;
			subroot = &mails_iter;
			mail_account_slaves = children;
		} else if (e_source_has_extension (source, E_SOURCE_EXTENSION_ADDRESS_BOOK)) {
			subroot_display_name = _("Address Books");
			subroot_icon_name = "x-office-address-book";
			subroot_sort_hint = ADDRESS_BOOKS_SORT_HINT;
			subroot_set = &books_iter_set;
			subroot = &books_iter;
		} else if (e_source_has_extension (source, E_SOURCE_EXTENSION_CALENDAR)) {
			subroot_display_name = _("Calendars");
			subroot_icon_name = "x-office-calendar";
			subroot_sort_hint = CALENDARS_SORT_HINT;
			subroot_set = &calendars_iter_set;
			subroot = &calendars_iter;
		} else if (e_source_has_extension (source, E_SOURCE_EXTENSION_MEMO_LIST)) {
			subroot_display_name = _("Memo Lists");
			subroot_icon_name = "evolution-memos";
			subroot_sort_hint = MEMO_LISTS_SORT_HINT;
			subroot_set = &memos_iter_set;
			subroot = &memos_iter;
		} else if (e_source_has_extension (source, E_SOURCE_EXTENSION_TASK_LIST)) {
			subroot_display_name = _("Task Lists");
			subroot_icon_name = "evolution-tasks";
			subroot_sort_hint = TASK_LISTS_SORT_HINT;
			subroot_set = &tasks_iter_set;
			subroot = &tasks_iter;
		} else {
			continue;
		}

		if (!*subroot_set && lookup_subroot)
			*subroot_set = accounts_window_find_child_with_sort_hint (accounts_window, tree_store, root, subroot_sort_hint, subroot);

		if (!*subroot_set) {
			*subroot_set = TRUE;

			gtk_tree_store_append (tree_store, subroot, root);
			accounts_window_fill_row_virtual (accounts_window, tree_store, subroot,
				subroot_display_name, subroot_icon_name, subroot_sort_hint);
		}

		if (!lookup_subroot ||
		    !accounts_window_find_source_iter (accounts_window, source, &iter, NULL))
			gtk_tree_store_append (tree_store, &iter, subroot);

		accounts_window_fill_row_with_source (accounts_window, tree_store, &iter, source, mail_account_slaves, !is_managed_collection);
	}
}

static void
accounts_window_fill_tree_view (EAccountsWindow *accounts_window)
{
	GtkTreeStore *tree_store;
	GtkTreeModelSort *sort_model;
	GHashTable *children; /* gchar * (parent-uid) ~> GSList { ESource * } */
	GHashTable *top_sources; /* gchar * (uid) ~> ESource * */
	GList *sources, *llink;
	GSList *collections = NULL, *top_mail_accounts = NULL, *slink;
	GtkTreeIter root;

	g_return_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window));

	children = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_slist_free);
	top_sources = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	sources = e_source_registry_list_sources (accounts_window->priv->registry, NULL);
	for (llink = sources; llink; llink = g_list_next (llink)) {
		ESource *source = llink->data;
		const gchar *parent_uid;

		if (!E_IS_SOURCE (source) ||
		    e_source_has_extension (source, E_SOURCE_EXTENSION_PROXY) ||
		    e_source_has_extension (source, E_SOURCE_EXTENSION_MAIL_SIGNATURE))
			continue;

		parent_uid = e_source_get_parent (source);
		if (!parent_uid || !*parent_uid) {
			if (e_source_has_extension (source, E_SOURCE_EXTENSION_COLLECTION))
				collections = g_slist_prepend (collections, source);
			else if (e_source_has_extension (source, E_SOURCE_EXTENSION_MAIL_ACCOUNT))
				top_mail_accounts = g_slist_prepend (top_mail_accounts, source);
			else
				g_hash_table_insert (top_sources, g_strdup (e_source_get_uid (source)), source);
		} else {
			g_hash_table_insert (children, g_strdup (parent_uid), g_slist_prepend (
				g_slist_copy (g_hash_table_lookup (children, parent_uid)), source));
		}
	}

	sort_model = GTK_TREE_MODEL_SORT (gtk_tree_view_get_model (GTK_TREE_VIEW (accounts_window->priv->tree_view)));
	tree_store = GTK_TREE_STORE (gtk_tree_model_sort_get_model (sort_model));

	gtk_tree_store_clear (tree_store);
	g_hash_table_remove_all (accounts_window->priv->references);

	for (slink = collections; slink; slink = g_slist_next (slink)) {
		ESource *source = slink->data;
		gboolean is_managed_collection;

		is_managed_collection = e_source_has_extension (source, E_SOURCE_EXTENSION_GOA) ||
			e_source_has_extension (source, E_SOURCE_EXTENSION_UOA);

		gtk_tree_store_append (tree_store, &root, NULL);

		accounts_window_fill_row_with_source (accounts_window, tree_store, &root, source, NULL, TRUE);
		accounts_window_fill_children (accounts_window, tree_store, &root, is_managed_collection, FALSE,
			g_hash_table_lookup (children, e_source_get_uid (source)));
	}

	if (top_mail_accounts) {
		gtk_tree_store_append (tree_store, &root, NULL);

		accounts_window_fill_row_virtual (accounts_window, tree_store, &root,
			_("Mail Accounts"), "evolution-mail", MAIL_ACCOUNTS_SORT_HINT);

		for (slink = top_mail_accounts; slink; slink = g_slist_next (slink)) {
			ESource *source = slink->data;
			GtkTreeIter iter;
			gboolean is_builtin = FALSE;

			if (e_source_has_extension (source, E_SOURCE_EXTENSION_MAIL_ACCOUNT)) {
				ESourceMailAccount *mail_account = e_source_get_extension (source, E_SOURCE_EXTENSION_MAIL_ACCOUNT);
				is_builtin = e_source_mail_account_get_builtin (mail_account);
			}

			/* Skip 'On This Computer' and 'Search Folders' mail accounts */
			if (is_builtin ||
			    g_strcmp0 (e_source_get_uid (source), "local") == 0 ||
			    g_strcmp0 (e_source_get_uid (source), "vfolder") == 0)
				continue;

			gtk_tree_store_append (tree_store, &iter, &root);
			accounts_window_fill_row_with_source (accounts_window, tree_store, &iter, source,
				g_hash_table_lookup (children, e_source_get_uid (source)), TRUE);
		}
	}

	if (g_hash_table_size (top_sources)) {
		/* This is getting complicated, because top_sources are On This Computer, CalDAV,
		   CardDAV, ..., which can be under multiple roots, like the On This Computer, which
		   is under all of Address Books, Calendars, Memo Lists and Task Lists. */
		struct _extension_info {
			const gchar *extension_name;
			const gchar *display_name;
			const gchar *icon_name;
			gint sort_hint;
			GtkTreeIter *root;
			GHashTable *slaves; /* gchar * (UID) ~> GtkTreeIter * */
		} infos[] = {
			{ E_SOURCE_EXTENSION_ADDRESS_BOOK, N_("Address Books"), "x-office-address-book", ADDRESS_BOOKS_SORT_HINT, NULL, NULL },
			{ E_SOURCE_EXTENSION_CALENDAR, N_("Calendars"), "x-office-calendar", CALENDARS_SORT_HINT, NULL, NULL },
			{ E_SOURCE_EXTENSION_MEMO_LIST, N_("Memo Lists"), "evolution-memos", MEMO_LISTS_SORT_HINT, NULL, NULL },
			{ E_SOURCE_EXTENSION_TASK_LIST, N_("Task Lists"), "evolution-tasks", TASK_LISTS_SORT_HINT, NULL, NULL }
		};
		GHashTableIter hiter;
		gpointer value;
		gint ii;

		for (ii = 0; ii < G_N_ELEMENTS (infos); ii++) {
			infos[ii].slaves = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
		}

		g_hash_table_iter_init (&hiter, top_sources);
		while (g_hash_table_iter_next (&hiter, NULL, &value)) {
			ESource *source = value;

			for (slink = g_hash_table_lookup (children, e_source_get_uid (source)); slink; slink = g_slist_next (slink)) {
				ESource *child = slink->data;

				for (ii = 0; ii < G_N_ELEMENTS (infos); ii++) {
					if (e_source_has_extension (child, infos[ii].extension_name)) {
						GtkTreeIter *sub_root;
						GtkTreeIter iter;

						if (!infos[ii].root) {
							gtk_tree_store_append (tree_store, &root, NULL);
							accounts_window_fill_row_virtual (accounts_window, tree_store, &root,
								_(infos[ii].display_name), infos[ii].icon_name, infos[ii].sort_hint);

							infos[ii].root = g_new (GtkTreeIter, 1);
							*(infos[ii].root) = root;
						}

						sub_root = g_hash_table_lookup (infos[ii].slaves, e_source_get_uid (source));
						if (sub_root) {
							root = *sub_root;
						} else {
							gtk_tree_store_append (tree_store, &root, infos[ii].root);
							accounts_window_fill_row_with_source (accounts_window, tree_store, &root, source, NULL, FALSE);

							sub_root = g_new (GtkTreeIter, 1);
							*sub_root = root;

							g_hash_table_insert (infos[ii].slaves, e_source_dup_uid (source), sub_root);
						}

						gtk_tree_store_append (tree_store, &iter, &root);
						accounts_window_fill_row_with_source (accounts_window, tree_store, &iter, child, NULL, TRUE);

						break;
					}
				}
			}
		}

		for (ii = 0; ii < G_N_ELEMENTS (infos); ii++) {
			g_hash_table_destroy (infos[ii].slaves);
			g_free (infos[ii].root);
		}
	}

	g_hash_table_destroy (children);
	g_hash_table_destroy (top_sources);
	g_slist_free (collections);
	g_slist_free (top_mail_accounts);
	g_list_free_full (sources, g_object_unref);
}

static void
accounts_window_update_enabled (EAccountsWindow *accounts_window,
				ESource *source,
				gboolean enabled)
{
	GtkTreeIter iter;
	GtkTreeModel *model = NULL;
	ESource *source2;

	g_return_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window));
	g_return_if_fail (E_IS_SOURCE (source));

	if (!accounts_window_find_source_iter (accounts_window, source, &iter, &model))
		return;

	gtk_tree_store_set (GTK_TREE_STORE (model), &iter, COLUMN_BOOL_ENABLED, enabled, -1);

	source2 = e_accounts_window_ref_selected_source (accounts_window);
	if (source == source2) {
		gtk_widget_set_sensitive (accounts_window->priv->refresh_backend_button,
			enabled && e_source_has_extension (source, E_SOURCE_EXTENSION_COLLECTION));
	}
	g_clear_object (&source2);
}

static void
accounts_window_source_enabled_cb (ESourceRegistry *registry,
				   ESource *source,
				   gpointer user_data)
{
	EAccountsWindow *accounts_window = user_data;

	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window));

	accounts_window_update_enabled (accounts_window, source, TRUE);
}

static void
accounts_window_source_disabled_cb (ESourceRegistry *registry,
				    ESource *source,
				    gpointer user_data)
{
	EAccountsWindow *accounts_window = user_data;

	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window));

	accounts_window_update_enabled (accounts_window, source, FALSE);
}

static void
accounts_window_source_added_cb (ESourceRegistry *registry,
				 ESource *source,
				 gpointer user_data)
{
	EAccountsWindow *accounts_window = user_data;
	GtkTreeStore *tree_store;
	GtkTreeIter iter, root;
	GSList *children_and_siblings = NULL;
	GList *sources, *llink;
	gboolean restart = FALSE;

	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window));

	if (accounts_window_get_sort_hint_for_source (source) == UNKNOWN_SORT_HINT ||
	    accounts_window_find_source_iter (accounts_window, source, &iter, NULL))
		return;

	g_object_ref (source);

	tree_store = GTK_TREE_STORE (gtk_tree_model_sort_get_model (
		GTK_TREE_MODEL_SORT (gtk_tree_view_get_model (
		GTK_TREE_VIEW (accounts_window->priv->tree_view)))));

	sources = e_source_registry_list_sources (accounts_window->priv->registry, NULL);
	for (llink = sources; llink; llink = restart ? sources : g_list_next (llink)) {
		ESource *other_source = llink->data;
		const gchar *parent_uid;

		restart = FALSE;

		if (!E_IS_SOURCE (other_source) ||
		    e_source_has_extension (other_source, E_SOURCE_EXTENSION_PROXY) ||
		    e_source_has_extension (other_source, E_SOURCE_EXTENSION_MAIL_SIGNATURE))
			continue;

		parent_uid = e_source_get_parent (other_source);
		if (parent_uid && *parent_uid && (
		    g_strcmp0 (parent_uid, e_source_get_parent (source)) == 0 ||
		    g_strcmp0 (parent_uid, e_source_get_uid (source)) == 0)) {
			children_and_siblings = g_slist_prepend (children_and_siblings, g_object_ref (other_source));
		} else if (e_source_has_extension (other_source, E_SOURCE_EXTENSION_COLLECTION) && other_source != source &&
			   !e_source_has_extension (source, E_SOURCE_EXTENSION_COLLECTION) &&
			   g_strcmp0 (e_source_get_uid (other_source), e_source_get_parent (source)) == 0) {
			/* Use the collection source when there's any such found */
			g_object_unref (source);
			source = g_object_ref (other_source);

			g_slist_free_full (children_and_siblings, g_object_unref);
			children_and_siblings = NULL;
			restart = TRUE;
		}
	}

	g_list_free_full (sources, g_object_unref);

	if (e_source_has_extension (source, E_SOURCE_EXTENSION_COLLECTION)) {
		gboolean is_managed_collection;

		is_managed_collection = e_source_has_extension (source, E_SOURCE_EXTENSION_GOA) ||
			e_source_has_extension (source, E_SOURCE_EXTENSION_UOA);

		if (!accounts_window_find_source_iter (accounts_window, source, &iter, NULL)) {
			gtk_tree_store_append (tree_store, &iter, NULL);
			accounts_window_fill_row_with_source (accounts_window, tree_store, &iter, source, NULL, TRUE);
		}

		accounts_window_fill_children (accounts_window, tree_store, &iter, is_managed_collection, TRUE, children_and_siblings);
	} else if (e_source_has_extension (source, E_SOURCE_EXTENSION_MAIL_ACCOUNT) && (
		   !e_source_get_parent (source) || g_strcmp0 (e_source_get_parent (source), "") == 0)) {
		ESourceMailAccount *mail_account = e_source_get_extension (source, E_SOURCE_EXTENSION_MAIL_ACCOUNT);
		gboolean is_builtin = e_source_mail_account_get_builtin (mail_account);

		/* Skip 'On This Computer' and 'Search Folders' mail accounts */
		if (!is_builtin &&
		    g_strcmp0 (e_source_get_uid (source), "local") != 0 &&
		    g_strcmp0 (e_source_get_uid (source), "vfolder") != 0) {
			if (!accounts_window_find_child_with_sort_hint (accounts_window, tree_store, NULL, MAIL_ACCOUNTS_SORT_HINT, &root)) {
				gtk_tree_store_append (tree_store, &root, NULL);

				accounts_window_fill_row_virtual (accounts_window, tree_store, &root,
					_("Mail Accounts"), "evolution-mail", MAIL_ACCOUNTS_SORT_HINT);
			}

			gtk_tree_store_append (tree_store, &iter, &root);

			accounts_window_fill_row_with_source (accounts_window, tree_store, &iter, source, children_and_siblings, TRUE);
		}
	} else if (e_source_get_parent (source) && g_strcmp0 (e_source_get_parent (source), "") != 0) {
		struct _extension_info {
			const gchar *extension_name;
			const gchar *display_name;
			const gchar *icon_name;
			gint sort_hint;
		} infos[] = {
			{ E_SOURCE_EXTENSION_ADDRESS_BOOK, N_("Address Books"), "x-office-address-book", ADDRESS_BOOKS_SORT_HINT },
			{ E_SOURCE_EXTENSION_CALENDAR, N_("Calendars"), "x-office-calendar", CALENDARS_SORT_HINT },
			{ E_SOURCE_EXTENSION_MEMO_LIST, N_("Memo Lists"), "evolution-memos", MEMO_LISTS_SORT_HINT },
			{ E_SOURCE_EXTENSION_TASK_LIST, N_("Task Lists"), "evolution-tasks", TASK_LISTS_SORT_HINT }
		};
		ESource *parent_source;
		gboolean done, is_in_collection = FALSE, is_managed_collection = FALSE;
		gint ii;

		parent_source = e_source_registry_ref_source (accounts_window->priv->registry, e_source_get_parent (source));
		done = !parent_source;

		if (parent_source &&
		    e_source_has_extension (parent_source, E_SOURCE_EXTENSION_COLLECTION)) {
			is_in_collection = TRUE;
			is_managed_collection = e_source_has_extension (parent_source, E_SOURCE_EXTENSION_GOA) ||
				e_source_has_extension (parent_source, E_SOURCE_EXTENSION_UOA);
		}

		for (ii = 0; !done && ii < G_N_ELEMENTS (infos); ii++) {
			if (e_source_has_extension (source, infos[ii].extension_name)) {
				GtkTreeIter sub_root;

				if (is_in_collection) {
					if (accounts_window_find_source_iter (accounts_window, parent_source, &iter, NULL)) {
						GSList *children;

						children = g_slist_append (NULL, source);

						accounts_window_fill_children (accounts_window, tree_store, &iter, is_managed_collection, TRUE, children);

						g_slist_free (children);
					}

					break;
				}

				if (!accounts_window_find_child_with_sort_hint (accounts_window, tree_store, NULL, infos[ii].sort_hint, &root)) {
					gtk_tree_store_append (tree_store, &root, NULL);

					accounts_window_fill_row_virtual (accounts_window, tree_store, &root,
						_(infos[ii].display_name), infos[ii].icon_name, infos[ii].sort_hint);
				}

				if (!accounts_window_find_child_with_source_uid (accounts_window, tree_store, &root, e_source_get_parent (source), &sub_root)) {
					gtk_tree_store_append (tree_store, &sub_root, &root);
					accounts_window_fill_row_with_source (accounts_window, tree_store, &sub_root, parent_source, NULL, FALSE);
				}

				gtk_tree_store_append (tree_store, &iter, &sub_root);
				accounts_window_fill_row_with_source (accounts_window, tree_store, &iter, source, NULL, !is_managed_collection);

				break;
			}
		}

		g_clear_object (&parent_source);
	}

	g_slist_free_full (children_and_siblings, g_object_unref);
	g_object_unref (source);

	if (accounts_window->priv->select_source_uid)
		e_accounts_window_select_source (accounts_window, accounts_window->priv->select_source_uid);
}

static void
accounts_window_source_removed_cb (ESourceRegistry *registry,
				   ESource *source,
				   gpointer user_data)
{
	EAccountsWindow *accounts_window = user_data;
	GtkTreeIter iter;
	GtkTreeModel *model = NULL;
	GtkTreeIter parent;
	gboolean parent_valid;

	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window));

	if (!accounts_window_find_source_iter (accounts_window, source, &iter, &model))
		return;

	parent_valid = gtk_tree_model_iter_parent (model, &parent, &iter);

	gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);
	g_hash_table_remove (accounts_window->priv->references, e_source_get_uid (source));

	while (parent_valid && !gtk_tree_model_iter_n_children (model, &parent)) {
		ESource *subsource = NULL;
		gboolean delete_when_no_children = FALSE;

		iter = parent;
		parent_valid = gtk_tree_model_iter_parent (model, &parent, &iter);

		gtk_tree_model_get (model, &iter,
			COLUMN_OBJECT_SOURCE, &subsource,
			COLUMN_BOOL_DELETE_WHEN_NO_CHILDREN, &delete_when_no_children,
			-1);

		if (!delete_when_no_children) {
			g_clear_object (&subsource);
			break;
		}

		gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);

		if (subsource)
			g_hash_table_remove (accounts_window->priv->references, e_source_get_uid (subsource));

		g_clear_object (&subsource);
	}
}

static void
accounts_window_source_changed_cb (ESourceRegistry *registry,
				   ESource *source,
				   gpointer user_data)
{
	EAccountsWindow *accounts_window = user_data;
	GtkTreeIter iter;
	GtkTreeModel *model = NULL;
	gpointer extension = NULL;
	GdkRGBA rgba;
	gboolean rgba_set = FALSE;

	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window));

	if (!accounts_window_find_source_iter (accounts_window, source, &iter, &model))
		return;

	if (e_source_has_extension (source, E_SOURCE_EXTENSION_CALENDAR))
		extension = e_source_get_extension (source, E_SOURCE_EXTENSION_CALENDAR);
	else if (e_source_has_extension (source, E_SOURCE_EXTENSION_MEMO_LIST))
		extension = e_source_get_extension (source, E_SOURCE_EXTENSION_MEMO_LIST);
	else if (e_source_has_extension (source, E_SOURCE_EXTENSION_TASK_LIST))
		extension = e_source_get_extension (source, E_SOURCE_EXTENSION_TASK_LIST);

	if (extension) {
		gchar *color;

		color = e_source_selectable_dup_color (extension);
		rgba_set = color && gdk_rgba_parse (&rgba, color);

		g_free (color);
	}

	gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
		COLUMN_BOOL_ENABLED, e_source_get_enabled (source),
		COLUMN_STRING_DISPLAY_NAME, e_source_get_display_name (source),
		COLUMN_RGBA_COLOR, rgba_set ? &rgba : NULL,
		COLUMN_BOOL_COLOR_VISIBLE, rgba_set,
		-1);
}

static void
accounts_window_selection_changed_cb (GtkTreeSelection *selection,
				      gpointer user_data)
{
	EAccountsWindow *accounts_window = user_data;
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	ESource *source = NULL;
	guint editing_flags = E_SOURCE_EDITING_FLAG_NONE;

	g_return_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window));

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gtk_tree_model_get (model, &iter,
			COLUMN_UINT_EDITING_FLAGS, &editing_flags,
			COLUMN_OBJECT_SOURCE, &source,
			-1);
	}

	gtk_widget_set_sensitive (accounts_window->priv->edit_button, (editing_flags & E_SOURCE_EDITING_FLAG_CAN_EDIT) != 0);
	gtk_widget_set_sensitive (accounts_window->priv->delete_button, (editing_flags & E_SOURCE_EDITING_FLAG_CAN_DELETE) != 0);
	gtk_widget_set_sensitive (accounts_window->priv->refresh_backend_button,
		source && e_source_has_extension (source, E_SOURCE_EXTENSION_COLLECTION) && e_source_get_enabled (source));

	g_signal_emit (accounts_window, signals[SELECTION_CHANGED], 0, source);

	g_clear_object (&source);
}

static void
accounts_window_source_written_cb (GObject *source_object,
				   GAsyncResult *result,
				   gpointer user_data)
{
	ESource *source;
	EAccountsWindow *accounts_window;
	GWeakRef *weak_ref = user_data;
	GError *error = NULL;

	g_return_if_fail (E_IS_SOURCE (source_object));

	source = E_SOURCE (source_object);

	if (!e_source_write_finish (source, result, &error)) {
		g_warning ("%s: Failed to save changes to source '%s' (%s): %s", G_STRFUNC,
			e_source_get_display_name (source),
			e_source_get_uid (source),
			error ? error->message : "Unknown error");
	} else {
		accounts_window = g_weak_ref_get (weak_ref);

		if (accounts_window)
			g_signal_emit (accounts_window, signals[ENABLED_TOGGLED], 0, source);

		g_clear_object (&accounts_window);
	}

	e_weak_ref_free (weak_ref);
	g_clear_error (&error);
}

static void
acconts_window_source_removed_cb (GObject *source_object,
				  GAsyncResult *result,
				  gpointer user_data)
{
	ESource *source;
	GError *error = NULL;

	g_return_if_fail (E_IS_SOURCE (source_object));

	source = E_SOURCE (source_object);

	if (!e_source_remove_finish (source, result, &error)) {
		g_warning ("%s: Failed to remove source '%s' (%s): %s", G_STRFUNC,
			e_source_get_display_name (source),
			e_source_get_uid (source),
			error ? error->message : "Unknown error");
	}

	g_clear_error (&error);
}

static void
accounts_window_tree_view_enabled_toggled_cb (GtkCellRendererToggle *cell_renderer,
					      const gchar *path_string,
					      gpointer user_data)
{
	EAccountsWindow *accounts_window = user_data;
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	gboolean set_enabled;

	g_return_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window));

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (accounts_window->priv->tree_view));

	/* Change the selection first so we act on the correct source. */
	path = gtk_tree_path_new_from_string (path_string);
	gtk_tree_selection_select_path (selection, path);
	gtk_tree_path_free (path);

	set_enabled = !gtk_cell_renderer_toggle_get_active (cell_renderer);

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		ESource *source = NULL;

		gtk_tree_model_get (model, &iter, COLUMN_OBJECT_SOURCE, &source, -1);

		if (source && (e_source_get_enabled (source) ? 1 : 0) != (set_enabled ? 1 : 0)) {
			ESource *collection;

			e_source_set_enabled (source, set_enabled);

			if (e_source_get_writable (source))
				e_source_write (source, NULL, accounts_window_source_written_cb, e_weak_ref_new (accounts_window));

			/* Update also identity and transport sources for mail accounts */
			if (e_source_has_extension (source, E_SOURCE_EXTENSION_MAIL_ACCOUNT)) {
				ESource *secondary;
				gchar *uid;

				uid = e_source_mail_account_dup_identity_uid (e_source_get_extension (source, E_SOURCE_EXTENSION_MAIL_ACCOUNT));
				if (uid && *uid) {
					secondary = e_source_registry_ref_source (accounts_window->priv->registry, uid);

					if (secondary && (e_source_get_enabled (secondary) ? 1 : 0) != (set_enabled ? 1 : 0)) {
						e_source_set_enabled (secondary, set_enabled);

						if (e_source_get_writable (secondary))
							e_source_write (secondary, NULL, accounts_window_source_written_cb, e_weak_ref_new (accounts_window));
					}

					if (secondary && e_source_has_extension (secondary, E_SOURCE_EXTENSION_MAIL_SUBMISSION)) {
						g_free (uid);
						uid = e_source_mail_submission_dup_transport_uid (e_source_get_extension (secondary, E_SOURCE_EXTENSION_MAIL_SUBMISSION));
					} else {
						g_free (uid);
						uid = NULL;
					}

					g_clear_object (&secondary);

					if (uid && *uid) {
						secondary = e_source_registry_ref_source (accounts_window->priv->registry, uid);

						if (secondary && (e_source_get_enabled (secondary) ? 1 : 0) != (set_enabled ? 1 : 0)) {
							e_source_set_enabled (secondary, set_enabled);

							if (e_source_get_writable (secondary))
								e_source_write (secondary, NULL, accounts_window_source_written_cb, e_weak_ref_new (accounts_window));
						}

						g_clear_object (&secondary);
					}
				}

				g_free (uid);
			}

			/* And finally the collection, but only to enable it, if disabled */
			collection = e_source_registry_find_extension (accounts_window->priv->registry, source, E_SOURCE_EXTENSION_COLLECTION);
			if (collection && set_enabled && (e_source_get_enabled (collection) ? 1 : 0) != 1) {
				e_source_set_enabled (collection, set_enabled);

				if (e_source_get_writable (collection))
					e_source_write (collection, NULL, accounts_window_source_written_cb, e_weak_ref_new (accounts_window));
			}
		}

		g_clear_object (&source);
	}
}

static gboolean
accounts_window_key_press_event_cb (GtkWidget *widget,
				    GdkEventKey *event,
				    gpointer user_data)
{
	EAccountsWindow *accounts_window = user_data;

	g_return_val_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window), FALSE);

	if (event->keyval == GDK_KEY_Delete) {
		if (gtk_widget_is_sensitive (accounts_window->priv->delete_button))
			gtk_button_clicked (GTK_BUTTON (accounts_window->priv->delete_button));

		return TRUE;
	}

	return FALSE;
}

static void
accounts_window_row_activated_cb (GtkTreeView *tree_view,
				  GtkTreePath *path,
				  GtkTreeViewColumn *column,
				  gpointer user_data)
{
	EAccountsWindow *accounts_window = user_data;

	g_return_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window));

	if (gtk_widget_is_sensitive (accounts_window->priv->edit_button))
		gtk_button_clicked (GTK_BUTTON (accounts_window->priv->edit_button));
}

static gboolean
accounts_window_get_editing_flags_default (EAccountsWindow *accounts_window,
					   ESource *source,
					   guint *out_flags)
{
	g_return_val_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window), FALSE);
	g_return_val_if_fail (E_IS_SOURCE (source), FALSE);
	g_return_val_if_fail (out_flags != NULL, FALSE);

	if (e_source_has_extension (source, E_SOURCE_EXTENSION_COLLECTION)) {
		*out_flags = E_SOURCE_EDITING_FLAG_CAN_ENABLE;

		if (!e_source_has_extension (source, E_SOURCE_EXTENSION_GOA) &&
		    !e_source_has_extension (source, E_SOURCE_EXTENSION_UOA)) {
			*out_flags = (*out_flags) | E_SOURCE_EDITING_FLAG_CAN_DELETE;
		}

		return TRUE;
	}

	return FALSE;
}

static gboolean
accounts_window_delete_source_default (EAccountsWindow *accounts_window,
				       ESource *source)
{
	g_return_val_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window), FALSE);

	if (e_source_get_removable (source)) {
		const gchar *alert_tag = NULL;

		if (e_source_has_extension (source, E_SOURCE_EXTENSION_COLLECTION) ||
		    e_source_has_extension (source, E_SOURCE_EXTENSION_MAIL_ACCOUNT))
			alert_tag = "mail:ask-delete-account";
		else if (e_source_has_extension (source, E_SOURCE_EXTENSION_ADDRESS_BOOK))
			alert_tag = "addressbook:ask-delete-addressbook";
		else if (e_source_has_extension (source, E_SOURCE_EXTENSION_CALENDAR))
			alert_tag = "calendar:prompt-delete-calendar";
		else if (e_source_has_extension (source, E_SOURCE_EXTENSION_MEMO_LIST))
			alert_tag = "calendar:prompt-delete-memo-list";
		else if (e_source_has_extension (source, E_SOURCE_EXTENSION_TASK_LIST))
			alert_tag = "calendar:prompt-delete-task-list";

		if (alert_tag &&
		    e_alert_run_dialog_for_args (GTK_WINDOW (accounts_window), alert_tag, e_source_get_display_name (source), NULL) == GTK_RESPONSE_YES)
			e_source_remove (source, NULL, acconts_window_source_removed_cb, NULL);
	}

	return TRUE;
}

static gint
accounts_window_compare_iters_cb (GtkTreeModel *model,
				  GtkTreeIter *aa,
				  GtkTreeIter *bb,
				  gpointer user_data)
{
	gint aa_sort_hint = -1, bb_sort_hint = -1;
	gchar *aa_display_name = NULL, *bb_display_name = NULL;
	gint res;

	if (!aa || !bb)
		return aa == bb ? 0 : bb ? -1 : 1;

	gtk_tree_model_get (model, aa, COLUMN_INT_SORT_HINT, &aa_sort_hint, -1);
	gtk_tree_model_get (model, bb, COLUMN_INT_SORT_HINT, &bb_sort_hint, -1);

	if (aa_sort_hint != bb_sort_hint)
		return aa_sort_hint < bb_sort_hint ? -1 : 1;

	gtk_tree_model_get (model, aa, COLUMN_STRING_DISPLAY_NAME, &aa_display_name, -1);
	gtk_tree_model_get (model, bb, COLUMN_STRING_DISPLAY_NAME, &bb_display_name, -1);

	if (!aa_display_name || !bb_display_name)
		res = g_strcmp0 (aa_display_name, bb_display_name);
	else
		res = g_utf8_collate (aa_display_name, bb_display_name);

	g_free (aa_display_name);
	g_free (bb_display_name);

	return res;
}

static GtkWidget *
accounts_window_tree_view_new (EAccountsWindow *accounts_window)
{
	GtkTreeStore *tree_store;
	GtkTreeView *tree_view;
	GtkTreeViewColumn *column;
	GtkTreeModel *sort_model;
	GtkCellRenderer *cell_renderer;

	g_return_val_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window), NULL);

	tree_store = gtk_tree_store_new (N_COLUMNS,
		G_TYPE_BOOLEAN,	/* COLUMN_BOOL_ENABLED */
		G_TYPE_BOOLEAN,	/* COLUMN_BOOL_ENABLED_VISIBLE */
		G_TYPE_STRING,	/* COLUMN_STRING_DISPLAY_NAME */
		G_TYPE_STRING,	/* COLUMN_STRING_ICON_NAME */
		G_TYPE_BOOLEAN,	/* COLUMN_BOOL_ICON_VISIBLE */
		GDK_TYPE_RGBA,	/* COLUMN_RGBA_COLOR */
		G_TYPE_BOOLEAN,	/* COLUMN_BOOL_COLOR_VISIBLE */
		G_TYPE_STRING,	/* COLUMN_STRING_TYPE */
		E_TYPE_SOURCE,	/* COLUMN_OBJECT_SOURCE */
		G_TYPE_INT,	/* COLUMN_INT_SORT_HINT */
		G_TYPE_UINT,	/* COLUMN_UINT_EDITING_FLAGS */
		G_TYPE_BOOLEAN	/* COLUMN_BOOL_DELETE_WHEN_NO_CHILDREN */
	);

	sort_model = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (tree_store));
	gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (sort_model),
		accounts_window_compare_iters_cb, NULL, NULL);

	tree_view = GTK_TREE_VIEW (gtk_tree_view_new_with_model (sort_model));

	g_object_unref (sort_model);
	g_object_unref (tree_store);

	gtk_tree_view_set_reorderable (tree_view, FALSE);

	/* Column: Enabled */

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_expand (column, FALSE);
	gtk_tree_view_column_set_title (column, _("Enabled"));

	cell_renderer = gtk_cell_renderer_toggle_new ();
	gtk_tree_view_column_pack_start (column, cell_renderer, TRUE);

	g_signal_connect (cell_renderer, "toggled",
		G_CALLBACK (accounts_window_tree_view_enabled_toggled_cb), accounts_window);

	gtk_tree_view_column_add_attribute (column, cell_renderer, "active", COLUMN_BOOL_ENABLED);
	gtk_tree_view_column_add_attribute (column, cell_renderer, "visible", COLUMN_BOOL_ENABLED_VISIBLE);
	gtk_tree_view_append_column (tree_view, column);

	/* Column: Account Name */

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_column_set_title (column, _("Account Name"));

	cell_renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (cell_renderer, "stock-size", GTK_ICON_SIZE_MENU, NULL);
	gtk_tree_view_column_pack_start (column, cell_renderer, FALSE);

	gtk_tree_view_column_add_attribute (column, cell_renderer, "icon-name", COLUMN_STRING_ICON_NAME);
	gtk_tree_view_column_add_attribute (column, cell_renderer, "visible", COLUMN_BOOL_ICON_VISIBLE);

	cell_renderer = e_cell_renderer_color_new ();
	gtk_tree_view_column_pack_start (column, cell_renderer, FALSE);

	gtk_tree_view_column_add_attribute (column, cell_renderer, "rgba", COLUMN_RGBA_COLOR);
	gtk_tree_view_column_add_attribute (column, cell_renderer, "visible", COLUMN_BOOL_COLOR_VISIBLE);

	cell_renderer = gtk_cell_renderer_text_new ();
	g_object_set (cell_renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_tree_view_column_pack_start (column, cell_renderer, FALSE);

	gtk_tree_view_column_add_attribute (column, cell_renderer, "text", COLUMN_STRING_DISPLAY_NAME);

	gtk_tree_view_append_column (tree_view, column);
	gtk_tree_view_set_expander_column (tree_view, column);

	/* Column: Type */

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_expand (column, FALSE);
	gtk_tree_view_column_set_title (column, _("Type"));

	cell_renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, cell_renderer, TRUE);

	gtk_tree_view_column_add_attribute (column, cell_renderer, "text", COLUMN_STRING_TYPE);

	gtk_tree_view_append_column (tree_view, column);

	return GTK_WIDGET (tree_view);
}

static void
accounts_window_add_menu_activate_cb (GObject *item,
				      gpointer user_data)
{
	EAccountsWindow *accounts_window = user_data;
	const gchar *kind;
	gboolean handled = FALSE;

	g_return_if_fail (GTK_IS_MENU_ITEM (item));
	g_return_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window));

	kind = g_object_get_data (item, ADD_POPUP_KEY_KIND);

	g_return_if_fail (kind && *kind);

	g_signal_emit (accounts_window, signals[ADD_SOURCE], 0, kind, &handled);
}

static void
accounts_window_show_add_popup (EAccountsWindow *accounts_window,
				const GdkEvent *event)
{
	struct _add_items {
		const gchar *kind;
		const gchar *text;
		const gchar *icon_name;
	} items[] = {
		{ "collection",	N_("Collection _Account"),	"evolution" },
		{ "mail",	N_("_Mail Account"),		"evolution-mail" },
		{ "book",	N_("Address _Book"),		"x-office-address-book" },
		{ "calendar",	N_("_Calendar"),		"x-office-calendar" },
		{ "memo-list",	N_("M_emo List"),		"evolution-memos" },
		{ "task-list",	N_("_Task List"),		"evolution-tasks" }
	};
	GtkWidget *popup_menu;
	GtkMenuShell *menu_shell;
	gint ii;

	g_return_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window));

	popup_menu = gtk_menu_new ();
	menu_shell = GTK_MENU_SHELL (popup_menu);

	for (ii = 0; ii < G_N_ELEMENTS (items); ii++) {
		e_accounts_window_insert_to_add_popup (accounts_window, menu_shell, items[ii].kind, _(items[ii].text), items[ii].icon_name);
	}

	g_signal_emit (accounts_window, signals[POPULATE_ADD_POPUP], 0, menu_shell);

	g_signal_connect (popup_menu, "deactivate", G_CALLBACK (gtk_menu_detach), NULL);

	gtk_widget_show_all (popup_menu);

	gtk_menu_attach_to_widget (GTK_MENU (popup_menu), accounts_window->priv->add_button, NULL);

	g_object_set (popup_menu,
	              "anchor-hints", (GDK_ANCHOR_FLIP_Y |
	                               GDK_ANCHOR_SLIDE |
	                               GDK_ANCHOR_RESIZE),
	              NULL);

	gtk_menu_popup_at_widget (GTK_MENU (popup_menu),
	                          accounts_window->priv->add_button,
	                          GDK_GRAVITY_SOUTH_WEST,
	                          GDK_GRAVITY_NORTH_WEST,
	                          event);
}

static void
accounts_window_add_clicked_cb (GtkButton *button,
				gpointer user_data)
{
	EAccountsWindow *accounts_window = user_data;
	GdkEvent *event;

	g_return_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window));

	event = gtk_get_current_event ();

	accounts_window_show_add_popup (accounts_window, event);

	if (event)
		gdk_event_free (event);
}

static GtkWidget *
accounts_window_create_add_button (EAccountsWindow *accounts_window)
{
	GtkWidget *box, *button, *arrow, *label;
	gboolean button_images = FALSE;

	g_return_val_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window), NULL);

	g_object_get (gtk_settings_get_default (),
		"gtk-button-images", &button_images, NULL);

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

	button = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER (button), box);

	if (button_images) {
		GtkWidget *image;

		image = gtk_image_new_from_icon_name ("list-add", GTK_ICON_SIZE_BUTTON);
		gtk_box_pack_start (GTK_BOX (box), image, FALSE, FALSE, 2);
	}

	label = gtk_label_new_with_mnemonic (_("_Add"));
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), button);
	g_object_set (G_OBJECT (label),
		"halign", GTK_ALIGN_START,
		"hexpand", FALSE,
		"xalign", 0.0,
		NULL);

	gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 2);

	arrow = gtk_image_new_from_icon_name ("pan-down-symbolic", GTK_ICON_SIZE_BUTTON);
	gtk_box_pack_start (GTK_BOX (box), arrow, FALSE, FALSE, 2);

	g_signal_connect (button, "clicked",
		G_CALLBACK (accounts_window_add_clicked_cb), accounts_window);

	gtk_widget_show_all (button);

	return button;
}

static void
accounts_window_set_registry (EAccountsWindow *accounts_window,
			      ESourceRegistry *registry)
{
	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (accounts_window->priv->registry == NULL);

	accounts_window->priv->registry = g_object_ref (registry);
}

static void
accounts_window_set_property (GObject *object,
			      guint property_id,
			      const GValue *value,
			      GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_REGISTRY:
			accounts_window_set_registry (
				E_ACCOUNTS_WINDOW (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
accounts_window_get_property (GObject *object,
			      guint property_id,
			      GValue *value,
			      GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_REGISTRY:
			g_value_set_object (
				value,
				e_accounts_window_get_registry (
				E_ACCOUNTS_WINDOW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
accounts_window_dispose (GObject *object)
{
	EAccountsWindow *accounts_window = E_ACCOUNTS_WINDOW (object);

	if (accounts_window->priv->registry) {
		e_signal_disconnect_notify_handler (accounts_window->priv->registry,
			&accounts_window->priv->source_enabled_handler_id);

		e_signal_disconnect_notify_handler (accounts_window->priv->registry,
			&accounts_window->priv->source_disabled_handler_id);

		e_signal_disconnect_notify_handler (accounts_window->priv->registry,
			&accounts_window->priv->source_added_handler_id);

		e_signal_disconnect_notify_handler (accounts_window->priv->registry,
			&accounts_window->priv->source_removed_handler_id);

		e_signal_disconnect_notify_handler (accounts_window->priv->registry,
			&accounts_window->priv->source_changed_handler_id);

		g_clear_object (&accounts_window->priv->registry);
	}

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_accounts_window_parent_class)->dispose (object);
}

static void
accounts_window_finalize (GObject *object)
{
	EAccountsWindow *accounts_window = E_ACCOUNTS_WINDOW (object);

	g_hash_table_destroy (accounts_window->priv->references);
	g_clear_pointer (&accounts_window->priv->select_source_uid, g_free);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_accounts_window_parent_class)->finalize (object);
}

static void
accounts_window_constructed (GObject *object)
{
	EAccountsWindow *accounts_window = E_ACCOUNTS_WINDOW (object);
	ESourceRegistry *registry;
	GtkTreeSelection *selection;
	GtkWidget *container;
	GtkWidget *widget;
	GtkGrid *grid;
	GtkAccelGroup *accel_group;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_accounts_window_parent_class)->constructed (object);

	gtk_window_set_default_size (GTK_WINDOW (accounts_window), 480, 410);
	gtk_window_set_title (GTK_WINDOW (accounts_window), _("Evolution Accounts"));
	gtk_window_set_type_hint (GTK_WINDOW (accounts_window), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_container_set_border_width (GTK_CONTAINER (accounts_window), 12);

	widget = gtk_notebook_new ();
	g_object_set (G_OBJECT (widget),
		"show-border", FALSE,
		"show-tabs", FALSE,
		NULL);

	accounts_window->priv->notebook = widget;
	gtk_container_add (GTK_CONTAINER (accounts_window), widget);

	container = widget;
	gtk_widget_show (widget);

	widget = gtk_grid_new ();
	gtk_notebook_append_page (GTK_NOTEBOOK (container), widget, NULL);

	grid = GTK_GRID (widget);
	gtk_grid_set_column_spacing (grid, 6);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (widget), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_widget_set_vexpand (widget, TRUE);
	gtk_grid_attach (grid, widget, 0, 0, 1, 1);

	container = widget;

	widget = accounts_window_tree_view_new (accounts_window);
	gtk_container_add (GTK_CONTAINER (container), widget);
	accounts_window->priv->tree_view = widget;

	g_signal_connect (
		widget, "key-press-event",
		G_CALLBACK (accounts_window_key_press_event_cb),
		accounts_window);

	g_signal_connect (
		widget, "row-activated",
		G_CALLBACK (accounts_window_row_activated_cb), accounts_window);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));

	g_signal_connect (selection, "changed",
		G_CALLBACK (accounts_window_selection_changed_cb), accounts_window);

	widget = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (widget), GTK_BUTTONBOX_START);
	gtk_box_set_spacing (GTK_BOX (widget), 6);
	gtk_grid_attach (grid, widget, 1, 0, 1, 1);
	accounts_window->priv->button_box = widget;

	container = widget;

	widget = accounts_window_create_add_button (accounts_window);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	accounts_window->priv->add_button = widget;

	widget = gtk_button_new_with_mnemonic (_("_Edit"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	accounts_window->priv->edit_button = widget;

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (accounts_window_emit_edit_source), accounts_window);

	widget = e_dialog_button_new_with_icon ("edit-delete", _("_Delete"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	accounts_window->priv->delete_button = widget;

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (accounts_window_emit_delete_source), accounts_window);

	widget = e_dialog_button_new_with_icon ("view-refresh", _("_Refresh"));
	gtk_widget_set_tooltip_text (widget, _("Initiates refresh of account sources"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	accounts_window->priv->refresh_backend_button = widget;

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (accounts_window_refresh_backend_cb), accounts_window);

	widget = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (widget), GTK_BUTTONBOX_END);
	gtk_box_set_spacing (GTK_BOX (widget), 6);
	gtk_widget_set_margin_top (widget, 12);
	gtk_grid_attach (grid, widget, 0, 1, 2, 1);

	container = widget;

	widget = e_dialog_button_new_with_icon ("window-close", _("_Close"));
	g_signal_connect_swapped (widget, "clicked",
		G_CALLBACK (gtk_window_close), accounts_window);
	gtk_widget_set_can_default (widget, TRUE);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	accel_group = gtk_accel_group_new ();
	gtk_widget_add_accelerator (
		widget, "activate", accel_group,
		GDK_KEY_Escape, (GdkModifierType) 0,
		GTK_ACCEL_VISIBLE);
	gtk_window_add_accel_group (GTK_WINDOW (accounts_window), accel_group);

	registry = e_accounts_window_get_registry (accounts_window);

	gtk_widget_show_all (GTK_WIDGET (grid));

	/* First load extensions, thus the fill-tree-view can call them. */
	e_extensible_load_extensions (E_EXTENSIBLE (object));

	accounts_window_fill_tree_view (accounts_window);

	accounts_window->priv->source_enabled_handler_id =
		g_signal_connect (registry, "source-enabled",
			G_CALLBACK (accounts_window_source_enabled_cb), accounts_window);

	accounts_window->priv->source_disabled_handler_id =
		g_signal_connect (registry, "source-disabled",
			G_CALLBACK (accounts_window_source_disabled_cb), accounts_window);

	accounts_window->priv->source_added_handler_id =
		g_signal_connect (registry, "source-added",
			G_CALLBACK (accounts_window_source_added_cb), accounts_window);

	accounts_window->priv->source_removed_handler_id =
		g_signal_connect (registry, "source-removed",
			G_CALLBACK (accounts_window_source_removed_cb), accounts_window);

	accounts_window->priv->source_changed_handler_id =
		g_signal_connect (registry, "source-changed",
			G_CALLBACK (accounts_window_source_changed_cb), accounts_window);
}

static void
e_accounts_window_class_init (EAccountsWindowClass *klass)
{
	GObjectClass *object_class;

	klass->get_editing_flags = accounts_window_get_editing_flags_default;
	klass->delete_source = accounts_window_delete_source_default;

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = accounts_window_set_property;
	object_class->get_property = accounts_window_get_property;
	object_class->dispose = accounts_window_dispose;
	object_class->finalize = accounts_window_finalize;
	object_class->constructed = accounts_window_constructed;

	/**
	 * EAccountsWindow:registry:
	 *
	 * The #ESourceRegistry manages #ESource instances.
	 *
	 * Since: 3.26
	 **/
	g_object_class_install_property (
		object_class,
		PROP_REGISTRY,
		g_param_spec_object (
			"registry",
			"Registry",
			"Data source registry",
			E_TYPE_SOURCE_REGISTRY,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EAccountsWindow::get-editing-flags:
	 * @source: an #ESource
	 * @out_flags: (out): bit-or of #ESourceEditingFlags
	 *
	 * Emitted to get editing flags for the given @source. The extensions listen
	 * to this signal and the one which can handle this @source sets @out_flags
	 * appropriately. It also returns %TRUE, to stop signal emission. If the extension
	 * cannot work with the given @source, then it simply returns %FALSE.
	 *
	 * Returns: Whether the signal had been handled by any extension.
	 *
	 * Since: 3.26
	 **/
	signals[GET_EDITING_FLAGS] = g_signal_new (
		"get-editing-flags",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EAccountsWindowClass, get_editing_flags),
		g_signal_accumulator_true_handled, NULL,
		NULL,
		G_TYPE_BOOLEAN, 2,
		E_TYPE_SOURCE,
		G_TYPE_POINTER);

	/**
	 * EAccountsWindow::add-source:
	 * @kind: a UTF-8 string of the kind of source to create
	 *
	 * Emitted to add (create) a new #ESource of the given kind. The extensions can listen
	 * to this signal and can step in and add the source with the appropriate editing dialog.
	 * Such extension also returns %TRUE, to stop signal emission.
	 * If the extension cannot work with the given @kind, then it returns %FALSE.
	 *
	 * Currently known kinds are "collection", "mail", "book", "calendar",	"memo-list" and
	 * "task-list". Extensions can add their own kinds with e_accounts_window_insert_to_add_popup()
	 * from EAccountsWindow::populate-add-popup signal.
	 *
	 * Returns: Whether the signal had been handled by any extension.
	 *
	 * Since: 3.26
	 **/
	signals[ADD_SOURCE] = g_signal_new (
		"add-source",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EAccountsWindowClass, add_source),
		g_signal_accumulator_true_handled, NULL,
		NULL,
		G_TYPE_BOOLEAN, 1,
		G_TYPE_STRING);

	/**
	 * EAccountsWindow::edit-source:
	 * @source: an #ESource
	 *
	 * Emitted to edit the given @source. The extensions listen to this signal and
	 * the one which also set #EAccountsWindow::get-edit-flags for this @source
	 * will open appropriate editing dialog. It also returns %TRUE, to stop signal emission.
	 * If the extension cannot work with the given @source, then it returns %FALSE.
	 *
	 * Returns: Whether the signal had been handled by any extension.
	 *
	 * Since: 3.26
	 **/
	signals[EDIT_SOURCE] = g_signal_new (
		"edit-source",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EAccountsWindowClass, edit_source),
		g_signal_accumulator_true_handled, NULL,
		NULL,
		G_TYPE_BOOLEAN, 1,
		E_TYPE_SOURCE);

	/**
	 * EAccountsWindow::delete-source:
	 * @source: an #ESource
	 *
	 * Emitted to delete the given @source. The extensions listen to this signal and
	 * the one which also set #EAccountsWindow::get-edit-flags for this @source
	 * will remove the source. It also returns %TRUE, to stop signal emission.
	 * If the extension cannot work with the given @source, then it returns %FALSE.
	 *
	 * Returns: Whether the signal had been handled by any extension.
	 *
	 * Since: 3.26
	 **/
	signals[DELETE_SOURCE] = g_signal_new (
		"delete-source",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EAccountsWindowClass, delete_source),
		g_signal_accumulator_true_handled, NULL,
		NULL,
		G_TYPE_BOOLEAN, 1,
		E_TYPE_SOURCE);

	/**
	 * EAccountsWindow::enabled-toggled:
	 * @source: an #ESource
	 *
	 * Emitted after @source-s enable property had been toggled in the tree view.
	 *
	 * Since: 3.26
	 **/
	signals[ENABLED_TOGGLED] = g_signal_new (
		"enabled-toggled",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAccountsWindowClass, enabled_toggled),
		NULL, NULL,
		NULL,
		G_TYPE_NONE, 1,
		E_TYPE_SOURCE);

	/**
	 * EAccountsWindow::populate-add-popup:
	 * @popup_menu: a #GtkMenuShell, the popup menu
	 *
	 * Emitted before Add popup is shown. It is already populated with default
	 * source types. The signal listener can use e_accounts_window_insert_to_add_popup()
	 * to add items to it.
	 *
	 * Since: 3.26
	 **/
	signals[POPULATE_ADD_POPUP] = g_signal_new (
		"populate-add-popup",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAccountsWindowClass, populate_add_popup),
		NULL, NULL,
		NULL,
		G_TYPE_NONE, 1,
		GTK_TYPE_MENU_SHELL);

	/**
	 * EAccountsWindow::selection-changed:
	 * @source: (nullable): an #ESource, or %NULL
	 *
	 * Emitted after selection in the account tree view change. The @source is the selected #ESource,
	 * but can be %NULL, when the selected row has no associated #ESource, or nothing is selected.
	 *
	 * Since: 3.26
	 **/
	signals[SELECTION_CHANGED] = g_signal_new (
		"selection-changed",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAccountsWindowClass, selection_changed),
		NULL, NULL,
		NULL,
		G_TYPE_NONE, 1,
		E_TYPE_SOURCE);
}

static void
e_accounts_window_init (EAccountsWindow *accounts_window)
{
	accounts_window->priv = e_accounts_window_get_instance_private (accounts_window);

	accounts_window->priv->references = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) gtk_tree_row_reference_free);
}

/**
 * e_accounts_window_new:
 * @registry: an #ESourceRegistry
 *
 * Creates a new #EAccountsWindow instance.
 *
 * Returns: (transfer full): an #EAccountsWindow as a #GtkWidget
 *
 * Since: 3.26
 **/
GtkWidget *
e_accounts_window_new (ESourceRegistry *registry)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	return g_object_new (E_TYPE_ACCOUNTS_WINDOW,
		"registry", registry,
		NULL);
}

/**
 * e_accounts_window_get_registry:
 * @accounts_window: an #EAccountsWindow
 *
 * Returns the #ESourceRegistry passed to e_accounts_window_new().
 *
 * Returns: (transfer none): an #ESourceRegistry
 *
 * Since: 3.26
 **/
ESourceRegistry *
e_accounts_window_get_registry (EAccountsWindow *accounts_window)
{
	g_return_val_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window), NULL);

	return accounts_window->priv->registry;
}

/**
 * e_accounts_window_show_with_parent:
 * @accounts_window: an #EAccountsWindow
 * @parent: (nullable): a #GtkWindow, parent to show the @accounts_window on top of, or %NULL
 *
 * Shows the @accounts_window on top of the @parent, if not %NULL.
 *
 * Since: 3.26
 **/
void
e_accounts_window_show_with_parent (EAccountsWindow *accounts_window,
				    GtkWindow *parent)
{
	GtkWindow *window;

	g_return_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window));
	if (parent)
		g_return_if_fail (GTK_IS_WINDOW (parent));

	window = GTK_WINDOW (accounts_window);

	gtk_window_set_transient_for (window, parent);
	gtk_window_set_position (window, parent ? GTK_WIN_POS_CENTER_ON_PARENT : GTK_WIN_POS_CENTER);

	gtk_window_present (window);
}

/**
 * e_accounts_window_ref_selected_source:
 * @accounts_window: an #EAccountsWindow
 *
 * Returns: (nullable) (transfer full): Referenced selected #ESource, which should be unreffed
 *    with g_object_unref(), when no longer needed, or %NULL, when there is no source selected.
 *
 * Since: 3.26
 **/
ESource *
e_accounts_window_ref_selected_source (EAccountsWindow *accounts_window)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	ESource *source = NULL;

	g_return_val_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window), NULL);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (accounts_window->priv->tree_view));
	if (gtk_tree_selection_get_selected (selection, &model, &iter))
		gtk_tree_model_get (model, &iter, COLUMN_OBJECT_SOURCE, &source, -1);

	return source;
}

/**
 * e_accounts_window_select_source:
 * @accounts_window: an #EAccountsWindow
 * @uid: (nullable): an #ESource UID to select
 *
 * Selects an #ESource with the given @uid. If no such is available in time
 * of this call, then it is remembered and selected once it appears.
 * The function doesn't change selection, when @uid is %NULL, but it
 * unsets remembered UID from any previous call.
 *
 * Since: 3.28
 **/
void
e_accounts_window_select_source (EAccountsWindow *accounts_window,
				 const gchar *uid)
{
	GtkTreeModel *model;
	GtkTreeIter child_iter;

	g_return_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window));

	if (!uid || !*uid) {
		g_clear_pointer (&accounts_window->priv->select_source_uid, g_free);
		return;
	}

	if (accounts_window_find_source_uid_iter (accounts_window, uid, &child_iter, &model)) {
		GtkTreeModel *sort_model;
		GtkTreeView *tree_view;
		GtkTreeIter iter;

		g_clear_pointer (&accounts_window->priv->select_source_uid, g_free);

		tree_view = GTK_TREE_VIEW (accounts_window->priv->tree_view);
		sort_model = gtk_tree_view_get_model (tree_view);

		if (gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (sort_model), &iter, &child_iter)) {
			GtkTreeSelection *selection;
			GtkTreePath *path;

			path = gtk_tree_model_get_path (sort_model, &iter);
			if (path) {
				gtk_tree_view_expand_to_path (tree_view, path);
				gtk_tree_view_scroll_to_cell (tree_view, path, NULL, FALSE, 0.0, 0.0);
			}

			gtk_tree_path_free (path);

			selection = gtk_tree_view_get_selection (tree_view);
			gtk_tree_selection_select_iter (selection, &iter);
		}

		return;
	}

	if (g_strcmp0 (accounts_window->priv->select_source_uid, uid) != 0) {
		g_clear_pointer (&accounts_window->priv->select_source_uid, g_free);
		accounts_window->priv->select_source_uid = g_strdup (uid);
	}
}

/**
 * e_accounts_window_insert_to_add_popup:
 * @accounts_window: an #EAccountsWindow
 * @popup_menu: a #GtkMenuShell
 * @kind: (nullable): item kind, or %NULL, when @label is "-"
 * @label: item label, possibly with a mnemonic
 * @icon_name: (nullable): optional icon name to use for the menu item, or %NULL
 *
 * Adds a new item into the @popup_menu, which will be labeled with @label.
 * Items added this way are executed with EAccountsWindow::add-source signal.
 *
 * Special case "-" can be used for the @label to add a separator. In that
 * case the @kind and the @icon_name parameters are ignored.
 *
 * Since: 3.26
 **/
void
e_accounts_window_insert_to_add_popup (EAccountsWindow *accounts_window,
				       GtkMenuShell *popup_menu,
				       const gchar *kind,
				       const gchar *label,
				       const gchar *icon_name)
{
	GtkWidget *item;

	g_return_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window));
	g_return_if_fail (GTK_IS_MENU_SHELL (popup_menu));

	if (g_strcmp0 (label, "-") == 0) {
		item = gtk_separator_menu_item_new ();
		gtk_menu_shell_append (popup_menu, item);

		return;
	}

	g_return_if_fail (kind != NULL);
	g_return_if_fail (label != NULL);

	if (icon_name) {
		item = gtk_image_menu_item_new_with_mnemonic (label);

		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
			gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU));
	} else {
		item = gtk_menu_item_new_with_mnemonic (label);
	}

	g_object_set_data_full (G_OBJECT (item), ADD_POPUP_KEY_KIND, g_strdup (kind), g_free);

	g_signal_connect (item, "activate", G_CALLBACK (accounts_window_add_menu_activate_cb), accounts_window);

	gtk_menu_shell_append (popup_menu, item);
}

/**
 * e_accounts_window_get_button_box:
 * @accounts_window: an #EAccountsWindow
 *
 * Returns: (transfer none): the button box of the main page, where action
 *    buttons are stored. It can be used to add other actions to it.
 *
 * Since: 3.26
 **/
GtkButtonBox *
e_accounts_window_get_button_box (EAccountsWindow *accounts_window)
{
	g_return_val_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window), NULL);

	return GTK_BUTTON_BOX (accounts_window->priv->button_box);
}

/**
 * e_accounts_window_add_page:
 * @accounts_window: an #EAccountsWindow
 * @content: a #GtkWidget, the page content
 *
 * Adds a new hidden page to the account window with content @content.
 * The returned integer is the index of the added page, which can be used
 * with e_accounts_window_activate_page() to make that page active.
 *
 * Returns: index of the added page, or -1 on error.
 *
 * Since: 3.26
 **/
gint
e_accounts_window_add_page (EAccountsWindow *accounts_window,
			    GtkWidget *content)
{
	g_return_val_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window), -1);
	g_return_val_if_fail (GTK_IS_WIDGET (content), -1);

	return gtk_notebook_append_page (GTK_NOTEBOOK (accounts_window->priv->notebook), content, NULL);
}

/**
 * e_accounts_window_activate_page:
 * @accounts_window: an #EAccountsWindow
 * @page_index: an index of the page to activate
 *
 * Activates certain page in the @accounts_window. The @page_index should
 * be the one returned by e_accounts_window_add_page(). Using value out of
 * bounds selects the main page, which shows listing of configured accounts.
 *
 * Since: 3.26
 **/
void
e_accounts_window_activate_page (EAccountsWindow *accounts_window,
				 gint page_index)
{
	GtkNotebook *notebook;

	g_return_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window));

	notebook = GTK_NOTEBOOK (accounts_window->priv->notebook);

	if (page_index < 0 || page_index >= gtk_notebook_get_n_pages (notebook))
		page_index = 0;

	gtk_notebook_set_current_page (notebook, page_index);
}
