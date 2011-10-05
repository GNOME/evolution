/*
 * e-account-tree-view.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-account-tree-view.h"

#include <glib/gi18n.h>
#include <camel/camel.h>

enum {
	COLUMN_ACCOUNT,
	COLUMN_DEFAULT,
	COLUMN_ENABLED,
	COLUMN_NAME,
	COLUMN_PROTOCOL,
	COLUMN_SORTORDER
};

enum {
	PROP_0,
	PROP_ACCOUNT_LIST,
	PROP_SELECTED,
	PROP_SORT_ALPHA,
	PROP_EXPRESS_MODE,
	PROP_ENABLE_LOCAL_FOLDERS,
	PROP_ENABLE_SEARCH_FOLDERS
};

enum {
	ENABLE_ACCOUNT,
	DISABLE_ACCOUNT,
	REFRESHED,
	SORT_ORDER_CHANGED,
	LAST_SIGNAL
};

struct _EAccountTreeViewPrivate {
	EAccountList *account_list;
	GHashTable *index;
	gboolean sort_alpha;
	gboolean express_mode;
	gboolean enable_local_folders;
	gboolean enable_search_folders;
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (
	EAccountTreeView,
	e_account_tree_view,
	GTK_TYPE_TREE_VIEW)

static gint
account_tree_view_sort (GtkTreeModel *model,
                        GtkTreeIter *a,
                        GtkTreeIter *b,
                        gpointer user_data)
{
	gint rv = -2;
	gchar *aname = NULL, *bname = NULL;
	EAccount *aaccount = NULL, *baccount = NULL;
	guint asortorder = 0, bsortorder = 0;

	gtk_tree_model_get (model, a,
		COLUMN_ACCOUNT, &aaccount,
		COLUMN_NAME, &aname,
		COLUMN_SORTORDER, &asortorder,
		-1);

	gtk_tree_model_get (model, b,
		COLUMN_ACCOUNT, &baccount,
		COLUMN_NAME, &bname,
		COLUMN_SORTORDER, &bsortorder,
		-1);

	if ((!aaccount || !baccount || !e_account_tree_view_get_sort_alpha (user_data)) && aname && bname) {
		if (e_account_tree_view_get_sort_alpha (user_data)) {
			const gchar *on_this_computer = _("On This Computer");
			const gchar *search_folders = _("Search Folders");

			if (e_account_tree_view_get_express_mode (user_data)) {
				if (g_str_equal (aname, on_this_computer) &&
				    g_str_equal (bname, search_folders))
					rv = -1;
				else if (g_str_equal (bname, on_this_computer) &&
					 g_str_equal (aname, search_folders))
					rv = 1;
				else if (g_str_equal (aname, on_this_computer))
					rv = 1;
				else if (g_str_equal (bname, on_this_computer))
					rv = -1;
				else if (g_str_equal (aname, search_folders))
					rv = 1;
				else if (g_str_equal (bname, search_folders))
					rv = -1;
			} else {
				if (g_str_equal (aname, on_this_computer))
					rv = -1;
				else if (g_str_equal (bname, on_this_computer))
					rv = 1;
				else if (g_str_equal (aname, search_folders))
					rv = 1;
				else if (g_str_equal (bname, search_folders))
					rv = -1;
			}
		} else {
			if (asortorder < bsortorder)
				rv = -1;
			else if (asortorder > bsortorder)
				rv = 1;
			else
				rv = 0;
		}
	}

	if (rv == -2) {
		if (aname == NULL) {
			if (bname == NULL)
				rv = 0;
			else
				rv = -1;
		} else if (bname == NULL)
			rv = 1;

		if (rv == -2)
			rv = g_utf8_collate (aname, bname);
	}

	g_free (aname);
	g_free (bname);

	if (aaccount)
		g_object_unref (aaccount);
	if (baccount)
		g_object_unref (baccount);

	return rv;
}

static void
account_tree_view_normalize_sortorder_column (EAccountTreeView *tree_view)
{
	GtkListStore *list_store;
	GtkTreeModel *model;
	GtkTreeIter iter;
	guint index;

	g_return_if_fail (tree_view != NULL);
	g_return_if_fail (E_IS_ACCOUNT_TREE_VIEW (tree_view));

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));
	if (!model || !gtk_tree_model_get_iter_first (model, &iter))
		return;

	list_store = GTK_LIST_STORE (model);
	g_return_if_fail (list_store != NULL);

	index = 1;
	do {
		gtk_list_store_set (list_store, &iter, COLUMN_SORTORDER, index, -1);

		index++;
	} while (gtk_tree_model_iter_next (model, &iter));
}

static gboolean
account_tree_view_refresh_timeout_cb (gpointer ptree_view)
{
	EAccountTreeView *tree_view;
	EAccountTreeViewSelectedType selected;
	EAccountList *account_list;
	EAccount *account;
	GtkListStore *store;
	GtkTreeModel *model;
	GtkTreeIter tree_iter;
	EIterator *account_iter;
	EAccount *default_account;
	GHashTable *index;
	GSList *sort_order;
	GList *list = NULL;
	GList *iter;

	tree_view = ptree_view;
	account_list = e_account_tree_view_get_account_list (tree_view);
	sort_order = e_account_tree_view_get_sort_order (tree_view);

	store = gtk_list_store_new (
		6, E_TYPE_ACCOUNT, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN,
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT);
	model = GTK_TREE_MODEL (store);
	index = tree_view->priv->index;

	gtk_tree_sortable_set_default_sort_func (
		GTK_TREE_SORTABLE (model),
		account_tree_view_sort, tree_view, NULL);
	gtk_tree_sortable_set_sort_column_id (
		GTK_TREE_SORTABLE (model),
		GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
		GTK_SORT_ASCENDING);

	g_hash_table_remove_all (index);

	if (account_list == NULL)
		goto skip;

	/* XXX EAccountList misuses const. */
	default_account = (EAccount *)
		e_account_list_get_default (account_list);

	/* Build a list of EAccounts to display. */
	account_iter = e_list_get_iterator (E_LIST (account_list));
	while (e_iterator_is_valid (account_iter)) {
		/* XXX EIterator misuses const. */
		account = (EAccount *) e_iterator_get (account_iter);
		list = g_list_prepend (list, account);
		e_iterator_next (account_iter);
	}
	g_object_unref (account_iter);

	list = g_list_reverse (list);

	/* Populate the list store and index. */
	for (iter = list; iter != NULL; iter = iter->next) {
		GtkTreeRowReference *reference;
		GtkTreePath *path;
		CamelURL *url = NULL;
		gboolean is_default;
		const gchar *protocol;

		account = iter->data;

		/* Skip proxy accounts. */
		if (account->parent_uid != NULL)
			continue;

		is_default = (account == default_account);

		if (account->source != NULL && account->source->url != NULL)
			url = camel_url_new (account->source->url, NULL);

		if (url != NULL && url->protocol != NULL)
			protocol = url->protocol;
		else
			protocol = C_("mail-receiving", "None");

		gtk_list_store_append (store, &tree_iter);
		gtk_list_store_set (
			store, &tree_iter,
			COLUMN_ACCOUNT, account,
			COLUMN_DEFAULT, is_default,
			COLUMN_ENABLED, account->enabled,
			COLUMN_NAME, account->name,
			COLUMN_PROTOCOL, protocol, -1);

		path = gtk_tree_model_get_path (model, &tree_iter);
		reference = gtk_tree_row_reference_new (model, path);
		g_hash_table_insert (index, account, reference);
		gtk_tree_path_free (path);

		if (url != NULL)
			camel_url_free (url);
	}

	gtk_list_store_append (store, &tree_iter);
	gtk_list_store_set (
		store, &tree_iter,
		COLUMN_ACCOUNT, NULL,
		COLUMN_DEFAULT, FALSE,
		COLUMN_ENABLED, tree_view->priv->enable_local_folders,
		COLUMN_NAME, _("On This Computer"),
		COLUMN_PROTOCOL, NULL,
		-1);

	gtk_list_store_append (store, &tree_iter);
	gtk_list_store_set (
		store, &tree_iter,
		COLUMN_ACCOUNT, NULL,
		COLUMN_DEFAULT, FALSE,
		COLUMN_ENABLED, tree_view->priv->enable_search_folders,
		COLUMN_NAME, _("Search Folders"),
		COLUMN_PROTOCOL, NULL,
		-1);
 skip:
	/* Restore the previously selected account. */
	selected = e_account_tree_view_get_selected_type (tree_view);
	account = e_account_tree_view_get_selected (tree_view);
	if (account != NULL)
		g_object_ref (account);
	gtk_tree_view_set_model (GTK_TREE_VIEW (tree_view), model);
	e_account_tree_view_set_selected (tree_view, account);
	if (account != NULL)
		g_object_unref (account);
	else if (selected == E_ACCOUNT_TREE_VIEW_SELECTED_LOCAL ||
		 selected == E_ACCOUNT_TREE_VIEW_SELECTED_VFOLDER)
		e_account_tree_view_set_selected_type (tree_view, selected);

	e_account_tree_view_set_sort_order (tree_view, sort_order);
	g_slist_foreach (sort_order, (GFunc) g_free, NULL);
	g_slist_free (sort_order);

	g_signal_emit (tree_view, signals[REFRESHED], 0);

	return FALSE;
}

