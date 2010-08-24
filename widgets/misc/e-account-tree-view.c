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

#include "e-account-tree-view.h"

#include <glib/gi18n.h>
#include <camel/camel.h>

#define E_ACCOUNT_TREE_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ACCOUNT_TREE_VIEW, EAccountTreeViewPrivate))

enum {
	COLUMN_ACCOUNT,
	COLUMN_DEFAULT,
	COLUMN_ENABLED,
	COLUMN_NAME,
	COLUMN_PROTOCOL
};

enum {
	PROP_0,
	PROP_ACCOUNT_LIST,
	PROP_SELECTED
};

enum {
	ENABLE_ACCOUNT,
	DISABLE_ACCOUNT,
	REFRESHED,
	LAST_SIGNAL
};

struct _EAccountTreeViewPrivate {
	EAccountList *account_list;
	GHashTable *index;
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (
	EAccountTreeView,
	e_account_tree_view,
	GTK_TYPE_TREE_VIEW)

static void
account_tree_view_refresh_cb (EAccountList *account_list,
                              EAccount *account,
                              EAccountTreeView *tree_view)
{
	GtkListStore *store;
	GtkTreeModel *model;
	GtkTreeIter tree_iter;
	EIterator *account_iter;
	EAccount *default_account;
	GHashTable *index;
	GList *list = NULL;
	GList *iter;

	store = gtk_list_store_new (
		5, E_TYPE_ACCOUNT, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN,
		G_TYPE_STRING, G_TYPE_STRING);
	model = GTK_TREE_MODEL (store);
	index = tree_view->priv->index;

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

skip:
	/* Restore the previously selected account. */
	account = e_account_tree_view_get_selected (tree_view);
	if (account != NULL)
		g_object_ref (account);
	gtk_tree_view_set_model (GTK_TREE_VIEW (tree_view), model);
	e_account_tree_view_set_selected (tree_view, account);
	if (account != NULL)
		g_object_unref (account);

	g_signal_emit (tree_view, signals[REFRESHED], 0);
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
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
account_tree_view_dispose (GObject *object)
{
	EAccountTreeViewPrivate *priv;

	priv = E_ACCOUNT_TREE_VIEW_GET_PRIVATE (object);

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

	priv = E_ACCOUNT_TREE_VIEW_GET_PRIVATE (object);

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

	tree_view->priv = E_ACCOUNT_TREE_VIEW_GET_PRIVATE (tree_view);
	tree_view->priv->index = index;

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

	priv = E_ACCOUNT_TREE_VIEW_GET_PRIVATE (tree_view);

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

	account_tree_view_refresh_cb (account_list, NULL, tree_view);

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
