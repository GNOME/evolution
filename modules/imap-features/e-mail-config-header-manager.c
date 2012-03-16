/*
 * e-mail-config-header-manager.c
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

#include "e-mail-config-header-manager.h"

#define E_MAIL_CONFIG_HEADER_MANAGER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_CONFIG_HEADER_MANAGER, EMailConfigHeaderManagerPrivate))

struct _EMailConfigHeaderManagerPrivate {
	GHashTable *headers;

	GtkWidget *entry;          /* not referenced */
	GtkWidget *tree_view;      /* not referenced */
	GtkWidget *add_button;     /* not referenced */
	GtkWidget *remove_button;  /* not referenced */
};

enum {
	PROP_0,
	PROP_HEADERS
};

G_DEFINE_DYNAMIC_TYPE (
	EMailConfigHeaderManager,
	e_mail_config_header_manager,
	GTK_TYPE_GRID)

static gboolean
mail_config_header_manager_header_is_valid (const gchar *header)
{
	gchar *stripped_header;
	gboolean is_valid;
	glong length;

	if (header == NULL)
		return FALSE;

	stripped_header = g_strstrip (g_strdup (header));
	length = g_utf8_strlen (stripped_header, -1);

	is_valid =
		(*stripped_header != '\0') &&
		(g_utf8_strchr (stripped_header, length, ':') == NULL) &&
		(g_utf8_strchr (stripped_header, length, ' ') == NULL);

	g_free (stripped_header);

	return is_valid;
}

static gboolean
mail_config_header_manager_header_to_boolean (GBinding *binding,
                                              const GValue *source_value,
                                              GValue *target_value,
                                              gpointer unused)
{
	gboolean is_valid;
	const gchar *string;

	string = g_value_get_string (source_value);
	is_valid = mail_config_header_manager_header_is_valid (string);
	g_value_set_boolean (target_value, is_valid);

	return TRUE;
}

static void
mail_config_header_manager_update_list (EMailConfigHeaderManager *manager)
{
	GtkTreeView *tree_view;
	GtkTreeModel *tree_model;
	GtkListStore *list_store;
	GtkTreeSelection *selection;
	GtkTreePath *path = NULL;
	GList *list, *link;

	tree_view = GTK_TREE_VIEW (manager->priv->tree_view);
	selection = gtk_tree_view_get_selection (tree_view);

	list = gtk_tree_selection_get_selected_rows (selection, &tree_model);
	if (g_list_length (list) == 1)
		path = gtk_tree_path_copy (list->data);
	g_list_free_full (list, (GDestroyNotify) gtk_tree_path_free);

	list_store = GTK_LIST_STORE (tree_model);
	gtk_list_store_clear (list_store);

	list = g_hash_table_get_keys (manager->priv->headers);
	list = g_list_sort (list, (GCompareFunc) g_utf8_collate);

	for (link = list; link != NULL; link = g_list_next (link)) {
		GtkTreeIter iter;
		const gchar *header = link->data;
		gtk_list_store_append (list_store, &iter);
		gtk_list_store_set (list_store, &iter, 0, header, -1);
	}

	g_list_free (list);

	if (path != NULL) {
		gtk_tree_selection_select_path (selection, path);
		if (!gtk_tree_selection_path_is_selected (selection, path))
			if (gtk_tree_path_prev (path))
				gtk_tree_selection_select_path (selection, path);
		gtk_tree_path_free (path);
	}
}

static gboolean
mail_config_header_manager_add_header (EMailConfigHeaderManager *manager,
                                       const gchar *header)
{
	gchar *stripped_header;

	if (!mail_config_header_manager_header_is_valid (header))
		return FALSE;

	stripped_header = g_strstrip (g_strdup (header));

	g_hash_table_replace (
		manager->priv->headers,
		stripped_header, stripped_header);

	g_object_notify (G_OBJECT (manager), "headers");

	mail_config_header_manager_update_list (manager);

	return TRUE;
}

static void
mail_config_header_manager_remove_selected (EMailConfigHeaderManager *manager)
{
	GtkTreeView *tree_view;
	GtkTreeModel *tree_model;
	GtkTreeSelection *selection;
	GList *list, *link;

	tree_view = GTK_TREE_VIEW (manager->priv->tree_view);
	selection = gtk_tree_view_get_selection (tree_view);
	list = gtk_tree_selection_get_selected_rows (selection, &tree_model);

	for (link = list; link != NULL; link = g_list_next (link)) {
		GtkTreePath *path = link->data;
		GtkTreeIter iter;
		gchar *header;

		gtk_tree_model_get_iter (tree_model, &iter, path);
		gtk_tree_model_get (tree_model, &iter, 0, &header, -1);
		g_hash_table_remove (manager->priv->headers, header);
	}

	g_list_free_full (list, (GDestroyNotify) gtk_tree_path_free);

	g_object_notify (G_OBJECT (manager), "headers");

	mail_config_header_manager_update_list (manager);
}