static void
account_tree_view_refresh_cb (EAccountList *account_list,
                              EAccount *account,
                              EAccountTreeView *tree_view)
{
	g_timeout_add (10, account_tree_view_refresh_timeout_cb, tree_view);
}

static void
account_tree_view_enabled_toggled_cb (EAccountTreeView *tree_view,
                                      gchar *path_string,
                                      GtkCellRendererToggle *renderer)
{
	GtkTreeSelection *selection;
	GtkTreePath *path;

	/* Change the selection first so we enable or disable the
	 * correct account. */
	path = gtk_tree_path_new_from_string (path_string);
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
	gtk_tree_selection_select_path (selection, path);
	gtk_tree_path_free (path);

	if (gtk_cell_renderer_toggle_get_active (renderer))
		e_account_tree_view_disable_account (tree_view);
	else
		e_account_tree_view_enable_account (tree_view);
}

static void
account_tree_view_selection_changed_cb (EAccountTreeView *tree_view)
{
	g_object_notify (G_OBJECT (tree_view), "selected");
}

static GObject *
account_tree_view_constructor (GType type,
                               guint n_construct_properties,
                               GObjectConstructParam *construct_properties)
{
	GObject *object;
	GObjectClass *parent_class;
	GtkTreeView *tree_view;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	/* Chain up to parent's constructor() method. */
	parent_class = G_OBJECT_CLASS (e_account_tree_view_parent_class);
	object = parent_class->constructor (
		type, n_construct_properties, construct_properties);

	tree_view = GTK_TREE_VIEW (object);
	gtk_tree_view_set_headers_visible (tree_view, TRUE);

	/* Column: Enabled */

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_expand (column, FALSE);
	gtk_tree_view_column_set_title (column, _("Enabled"));

	renderer = gtk_cell_renderer_toggle_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);

	g_signal_connect_swapped (
		renderer, "toggled",
		G_CALLBACK (account_tree_view_enabled_toggled_cb),
		tree_view);

	gtk_tree_view_column_add_attribute (
		column, renderer, "active", COLUMN_ENABLED);

	gtk_tree_view_append_column (tree_view, column);

	/* Column: Account Name */

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_column_set_title (column, _("Account Name"));

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_tree_view_column_pack_start (column, renderer, TRUE);

	gtk_tree_view_column_add_attribute (
		column, renderer, "text", COLUMN_NAME);

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "text", _("Default"), NULL);
	gtk_tree_view_column_pack_end (column, renderer, FALSE);

	gtk_tree_view_column_add_attribute (
		column, renderer, "visible", COLUMN_DEFAULT);

	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (
		renderer, "icon-name", "emblem-default",
		"stock-size", GTK_ICON_SIZE_MENU, NULL);
	gtk_tree_view_column_pack_end (column, renderer, FALSE);

	gtk_tree_view_column_add_attribute (
		column, renderer, "visible", COLUMN_DEFAULT);

	gtk_tree_view_append_column (tree_view, column);

	/* Column: Protocol */

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_expand (column, FALSE);
	gtk_tree_view_column_set_title (column, _("Protocol"));

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);

	gtk_tree_view_column_add_attribute (
		column, renderer, "text", COLUMN_PROTOCOL);

	gtk_tree_view_append_column (tree_view, column);

	return object;
}

