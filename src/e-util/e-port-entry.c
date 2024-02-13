/*
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *	Dan Vratil <dvratil@redhat.com>
 */

#include "evolution-config.h"

#include <errno.h>
#include <stddef.h>
#include <string.h>

#include "e-port-entry.h"

struct _EPortEntryPrivate {
	CamelNetworkSecurityMethod method;
	CamelProviderPortEntry *entries;
};

enum {
	PORT_NUM_COLUMN,
	PORT_DESC_COLUMN,
	PORT_IS_SSL_COLUMN
};

enum {
	PROP_0,
	PROP_IS_VALID,
	PROP_PORT,
	PROP_SECURITY_METHOD
};

G_DEFINE_TYPE_WITH_PRIVATE (EPortEntry, e_port_entry, GTK_TYPE_COMBO_BOX)

static GtkEntry *
port_entry_get_entry (EPortEntry *port_entry)
{
	return GTK_ENTRY (gtk_bin_get_child (GTK_BIN (port_entry)));
}

static gboolean
port_entry_get_numeric_port (EPortEntry *port_entry,
                             gint *out_port)
{
	GtkEntry *entry;
	const gchar *port_string;
	gboolean valid;
	gint port;

	entry = port_entry_get_entry (port_entry);

	port_string = gtk_entry_get_text (entry);
	g_return_val_if_fail (port_string != NULL, FALSE);

	errno = 0;
	port = strtol (port_string, NULL, 10);
	valid = (errno == 0) && (port == CLAMP (port, 1, G_MAXUINT16));

	if (valid && out_port != NULL)
		*out_port = port;

	return valid;
}

static void
port_entry_text_changed (GtkEditable *editable,
                         EPortEntry *port_entry)
{
	GObject *object = G_OBJECT (port_entry);
	const gchar *desc = NULL;
	gint port = 0;
	gint ii = 0;

	g_object_freeze_notify (object);

	port_entry_get_numeric_port (port_entry, &port);

	if (port_entry->priv->entries != NULL) {
		while (port_entry->priv->entries[ii].port > 0) {
			if (port == port_entry->priv->entries[ii].port) {
				desc = port_entry->priv->entries[ii].desc;
				break;
			}
			ii++;
		}
	}

	if (desc != NULL)
		gtk_widget_set_tooltip_text (GTK_WIDGET (port_entry), desc);
	else
		gtk_widget_set_has_tooltip (GTK_WIDGET (port_entry), FALSE);

	g_object_notify (object, "port");
	g_object_notify (object, "is-valid");

	g_object_thaw_notify (object);
}

static void
port_entry_method_changed (EPortEntry *port_entry)
{
	CamelNetworkSecurityMethod method;
	gboolean standard_port = FALSE;
	gboolean valid, have_ssl = FALSE, have_nossl = FALSE;
	gint port = 0;
	gint ii;

	method = e_port_entry_get_security_method (port_entry);
	valid = port_entry_get_numeric_port (port_entry, &port);

	/* Only change the port number if it's currently on a standard
	 * port (i.e. listed in a CamelProviderPortEntry).  Otherwise,
	 * leave custom port numbers alone. */

	if (valid && port_entry->priv->entries != NULL) {
		for (ii = 0; port_entry->priv->entries[ii].port > 0 && (!have_ssl || !have_nossl); ii++) {
			/* Use only the first SSL/no-SSL port as a default in the list
			 * and skip the others */
			if (port_entry->priv->entries[ii].is_ssl) {
				if (have_ssl)
					continue;
				have_ssl = TRUE;
			} else {
				if (have_nossl)
					continue;
				have_nossl = TRUE;
			}

			if (port == port_entry->priv->entries[ii].port) {
				standard_port = TRUE;
				break;
			}
		}
	}

	if (valid && !standard_port)
		return;

	switch (method) {
		case CAMEL_NETWORK_SECURITY_METHOD_SSL_ON_ALTERNATE_PORT:
			e_port_entry_activate_secured_port (port_entry, 0);
			break;
		default:
			e_port_entry_activate_nonsecured_port (port_entry, 0);
			break;
	}
}

