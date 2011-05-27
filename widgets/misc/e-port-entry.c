/*
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 * Authors:
 *	Dan Vratil <dvratil@redhat.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-port-entry.h"

#include <stddef.h>
#include <string.h>
#include <glib.h>

struct _EPortEntryPrivate {
	guint port;
	gboolean is_valid;
};

enum {
	PORT_NUM_COLUMN,
	PORT_DESC_COLUMN,
	PORT_IS_SSL_COLUMN
};

enum {
	PROP_0,
	PROP_IS_VALID,
	PROP_PORT
};

G_DEFINE_TYPE (
	EPortEntry,
	e_port_entry,
	GTK_TYPE_COMBO_BOX)

static void
port_entry_set_is_valid (EPortEntry *port_entry,
                         gboolean is_valid)
{
	g_return_if_fail (E_IS_PORT_ENTRY (port_entry));

	port_entry->priv->is_valid = is_valid;

	g_object_notify (G_OBJECT (port_entry), "is-valid");
}

/**
 * Returns number of port currently selected in the widget, no matter
 * what value is in the PORT property
 */
static gint
port_entry_get_model_active_port (EPortEntry *port_entry)
{
	const gchar *port;

	port = gtk_combo_box_get_active_id (GTK_COMBO_BOX (port_entry));

	if (!port) {
		GtkWidget *entry = gtk_bin_get_child (GTK_BIN (port_entry));
		port = gtk_entry_get_text (GTK_ENTRY (entry));
	}

	return atoi (port);
}