static void
account_tree_view_set_property (GObject *object,
                                guint property_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACCOUNT_LIST:
			e_account_tree_view_set_account_list (
				E_ACCOUNT_TREE_VIEW (object),
				g_value_get_object (value));
			return;

		case PROP_SELECTED:
			e_account_tree_view_set_selected (
				E_ACCOUNT_TREE_VIEW (object),
				g_value_get_object (value));
			return;
		case PROP_SORT_ALPHA:
			e_account_tree_view_set_sort_alpha (
				E_ACCOUNT_TREE_VIEW (object),
				g_value_get_boolean (value));
			return;
		case PROP_EXPRESS_MODE:
			e_account_tree_view_set_express_mode (
				E_ACCOUNT_TREE_VIEW (object),
				g_value_get_boolean (value));
			return;
		case PROP_ENABLE_LOCAL_FOLDERS:
			e_account_tree_view_set_enable_local_folders (
				E_ACCOUNT_TREE_VIEW (object),
				g_value_get_boolean (value));
			return;
		case PROP_ENABLE_SEARCH_FOLDERS:
			e_account_tree_view_set_enable_search_folders (
				E_ACCOUNT_TREE_VIEW (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
account_tree_view_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACCOUNT_LIST:
			g_value_set_object (
				value,
				e_account_tree_view_get_account_list (
				E_ACCOUNT_TREE_VIEW (object)));
			return;

		case PROP_SELECTED:
			g_value_set_object (
				value,
				e_account_tree_view_get_selected (
				E_ACCOUNT_TREE_VIEW (object)));
			return;
		case PROP_SORT_ALPHA:
			g_value_set_boolean (
				value,
				e_account_tree_view_get_sort_alpha (
				E_ACCOUNT_TREE_VIEW (object)));
			return;
		case PROP_EXPRESS_MODE:
			g_value_set_boolean (
				value,
				e_account_tree_view_get_express_mode (
				E_ACCOUNT_TREE_VIEW (object)));
			return;
		case PROP_ENABLE_LOCAL_FOLDERS:
			g_value_set_boolean (
				value,
				e_account_tree_view_get_enable_local_folders (
				E_ACCOUNT_TREE_VIEW (object)));
			return;
		case PROP_ENABLE_SEARCH_FOLDERS:
			g_value_set_boolean (
				value,
				e_account_tree_view_get_enable_search_folders (
				E_ACCOUNT_TREE_VIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
account_tree_view_dispose (GObject *object)
{
	EAccountTreeViewPrivate *priv;

	priv = E_ACCOUNT_TREE_VIEW (object)->priv;

	if (priv->account_list != NULL) {
		g_signal_handlers_disconnect_by_func (
			priv->account_list,
			account_tree_view_refresh_cb, object);
		g_object_unref (priv->account_list);
		priv->account_list = NULL;
	}

	g_hash_table_remove_all (priv->index);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_account_tree_view_parent_class)->dispose (object);
}

static void
account_tree_view_finalize (GObject *object)
{
	EAccountTreeViewPrivate *priv;

	priv = E_ACCOUNT_TREE_VIEW (object)->priv;

	g_hash_table_destroy (priv->index);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_account_tree_view_parent_class)->finalize (object);
}

static void
account_tree_view_enable_account (EAccountTreeView *tree_view)
{
	EAccountList *account_list;
	EAccount *account;

	account = e_account_tree_view_get_selected (tree_view);
	if (account == NULL || account->enabled)
		return;

	account_list = e_account_tree_view_get_account_list (tree_view);
	g_return_if_fail (account_list != NULL);

	account->enabled = TRUE;
	e_account_list_change (account_list, account);

	e_account_list_save (account_list);
}

static void
account_tree_view_disable_account (EAccountTreeView *tree_view)
{
	EAccountList *account_list;
	EAccount *account;

	account = e_account_tree_view_get_selected (tree_view);
	if (account == NULL || !account->enabled)
		return;

	account_list = e_account_tree_view_get_account_list (tree_view);
	g_return_if_fail (account_list != NULL);

	account->enabled = FALSE;
	e_account_list_change (account_list, account);

	e_account_list_save (account_list);
}

static void
e_account_tree_view_class_init (EAccountTreeViewClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EAccountTreeViewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructor = account_tree_view_constructor;
	object_class->set_property = account_tree_view_set_property;
	object_class->get_property = account_tree_view_get_property;
	object_class->dispose = account_tree_view_dispose;
	object_class->finalize = account_tree_view_finalize;

	class->enable_account = account_tree_view_enable_account;
	class->disable_account = account_tree_view_disable_account;

	g_object_class_install_property (
		object_class,
		PROP_SELECTED,
		g_param_spec_object (
			"selected",
			"Selected Account",
			NULL,
			E_TYPE_ACCOUNT,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_ACCOUNT_LIST,
		g_param_spec_object (
			"account-list",
			"Account List",
			NULL,
			E_TYPE_ACCOUNT_LIST,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_SORT_ALPHA,
		g_param_spec_boolean (
			"sort-alpha",
			"Sort alphabetically",
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_EXPRESS_MODE,
		g_param_spec_boolean (
			"express-mode",
			"Express Mode sorting",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_ENABLE_LOCAL_FOLDERS,
		g_param_spec_boolean (
			"enable-local-folders",
			"Enable Local Folders",
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_ENABLE_SEARCH_FOLDERS,
		g_param_spec_boolean (
			"enable-search-folders",
			"Enable Search Folders",
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	signals[ENABLE_ACCOUNT] = g_signal_new (
		"enable-account",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAccountTreeViewClass, enable_account),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[DISABLE_ACCOUNT] = g_signal_new (
		"disable-account",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAccountTreeViewClass, disable_account),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[REFRESHED] = g_signal_new (
		"refreshed",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAccountTreeViewClass, refreshed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[SORT_ORDER_CHANGED] = g_signal_new (
		"sort-order-changed",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAccountTreeViewClass, sort_order_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_account_tree_view_init (EAccountTreeView *tree_view)
{
	GHashTable *index;
	GtkTreeSelection *selection;

	/* Reverse-lookup index */
	index = g_hash_table_new_full (
		g_direct_hash, g_direct_equal,
		(GDestroyNotify) g_object_unref,
		(GDestroyNotify) gtk_tree_row_reference_free);

	tree_view->priv = G_TYPE_INSTANCE_GET_PRIVATE (
		tree_view, E_TYPE_ACCOUNT_TREE_VIEW, EAccountTreeViewPrivate);
	tree_view->priv->index = index;
	tree_view->priv->sort_alpha = TRUE;
	tree_view->priv->express_mode = FALSE;
	tree_view->priv->enable_local_folders = TRUE;
	tree_view->priv->enable_search_folders = TRUE;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));

	g_signal_connect_swapped (
		selection, "changed",
		G_CALLBACK (account_tree_view_selection_changed_cb),
		tree_view);
}

GtkWidget *
e_account_tree_view_new (void)
{
	return g_object_new (E_TYPE_ACCOUNT_TREE_VIEW, NULL);
}

void
e_account_tree_view_enable_account (EAccountTreeView *tree_view)
{
	g_return_if_fail (E_IS_ACCOUNT_TREE_VIEW (tree_view));

	g_signal_emit (tree_view, signals[ENABLE_ACCOUNT], 0);
}

void
e_account_tree_view_disable_account (EAccountTreeView *tree_view)
{
	g_return_if_fail (E_IS_ACCOUNT_TREE_VIEW (tree_view));

	g_signal_emit (tree_view, signals[DISABLE_ACCOUNT], 0);
}

EAccountList *
e_account_tree_view_get_account_list (EAccountTreeView *tree_view)
{
	g_return_val_if_fail (E_IS_ACCOUNT_TREE_VIEW (tree_view), NULL);

	return tree_view->priv->account_list;
}

void
e_account_tree_view_set_account_list (EAccountTreeView *tree_view,
                                      EAccountList *account_list)
{
	EAccountTreeViewPrivate *priv;

	g_return_if_fail (E_IS_ACCOUNT_TREE_VIEW (tree_view));

	if (account_list != NULL)
		g_return_if_fail (E_IS_ACCOUNT_LIST (account_list));

	priv = tree_view->priv;

	if (priv->account_list != NULL) {
		g_signal_handlers_disconnect_by_func (
			priv->account_list,
			account_tree_view_refresh_cb, tree_view);
		g_object_unref (priv->account_list);
		priv->account_list = NULL;
	}

	if (account_list != NULL) {
		priv->account_list = g_object_ref (account_list);

		/* Listen for changes to the account list. */
		g_signal_connect (
			priv->account_list, "account-added",
			G_CALLBACK (account_tree_view_refresh_cb),
			tree_view);
		g_signal_connect (
			priv->account_list, "account-changed",
			G_CALLBACK (account_tree_view_refresh_cb),
			tree_view);
		g_signal_connect (
			priv->account_list, "account-removed",
			G_CALLBACK (account_tree_view_refresh_cb),
			tree_view);
	}

	account_tree_view_refresh_timeout_cb (tree_view);

	g_object_notify (G_OBJECT (tree_view), "account-list");
}

EAccount *
e_account_tree_view_get_selected (EAccountTreeView *tree_view)
{
	EAccount *account;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_return_val_if_fail (E_IS_ACCOUNT_TREE_VIEW (tree_view), NULL);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return NULL;

	gtk_tree_model_get (model, &iter, COLUMN_ACCOUNT, &account, -1);

	return account;
}

gboolean
e_account_tree_view_set_selected (EAccountTreeView *tree_view,
                                  EAccount *account)
{
	GtkTreeRowReference *reference;
	GtkTreeSelection *selection;
	GtkTreePath *path;

	g_return_val_if_fail (E_IS_ACCOUNT_TREE_VIEW (tree_view), FALSE);

	if (account != NULL)
		g_return_val_if_fail (E_IS_ACCOUNT (account), FALSE);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));

	/* NULL means clear the selection. */
	if (account == NULL) {
		gtk_tree_selection_unselect_all (selection);
		return TRUE;
	}

	/* Lookup the tree row reference for the account. */
	reference = g_hash_table_lookup (tree_view->priv->index, account);
	if (reference == NULL)
		return FALSE;

	/* Select the referenced path. */
	path = gtk_tree_row_reference_get_path (reference);
	gtk_tree_selection_select_path (selection, path);
	gtk_tree_path_free (path);

	g_object_notify (G_OBJECT (tree_view), "selected");

	return TRUE;
}

/**
 * e_account_tree_view_get_selected_type:
 * @tree_view: an #EAccountTreeView
 *
 * Returns: What node type is selected. This is useful for virtual
 * nodes "On This Computer" and "Search Folders", which doesn't have
 * their #EAccount representations. if the function returns
 * #E_ACCOUNT_TREE_VIEW_SELECTED_ACCOUNT, then the selected account
 * can be obtained with e_account_tree_view_get_selected().
 *
 * Since: 3.4
 **/
EAccountTreeViewSelectedType
e_account_tree_view_get_selected_type (EAccountTreeView *tree_view)
{
	EAccountTreeViewSelectedType res = E_ACCOUNT_TREE_VIEW_SELECTED_NONE;
	EAccount *account = NULL;
	gchar *name = NULL;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_return_val_if_fail (tree_view != NULL, E_ACCOUNT_TREE_VIEW_SELECTED_NONE);
	g_return_val_if_fail (E_IS_ACCOUNT_TREE_VIEW (tree_view), E_ACCOUNT_TREE_VIEW_SELECTED_NONE);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return E_ACCOUNT_TREE_VIEW_SELECTED_NONE;

	gtk_tree_model_get (model, &iter,
		COLUMN_ACCOUNT, &account,
		COLUMN_NAME, &name,
		-1);

	if (account) {
		res = E_ACCOUNT_TREE_VIEW_SELECTED_ACCOUNT;
		g_object_unref (account);
	} else if (name) {
		if (g_str_equal (name, _("On This Computer")))
			res = E_ACCOUNT_TREE_VIEW_SELECTED_LOCAL;
		else if (g_str_equal (name, _("Search Folders")))
			res = E_ACCOUNT_TREE_VIEW_SELECTED_VFOLDER;
	}

	g_free (name);

	return res;
}

/**
 * e_account_tree_view_set_selected_type:
 * @tree_view: an #EAccountTreeView
 * @select: what to select; see below what can be used here
 *
 * Selects special nodes in a view. Can be only either #E_ACCOUNT_TREE_VIEW_SELECTED_LOCAL
 * or #E_ACCOUNT_TREE_VIEW_SELECTED_VFOLDER.
 *
 * Since: 3.4
 **/
void
e_account_tree_view_set_selected_type (EAccountTreeView *tree_view,
                                       EAccountTreeViewSelectedType select)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean found;

	g_return_if_fail (tree_view != NULL);
	g_return_if_fail (E_IS_ACCOUNT_TREE_VIEW (tree_view));

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));

	if (!model || !gtk_tree_model_get_iter_first (model, &iter))
		return;

	if (select != E_ACCOUNT_TREE_VIEW_SELECTED_LOCAL &&
	    select != E_ACCOUNT_TREE_VIEW_SELECTED_VFOLDER)
		return;

	found = FALSE;
	do {
		gchar *name = NULL;
		EAccount *account = NULL;

		gtk_tree_model_get (model, &iter,
			COLUMN_ACCOUNT, &account,
			COLUMN_NAME, &name,
			-1);

		if (account) {
			g_object_unref (account);
		} else {
			switch (select) {
			case E_ACCOUNT_TREE_VIEW_SELECTED_LOCAL:
				found = g_strcmp0 (name, _("On This Computer")) == 0;
				break;
			case E_ACCOUNT_TREE_VIEW_SELECTED_VFOLDER:
				found = g_strcmp0 (name, _("Search Folders")) == 0;
				break;
			default:
				break;
			}
		}

		g_free (name);
	} while (!found && gtk_tree_model_iter_next (model, &iter));

	if (found)
		gtk_tree_selection_select_iter (selection, &iter);
}

static guint
account_tree_view_get_slist_index (const GSList *account_uids,
                                   const gchar *uid)
{
	guint res = 0;

	while (account_uids) {
		if (g_strcmp0 (uid, account_uids->data) == 0)
			return res;

		account_uids = account_uids->next;
		res++;
	}

	return -1;
}

/**
 * e_account_tree_view_set_sort_order:
 * @tree_view: an #EAccountTreeView
 * @account_uids: a #GSList of account uids as string
 *
 * Sets user sort order for accounts based on the order
 * in @account_uids. This is used only when sort
 * alphabetically is set to #FALSE.
 *
 * Since: 3.4
 **/
void
e_account_tree_view_set_sort_order (EAccountTreeView *tree_view,
                                    const GSList *account_uids)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_return_if_fail (tree_view != NULL);
	g_return_if_fail (E_IS_ACCOUNT_TREE_VIEW (tree_view));

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));

	if (!model || !gtk_tree_model_get_iter_first (model, &iter))
		return;

	do {
		gchar *name = NULL;
		EAccount *account = NULL;
		guint sort_order = 0;

		gtk_tree_model_get (model, &iter,
			COLUMN_ACCOUNT, &account,
			COLUMN_NAME, &name,
			-1);

		if (account) {
			sort_order = account_tree_view_get_slist_index (account_uids, account->uid) + 1;
			g_object_unref (account);
		} else if (g_strcmp0 (name, _("On This Computer")) == 0) {
			sort_order = account_tree_view_get_slist_index (account_uids, "local") + 1;
		} else if (g_strcmp0 (name, _("Search Folders")) == 0) {
			sort_order = account_tree_view_get_slist_index (account_uids, "vfolder") + 1;
		} else {
			g_warn_if_reached ();
		}

		gtk_list_store_set (GTK_LIST_STORE (model), &iter, COLUMN_SORTORDER, sort_order, -1);
		g_free (name);
	} while (gtk_tree_model_iter_next (model, &iter));

	account_tree_view_normalize_sortorder_column (tree_view);
	e_account_tree_view_sort_changed (tree_view);
}

static gint
eval_order_by_sort_hash_cb (gconstpointer a,
                            gconstpointer b,
                            gpointer user_data)
{
	guint asortorder = GPOINTER_TO_UINT (g_hash_table_lookup (user_data, a));
	guint bsortorder = GPOINTER_TO_UINT (g_hash_table_lookup (user_data, b));

	if (asortorder < bsortorder)
		return -1;
	if (asortorder > bsortorder)
		return 1;

	return 0;
}

/**
 * e_account_tree_view_get_sort_order:
 * @tree_view: an #EAccountTreeView
 *
 * Returns: Newly allocated #GSList of newly allocated strings
 * containing account UIDs in order as user wish to see them.
 * Each item of the returned list should be freed with g_free()
 * and the list itself should be freed with g_slist_free(), when
 * no longer needed.
 *
 * Since: 3.4
 **/
GSList *
e_account_tree_view_get_sort_order (EAccountTreeView *tree_view)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GHashTable *hash;
	GSList *res = NULL;

	g_return_val_if_fail (tree_view != NULL, NULL);
	g_return_val_if_fail (E_IS_ACCOUNT_TREE_VIEW (tree_view), NULL);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));

	if (!model || !gtk_tree_model_get_iter_first (model, &iter))
		return NULL;

	hash = g_hash_table_new (g_direct_hash, g_direct_equal);

	do {
		gchar *toadd = NULL;
		gchar *name = NULL;
		EAccount *account = NULL;
		guint sort_order = 0;

		gtk_tree_model_get (model, &iter,
			COLUMN_ACCOUNT, &account,
			COLUMN_NAME, &name,
			COLUMN_SORTORDER, &sort_order,
			-1);

		if (account) {
			toadd = g_strdup (account->uid);
			g_object_unref (account);
		} else if (g_strcmp0 (name, _("On This Computer")) == 0) {
			toadd = g_strdup ("local");
		} else if (g_strcmp0 (name, _("Search Folders")) == 0) {
			toadd = g_strdup ("vfolder");
		} else {
			g_warn_if_reached ();
		}

		if (toadd) {
			g_hash_table_insert (hash, toadd, GUINT_TO_POINTER (sort_order));
			res = g_slist_prepend (res, toadd);
		}

		g_free (name);
	} while (gtk_tree_model_iter_next (model, &iter));

	res = g_slist_sort_with_data (res, eval_order_by_sort_hash_cb, hash);

	g_hash_table_destroy (hash);

	return res;
}

