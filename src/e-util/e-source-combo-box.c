/* e-source-combo-box.c
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
 */

#include "evolution-config.h"

#include <libedataserverui/libedataserverui.h>

#include "e-source-combo-box.h"

struct _ESourceComboBoxPrivate {
	ESourceRegistry *registry;
	gchar *extension_name;
	GHashTable *hide_sources;
	GtkCellRenderer *name_renderer;

	gulong source_added_handler_id;
	gulong source_removed_handler_id;
	gulong source_enabled_handler_id;
	gulong source_disabled_handler_id;

	gboolean show_colors;
	gint max_natural_width;
	gboolean show_full_name;
};

enum {
	PROP_0,
	PROP_EXTENSION_NAME,
	PROP_REGISTRY,
	PROP_SHOW_COLORS,
	PROP_MAX_NATURAL_WIDTH
};

enum {
	COLUMN_COLOR,		/* GDK_TYPE_RGBA */
	COLUMN_NAME,		/* G_TYPE_STRING */
	COLUMN_FULL_NAME,	/* G_TYPE_STRING */
	COLUMN_SENSITIVE,	/* G_TYPE_BOOLEAN */
	COLUMN_UID,		/* G_TYPE_STRING */
	NUM_COLUMNS
};

G_DEFINE_TYPE_WITH_PRIVATE (ESourceComboBox, e_source_combo_box, GTK_TYPE_COMBO_BOX)

static gboolean
source_combo_box_traverse (GNode *node,
                           ESourceComboBox *combo_box)
{
	ESource *source;
	ESourceSelectable *extension = NULL;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GString *indented;
	GdkRGBA rgba;
	const gchar *ext_name;
	const gchar *display_name;
	const gchar *uid;
	gchar *full_name;
	gboolean sensitive = FALSE;
	gboolean use_color = FALSE;
	guint depth;

	/* Skip the root node. */
	if (G_NODE_IS_ROOT (node))
		return FALSE;

	ext_name = e_source_combo_box_get_extension_name (combo_box);

	source = E_SOURCE (node->data);

	if (ext_name != NULL && e_source_has_extension (source, ext_name)) {
		extension = e_source_get_extension (source, ext_name);
		sensitive = TRUE;

		if (g_hash_table_size (combo_box->priv->hide_sources) && E_IS_SOURCE_BACKEND (extension)) {
			ESourceBackend *backend = E_SOURCE_BACKEND (extension);

			if (e_source_backend_get_backend_name (backend) &&
			    g_hash_table_contains (combo_box->priv->hide_sources, e_source_backend_get_backend_name (backend))) {
				return FALSE;
			}
		}
	}

	uid = e_source_get_uid (source);

	if (g_hash_table_contains (combo_box->priv->hide_sources, uid) || (e_source_get_parent (source) &&
	    g_hash_table_contains (combo_box->priv->hide_sources, e_source_get_parent (source)))) {
		return FALSE;
	}

	display_name = e_source_get_display_name (source);
	full_name = e_util_get_source_full_name (combo_box->priv->registry, source);

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo_box));
	gtk_list_store_append (GTK_LIST_STORE (model), &iter);

	indented = g_string_new (NULL);

	depth = g_node_depth (node);
	g_warn_if_fail (depth > 1);
	while (--depth > 1)
		g_string_append (indented, "    ");
	g_string_append (indented, display_name);

	if (E_IS_SOURCE_SELECTABLE (extension)) {
		const gchar *color_spec;

		color_spec = e_source_selectable_get_color (extension);
		if (color_spec != NULL && *color_spec != '\0')
			use_color = gdk_rgba_parse (&rgba, color_spec);
	}

	gtk_list_store_set (
		GTK_LIST_STORE (model), &iter,
		COLUMN_COLOR, use_color ? &rgba : NULL,
		COLUMN_NAME, indented->str,
		COLUMN_FULL_NAME, full_name && *full_name ? full_name : display_name,
		COLUMN_SENSITIVE, sensitive,
		COLUMN_UID, uid,
		-1);

	g_string_free (indented, TRUE);
	g_free (full_name);

	return FALSE;
}

