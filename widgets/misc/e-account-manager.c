/*
 * e-account-manager.c
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

#include "e-account-manager.h"

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include "e-util/e-binding.h"
#include "e-account-tree-view.h"

#define E_ACCOUNT_MANAGER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ACCOUNT_MANAGER, EAccountManagerPrivate))

struct _EAccountManagerPrivate {
	EAccountList *account_list;

	GtkWidget *tree_view;
	GtkWidget *add_button;
	GtkWidget *edit_button;
	GtkWidget *delete_button;
	GtkWidget *default_button;
};

enum {
	PROP_0,
	PROP_ACCOUNT_LIST
};

enum {
	ADD_ACCOUNT,
	EDIT_ACCOUNT,
	DELETE_ACCOUNT,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (
	EAccountManager,
	e_account_manager,
	GTK_TYPE_TABLE)

static void
account_manager_default_clicked_cb (EAccountManager *manager)
{
	EAccountTreeView *tree_view;
	EAccountList *account_list;
	EAccount *account;

	tree_view = e_account_manager_get_tree_view (manager);
	account_list = e_account_manager_get_account_list (manager);
	account = e_account_tree_view_get_selected (tree_view);
	g_return_if_fail (account != NULL);

	e_account_list_set_default (account_list, account);

	/* This signals the tree view to refresh. */
	e_account_list_change (account_list, account);
}

static gboolean
account_manager_key_press_event_cb (EAccountManager *manager,
                                    GdkEventKey *event)
{
	if (event->keyval == GDK_Delete) {
		e_account_manager_delete_account (manager);
		return TRUE;
	}

	return FALSE;
}

static void
account_manager_selection_changed_cb (EAccountManager *manager,
                                      GtkTreeSelection *selection)
{
	EAccountTreeView *tree_view;
	EAccountList *account_list;
	EAccount *default_account;
	EAccount *account;
	GtkWidget *add_button;
	GtkWidget *edit_button;
	GtkWidget *delete_button;
	GtkWidget *default_button;
	gboolean sensitive;

	add_button = manager->priv->add_button;
	edit_button = manager->priv->edit_button;
	delete_button = manager->priv->delete_button;
	default_button = manager->priv->default_button;

	tree_view = e_account_manager_get_tree_view (manager);
	account = e_account_tree_view_get_selected (tree_view);
	account_list = e_account_tree_view_get_account_list (tree_view);

	if (account == NULL)
		gtk_widget_grab_focus (add_button);

	/* XXX EAccountList misuses const */
	default_account = (EAccount *)
		e_account_list_get_default (account_list);

	sensitive = (account != NULL);
	gtk_widget_set_sensitive (edit_button, sensitive);

	sensitive = (account != NULL);
	gtk_widget_set_sensitive (delete_button, sensitive);

	sensitive = (account != NULL && account != default_account);
	gtk_widget_set_sensitive (default_button, sensitive);
}