static void
port_entry_set_property (GObject *object,
                         guint property_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_IS_VALID:
			port_entry_set_is_valid (
				E_PORT_ENTRY (object),
				g_value_get_boolean (value));
			return;
		case PROP_PORT:
			e_port_entry_set_port (
				E_PORT_ENTRY (object),
				g_value_get_uint (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
port_entry_get_property (GObject *object,
                         guint property_id,
                         GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_IS_VALID:
			g_value_set_boolean (
				value, e_port_entry_is_valid (
				E_PORT_ENTRY (object)));
			return;
		case PROP_PORT:
			g_value_set_uint (
				value, e_port_entry_get_port (
				E_PORT_ENTRY (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
port_entry_port_changed (EPortEntry *port_entry)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	const gchar *port;
	const gchar *tooltip;

	g_return_if_fail (E_IS_PORT_ENTRY (port_entry));

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (port_entry));
	g_return_if_fail (model);

	if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (port_entry), &iter)) {
		GtkWidget *entry = gtk_bin_get_child (GTK_BIN (port_entry));
		port = gtk_entry_get_text (GTK_ENTRY (entry));

		/* Try if user just haven't happened to enter a default port */
		gtk_combo_box_set_active_id (GTK_COMBO_BOX (port_entry), port);
	} else {
		gtk_tree_model_get (model, &iter, PORT_NUM_COLUMN, &port, -1);
	}

	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (port_entry), &iter)) {
		gtk_tree_model_get (model, &iter, PORT_DESC_COLUMN, &tooltip, -1);
		gtk_widget_set_tooltip_text (GTK_WIDGET (port_entry), tooltip);
	} else {
		gtk_widget_set_has_tooltip (GTK_WIDGET (port_entry), FALSE);
	}

	if (port == NULL || *port == '\0') {
		port_entry->priv->port = 0;
		port_entry_set_is_valid (port_entry, FALSE);
	} else {
		port_entry->priv->port = atoi (port);
		if ((port_entry->priv->port <= 0) ||
		    (port_entry->priv->port > G_MAXUINT16)) {
			port_entry->priv->port = 0;
			port_entry_set_is_valid (port_entry, FALSE);
		} else {
			port_entry_set_is_valid (port_entry, TRUE);
		}
	}

	g_object_notify (G_OBJECT (port_entry), "port");
}

static void
port_entry_get_preferred_width (GtkWidget *widget,
                                gint *minimum_size,
                                gint *natural_size)
{
	PangoContext *context;
	PangoFontMetrics *metrics;
	PangoFontDescription *font_desc;
	GtkStyleContext	*style_context;
	GtkStateFlags state;
	gint digit_width;
	gint parent_entry_width_min;
	gint parent_width_min;
	GtkWidget *entry;

	style_context = gtk_widget_get_style_context (widget);
	state = gtk_widget_get_state_flags (widget);
	gtk_style_context_get (
		style_context, state, "font", &font_desc, NULL);
	context = gtk_widget_get_pango_context (GTK_WIDGET (widget));
	metrics = pango_context_get_metrics (
		context, font_desc, pango_context_get_language (context));

	digit_width = PANGO_PIXELS (
		pango_font_metrics_get_approximate_digit_width (metrics));

	/* Preferred width of the entry */
	entry = gtk_bin_get_child (GTK_BIN (widget));
	gtk_widget_get_preferred_width (entry, NULL, &parent_entry_width_min);

	/* Preferred width of a standard combobox */
	GTK_WIDGET_CLASS (e_port_entry_parent_class)->
		get_preferred_width (widget, &parent_width_min, NULL);

	/* 6 * digit_width - port number has max 5
	 * digits + extra free space for better look */
	if (minimum_size != NULL)
		*minimum_size =
			parent_width_min - parent_entry_width_min +
			6 * digit_width;

	if (natural_size != NULL)
		*natural_size =
			parent_width_min - parent_entry_width_min +
			6 * digit_width;

	pango_font_metrics_unref (metrics);
	pango_font_description_free (font_desc);
}

static void
e_port_entry_class_init (EPortEntryClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (EPortEntryPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = port_entry_set_property;
	object_class->get_property = port_entry_get_property;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->get_preferred_width = port_entry_get_preferred_width;

	g_object_class_install_property (
		object_class,
		PROP_IS_VALID,
		g_param_spec_boolean (
			"is-valid",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_PORT,
		g_param_spec_uint (
			"port",
			NULL,
			NULL,
			0,		/* Min port, 0 = invalid port */
			G_MAXUINT16,	/* Max port */
			0,
			G_PARAM_READWRITE));
}

static void
e_port_entry_init (EPortEntry *port_entry)
{
	GtkCellRenderer *renderer;
	GtkListStore *store;

	port_entry->priv = G_TYPE_INSTANCE_GET_PRIVATE (
		port_entry, E_TYPE_PORT_ENTRY, EPortEntryPrivate);
	port_entry->priv->port = 0;
	port_entry->priv->is_valid = FALSE;

	store = gtk_list_store_new (
		3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);

	gtk_combo_box_set_model (
		GTK_COMBO_BOX (port_entry), GTK_TREE_MODEL (store));
	gtk_combo_box_set_entry_text_column (
		GTK_COMBO_BOX (port_entry), PORT_NUM_COLUMN);
	gtk_combo_box_set_id_column (
		GTK_COMBO_BOX (port_entry), PORT_NUM_COLUMN);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_renderer_set_sensitive (renderer, TRUE);
	gtk_cell_layout_pack_start (
		GTK_CELL_LAYOUT (port_entry), renderer, FALSE);
	gtk_cell_layout_add_attribute (
		GTK_CELL_LAYOUT (port_entry),
		renderer, "text", PORT_NUM_COLUMN);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_renderer_set_sensitive (renderer, FALSE);
	gtk_cell_layout_pack_start (
		GTK_CELL_LAYOUT (port_entry), renderer, TRUE);
	gtk_cell_layout_add_attribute (
		GTK_CELL_LAYOUT (port_entry),
		renderer, "text", PORT_DESC_COLUMN);

	/* Update the port property when port is changed */
	g_signal_connect (
		port_entry, "changed",
		G_CALLBACK (port_entry_port_changed), NULL);
}

GtkWidget *
e_port_entry_new (void)
{
	return g_object_new (
		E_TYPE_PORT_ENTRY, "has-entry", TRUE, NULL);
}

void
e_port_entry_set_camel_entries (EPortEntry *port_entry,
                                CamelProviderPortEntry *entries)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkListStore *store;
	gint i = 0;

	g_return_if_fail (E_IS_PORT_ENTRY (port_entry));
	g_return_if_fail (entries);

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (port_entry));
	store = GTK_LIST_STORE (model);

	gtk_list_store_clear (store);

	while (entries[i].port > 0) {
		gchar *port_string;

		port_string = g_strdup_printf ("%i", entries[i].port);

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (
			store, &iter,
			PORT_NUM_COLUMN, port_string,
			PORT_DESC_COLUMN, entries[i].desc,
			PORT_IS_SSL_COLUMN, entries[i].is_ssl,
			-1);
		i++;

		g_free (port_string);
	}

	/* Activate the first port */
	if (i > 0)
		e_port_entry_set_port (port_entry, entries[0].port);
}

void
e_port_entry_security_port_changed (EPortEntry *port_entry,
                                    gchar *ssl)
{
	g_return_if_fail (E_IS_PORT_ENTRY (port_entry));
	g_return_if_fail (ssl != NULL);

	if (strcmp (ssl, "always") == 0) {
		e_port_entry_activate_secured_port (port_entry, 0);
	} else {
		e_port_entry_activate_nonsecured_port (port_entry, 0);
	}
}

gint
e_port_entry_get_port (EPortEntry *port_entry)
{
	g_return_val_if_fail (E_IS_PORT_ENTRY (port_entry), 0);

	return port_entry->priv->port;
}

void
e_port_entry_set_port (EPortEntry *port_entry,
                       gint port)
{
	g_return_if_fail (E_IS_PORT_ENTRY (port_entry));

	port_entry->priv->port = port;
	if ((port <= 0) || (port > G_MAXUINT16))
		port_entry_set_is_valid (port_entry, FALSE);
	else {
		gchar *port_string;

		port_string = g_strdup_printf ("%i", port);

		gtk_combo_box_set_active_id (
			GTK_COMBO_BOX (port_entry), port_string);

		if (port_entry_get_model_active_port (port_entry) != port) {
			GtkWidget *entry;

			entry = gtk_bin_get_child (GTK_BIN (port_entry));
			gtk_entry_set_text (GTK_ENTRY (entry), port_string);
		}

		port_entry_set_is_valid (port_entry, TRUE);

		g_free (port_string);
	}

	g_object_notify (G_OBJECT (port_entry), "port");
}

gboolean
e_port_entry_is_valid (EPortEntry *port_entry)
{
	g_return_val_if_fail (E_IS_PORT_ENTRY (port_entry), FALSE);

	return port_entry->priv->is_valid;
}

/**
 * If there are more then one secured port in the model, you can specify
 * which of the secured ports should be activated by specifying the index.
 * The index counts only for secured ports, so if you have 5 ports of which
 * ports 1, 3 and 5 are secured, the association is 0=>1, 1=>3, 2=>5
 */
void
e_port_entry_activate_secured_port (EPortEntry *port_entry,
                                    gint index)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean is_ssl;
	gint iters = 0;

	g_return_if_fail (E_IS_PORT_ENTRY (port_entry));

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (port_entry));

	if (!gtk_tree_model_get_iter_first (model, &iter))
		return;

	do {
		gtk_tree_model_get (
			model, &iter, PORT_IS_SSL_COLUMN, &is_ssl, -1);
		if (is_ssl && (iters == index)) {
			gtk_combo_box_set_active_iter (
				GTK_COMBO_BOX (port_entry), &iter);
			return;
		}

		if (is_ssl)
			iters++;

	} while (gtk_tree_model_iter_next (model, &iter));
}

/**
 * If there are more then one unsecured port in the model, you can specify
 * which of the unsecured ports should be activated by specifiying the index.
 * The index counts only for unsecured ports, so if you have 5 ports, of which
 * ports 2 and 4 are unsecured, the associtation is 0=>2, 1=>4
 */
void
e_port_entry_activate_nonsecured_port (EPortEntry *port_entry,
                                       gint index)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean is_ssl;
	gint iters = 0;

	g_return_if_fail (E_IS_PORT_ENTRY (port_entry));

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (port_entry));

	if (!gtk_tree_model_get_iter_first (model, &iter))
		return;

	do {
		gtk_tree_model_get (model, &iter, PORT_IS_SSL_COLUMN, &is_ssl, -1);
		if (!is_ssl && (iters == index)) {
			gtk_combo_box_set_active_iter (
				GTK_COMBO_BOX (port_entry), &iter);
			return;
		}

		if (!is_ssl)
			iters++;

	} while (gtk_tree_model_iter_next (model, &iter));
}