static void
port_entry_set_property (GObject *object,
                         guint property_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_PORT:
			e_port_entry_set_port (
				E_PORT_ENTRY (object),
				g_value_get_uint (value));
			return;

		case PROP_SECURITY_METHOD:
			e_port_entry_set_security_method (
				E_PORT_ENTRY (object),
				g_value_get_enum (value));
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

		case PROP_SECURITY_METHOD:
			g_value_set_enum (
				value, e_port_entry_get_security_method (
				E_PORT_ENTRY (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
port_entry_constructed (GObject *object)
{
	GtkEntry *entry;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_port_entry_parent_class)->constructed (object);

	entry = port_entry_get_entry (E_PORT_ENTRY (object));

	g_signal_connect_after (
		entry, "changed",
		G_CALLBACK (port_entry_text_changed), object);

	gtk_entry_set_width_chars (entry, 5);
}

static void
port_entry_get_preferred_width (GtkWidget *widget,
                                gint *minimum_size,
                                gint *natural_size)
{
	PangoContext *context;
	PangoFontMetrics *metrics;
	PangoFontDescription *font_desc;
	GtkStyleContext *style_context;
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

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = port_entry_set_property;
	object_class->get_property = port_entry_get_property;
	object_class->constructed = port_entry_constructed;

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
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

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
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SECURITY_METHOD,
		g_param_spec_enum (
			"security-method",
			"Security Method",
			"Method used to establish a network connection",
			CAMEL_TYPE_NETWORK_SECURITY_METHOD,
			CAMEL_NETWORK_SECURITY_METHOD_NONE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));
}

static void
e_port_entry_init (EPortEntry *port_entry)
{
	GtkCellRenderer *renderer;
	GtkListStore *store;

	port_entry->priv = e_port_entry_get_instance_private (port_entry);

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
	GtkComboBox *combo_box;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkListStore *store;
	gint port = 0;
	gint i = 0;

	g_return_if_fail (E_IS_PORT_ENTRY (port_entry));
	g_return_if_fail (entries);

	port_entry->priv->entries = entries;

	combo_box = GTK_COMBO_BOX (port_entry);
	model = gtk_combo_box_get_model (combo_box);

	store = GTK_LIST_STORE (model);
	gtk_list_store_clear (store);

	while (entries[i].port > 0) {
		gchar *port_string;

		/* Grab the first port number. */
		if (port == 0)
			port = entries[i].port;

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

	e_port_entry_set_port (port_entry, port);
}

gint
e_port_entry_get_port (EPortEntry *port_entry)
{
	gint port = 0;

	g_return_val_if_fail (E_IS_PORT_ENTRY (port_entry), 0);

	port_entry_get_numeric_port (port_entry, &port);

	return port;
}

void
e_port_entry_set_port (EPortEntry *port_entry,
                       gint port)
{
	GtkEntry *entry;
	gchar *port_string;

	g_return_if_fail (E_IS_PORT_ENTRY (port_entry));

	entry = port_entry_get_entry (port_entry);
	port_string = g_strdup_printf ("%i", port);
	gtk_entry_set_text (entry, port_string);
	g_free (port_string);
}

gboolean
e_port_entry_is_valid (EPortEntry *port_entry)
{
	g_return_val_if_fail (E_IS_PORT_ENTRY (port_entry), FALSE);

	return port_entry_get_numeric_port (port_entry, NULL);
}

CamelNetworkSecurityMethod
e_port_entry_get_security_method (EPortEntry *port_entry)
{
	g_return_val_if_fail (
		E_IS_PORT_ENTRY (port_entry),
		CAMEL_NETWORK_SECURITY_METHOD_NONE);

	return port_entry->priv->method;
}

void
e_port_entry_set_security_method (EPortEntry *port_entry,
                                  CamelNetworkSecurityMethod method)
{
	g_return_if_fail (E_IS_PORT_ENTRY (port_entry));

	port_entry->priv->method = method;

	port_entry_method_changed (port_entry);

	g_object_notify (G_OBJECT (port_entry), "security-method");
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
