/*
 * e-proxy-combo-box.c
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

/**
 * SECTION: e-proxy-combo-box
 * @include: e-util/e-util.h
 * @short_description: Combo box of proxy profiles
 *
 * #EProxyComboBox is a combo box of available proxy profiles, as described
 * by #ESource instances with an #ESourceProxy extension.  For convenience,
 * the combo box model's #GtkComboBox:id-column is populated with #ESource
 * #ESource:uid strings.
 **/

#include "evolution-config.h"

#include "e-proxy-combo-box.h"

struct _EProxyComboBoxPrivate {
	ESourceRegistry *registry;
	gulong source_added_handler_id;
	gulong source_changed_handler_id;
	gulong source_removed_handler_id;

	guint refresh_idle_id;
};

enum {
	PROP_0,
	PROP_REGISTRY
};

enum {
	COLUMN_DISPLAY_NAME,
	COLUMN_UID
};

G_DEFINE_TYPE_WITH_PRIVATE (EProxyComboBox, e_proxy_combo_box, GTK_TYPE_COMBO_BOX)

static gboolean
proxy_combo_box_refresh_idle_cb (gpointer user_data)
{
	EProxyComboBox *combo_box = user_data;

	/* The refresh function will clear the idle ID. */
	e_proxy_combo_box_refresh (combo_box);

	return FALSE;
}

static void
proxy_combo_box_schedule_refresh (EProxyComboBox *combo_box)
{
	/* Use an idle callback to limit how frequently we refresh
	 * the tree model in case the registry is emitting lots of
	 * signals at once. */

	if (combo_box->priv->refresh_idle_id == 0) {
		combo_box->priv->refresh_idle_id = g_idle_add (
			proxy_combo_box_refresh_idle_cb, combo_box);
	}
}

static void
proxy_combo_box_source_added_cb (ESourceRegistry *registry,
                                 ESource *source,
                                 EProxyComboBox *combo_box)
{
	if (e_source_has_extension (source, E_SOURCE_EXTENSION_PROXY))
		proxy_combo_box_schedule_refresh (combo_box);
}

static void
proxy_combo_box_source_changed_cb (ESourceRegistry *registry,
                                   ESource *source,
                                   EProxyComboBox *combo_box)
{
	if (e_source_has_extension (source, E_SOURCE_EXTENSION_PROXY))
		proxy_combo_box_schedule_refresh (combo_box);
}

static void
proxy_combo_box_source_removed_cb (ESourceRegistry *registry,
                                   ESource *source,
                                   EProxyComboBox *combo_box)
{
	if (e_source_has_extension (source, E_SOURCE_EXTENSION_PROXY))
		proxy_combo_box_schedule_refresh (combo_box);
}

static void
proxy_combo_box_set_registry (EProxyComboBox *combo_box,
                              ESourceRegistry *registry)
{
	gulong handler_id;

	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (combo_box->priv->registry == NULL);

	combo_box->priv->registry = g_object_ref (registry);

	handler_id = g_signal_connect (
		registry, "source-added",
		G_CALLBACK (proxy_combo_box_source_added_cb), combo_box);
	combo_box->priv->source_added_handler_id = handler_id;

	handler_id = g_signal_connect (
		registry, "source-changed",
		G_CALLBACK (proxy_combo_box_source_changed_cb), combo_box);
	combo_box->priv->source_changed_handler_id = handler_id;

	handler_id = g_signal_connect (
		registry, "source-removed",
		G_CALLBACK (proxy_combo_box_source_removed_cb), combo_box);
	combo_box->priv->source_removed_handler_id = handler_id;
}

static void
proxy_combo_box_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_REGISTRY:
			proxy_combo_box_set_registry (
				E_PROXY_COMBO_BOX (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
proxy_combo_box_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_REGISTRY:
			g_value_set_object (
				value,
				e_proxy_combo_box_get_registry (
				E_PROXY_COMBO_BOX (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
proxy_combo_box_dispose (GObject *object)
{
	EProxyComboBox *self = E_PROXY_COMBO_BOX (object);

	if (self->priv->source_added_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->registry,
			self->priv->source_added_handler_id);
		self->priv->source_added_handler_id = 0;
	}

	if (self->priv->source_changed_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->registry,
			self->priv->source_changed_handler_id);
		self->priv->source_changed_handler_id = 0;
	}

	if (self->priv->source_removed_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->registry,
			self->priv->source_removed_handler_id);
		self->priv->source_removed_handler_id = 0;
	}

	if (self->priv->refresh_idle_id > 0) {
		g_source_remove (self->priv->refresh_idle_id);
		self->priv->refresh_idle_id = 0;
	}

	g_clear_object (&self->priv->registry);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_proxy_combo_box_parent_class)->dispose (object);
}

static void
proxy_combo_box_constructed (GObject *object)
{
	GtkListStore *list_store;
	GtkComboBox *combo_box;
	GtkCellLayout *cell_layout;
	GtkCellRenderer *cell_renderer;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_proxy_combo_box_parent_class)->constructed (object);

	combo_box = GTK_COMBO_BOX (object);
	cell_layout = GTK_CELL_LAYOUT (object);

	list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
	gtk_combo_box_set_model (combo_box, GTK_TREE_MODEL (list_store));
	gtk_combo_box_set_id_column (combo_box, COLUMN_UID);
	g_object_unref (list_store);

	cell_renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (cell_layout, cell_renderer, TRUE);
	gtk_cell_layout_add_attribute (
		cell_layout, cell_renderer, "text", COLUMN_DISPLAY_NAME);

	e_proxy_combo_box_refresh (E_PROXY_COMBO_BOX (object));
}