/**
 * e_account_tree_view_sort_changed:
 * @tree_view: an #EAccountTreeView
 *
 * Notifies @tree_view about sort order change, thus it resorts
 * items in a view.
 *
 * Since: 3.4
 **/
void
e_account_tree_view_sort_changed (EAccountTreeView *tree_view)
{
	GtkTreeModel *model;

	g_return_if_fail (tree_view != NULL);
	g_return_if_fail (E_IS_ACCOUNT_TREE_VIEW (tree_view));

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));
	if (!model)
		return;

	/* this invokes also sort on a GtkListStore */
	gtk_tree_sortable_set_default_sort_func (
		GTK_TREE_SORTABLE (model),
		account_tree_view_sort, tree_view, NULL);
}

static void
account_tree_view_swap_sort_order (EAccountTreeView *tree_view,
                                   gint direction)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter1, iter2;
	guint sortorder1, sortorder2;

	g_return_if_fail (tree_view != NULL);
	g_return_if_fail (E_IS_ACCOUNT_TREE_VIEW (tree_view));
	g_return_if_fail (direction != 0);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter1))
		return;

	iter2 = iter1;
	if ((direction < 0 && !gtk_tree_model_iter_previous (model, &iter2)) ||
	    (direction > 0 && !gtk_tree_model_iter_next (model, &iter2)))
		return;

	gtk_tree_model_get (model, &iter1, COLUMN_SORTORDER, &sortorder1, -1);
	gtk_tree_model_get (model, &iter2, COLUMN_SORTORDER, &sortorder2, -1);

	gtk_list_store_set (GTK_LIST_STORE (model), &iter1, COLUMN_SORTORDER, sortorder2, -1);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter2, COLUMN_SORTORDER, sortorder1, -1);

	e_account_tree_view_sort_changed (tree_view);

	g_signal_emit (tree_view, signals[SORT_ORDER_CHANGED], 0);
}