static void
source_combo_box_build_model (ESourceComboBox *combo_box)
{
	ESourceRegistry *registry;
	GtkComboBox *gtk_combo_box;
	GtkTreeModel *model;
	GNode *root;
	const gchar *active_id;
	const gchar *extension_name;

	registry = e_source_combo_box_get_registry (combo_box);
	extension_name = e_source_combo_box_get_extension_name (combo_box);

	gtk_combo_box = GTK_COMBO_BOX (combo_box);
	model = gtk_combo_box_get_model (gtk_combo_box);

	/* Constructor properties trigger this function before the
	 * list store is configured.  Detect it and return silently. */
	if (model == NULL)
		return;

	/* Remember the active ID so we can try to restore it. */
	active_id = gtk_combo_box_get_active_id (gtk_combo_box);

	gtk_list_store_clear (GTK_LIST_STORE (model));

	/* If we have no registry, leave the combo box empty. */
	if (registry == NULL)
		return;

	/* If we have no extension name, leave the combo box empty. */
	if (extension_name == NULL)
		return;

	root = e_source_registry_build_display_tree (registry, extension_name);

	g_node_traverse (
		root, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
		(GNodeTraverseFunc) source_combo_box_traverse,
		combo_box);

	e_source_registry_free_display_tree (root);

	/* Restore the active ID, or else set it to something reasonable. */
	gtk_combo_box_set_active_id (gtk_combo_box, active_id);
	if (gtk_combo_box_get_active_id (gtk_combo_box) == NULL) {
		ESource *source;

		source = e_source_registry_ref_default_for_extension_name (
			registry, extension_name);
		if (source != NULL) {
			e_source_combo_box_set_active (combo_box, source);
			g_object_unref (source);
		}
	}

	if (!gtk_combo_box_get_active_id (gtk_combo_box)) {
		GtkTreeIter iter;

		if (gtk_tree_model_get_iter_first (model, &iter)) {
			do {
				gboolean sensitive = FALSE;

				gtk_tree_model_get (model, &iter, COLUMN_SENSITIVE, &sensitive, -1);

				if (sensitive) {
					gchar *uid = NULL;

					gtk_tree_model_get (model, &iter, COLUMN_UID, &uid, -1);

					if (uid) {
						gtk_combo_box_set_active_id (gtk_combo_box, uid);
						g_free (uid);
						break;
					} else {
						g_free (uid);
					}
				}
			} while (gtk_tree_model_iter_next (model, &iter));
		}
	}
}

static void
source_combo_box_source_added_cb (ESourceRegistry *registry,
                                  ESource *source,
                                  ESourceComboBox *combo_box)
{
	source_combo_box_build_model (combo_box);
}

static void
source_combo_box_source_removed_cb (ESourceRegistry *registry,
                                    ESource *source,
                                    ESourceComboBox *combo_box)
{
	source_combo_box_build_model (combo_box);
}

static void
source_combo_box_source_enabled_cb (ESourceRegistry *registry,
                                    ESource *source,
                                    ESourceComboBox *combo_box)
{
	source_combo_box_build_model (combo_box);
}

static void
source_combo_box_source_disabled_cb (ESourceRegistry *registry,
                                     ESource *source,
                                     ESourceComboBox *combo_box)
{
	source_combo_box_build_model (combo_box);
}

static void
source_combo_box_get_preferred_width (GtkWidget *widget,
				      gint *minimum_width,
				      gint *natural_width)
{
	ESourceComboBox *combo_box = E_SOURCE_COMBO_BOX (widget);

	GTK_WIDGET_CLASS (e_source_combo_box_parent_class)->get_preferred_width (widget, minimum_width, natural_width);

	/* 50 = 25 for color + 25 for dropdown */
	if (combo_box->priv->max_natural_width > 0 &&
	    *natural_width > combo_box->priv->max_natural_width + (50 * gtk_widget_get_scale_factor (widget)))
		*natural_width = combo_box->priv->max_natural_width;
}

