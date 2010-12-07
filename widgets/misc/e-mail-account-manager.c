/*
 * e-mail-account-manager.c
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
 */

#include "e-mail-account-manager.h"

#include <config.h>
#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>

#include "e-mail-account-tree-view.h"

#define E_MAIL_ACCOUNT_MANAGER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_ACCOUNT_MANAGER, EMailAccountManagerPrivate))

struct _EMailAccountManagerPrivate {
	ESourceRegistry *registry;

	GtkWidget *tree_view;		/* not referenced */
	GtkWidget *add_button;		/* not referenced */
	GtkWidget *edit_button;		/* not referenced */
	GtkWidget *delete_button;	/* not referenced */
	GtkWidget *default_button;	/* not referenced */
};

enum {
	PROP_0,
	PROP_REGISTRY
};

enum {
	ADD_ACCOUNT,
	EDIT_ACCOUNT,
	DELETE_ACCOUNT,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (
	EMailAccountManager,
	e_mail_account_manager,
	GTK_TYPE_TABLE)

static gboolean
mail_account_manager_key_press_event_cb (EMailAccountManager *manager,
                                         GdkEventKey *event)
{
	if (event->keyval == GDK_KEY_Delete) {
		e_mail_account_manager_delete_account (manager);
		return TRUE;
	}

	return FALSE;
}

static void
mail_account_manager_selection_changed_cb (EMailAccountManager *manager,
                                           GtkTreeSelection *selection)
{
	EMailAccountTreeView *tree_view;
	ESourceRegistry *registry;
	ESource *default_source;
	ESource *source;
	GtkWidget *add_button;
	GtkWidget *edit_button;
	GtkWidget *delete_button;
	GtkWidget *default_button;
	gboolean sensitive;

	add_button = manager->priv->add_button;
	edit_button = manager->priv->edit_button;
	delete_button = manager->priv->delete_button;
	default_button = manager->priv->default_button;

	registry = e_mail_account_manager_get_registry (manager);
	tree_view = E_MAIL_ACCOUNT_TREE_VIEW (manager->priv->tree_view);

	source = e_mail_account_tree_view_get_selected_source (tree_view);
	default_source = e_source_registry_get_default_mail_account (registry);

	if (source == NULL)
		gtk_widget_grab_focus (add_button);

	sensitive = (source != NULL);
	gtk_widget_set_sensitive (edit_button, sensitive);

	sensitive = (source != NULL);
	gtk_widget_set_sensitive (delete_button, sensitive);

	sensitive = (source != NULL && source != default_source);
	gtk_widget_set_sensitive (default_button, sensitive);
}

static void
mail_account_manager_set_registry (EMailAccountManager *manager,
                                   ESourceRegistry *registry)
{
	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (manager->priv->registry == NULL);

	manager->priv->registry = g_object_ref (registry);
}

static void
mail_account_manager_set_property (GObject *object,
                                   guint property_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_REGISTRY:
			mail_account_manager_set_registry (
				E_MAIL_ACCOUNT_MANAGER (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_account_manager_get_property (GObject *object,
                                   guint property_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_REGISTRY:
			g_value_set_object (
				value,
				e_mail_account_manager_get_registry (
				E_MAIL_ACCOUNT_MANAGER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_account_manager_dispose (GObject *object)
{
	EMailAccountManagerPrivate *priv;

	priv = E_MAIL_ACCOUNT_MANAGER_GET_PRIVATE (object);

	if (priv->registry != NULL) {
		g_object_unref (priv->registry);
		priv->registry = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_account_manager_parent_class)->dispose (object);
}

static void
mail_account_manager_constructed (GObject *object)
{
	EMailAccountManager *manager;
	ESourceRegistry *registry;
	GtkTreeSelection *selection;
	GtkWidget *container;
	GtkWidget *widget;

	manager = E_MAIL_ACCOUNT_MANAGER (object);
	registry = e_mail_account_manager_get_registry (manager);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_account_manager_parent_class)->
		constructed (object);

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

	widget = e_mail_account_tree_view_new (registry);
	gtk_container_add (GTK_CONTAINER (container), widget);
	manager->priv->tree_view = widget;  /* not referenced */
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "key-press-event",
		G_CALLBACK (mail_account_manager_key_press_event_cb),
		manager);

	g_signal_connect_swapped (
		widget, "row-activated",
		G_CALLBACK (e_mail_account_manager_edit_account),
		manager);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));

	g_signal_connect_swapped (
		selection, "changed",
		G_CALLBACK (mail_account_manager_selection_changed_cb),
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
	manager->priv->add_button = widget;  /* not referenced */
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (e_mail_account_manager_add_account), manager);

	widget = gtk_button_new_from_stock (GTK_STOCK_EDIT);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	manager->priv->edit_button = widget;  /* not referenced */
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (e_mail_account_manager_edit_account), manager);

	widget = gtk_button_new_from_stock (GTK_STOCK_DELETE);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	manager->priv->delete_button = widget;  /* not referenced */
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (e_mail_account_manager_delete_account), manager);

	widget = gtk_button_new_with_mnemonic (_("De_fault"));
	gtk_button_set_image (
		GTK_BUTTON (widget),
		gtk_image_new_from_icon_name (
		"emblem-default", GTK_ICON_SIZE_BUTTON));
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	manager->priv->default_button = widget;  /* not referenced */
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (e_mail_account_tree_view_enable_selected),
		manager->priv->tree_view);
}