/**
 * e_account_tree_view_move_up:
 * @tree_view: an #EAccountTreeView
 * 
 * Moves currently selected node up within user's sort order.
 *
 * Since: 3.4
 **/
void
e_account_tree_view_move_up (EAccountTreeView *tree_view)
{
	g_return_if_fail (tree_view != NULL);
	g_return_if_fail (E_IS_ACCOUNT_TREE_VIEW (tree_view));

	account_tree_view_swap_sort_order (tree_view, -1);
}

/**
 * e_account_tree_view_move_down:
 * @tree_view: an #EAccountTreeView
 * 
 * Moves currently selected node down within user's sort order.
 *
 * Since: 3.4
 **/
void
e_account_tree_view_move_down (EAccountTreeView *tree_view)
{
	g_return_if_fail (tree_view != NULL);
	g_return_if_fail (E_IS_ACCOUNT_TREE_VIEW (tree_view));

	account_tree_view_swap_sort_order (tree_view, +1);
}

void
e_account_tree_view_set_sort_alpha (EAccountTreeView *tree_view,
                                    gboolean sort_alpha)
{
	g_return_if_fail (tree_view != NULL);
	g_return_if_fail (E_IS_ACCOUNT_TREE_VIEW (tree_view));
	g_return_if_fail (tree_view->priv != NULL);

	if ((tree_view->priv->sort_alpha ? 1 : 0) == (sort_alpha ? 1 : 0))
		return;

	tree_view->priv->sort_alpha = sort_alpha;

	g_object_notify (G_OBJECT (tree_view), "sort-alpha");
	e_account_tree_view_sort_changed (tree_view);
}