static void
account_manager_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACCOUNT_LIST:
			e_account_manager_set_account_list (
				E_ACCOUNT_MANAGER (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
account_manager_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACCOUNT_LIST:
			g_value_set_object (
				value,
				e_account_manager_get_account_list (
				E_ACCOUNT_MANAGER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
account_manager_dispose (GObject *object)
{
	EAccountManagerPrivate *priv;

	priv = E_ACCOUNT_MANAGER_GET_PRIVATE (object);

	if (priv->account_list != NULL) {
		g_object_unref (priv->account_list);
		priv->account_list = NULL;
	}

	if (priv->tree_view != NULL) {
		g_object_unref (priv->tree_view);
		priv->tree_view = NULL;
	}

	if (priv->add_button != NULL) {
		g_object_unref (priv->add_button);
		priv->add_button = NULL;
	}

	if (priv->edit_button != NULL) {
		g_object_unref (priv->edit_button);
		priv->edit_button = NULL;
	}

	if (priv->delete_button != NULL) {
		g_object_unref (priv->delete_button);
		priv->delete_button = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_account_manager_parent_class)->dispose (object);
}

static void
e_account_manager_class_init (EAccountManagerClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EAccountManagerPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = account_manager_set_property;
	object_class->get_property = account_manager_get_property;
	object_class->dispose = account_manager_dispose;

	/* XXX If we moved the account editor to /widgets/misc we
	 *     could handle adding and editing accounts directly. */

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

	signals[ADD_ACCOUNT] = g_signal_new (
		"add-account",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EAccountManagerClass, add_account),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[EDIT_ACCOUNT] = g_signal_new (
		"edit-account",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EAccountManagerClass, edit_account),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[DELETE_ACCOUNT] = g_signal_new (
		"delete-account",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EAccountManagerClass, delete_account),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_account_manager_init (EAccountManager *manager)
{
	GtkTreeSelection *selection;
	GtkWidget *container;
	GtkWidget *widget;

	manager->priv = E_ACCOUNT_MANAGER_GET_PRIVATE (manager);

	gtk_table_resize (GTK_TABLE (manager), 1, 2);
	gtk_table_set_col_spacings (GTK_TABLE (manager), 6);
	gtk_table_set_row_spacings (GTK_TABLE (manager), 12);

	container = GTK_WIDGET (manager);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_table_attach (
		GTK_TABLE (container), widget, 0, 1, 0, 1,
		GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = e_account_tree_view_new ();
	gtk_container_add (GTK_CONTAINER (container), widget);
	manager->priv->tree_view = g_object_ref (widget);
	gtk_widget_show (widget);

	e_mutual_binding_new (
		manager, "account-list",
		widget, "account-list");

	g_signal_connect_swapped (
		widget, "key-press-event",
		G_CALLBACK (account_manager_key_press_event_cb),
		manager);

	g_signal_connect_swapped (
		widget, "row-activated",
		G_CALLBACK (e_account_manager_edit_account),
		manager);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));

	g_signal_connect_swapped (
		selection, "changed",
		G_CALLBACK (account_manager_selection_changed_cb),
		manager);

	container = GTK_WIDGET (manager);

	widget = gtk_vbutton_box_new ();
	gtk_button_box_set_layout (
		GTK_BUTTON_BOX (widget), GTK_BUTTONBOX_START);
	gtk_box_set_spacing (GTK_BOX (widget), 6);
	gtk_table_attach (
		GTK_TABLE (container), widget,
		1, 2, 0, 2, 0, GTK_FILL, 0, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_button_new_from_stock (GTK_STOCK_ADD);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	manager->priv->add_button = g_object_ref (widget);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (e_account_manager_add_account), manager);

	widget = gtk_button_new_from_stock (GTK_STOCK_EDIT);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	manager->priv->edit_button = g_object_ref (widget);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (e_account_manager_edit_account), manager);

	widget = gtk_button_new_from_stock (GTK_STOCK_DELETE);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	manager->priv->delete_button = g_object_ref (widget);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (e_account_manager_delete_account), manager);

	widget = gtk_button_new_with_mnemonic (_("De_fault"));
	gtk_button_set_image (
		GTK_BUTTON (widget), gtk_image_new_from_icon_name (
		"emblem-default", GTK_ICON_SIZE_BUTTON));
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	manager->priv->default_button = g_object_ref (widget);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (account_manager_default_clicked_cb), manager);
}

GtkWidget *
e_account_manager_new (EAccountList *account_list)
{
	g_return_val_if_fail (E_IS_ACCOUNT_LIST (account_list), NULL);

	return g_object_new (
		E_TYPE_ACCOUNT_MANAGER,
		"account-list", account_list, NULL);
}

void
e_account_manager_add_account (EAccountManager *manager)
{
	g_return_if_fail (E_IS_ACCOUNT_MANAGER (manager));

	g_signal_emit (manager, signals[ADD_ACCOUNT], 0);
}

void
e_account_manager_edit_account (EAccountManager *manager)
{
	g_return_if_fail (E_IS_ACCOUNT_MANAGER (manager));

	g_signal_emit (manager, signals[EDIT_ACCOUNT], 0);
}

void
e_account_manager_delete_account (EAccountManager *manager)
{
	g_return_if_fail (E_IS_ACCOUNT_MANAGER (manager));

	g_signal_emit (manager, signals[DELETE_ACCOUNT], 0);
}

EAccountList *
e_account_manager_get_account_list (EAccountManager *manager)
{
	g_return_val_if_fail (E_IS_ACCOUNT_MANAGER (manager), NULL);

	return manager->priv->account_list;
}

void
e_account_manager_set_account_list (EAccountManager *manager,
                                    EAccountList *account_list)
{
	g_return_if_fail (E_IS_ACCOUNT_MANAGER (manager));

	if (account_list != NULL) {
		g_return_if_fail (E_IS_ACCOUNT_LIST (account_list));
		g_object_ref (account_list);
	}

	if (manager->priv->account_list != NULL)
		g_object_unref (manager->priv->account_list);

	manager->priv->account_list = account_list;

	g_object_notify (G_OBJECT (manager), "account-list");
}

EAccountTreeView *
e_account_manager_get_tree_view (EAccountManager *manager)
{
	g_return_val_if_fail (E_IS_ACCOUNT_MANAGER (manager), NULL);

	return E_ACCOUNT_TREE_VIEW (manager->priv->tree_view);
}
