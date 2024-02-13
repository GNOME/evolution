/*
 * e-mail-config-sidebar.c
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
 */

#include "evolution-config.h"

#include "e-mail-config-sidebar.h"

struct _EMailConfigSidebarPrivate {
	EMailConfigNotebook *notebook;
	gint active;

	GHashTable *buttons_to_pages;
	GHashTable *pages_to_buttons;

	gulong page_added_handler_id;
	gulong page_removed_handler_id;
	gulong page_reordered_handler_id;
};

enum {
	PROP_0,
	PROP_ACTIVE,
	PROP_NOTEBOOK
};

G_DEFINE_TYPE_WITH_PRIVATE (EMailConfigSidebar, e_mail_config_sidebar, GTK_TYPE_BUTTON_BOX)

static void
mail_config_sidebar_button_toggled (GtkToggleButton *button,
                                    EMailConfigSidebar *sidebar)
{
	if (gtk_toggle_button_get_active (button)) {
		GHashTable *hash_table;
		GtkNotebook *notebook;
		GtkWidget *page;
		gint page_num;

		hash_table = sidebar->priv->buttons_to_pages;
		page = g_hash_table_lookup (hash_table, button);
		g_return_if_fail (GTK_IS_WIDGET (page));

		notebook = GTK_NOTEBOOK (sidebar->priv->notebook);
		page_num = gtk_notebook_page_num (notebook, page);
		e_mail_config_sidebar_set_active (sidebar, page_num);
	}
}

static void
mail_config_sidebar_notebook_page_added (GtkNotebook *notebook,
                                         GtkWidget *page,
                                         guint page_num,
                                         EMailConfigSidebar *sidebar)
{
	GtkRadioButton *group_member;
	GtkWidget *button;
	GList *keys;
	gchar *tab_label = NULL;

	/* Grab another radio button if we have any. */
	keys = g_hash_table_get_keys (sidebar->priv->buttons_to_pages);
	group_member = (keys != NULL) ? GTK_RADIO_BUTTON (keys->data) : NULL;
	g_list_free (keys);

	gtk_container_child_get (
		GTK_CONTAINER (notebook), page,
		"tab-label", &tab_label, NULL);

	button = gtk_radio_button_new_with_label_from_widget (
		group_member, tab_label);
	g_object_set (button, "draw-indicator", FALSE, NULL);
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	gtk_box_pack_start (GTK_BOX (sidebar), button, FALSE, FALSE, 0);
	gtk_box_reorder_child (GTK_BOX (sidebar), button, page_num);
	gtk_widget_show (button);

	g_signal_connect (
		button, "toggled",
		G_CALLBACK (mail_config_sidebar_button_toggled), sidebar);

	g_hash_table_insert (
		sidebar->priv->pages_to_buttons,
		g_object_ref (page), g_object_ref (button));

	g_hash_table_insert (
		sidebar->priv->buttons_to_pages,
		g_object_ref (button), g_object_ref (page));

	g_free (tab_label);
}

static void
mail_config_sidebar_notebook_page_removed (GtkNotebook *notebook,
                                           GtkWidget *page,
                                           guint page_num,
                                           EMailConfigSidebar *sidebar)
{
	GHashTable *hash_table;
	GtkWidget *button;

	hash_table = sidebar->priv->pages_to_buttons;
	button = g_hash_table_lookup (hash_table, page);
	g_return_if_fail (GTK_IS_WIDGET (button));

	gtk_container_remove (GTK_CONTAINER (sidebar), button);

	g_hash_table_remove (sidebar->priv->pages_to_buttons, page);
	g_hash_table_remove (sidebar->priv->buttons_to_pages, button);
}