gboolean
e_account_tree_view_get_sort_alpha (EAccountTreeView *tree_view)
{
	g_return_val_if_fail (tree_view != NULL, FALSE);
	g_return_val_if_fail (E_IS_ACCOUNT_TREE_VIEW (tree_view), FALSE);
	g_return_val_if_fail (tree_view->priv != NULL, FALSE);

	return tree_view->priv->sort_alpha;
}

void
e_account_tree_view_set_express_mode (EAccountTreeView *tree_view,
                                      gboolean express_mode)
{
	g_return_if_fail (tree_view != NULL);
	g_return_if_fail (E_IS_ACCOUNT_TREE_VIEW (tree_view));
	g_return_if_fail (tree_view->priv != NULL);

	if ((tree_view->priv->express_mode ? 1 : 0) == (express_mode ? 1 : 0))
		return;

	tree_view->priv->express_mode = express_mode;

	g_object_notify (G_OBJECT (tree_view), "express-mode");
	e_account_tree_view_sort_changed (tree_view);
}

gboolean
e_account_tree_view_get_express_mode (EAccountTreeView *tree_view)
{
	g_return_val_if_fail (tree_view != NULL, FALSE);
	g_return_val_if_fail (E_IS_ACCOUNT_TREE_VIEW (tree_view), FALSE);
	g_return_val_if_fail (tree_view->priv != NULL, FALSE);

	return tree_view->priv->express_mode;
}