static void
mail_config_header_manager_entry_activate (GtkEntry *entry,
                                           EMailConfigHeaderManager *manager)
{
	const gchar *header;

	header = gtk_entry_get_text (entry);
	if (mail_config_header_manager_add_header (manager, header))
		gtk_entry_set_text (entry, "");
}

static gboolean
mail_config_header_manager_key_press_event (GtkTreeView *tree_view,
                                            GdkEventKey *event,
                                            EMailConfigHeaderManager *manager)
{
	if (event->keyval == GDK_KEY_Delete) {
		mail_config_header_manager_remove_selected (manager);
		return TRUE;
	}

	return FALSE;
}

static void
mail_config_header_manager_add_clicked (GtkButton *button,
                                        EMailConfigHeaderManager *manager)
{
	GtkEntry *entry;
	const gchar *header;

	entry = GTK_ENTRY (manager->priv->entry);

	header = gtk_entry_get_text (entry);
	if (mail_config_header_manager_add_header (manager, header))
		gtk_entry_set_text (entry, "");
}

static void
mail_config_header_manager_remove_clicked (GtkButton *button,
                                           EMailConfigHeaderManager *manager)
{
	mail_config_header_manager_remove_selected (manager);
}

static void
mail_config_header_manager_selection_changed (GtkTreeSelection *selection,
                                              EMailConfigHeaderManager *manager)
{
	gint n_rows;

	n_rows = gtk_tree_selection_count_selected_rows (selection);
	gtk_widget_set_sensitive (manager->priv->remove_button, n_rows > 0);
}