static void
e_mail_account_manager_class_init (EMailAccountManagerClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EMailAccountManagerPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_account_manager_set_property;
	object_class->get_property = mail_account_manager_get_property;
	object_class->dispose = mail_account_manager_dispose;
	object_class->constructed = mail_account_manager_constructed;

	/* XXX If we moved the account editor to /widgets/misc we
	 *     could handle adding and editing accounts directly. */

	g_object_class_install_property (
		object_class,
		PROP_REGISTRY,
		g_param_spec_object (
			"registry",
			"Registry",
			NULL,
			E_TYPE_SOURCE_REGISTRY,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	signals[ADD_ACCOUNT] = g_signal_new (
		"add-account",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailAccountManagerClass, add_account),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[EDIT_ACCOUNT] = g_signal_new (
		"edit-account",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailAccountManagerClass, edit_account),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[DELETE_ACCOUNT] = g_signal_new (
		"delete-account",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailAccountManagerClass, delete_account),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_mail_account_manager_init (EMailAccountManager *manager)
{
	manager->priv = E_MAIL_ACCOUNT_MANAGER_GET_PRIVATE (manager);
}

GtkWidget *
e_mail_account_manager_new (ESourceRegistry *registry)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	return g_object_new (
		E_TYPE_MAIL_ACCOUNT_MANAGER,
		"registry", registry, NULL);
}

void
e_mail_account_manager_add_account (EMailAccountManager *manager)
{
	g_return_if_fail (E_IS_MAIL_ACCOUNT_MANAGER (manager));

	g_signal_emit (manager, signals[ADD_ACCOUNT], 0);
}

void
e_mail_account_manager_edit_account (EMailAccountManager *manager)
{
	g_return_if_fail (E_IS_MAIL_ACCOUNT_MANAGER (manager));

	g_signal_emit (manager, signals[EDIT_ACCOUNT], 0);
}

void
e_mail_account_manager_delete_account (EMailAccountManager *manager)
{
	g_return_if_fail (E_IS_MAIL_ACCOUNT_MANAGER (manager));

	g_signal_emit (manager, signals[DELETE_ACCOUNT], 0);
}

ESourceRegistry *
e_mail_account_manager_get_registry (EMailAccountManager *manager)
{
	g_return_val_if_fail (E_IS_MAIL_ACCOUNT_MANAGER (manager), NULL);

	return manager->priv->registry;
}