static void
update_special_enable_state (EAccountTreeView *tree_view,
                             const gchar *display_name,
                             gboolean enabled)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkListStore *list_store;

	g_return_if_fail (tree_view != NULL);
	g_return_if_fail (display_name != NULL);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));
	if (!model)
		return;

	list_store = GTK_LIST_STORE (model);
	g_return_if_fail (list_store != NULL);

	if (!gtk_tree_model_get_iter_first (model, &iter))
		return;

	do {
		gchar *name = NULL;
		EAccount *account = NULL;

		gtk_tree_model_get (model, &iter,
			COLUMN_ACCOUNT, &account,
			COLUMN_NAME, &name,
			-1);

		if (account) {
			g_object_unref (account);
		} else if (g_strcmp0 (name, display_name) == 0) {
			gtk_list_store_set (list_store, &iter, COLUMN_ENABLED, enabled, -1);
			g_free (name);
			break;
		}

		g_free (name);
	} while (gtk_tree_model_iter_next (model, &iter));
}

void
e_account_tree_view_set_enable_local_folders (EAccountTreeView *tree_view,
                                              gboolean enabled)
{
	g_return_if_fail (tree_view != NULL);
	g_return_if_fail (E_IS_ACCOUNT_TREE_VIEW (tree_view));
	g_return_if_fail (tree_view->priv != NULL);

	if ((tree_view->priv->enable_local_folders ? 1 : 0) == (enabled ? 1 : 0))
		return;

	tree_view->priv->enable_local_folders = enabled;

	g_object_notify (G_OBJECT (tree_view), "enable-local-folders");

	update_special_enable_state (tree_view, _("On This Computer"), enabled);
}