static void
mail_config_header_manager_set_property (GObject *object,
                                         guint property_id,
                                         const GValue *value,
                                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_HEADERS:
			e_mail_config_header_manager_set_headers (
				E_MAIL_CONFIG_HEADER_MANAGER (object),
				g_value_get_boxed (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_header_manager_get_property (GObject *object,
                                         guint property_id,
                                         GValue *value,
                                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_HEADERS:
			g_value_take_boxed (
				value,
				e_mail_config_header_manager_dup_headers (
				E_MAIL_CONFIG_HEADER_MANAGER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_header_manager_finalize (GObject *object)
{
	EMailConfigHeaderManagerPrivate *priv;

	priv = E_MAIL_CONFIG_HEADER_MANAGER_GET_PRIVATE (object);

	g_hash_table_destroy (priv->headers);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_config_header_manager_parent_class)->
		finalize (object);
}

static void
mail_config_header_manager_constructed (GObject *object)
{
	EMailConfigHeaderManager *manager;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	GtkCellRenderer *renderer;
	GtkListStore *list_store;
	GtkWidget *widget;
	GtkWidget *container;

	manager = E_MAIL_CONFIG_HEADER_MANAGER (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_config_header_manager_parent_class)->
		constructed (object);

	gtk_grid_set_row_spacing (GTK_GRID (manager), 6);
	gtk_grid_set_column_spacing (GTK_GRID (manager), 12);

	widget = gtk_entry_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_grid_attach (GTK_GRID (manager), widget, 0, 0, 1, 1);
	manager->priv->entry = widget;  /* not referenced */
	gtk_widget_show (widget);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_widget_set_vexpand (widget, TRUE);
	gtk_grid_attach (GTK_GRID (manager), widget, 0, 1, 1, 1);
	gtk_widget_show (widget);

	container = widget;

	list_store = gtk_list_store_new (1, G_TYPE_STRING);
	widget = gtk_tree_view_new_with_model (GTK_TREE_MODEL (list_store));
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (widget), FALSE);
	gtk_container_add (GTK_CONTAINER (container), widget);
	manager->priv->tree_view = widget;  /* not referenced */
	gtk_widget_show (widget);
	g_object_unref (list_store);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

	widget = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
	gtk_box_set_spacing (GTK_BOX (widget), 6);
	gtk_button_box_set_layout (
		GTK_BUTTON_BOX (widget), GTK_BUTTONBOX_START);
	gtk_grid_attach (GTK_GRID (manager), widget, 1, 0, 1, 2);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_button_new_from_stock (GTK_STOCK_ADD);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	manager->priv->add_button = widget;  /* not referenced */
	gtk_widget_show (widget);

	widget = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	manager->priv->remove_button = widget;  /* not referenced */
	gtk_widget_show (widget);

	column = gtk_tree_view_column_new ();
	gtk_tree_view_append_column (
		GTK_TREE_VIEW (manager->priv->tree_view), column);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_cell_layout_add_attribute (
		GTK_CELL_LAYOUT (column), renderer, "text", 0);

	g_object_bind_property_full (
		manager->priv->entry, "text",
		manager->priv->add_button, "sensitive",
		G_BINDING_SYNC_CREATE,
		mail_config_header_manager_header_to_boolean,
		NULL,
		NULL, (GDestroyNotify) NULL);

	g_signal_connect (
		manager->priv->entry, "activate",
		G_CALLBACK (mail_config_header_manager_entry_activate),
		manager);

	g_signal_connect (
		manager->priv->tree_view, "key-press-event",
		G_CALLBACK (mail_config_header_manager_key_press_event),
		manager);

	g_signal_connect (
		manager->priv->add_button, "clicked",
		G_CALLBACK (mail_config_header_manager_add_clicked),
		manager);

	g_signal_connect (
		manager->priv->remove_button, "clicked",
		G_CALLBACK (mail_config_header_manager_remove_clicked),
		manager);

	g_signal_connect (
		selection, "changed",
		G_CALLBACK (mail_config_header_manager_selection_changed),
		manager);

	mail_config_header_manager_selection_changed (selection, manager);
}

static void
e_mail_config_header_manager_class_init (EMailConfigHeaderManagerClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (
		class, sizeof (EMailConfigHeaderManagerPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_config_header_manager_set_property;
	object_class->get_property = mail_config_header_manager_get_property;
	object_class->finalize = mail_config_header_manager_finalize;
	object_class->constructed = mail_config_header_manager_constructed;

	g_object_class_install_property (
		object_class,
		PROP_HEADERS,
		g_param_spec_boxed (
			"headers",
			"Headers",
			"Array of header names",
			G_TYPE_STRV,
			G_PARAM_READWRITE));
}

static void
e_mail_config_header_manager_class_finalize (EMailConfigHeaderManagerClass *class)
{
}

static void
e_mail_config_header_manager_init (EMailConfigHeaderManager *manager)
{
	manager->priv = E_MAIL_CONFIG_HEADER_MANAGER_GET_PRIVATE (manager);

	manager->priv->headers = g_hash_table_new_full (
		(GHashFunc) g_str_hash,
		(GEqualFunc) g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) NULL);
}

void
e_mail_config_header_manager_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_mail_config_header_manager_register_type (type_module);
}

GtkWidget *
e_mail_config_header_manager_new (void)
{
	return g_object_new (E_TYPE_MAIL_CONFIG_HEADER_MANAGER, NULL);
}

gchar **
e_mail_config_header_manager_dup_headers (EMailConfigHeaderManager *manager)
{
	GList *list, *link;
	gchar **headers = NULL;
	guint ii = 0;

	g_return_val_if_fail (E_IS_MAIL_CONFIG_HEADER_MANAGER (manager), NULL);

	list = g_hash_table_get_keys (manager->priv->headers);
	list = g_list_sort (list, (GCompareFunc) g_utf8_collate);

	if (list != NULL) {
		headers = g_new0 (gchar *, g_list_length (list) + 1);
		for (link = list; link != NULL; link = g_list_next (link))
			headers[ii++] = g_strdup (link->data);
		g_list_free (list);
	}

	return headers;
}

void
e_mail_config_header_manager_set_headers (EMailConfigHeaderManager *manager,
                                          const gchar * const *headers)
{
	gint ii = 0;

	g_return_if_fail (E_IS_MAIL_CONFIG_HEADER_MANAGER (manager));

	g_hash_table_remove_all (manager->priv->headers);

	while (headers != NULL && headers[ii] != NULL) {
		gchar *stripped_header;

		stripped_header = g_strstrip (g_strdup (headers[ii++]));

		if (*stripped_header != '\0')
			g_hash_table_insert (
				manager->priv->headers,
				stripped_header, stripped_header);
		else
			g_free (stripped_header);
	}

	g_object_notify (G_OBJECT (manager), "headers");

	mail_config_header_manager_update_list (manager);
}

