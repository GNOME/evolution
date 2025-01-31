/*
 * e-mail-label-manager.c
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-mail-label-manager.h"

#include <glib/gi18n.h>
#include <e-util/e-util.h>

#include "e-mail-label-dialog.h"
#include "e-mail-label-tree-view.h"

struct _EMailLabelManagerPrivate {
	GtkWidget *tree_view;
	GtkWidget *add_button;
	GtkWidget *edit_button;
	GtkWidget *remove_button;
};

enum {
	PROP_0,
	PROP_LIST_STORE
};

enum {
	ADD_LABEL,
	EDIT_LABEL,
	REMOVE_LABEL,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (EMailLabelManager, e_mail_label_manager, GTK_TYPE_GRID)

static void
mail_label_manager_selection_changed_cb (EMailLabelManager *manager,
                                         GtkTreeSelection *selection)
{
	GtkWidget *edit_button;
	GtkWidget *remove_button;
	GtkTreeModel *model;
	GtkTreeIter iter;

	edit_button = manager->priv->edit_button;
	remove_button = manager->priv->remove_button;

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		EMailLabelListStore *store;
		gboolean sensitive;
		gchar *label_tag;

		store = E_MAIL_LABEL_LIST_STORE (model);
		label_tag = e_mail_label_list_store_get_tag (store, &iter);
		sensitive = !e_mail_label_tag_is_default (label_tag);
		g_free (label_tag);

		/* Disallow removing default labels. */
		gtk_widget_set_sensitive (edit_button, TRUE);
		gtk_widget_set_sensitive (remove_button, sensitive);
	} else {
		gtk_widget_set_sensitive (edit_button, FALSE);
		gtk_widget_set_sensitive (remove_button, FALSE);
	}
}

static void
mail_label_manager_set_property (GObject *object,
                                 guint property_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_LIST_STORE:
			e_mail_label_manager_set_list_store (
				E_MAIL_LABEL_MANAGER (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_label_manager_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_LIST_STORE:
			g_value_set_object (
				value, e_mail_label_manager_get_list_store (
				E_MAIL_LABEL_MANAGER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_label_manager_dispose (GObject *object)
{
	EMailLabelManager *self = E_MAIL_LABEL_MANAGER (object);

	g_clear_object (&self->priv->tree_view);
	g_clear_object (&self->priv->add_button);
	g_clear_object (&self->priv->edit_button);
	g_clear_object (&self->priv->remove_button);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_label_manager_parent_class)->dispose (object);
}

static void
mail_label_manager_add_label (EMailLabelManager *manager)
{
	EMailLabelDialog *label_dialog;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkWidget *dialog;
	gpointer parent;
	GdkRGBA label_color;
	const gchar *label_name;
	gboolean repeat = TRUE;

	parent = gtk_widget_get_toplevel (GTK_WIDGET (manager));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;
	dialog = e_mail_label_dialog_new (parent);
	label_dialog = E_MAIL_LABEL_DIALOG (dialog);
	tree_view = GTK_TREE_VIEW (manager->priv->tree_view);
	model = gtk_tree_view_get_model (tree_view);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Add Label"));

	while (repeat) {
		repeat = FALSE;

		if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK)
			break;

		label_name = e_mail_label_dialog_get_label_name (label_dialog);
		e_mail_label_dialog_get_label_color (label_dialog, &label_color);

		if (e_mail_label_list_store_lookup_by_name (E_MAIL_LABEL_LIST_STORE (model), label_name, NULL)) {
			repeat = TRUE;
			e_alert_run_dialog_for_args (
				GTK_WINDOW (dialog),
				"mail:error-label-exists", label_name, NULL);
			continue;
		}

		e_mail_label_list_store_set (
			E_MAIL_LABEL_LIST_STORE (model),
			NULL, label_name, &label_color);
	}

	gtk_widget_destroy (dialog);
}

static void
mail_label_manager_edit_label (EMailLabelManager *manager)
{
	EMailLabelDialog *label_dialog;
	EMailLabelListStore *label_store;
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *dialog;
	GtkWidget *parent;
	GdkRGBA label_color;
	const gchar *new_name;
	gchar *label_name;
	gboolean repeat = TRUE;

	tree_view = GTK_TREE_VIEW (manager->priv->tree_view);
	selection = gtk_tree_view_get_selection (tree_view);

	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	label_store = E_MAIL_LABEL_LIST_STORE (model);
	label_name = e_mail_label_list_store_get_name (label_store, &iter);
	e_mail_label_list_store_get_color (label_store, &iter, &label_color);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (manager));
	dialog = e_mail_label_dialog_new (GTK_WINDOW (parent));
	label_dialog = E_MAIL_LABEL_DIALOG (dialog);

	e_mail_label_dialog_set_label_name (label_dialog, label_name);
	e_mail_label_dialog_set_label_color (label_dialog, &label_color);
	gtk_window_set_title (GTK_WINDOW (dialog), _("Edit Label"));

	while (repeat) {
		repeat = FALSE;

		if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK)
			break;

		new_name = e_mail_label_dialog_get_label_name (label_dialog);
		e_mail_label_dialog_get_label_color (label_dialog, &label_color);

		if (g_strcmp0 (new_name, label_name) != 0 &&
		    e_mail_label_list_store_lookup_by_name (label_store, new_name, NULL)) {
			repeat = TRUE;
			e_alert_run_dialog_for_args (
				GTK_WINDOW (dialog),
				"mail:error-label-exists", new_name, NULL);
			continue;
		}

		e_mail_label_list_store_set (
			label_store, &iter, new_name, &label_color);
	}

	gtk_widget_destroy (dialog);

	g_free (label_name);
}

static void
mail_label_manager_remove_label (EMailLabelManager *manager)
{
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreeIter iter;

	tree_view = GTK_TREE_VIEW (manager->priv->tree_view);
	selection = gtk_tree_view_get_selection (tree_view);

	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
}

static void
e_mail_label_manager_class_init (EMailLabelManagerClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_label_manager_set_property;
	object_class->get_property = mail_label_manager_get_property;
	object_class->dispose = mail_label_manager_dispose;

	class->add_label = mail_label_manager_add_label;
	class->edit_label = mail_label_manager_edit_label;
	class->remove_label = mail_label_manager_remove_label;

	g_object_class_install_property (
		object_class,
		PROP_LIST_STORE,
		g_param_spec_object (
			"list-store",
			"List Store",
			NULL,
			E_TYPE_MAIL_LABEL_LIST_STORE,
			G_PARAM_READWRITE));

	signals[ADD_LABEL] = g_signal_new (
		"add-label",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMailLabelManagerClass, add_label),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[EDIT_LABEL] = g_signal_new (
		"edit-label",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMailLabelManagerClass, edit_label),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[REMOVE_LABEL] = g_signal_new (
		"remove-label",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMailLabelManagerClass, remove_label),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_mail_label_manager_init (EMailLabelManager *manager)
{
	GtkTreeSelection *selection;
	GtkWidget *container;
	GtkWidget *widget;

	manager->priv = e_mail_label_manager_get_instance_private (manager);

	gtk_grid_set_column_spacing (GTK_GRID (manager), 6);
	gtk_grid_set_row_spacing (GTK_GRID (manager), 12);

	container = GTK_WIDGET (manager);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_widget_set_vexpand (widget, TRUE);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 1, 1);
	gtk_widget_show (widget);

	container = widget;

	widget = e_mail_label_tree_view_new ();
	gtk_container_add (GTK_CONTAINER (container), widget);
	manager->priv->tree_view = g_object_ref (widget);
	gtk_widget_show (widget);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));

	g_signal_connect_swapped (
		selection, "changed",
		G_CALLBACK (mail_label_manager_selection_changed_cb),
		manager);

	container = GTK_WIDGET (manager);

	/* FIXME Clarify this.  What menu? */
	widget = gtk_label_new (
		_("Note: Underscore in the label name is used\n"
		"as mnemonic identifier in menu."));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_CENTER);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 1, 1, 1);
	gtk_widget_show (widget);

	widget = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
	gtk_button_box_set_layout (
		GTK_BUTTON_BOX (widget), GTK_BUTTONBOX_START);
	gtk_box_set_spacing (GTK_BOX (widget), 6);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 0, 1, 2);
	gtk_widget_show (widget);

	container = widget;

	widget = e_dialog_button_new_with_icon ("list-add", _("_Add"));
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	manager->priv->add_button = g_object_ref (widget);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (e_mail_label_manager_add_label), manager);

	widget = gtk_button_new_with_mnemonic (_("_Edit"));
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	manager->priv->edit_button = g_object_ref (widget);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (e_mail_label_manager_edit_label), manager);

	widget = e_dialog_button_new_with_icon ("list-remove", _("_Remove"));
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	manager->priv->remove_button = g_object_ref (widget);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (e_mail_label_manager_remove_label), manager);
}