gboolean
e_account_tree_view_get_enable_local_folders (EAccountTreeView *tree_view)
{
	g_return_val_if_fail (tree_view != NULL, FALSE);
	g_return_val_if_fail (E_IS_ACCOUNT_TREE_VIEW (tree_view), FALSE);
	g_return_val_if_fail (tree_view->priv != NULL, FALSE);

	return tree_view->priv->enable_local_folders;
}

void
e_account_tree_view_set_enable_search_folders (EAccountTreeView *tree_view,
                                               gboolean enabled)
{
	g_return_if_fail (tree_view != NULL);
	g_return_if_fail (E_IS_ACCOUNT_TREE_VIEW (tree_view));
	g_return_if_fail (tree_view->priv != NULL);

	if ((tree_view->priv->enable_search_folders ? 1 : 0) == (enabled ? 1 : 0))
		return;

	tree_view->priv->enable_search_folders = enabled;

	g_object_notify (G_OBJECT (tree_view), "enable-search-folders");

	update_special_enable_state (tree_view, _("Search Folders"), enabled);
}

gboolean
e_account_tree_view_get_enable_search_folders (EAccountTreeView *tree_view)
{
	g_return_val_if_fail (tree_view != NULL, FALSE);
	g_return_val_if_fail (E_IS_ACCOUNT_TREE_VIEW (tree_view), FALSE);
	g_return_val_if_fail (tree_view->priv != NULL, FALSE);

	return tree_view->priv->enable_search_folders;
}