static void
source_combo_box_set_property (GObject *object,
                               guint property_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_EXTENSION_NAME:
			e_source_combo_box_set_extension_name (
				E_SOURCE_COMBO_BOX (object),
				g_value_get_string (value));
			return;

		case PROP_REGISTRY:
			e_source_combo_box_set_registry (
				E_SOURCE_COMBO_BOX (object),
				g_value_get_object (value));
			return;

		case PROP_SHOW_COLORS:
			e_source_combo_box_set_show_colors (
				E_SOURCE_COMBO_BOX (object),
				g_value_get_boolean (value));
			return;

		case PROP_MAX_NATURAL_WIDTH:
			e_source_combo_box_set_max_natural_width (
				E_SOURCE_COMBO_BOX (object),
				g_value_get_int (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
source_combo_box_get_property (GObject *object,
                               guint property_id,
                               GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_EXTENSION_NAME:
			g_value_set_string (
				value,
				e_source_combo_box_get_extension_name (
				E_SOURCE_COMBO_BOX (object)));
			return;

		case PROP_REGISTRY:
			g_value_set_object (
				value,
				e_source_combo_box_get_registry (
				E_SOURCE_COMBO_BOX (object)));
			return;

		case PROP_SHOW_COLORS:
			g_value_set_boolean (
				value,
				e_source_combo_box_get_show_colors (
				E_SOURCE_COMBO_BOX (object)));
			return;

		case PROP_MAX_NATURAL_WIDTH:
			g_value_set_int (
				value,
				e_source_combo_box_get_max_natural_width (
				E_SOURCE_COMBO_BOX (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
source_combo_box_dispose (GObject *object)
{
	ESourceComboBox *self = E_SOURCE_COMBO_BOX (object);

	self->priv->name_renderer = NULL;

	if (self->priv->registry) {
		g_signal_handler_disconnect (
			self->priv->registry,
			self->priv->source_added_handler_id);
		g_signal_handler_disconnect (
			self->priv->registry,
			self->priv->source_removed_handler_id);
		g_signal_handler_disconnect (
			self->priv->registry,
			self->priv->source_enabled_handler_id);
		g_signal_handler_disconnect (
			self->priv->registry,
			self->priv->source_disabled_handler_id);
		g_clear_object (&self->priv->registry);
	}

	/* Chain up to parent's "dispose" method. */
	G_OBJECT_CLASS (e_source_combo_box_parent_class)->dispose (object);
}

static void
source_combo_box_finalize (GObject *object)
{
	ESourceComboBox *self = E_SOURCE_COMBO_BOX (object);

	g_free (self->priv->extension_name);
	g_hash_table_destroy (self->priv->hide_sources);

	/* Chain up to parent's "finalize" method. */
	G_OBJECT_CLASS (e_source_combo_box_parent_class)->finalize (object);
}

static void
source_combo_box_constructed (GObject *object)
{
	ESourceComboBox *combo_box;
	GtkCellRenderer *renderer;
	GtkCellLayout *layout;
	GtkListStore *store;

	combo_box = E_SOURCE_COMBO_BOX (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_source_combo_box_parent_class)->constructed (object);

	store = gtk_list_store_new (
		NUM_COLUMNS,
		GDK_TYPE_RGBA,		/* COLUMN_COLOR */
		G_TYPE_STRING,		/* COLUMN_NAME */
		G_TYPE_STRING,		/* COLUMN_FULL_NAME */
		G_TYPE_BOOLEAN,		/* COLUMN_SENSITIVE */
		G_TYPE_STRING);		/* COLUMN_UID */
	gtk_combo_box_set_model (
		GTK_COMBO_BOX (combo_box),
		GTK_TREE_MODEL (store));
	g_object_unref (store);

	gtk_combo_box_set_id_column (GTK_COMBO_BOX (combo_box), COLUMN_UID);

	layout = GTK_CELL_LAYOUT (combo_box);

	renderer = e_cell_renderer_color_new ();
	gtk_cell_layout_pack_start (layout, renderer, FALSE);
	gtk_cell_layout_set_attributes (
		layout, renderer,
		"rgba", COLUMN_COLOR,
		"sensitive", COLUMN_SENSITIVE,
		NULL);

	e_binding_bind_property (
		combo_box, "show-colors",
		renderer, "visible",
		G_BINDING_SYNC_CREATE);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (layout, renderer, TRUE);
	gtk_cell_layout_set_attributes (
		layout, renderer,
		"text", combo_box->priv->show_full_name ? COLUMN_FULL_NAME : COLUMN_NAME,
		"sensitive", COLUMN_SENSITIVE,
		NULL);

	combo_box->priv->name_renderer = renderer;

	g_object_set (combo_box->priv->name_renderer,
		"ellipsize", combo_box->priv->max_natural_width > 0 ? PANGO_ELLIPSIZE_END : PANGO_ELLIPSIZE_NONE,
		NULL);

	source_combo_box_build_model (combo_box);
}

static void
e_source_combo_box_class_init (ESourceComboBoxClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

	widget_class->get_preferred_width = source_combo_box_get_preferred_width;

	object_class->set_property = source_combo_box_set_property;
	object_class->get_property = source_combo_box_get_property;
	object_class->dispose = source_combo_box_dispose;
	object_class->finalize = source_combo_box_finalize;
	object_class->constructed = source_combo_box_constructed;

	g_object_class_install_property (
		object_class,
		PROP_EXTENSION_NAME,
		g_param_spec_string (
			"extension-name",
			"Extension Name",
			"ESource extension name to filter",
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	/* XXX Don't use G_PARAM_CONSTRUCT_ONLY here.  We need to allow
	 *     for this class to be instantiated by a GtkBuilder with no
	 *     special construct parameters, and then subsequently give
	 *     it an ESourceRegistry. */
	g_object_class_install_property (
		object_class,
		PROP_REGISTRY,
		g_param_spec_object (
			"registry",
			"Registry",
			"Data source registry",
			E_TYPE_SOURCE_REGISTRY,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_COLORS,
		g_param_spec_boolean (
			"show-colors",
			"Show Colors",
			"Whether to show colors next to names",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_MAX_NATURAL_WIDTH,
		g_param_spec_int (
			"max-natural-width",
			"Max Natural Width",
			NULL,
			G_MININT, G_MAXINT, -1,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS |
			G_PARAM_EXPLICIT_NOTIFY));
}

static void
e_source_combo_box_init (ESourceComboBox *combo_box)
{
	combo_box->priv = e_source_combo_box_get_instance_private (combo_box);
	combo_box->priv->hide_sources = g_hash_table_new_full (camel_strcase_hash, camel_strcase_equal, g_free, NULL);
}

/**
 * e_source_combo_box_new:
 * @registry: an #ESourceRegistry, or %NULL
 * @extension_name: an #ESource extension name
 *
 * Creates a new #ESourceComboBox widget that lets the user pick an #ESource
 * from the provided #ESourceRegistry.  The displayed sources are restricted
 * to those which have an @extension_name extension.
 *
 * Returns: a new #ESourceComboBox
 *
 * Since: 2.22
 **/
GtkWidget *
e_source_combo_box_new (ESourceRegistry *registry,
                        const gchar *extension_name)
{
	if (registry != NULL)
		g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	return g_object_new (
		E_TYPE_SOURCE_COMBO_BOX, "registry", registry,
		"extension-name", extension_name, NULL);
}

/**
 * e_source_combo_box_get_registry:
 * @combo_box: an #ESourceComboBox
 *
 * Returns the #ESourceRegistry used to populate @combo_box.
 *
 * Returns: the #ESourceRegistry, or %NULL
 *
 * Since: 3.6
 **/
ESourceRegistry *
e_source_combo_box_get_registry (ESourceComboBox *combo_box)
{
	g_return_val_if_fail (E_IS_SOURCE_COMBO_BOX (combo_box), NULL);

	return combo_box->priv->registry;
}

/**
 * e_source_combo_box_set_registry:
 * @combo_box: an #ESourceComboBox
 * @registry: an #ESourceRegistry
 *
 * Sets the #ESourceRegistry used to populate @combo_box.
 *
 * This function is intended for cases where @combo_box is instantiated
 * by a #GtkBuilder and has to be given an #ESourceRegistry after it is
 * fully constructed.
 *
 * Since: 3.6
 **/
void
e_source_combo_box_set_registry (ESourceComboBox *combo_box,
                                 ESourceRegistry *registry)
{
	g_return_if_fail (E_IS_SOURCE_COMBO_BOX (combo_box));

	if (combo_box->priv->registry == registry)
		return;

	if (registry != NULL) {
		g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
		g_object_ref (registry);
	}

	if (combo_box->priv->registry != NULL) {
		g_signal_handler_disconnect (
			combo_box->priv->registry,
			combo_box->priv->source_added_handler_id);
		g_signal_handler_disconnect (
			combo_box->priv->registry,
			combo_box->priv->source_removed_handler_id);
		g_signal_handler_disconnect (
			combo_box->priv->registry,
			combo_box->priv->source_enabled_handler_id);
		g_signal_handler_disconnect (
			combo_box->priv->registry,
			combo_box->priv->source_disabled_handler_id);
		g_object_unref (combo_box->priv->registry);
	}

	combo_box->priv->registry = registry;

	combo_box->priv->source_added_handler_id = 0;
	combo_box->priv->source_removed_handler_id = 0;
	combo_box->priv->source_enabled_handler_id = 0;
	combo_box->priv->source_disabled_handler_id = 0;

	if (registry != NULL) {
		gulong handler_id;

		handler_id = g_signal_connect (
			registry, "source-added",
			G_CALLBACK (source_combo_box_source_added_cb),
			combo_box);
		combo_box->priv->source_added_handler_id = handler_id;

		handler_id = g_signal_connect (
			registry, "source-removed",
			G_CALLBACK (source_combo_box_source_removed_cb),
			combo_box);
		combo_box->priv->source_removed_handler_id = handler_id;

		handler_id = g_signal_connect (
			registry, "source-enabled",
			G_CALLBACK (source_combo_box_source_enabled_cb),
			combo_box);
		combo_box->priv->source_enabled_handler_id = handler_id;

		handler_id = g_signal_connect (
			registry, "source-disabled",
			G_CALLBACK (source_combo_box_source_disabled_cb),
			combo_box);
		combo_box->priv->source_disabled_handler_id = handler_id;
	}

	source_combo_box_build_model (combo_box);

	g_object_notify (G_OBJECT (combo_box), "registry");
}

/**
 * e_source_combo_box_get_extension_name:
 * @combo_box: an #ESourceComboBox
 *
 * Returns the extension name used to filter which data sources are
 * shown in @combo_box.
 *
 * Returns: the #ESource extension name
 *
 * Since: 3.6
 **/
const gchar *
e_source_combo_box_get_extension_name (ESourceComboBox *combo_box)
{
	g_return_val_if_fail (E_IS_SOURCE_COMBO_BOX (combo_box), NULL);

	return combo_box->priv->extension_name;
}

/**
 * e_source_combo_box_set_extension_name:
 * @combo_box: an #ESourceComboBox
 * @extension_name: an #ESource extension name
 *
 * Sets the extension name used to filter which data sources are shown in
 * @combo_box.
 *
 * Since: 3.6
 **/
void
e_source_combo_box_set_extension_name (ESourceComboBox *combo_box,
                                       const gchar *extension_name)
{
	g_return_if_fail (E_IS_SOURCE_COMBO_BOX (combo_box));

	if (g_strcmp0 (combo_box->priv->extension_name, extension_name) == 0)
		return;

	g_free (combo_box->priv->extension_name);
	combo_box->priv->extension_name = g_strdup (extension_name);

	source_combo_box_build_model (combo_box);

	g_object_notify (G_OBJECT (combo_box), "extension-name");
}

/**
 * e_source_combo_box_get_show_colors:
 * @combo_box: an #ESourceComboBox
 *
 * Returns whether colors are shown next to data sources.
 *
 * Returns: %TRUE if colors are being shown
 *
 * Since: 3.6
 **/
gboolean
e_source_combo_box_get_show_colors (ESourceComboBox *combo_box)
{
	g_return_val_if_fail (E_IS_SOURCE_COMBO_BOX (combo_box), FALSE);

	return combo_box->priv->show_colors;
}

/**
 * e_source_combo_box_set_show_colors:
 * @combo_box: an #ESourceComboBox
 * @show_colors: whether to show colors
 *
 * Sets whether to show colors next to data sources.
 *
 * Since: 3.6
 **/
void
e_source_combo_box_set_show_colors (ESourceComboBox *combo_box,
                                    gboolean show_colors)
{
	g_return_if_fail (E_IS_SOURCE_COMBO_BOX (combo_box));

	if ((show_colors ? 1 : 0) == (combo_box->priv->show_colors ? 1 : 0))
		return;

	combo_box->priv->show_colors = show_colors;

	source_combo_box_build_model (combo_box);

	g_object_notify (G_OBJECT (combo_box), "show-colors");
}

/**
 * e_source_combo_box_ref_active:
 * @combo_box: an #ESourceComboBox
 *
 * Returns the #ESource corresponding to the currently active item,
 * or %NULL if there is no active item.
 *
 * The returned #ESource is referenced for thread-safety and must be
 * unreferenced with g_object_unref() when finished with it.
 *
 * Returns: an #ESource or %NULL
 *
 * Since: 3.6
 **/
ESource *
e_source_combo_box_ref_active (ESourceComboBox *combo_box)
{
	ESourceRegistry *registry;
	GtkComboBox *gtk_combo_box;
	const gchar *active_id;

	g_return_val_if_fail (E_IS_SOURCE_COMBO_BOX (combo_box), NULL);

	registry = e_source_combo_box_get_registry (combo_box);

	gtk_combo_box = GTK_COMBO_BOX (combo_box);
	active_id = gtk_combo_box_get_active_id (gtk_combo_box);

	if (active_id == NULL)
		return NULL;

	return e_source_registry_ref_source (registry, active_id);
}

/**
 * e_source_combo_box_set_active:
 * @combo_box: an #ESourceComboBox
 * @source: an #ESource
 *
 * Sets the active item to the one corresponding to @source.
 *
 * Since: 2.22
 **/
void
e_source_combo_box_set_active (ESourceComboBox *combo_box,
                               ESource *source)
{
	GtkComboBox *gtk_combo_box;
	const gchar *uid;

	g_return_if_fail (E_IS_SOURCE_COMBO_BOX (combo_box));
	g_return_if_fail (E_IS_SOURCE (source));

	uid = e_source_get_uid (source);

	gtk_combo_box = GTK_COMBO_BOX (combo_box);
	gtk_combo_box_set_active_id (gtk_combo_box, uid);
}

/**
 * e_source_combo_box_hide_sources:
 * @combo_box: an #ESourceComboBox
 * @...: a NULL-terminated list of UID-s of the sources to hide
 *
 * The UID-s can be also backend names. Apart of that, these are checked
 * for both the ESource::uid and the ESource::parent.
 *
 * The next call replaces the list of source UID-s to be hidden.
 *
 * Since: 3.40
 **/
void
e_source_combo_box_hide_sources (ESourceComboBox *combo_box,
				 ...)
{
	const gchar *backend_name;
	va_list va;

	g_return_if_fail (E_IS_SOURCE_COMBO_BOX (combo_box));

	g_hash_table_remove_all (combo_box->priv->hide_sources);

	va_start (va, combo_box);

	while (backend_name = va_arg (va, const gchar *), backend_name) {
		g_hash_table_insert (combo_box->priv->hide_sources, g_strdup (backend_name), NULL);
	}

	va_end (va);

	source_combo_box_build_model (combo_box);
}

/**
 * e_source_combo_box_get_max_natural_width:
 * @combo_box: an #ESourceComboBox
 *
 * Returns max natural width for the combo box. The default is -1, which means
 * to use what the gtk+ calculates. Positive values clamp the natural width and
 * enable ellipsizing for the #ESource name.
 *
 * Returns: max natural width for the combo box
 *
 * Since: 3.46
 **/
gint
e_source_combo_box_get_max_natural_width (ESourceComboBox *combo_box)
{
	g_return_val_if_fail (E_IS_SOURCE_COMBO_BOX (combo_box), -1);

	return combo_box->priv->max_natural_width;
}

/**
 * e_source_combo_box_set_max_natural_width:
 * @combo_box: an #ESourceComboBox
 * @value: a value to set
 *
 * Sets max natural width for the combo box. Use -1 to use what
 * the gtk+ calculates. Positive values clamp the natural width
 * and enable ellipsizing for the #ESource name.
 *
 * Since: 3.46
 **/
void
e_source_combo_box_set_max_natural_width (ESourceComboBox *combo_box,
					  gint value)
{
	GtkWidget *widget;

	g_return_if_fail (E_IS_SOURCE_COMBO_BOX (combo_box));

	if (value == combo_box->priv->max_natural_width ||
	    (value <= 0 && combo_box->priv->max_natural_width <= 0))
		return;

	combo_box->priv->max_natural_width = value;

	if (combo_box->priv->name_renderer) {
		g_object_set (combo_box->priv->name_renderer,
			"ellipsize", combo_box->priv->max_natural_width > 0 ? PANGO_ELLIPSIZE_END : PANGO_ELLIPSIZE_NONE,
			NULL);
	}

	widget = GTK_WIDGET (combo_box);

	if (gtk_widget_get_realized (widget))
		gtk_widget_queue_resize (widget);

	g_object_notify (G_OBJECT (combo_box), "max-natural-width");
}

/**
 * e_source_combo_box_get_show_full_name:
 * @combo_box: an #ESourceComboBox
 *
 * Returns whether should show full name of the sources.
 *
 * Returns: whether should show full name of the sources
 *
 * Since: 3.50
 **/
gboolean
e_source_combo_box_get_show_full_name (ESourceComboBox *combo_box)
{
	g_return_val_if_fail (E_IS_SOURCE_COMBO_BOX (combo_box), FALSE);

	return combo_box->priv->show_full_name;
}

/**
 * e_source_combo_box_set_show_full_name:
 * @combo_box: an #ESourceComboBox
 * @show_full_name: value to set
 *
 * Sets whether should show full name of the sources.
 *
 * Since: 3.50
 **/
void
e_source_combo_box_set_show_full_name (ESourceComboBox *combo_box,
				       gboolean show_full_name)
{
	g_return_if_fail (E_IS_SOURCE_COMBO_BOX (combo_box));

	if ((combo_box->priv->show_full_name ? 1 : 0) == (show_full_name ? 1 : 0))
		return;

	combo_box->priv->show_full_name = show_full_name;

	if (combo_box->priv->name_renderer) {
		gtk_cell_layout_set_attributes (
			GTK_CELL_LAYOUT (combo_box), combo_box->priv->name_renderer,
			"text", combo_box->priv->show_full_name ? COLUMN_FULL_NAME : COLUMN_NAME,
			NULL);
	}
}