GtkWidget *
e_mail_label_manager_new (void)
{
	return g_object_new (E_TYPE_MAIL_LABEL_MANAGER, NULL);
}

void
e_mail_label_manager_add_label (EMailLabelManager *manager)
{
	g_return_if_fail (E_IS_MAIL_LABEL_MANAGER (manager));

	g_signal_emit (manager, signals[ADD_LABEL], 0);
}

void
e_mail_label_manager_edit_label (EMailLabelManager *manager)
{
	g_return_if_fail (E_IS_MAIL_LABEL_MANAGER (manager));

	g_signal_emit (manager, signals[EDIT_LABEL], 0);
}

void
e_mail_label_manager_remove_label (EMailLabelManager *manager)
{
	g_return_if_fail (E_IS_MAIL_LABEL_MANAGER (manager));

	g_signal_emit (manager, signals[REMOVE_LABEL], 0);
}

EMailLabelListStore *
e_mail_label_manager_get_list_store (EMailLabelManager *manager)
{
	GtkTreeView *tree_view;
	GtkTreeModel *model;

	g_return_val_if_fail (E_IS_MAIL_LABEL_MANAGER (manager), NULL);

	tree_view = GTK_TREE_VIEW (manager->priv->tree_view);
	model = gtk_tree_view_get_model (tree_view);

	return E_MAIL_LABEL_LIST_STORE (model);
}

void
e_mail_label_manager_set_list_store (EMailLabelManager *manager,
                                     EMailLabelListStore *list_store)
{
	GtkTreeView *tree_view;

	g_return_if_fail (E_IS_MAIL_LABEL_MANAGER (manager));
	g_return_if_fail (E_IS_MAIL_LABEL_LIST_STORE (list_store));

	tree_view = GTK_TREE_VIEW (manager->priv->tree_view);
	gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (list_store));

	g_object_notify (G_OBJECT (manager), "list-store");
}