static void
mail_config_sidebar_notebook_page_reordered (GtkNotebook *notebook,
                                             GtkWidget *page,
                                             guint page_num,
                                             EMailConfigSidebar *sidebar)
{
	GHashTable *hash_table;
	GtkWidget *button;

	hash_table = sidebar->priv->pages_to_buttons;
	button = g_hash_table_lookup (hash_table, page);
	g_return_if_fail (GTK_IS_WIDGET (button));

	gtk_box_reorder_child (GTK_BOX (sidebar), button, page_num);
}

static void
mail_config_sidebar_set_notebook (EMailConfigSidebar *sidebar,
                                  EMailConfigNotebook *notebook)
{
	g_return_if_fail (E_IS_MAIL_CONFIG_NOTEBOOK (notebook));
	g_return_if_fail (sidebar->priv->notebook == NULL);

	sidebar->priv->notebook = g_object_ref (notebook);
}

static void
mail_config_sidebar_set_property (GObject *object,
                                  guint property_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACTIVE:
			e_mail_config_sidebar_set_active (
				E_MAIL_CONFIG_SIDEBAR (object),
				g_value_get_int (value));
			return;

		case PROP_NOTEBOOK:
			mail_config_sidebar_set_notebook (
				E_MAIL_CONFIG_SIDEBAR (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_sidebar_get_property (GObject *object,
                                  guint property_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACTIVE:
			g_value_set_int (
				value,
				e_mail_config_sidebar_get_active (
				E_MAIL_CONFIG_SIDEBAR (object)));
			return;

		case PROP_NOTEBOOK:
			g_value_set_object (
				value,
				e_mail_config_sidebar_get_notebook (
				E_MAIL_CONFIG_SIDEBAR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_sidebar_dispose (GObject *object)
{
	EMailConfigSidebar *self = E_MAIL_CONFIG_SIDEBAR (object);

	if (self->priv->notebook != NULL) {
		g_signal_handler_disconnect (
			self->priv->notebook, self->priv->page_added_handler_id);
		g_signal_handler_disconnect (
			self->priv->notebook, self->priv->page_removed_handler_id);
		g_signal_handler_disconnect (
			self->priv->notebook, self->priv->page_reordered_handler_id);
		g_clear_object (&self->priv->notebook);
	}

	g_hash_table_remove_all (self->priv->buttons_to_pages);
	g_hash_table_remove_all (self->priv->pages_to_buttons);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_config_sidebar_parent_class)->dispose (object);
}

static void
mail_config_sidebar_finalize (GObject *object)
{
	EMailConfigSidebar *self = E_MAIL_CONFIG_SIDEBAR (object);

	g_hash_table_destroy (self->priv->buttons_to_pages);
	g_hash_table_destroy (self->priv->pages_to_buttons);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_config_sidebar_parent_class)->finalize (object);
}

static void
mail_config_sidebar_constructed (GObject *object)
{
	EMailConfigSidebar *sidebar;
	GtkNotebook *notebook;
	gulong handler_id;
	gint n_pages, ii;

	sidebar = E_MAIL_CONFIG_SIDEBAR (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_config_sidebar_parent_class)->constructed (object);

	gtk_orientable_set_orientation (
		GTK_ORIENTABLE (sidebar), GTK_ORIENTATION_VERTICAL);

	gtk_button_box_set_layout (
		GTK_BUTTON_BOX (sidebar), GTK_BUTTONBOX_START);

	gtk_box_set_homogeneous (GTK_BOX (sidebar), TRUE);
	gtk_box_set_spacing (GTK_BOX (sidebar), 6);

	/* Add buttons for existing notebook pages before
	 * binding to properties or connecting to signals. */

	notebook = GTK_NOTEBOOK (sidebar->priv->notebook);
	n_pages = gtk_notebook_get_n_pages (notebook);

	for (ii = 0; ii < n_pages; ii++) {
		GtkWidget *page;

		page = gtk_notebook_get_nth_page (notebook, ii);
		mail_config_sidebar_notebook_page_added (
			notebook, page, (guint) ii, sidebar);
	}

	e_binding_bind_property (
		sidebar, "active",
		notebook, "page",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	handler_id = g_signal_connect (
		notebook, "page-added",
		G_CALLBACK (mail_config_sidebar_notebook_page_added),
		sidebar);
	sidebar->priv->page_added_handler_id = handler_id;

	handler_id = g_signal_connect (
		notebook, "page-removed",
		G_CALLBACK (mail_config_sidebar_notebook_page_removed),
		sidebar);
	sidebar->priv->page_removed_handler_id = handler_id;

	handler_id = g_signal_connect (
		notebook, "page-reordered",
		G_CALLBACK (mail_config_sidebar_notebook_page_reordered),
		sidebar);
	sidebar->priv->page_reordered_handler_id = handler_id;
}

static void
e_mail_config_sidebar_class_init (EMailConfigSidebarClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_config_sidebar_set_property;
	object_class->get_property = mail_config_sidebar_get_property;
	object_class->dispose = mail_config_sidebar_dispose;
	object_class->finalize = mail_config_sidebar_finalize;
	object_class->constructed = mail_config_sidebar_constructed;

	/* Use the same constraints as GtkNotebook:page. */
	g_object_class_install_property (
		object_class,
		PROP_ACTIVE,
		g_param_spec_int (
			"active",
			"Active",
			"Index of the currently active button",
			-1, G_MAXINT, -1,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_NOTEBOOK,
		g_param_spec_object (
			"notebook",
			"Notebook",
			"Mail configuration notebook",
			E_TYPE_MAIL_CONFIG_NOTEBOOK,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));
}

static void
e_mail_config_sidebar_init (EMailConfigSidebar *sidebar)
{
	GHashTable *buttons_to_pages;
	GHashTable *pages_to_buttons;

	buttons_to_pages = g_hash_table_new_full (
		(GHashFunc) g_direct_hash,
		(GEqualFunc) g_direct_equal,
		(GDestroyNotify) g_object_unref,
		(GDestroyNotify) g_object_unref);

	pages_to_buttons = g_hash_table_new_full (
		(GHashFunc) g_direct_hash,
		(GEqualFunc) g_direct_equal,
		(GDestroyNotify) g_object_unref,
		(GDestroyNotify) g_object_unref);

	sidebar->priv = e_mail_config_sidebar_get_instance_private (sidebar);
	sidebar->priv->buttons_to_pages = buttons_to_pages;
	sidebar->priv->pages_to_buttons = pages_to_buttons;
}

GtkWidget *
e_mail_config_sidebar_new (EMailConfigNotebook *notebook)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_NOTEBOOK (notebook), NULL);

	return g_object_new (
		E_TYPE_MAIL_CONFIG_SIDEBAR,
		"notebook", notebook, NULL);
}

gint
e_mail_config_sidebar_get_active (EMailConfigSidebar *sidebar)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_SIDEBAR (sidebar), -1);

	return sidebar->priv->active;
}

void
e_mail_config_sidebar_set_active (EMailConfigSidebar *sidebar,
                                  gint active)
{
	GtkNotebook *notebook;
	GtkWidget *page;

	g_return_if_fail (E_IS_MAIL_CONFIG_SIDEBAR (sidebar));

	notebook = GTK_NOTEBOOK (sidebar->priv->notebook);
	page = gtk_notebook_get_nth_page (notebook, active);

	sidebar->priv->active = (page != NULL) ? active : -1;

	g_object_notify (G_OBJECT (sidebar), "active");

	if (page != NULL) {
		GHashTable *hash_table;
		GtkToggleButton *button;

		hash_table = sidebar->priv->pages_to_buttons;
		button = g_hash_table_lookup (hash_table, page);
		gtk_toggle_button_set_active (button, TRUE);
	}
}

EMailConfigNotebook *
e_mail_config_sidebar_get_notebook (EMailConfigSidebar *sidebar)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_SIDEBAR (sidebar), NULL);

	return sidebar->priv->notebook;
}