static void
e_proxy_combo_box_class_init (EProxyComboBoxClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = proxy_combo_box_set_property;
	object_class->get_property = proxy_combo_box_get_property;
	object_class->dispose = proxy_combo_box_dispose;
	object_class->constructed = proxy_combo_box_constructed;

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
}

static void
e_proxy_combo_box_init (EProxyComboBox *combo_box)
{
	combo_box->priv = e_proxy_combo_box_get_instance_private (combo_box);
}

/**
 * e_proxy_combo_box_new:
 * @registry: an #ESourceRegistry
 *
 * Creates a new #EProxyComboBox widget using #ESource instances in @registry.
 *
 * Returns: a new #EProxyComboBox
 **/
GtkWidget *
e_proxy_combo_box_new (ESourceRegistry *registry)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	return g_object_new (
		E_TYPE_PROXY_COMBO_BOX,
		"registry", registry, NULL);
}

/**
 * e_proxy_combo_box_refresh:
 * @combo_box: an #EProxyComboBox
 *
 * Rebuilds the combo box model with an updated list of #ESource instances
 * that describe a network proxy profile, without disrupting the previously
 * active item (if possible).
 *
 * This function is called automatically in response to #ESourceRegistry
 * signals which are pertinent to the @combo_box.
 **/
void
e_proxy_combo_box_refresh (EProxyComboBox *combo_box)
{
	ESourceRegistry *registry;
	GtkTreeModel *tree_model;
	GtkComboBox *gtk_combo_box;
	ESource *builtin_source;
	GList *list, *link;
	const gchar *extension_name;
	const gchar *saved_uid;

	g_return_if_fail (E_IS_PROXY_COMBO_BOX (combo_box));

	if (combo_box->priv->refresh_idle_id > 0) {
		g_source_remove (combo_box->priv->refresh_idle_id);
		combo_box->priv->refresh_idle_id = 0;
	}

	gtk_combo_box = GTK_COMBO_BOX (combo_box);
	tree_model = gtk_combo_box_get_model (gtk_combo_box);

	/* This is an interned string, which means it's safe
	 * to use even after clearing the combo box model. */
	saved_uid = gtk_combo_box_get_active_id (gtk_combo_box);

	gtk_list_store_clear (GTK_LIST_STORE (tree_model));

	extension_name = E_SOURCE_EXTENSION_PROXY;
	registry = e_proxy_combo_box_get_registry (combo_box);
	list = e_source_registry_list_enabled (registry, extension_name);

	builtin_source = e_source_registry_ref_builtin_proxy (registry);
	g_warn_if_fail (builtin_source != NULL);

	/* Always list the built-in proxy profile first. */
	link = g_list_find (list, builtin_source);
	if (link != NULL && list != link) {
		list = g_list_remove_link (list, link);
		list = g_list_concat (link, list);
	}

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *source;
		GtkTreeIter iter;
		const gchar *display_name;
		const gchar *uid;

		source = E_SOURCE (link->data);
		display_name = e_source_get_display_name (source);
		uid = e_source_get_uid (source);

		gtk_list_store_append (GTK_LIST_STORE (tree_model), &iter);

		gtk_list_store_set (
			GTK_LIST_STORE (tree_model), &iter,
			COLUMN_DISPLAY_NAME, display_name,
			COLUMN_UID, uid, -1);
	}

	g_clear_object (&builtin_source);

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	/* Try and restore the previous selected source or else pick
	 * the built-in proxy profile, which is always listed first. */

	if (saved_uid != NULL)
		gtk_combo_box_set_active_id (gtk_combo_box, saved_uid);

	if (gtk_combo_box_get_active_id (gtk_combo_box) == NULL)
		gtk_combo_box_set_active (gtk_combo_box, 0);
}

/**
 * e_proxy_combo_box_get_registry:
 * @combo_box: an #EProxyComboBox
 *
 * Returns the #ESourceRegistry passed to e_proxy_combo_box_new().
 *
 * Returns: an #ESourceRegistry
 **/
ESourceRegistry *
e_proxy_combo_box_get_registry (EProxyComboBox *combo_box)
{
	g_return_val_if_fail (E_IS_PROXY_COMBO_BOX (combo_box), NULL);

	return combo_box->priv->registry;
}

