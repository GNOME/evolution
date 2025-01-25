/*
 * e-mail-config-provider-page.c
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

#include <string.h>
#include <glib/gi18n-lib.h>

#include <libebackend/libebackend.h>

#include "e-mail-config-provider-page.h"

#define STANDARD_MARGIN   12
#define DEPENDENCY_MARGIN 24

struct _EMailConfigProviderPagePrivate {
	EMailConfigServiceBackend *backend;
	gboolean is_empty;
};

enum {
	PROP_0,
	PROP_BACKEND
};

/* Forward Declarations */
static void	e_mail_config_provider_page_interface_init
					(EMailConfigPageInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EMailConfigProviderPage, e_mail_config_provider_page, E_TYPE_MAIL_CONFIG_ACTIVITY_PAGE,
	G_ADD_PRIVATE (EMailConfigProviderPage)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL)
	G_IMPLEMENT_INTERFACE (E_TYPE_MAIL_CONFIG_PAGE, e_mail_config_provider_page_interface_init))

static void
mail_config_provider_page_handle_dependency (CamelSettings *settings,
                                             CamelProviderConfEntry *entry,
                                             GtkWidget *widget)
{
	GBindingFlags binding_flags = G_BINDING_SYNC_CREATE;
	const gchar *depname = entry->depname;
	gint margin;

	if (depname == NULL)
		return;

	if (*depname == '!') {
		binding_flags |= G_BINDING_INVERT_BOOLEAN;
		depname++;
	}

	e_binding_bind_property (
		settings, depname,
		widget, "sensitive",
		binding_flags);

	/* Further indent the widget to show its dependency. */
	margin = gtk_widget_get_margin_start (widget);
	gtk_widget_set_margin_start (widget, margin + DEPENDENCY_MARGIN);
}

static void
mail_config_provider_page_add_section (GtkBox *main_box,
				       CamelProvider *provider,
				       CamelProviderConfEntry *entry,
				       gboolean skip_first_section_name)
{
	GtkWidget *widget;
	gchar *markup;

	g_return_if_fail (entry->text != NULL);

	markup = g_markup_printf_escaped ("<b>%s</b>", entry->text);

	widget = gtk_label_new (markup);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_box_pack_start (main_box, widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	/* Skip the top margin if this is the first entry. */
	if (entry != provider->extra_conf || !skip_first_section_name)
		gtk_widget_set_margin_top (widget, 6);

	g_free (markup);
}

static void
mail_config_provider_page_add_checkbox (GtkBox *main_box,
					CamelSettings *settings,
					CamelProviderConfEntry *entry)
{
	GtkWidget *widget;

	g_return_if_fail (entry->text != NULL);

	widget = gtk_check_button_new_with_mnemonic (entry->text);
	gtk_widget_set_margin_start (widget, STANDARD_MARGIN);
	gtk_box_pack_start (main_box, widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	e_binding_bind_property (
		settings, entry->name,
		widget, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	mail_config_provider_page_handle_dependency (settings, entry, widget);
}

static void
mail_config_provider_page_add_checkspin (GtkBox *main_box,
					 CamelSettings *settings,
					 CamelProviderConfEntry *entry)
{
	GObjectClass *class;
	GParamSpec *pspec;
	GParamSpec *use_pspec;
	GtkAdjustment *adjustment;
	GtkWidget *hbox, *spin;
	GtkWidget *prefix;
	gchar *use_property_name;
	gchar *pre, *post;

	g_return_if_fail (entry->text != NULL);

	/* The entry->name property (e.g. "foo") should be numeric for the
	 * spin button.  If a "use" boolean property exists (e.g. "use-foo")
	 * then a checkbox is also shown. */

	class = G_OBJECT_GET_CLASS (settings);
	pspec = g_object_class_find_property (class, entry->name);
	g_return_if_fail (pspec != NULL);

	use_property_name = g_strconcat ("use-", entry->name, NULL);
	use_pspec = g_object_class_find_property (class, use_property_name);
	if (use_pspec != NULL && use_pspec->value_type != G_TYPE_BOOLEAN)
		use_pspec = NULL;
	g_free (use_property_name);

	/* Make sure we can convert to and from doubles. */
	g_return_if_fail (
		g_value_type_transformable (
		pspec->value_type, G_TYPE_DOUBLE));
	g_return_if_fail (
		g_value_type_transformable (
		G_TYPE_DOUBLE, pspec->value_type));

	if (G_IS_PARAM_SPEC_CHAR (pspec)) {
		GParamSpecChar *pspec_char;
		pspec_char = G_PARAM_SPEC_CHAR (pspec);
		adjustment = gtk_adjustment_new (
			(gdouble) pspec_char->default_value,
			(gdouble) pspec_char->minimum,
			(gdouble) pspec_char->maximum,
			1.0, 1.0, 0.0);

	} else if (G_IS_PARAM_SPEC_UCHAR (pspec)) {
		GParamSpecUChar *pspec_uchar;
		pspec_uchar = G_PARAM_SPEC_UCHAR (pspec);
		adjustment = gtk_adjustment_new (
			(gdouble) pspec_uchar->default_value,
			(gdouble) pspec_uchar->minimum,
			(gdouble) pspec_uchar->maximum,
			1.0, 1.0, 0.0);

	} else if (G_IS_PARAM_SPEC_INT (pspec)) {
		GParamSpecInt *pspec_int;
		pspec_int = G_PARAM_SPEC_INT (pspec);
		adjustment = gtk_adjustment_new (
			(gdouble) pspec_int->default_value,
			(gdouble) pspec_int->minimum,
			(gdouble) pspec_int->maximum,
			1.0, 1.0, 0.0);

	} else if (G_IS_PARAM_SPEC_UINT (pspec)) {
		GParamSpecUInt *pspec_uint;
		pspec_uint = G_PARAM_SPEC_UINT (pspec);
		adjustment = gtk_adjustment_new (
			(gdouble) pspec_uint->default_value,
			(gdouble) pspec_uint->minimum,
			(gdouble) pspec_uint->maximum,
			1.0, 1.0, 0.0);

	} else if (G_IS_PARAM_SPEC_LONG (pspec)) {
		GParamSpecLong *pspec_long;
		pspec_long = G_PARAM_SPEC_LONG (pspec);
		adjustment = gtk_adjustment_new (
			(gdouble) pspec_long->default_value,
			(gdouble) pspec_long->minimum,
			(gdouble) pspec_long->maximum,
			1.0, 1.0, 0.0);

	} else if (G_IS_PARAM_SPEC_ULONG (pspec)) {
		GParamSpecULong *pspec_ulong;
		pspec_ulong = G_PARAM_SPEC_ULONG (pspec);
		adjustment = gtk_adjustment_new (
			(gdouble) pspec_ulong->default_value,
			(gdouble) pspec_ulong->minimum,
			(gdouble) pspec_ulong->maximum,
			1.0, 1.0, 0.0);

	} else if (G_IS_PARAM_SPEC_FLOAT (pspec)) {
		GParamSpecFloat *pspec_float;
		pspec_float = G_PARAM_SPEC_FLOAT (pspec);
		adjustment = gtk_adjustment_new (
			(gdouble) pspec_float->default_value,
			(gdouble) pspec_float->minimum,
			(gdouble) pspec_float->maximum,
			1.0, 1.0, 0.0);

	} else if (G_IS_PARAM_SPEC_DOUBLE (pspec)) {
		GParamSpecDouble *pspec_double;
		pspec_double = G_PARAM_SPEC_DOUBLE (pspec);
		adjustment = gtk_adjustment_new (
			(gdouble) pspec_double->default_value,
			(gdouble) pspec_double->minimum,
			(gdouble) pspec_double->maximum,
			1.0, 1.0, 0.0);

	} else
		g_return_if_reached ();

	pre = g_alloca (strlen (entry->text) + 1);
	strcpy (pre, entry->text);
	post = strstr (pre, "%s");
	if (post != NULL) {
		*post = '\0';
		post += 2;
	}

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
	gtk_widget_set_margin_start (hbox, STANDARD_MARGIN);
	gtk_box_pack_start (main_box, hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);

	if (use_pspec != NULL) {
		prefix = gtk_check_button_new_with_mnemonic (pre);

		e_binding_bind_property (
			settings, use_pspec->name,
			prefix, "active",
			G_BINDING_BIDIRECTIONAL |
			G_BINDING_SYNC_CREATE);
	} else {
		prefix = gtk_label_new_with_mnemonic (pre);
	}
	gtk_box_pack_start (GTK_BOX (hbox), prefix, FALSE, TRUE, 0);
	gtk_widget_show (prefix);

	spin = gtk_spin_button_new (adjustment, 1.0, 0);
	gtk_box_pack_start (GTK_BOX (hbox), spin, FALSE, TRUE, 0);
	gtk_widget_show (spin);

	if (!use_pspec)
		gtk_label_set_mnemonic_widget (GTK_LABEL (prefix), spin);

	e_binding_bind_property (
		settings, entry->name,
		spin, "value",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	if (use_pspec != NULL)
		e_binding_bind_property (
			prefix, "active",
			spin, "sensitive",
			G_BINDING_SYNC_CREATE);

	if (post != NULL) {
		GtkWidget *label = gtk_label_new_with_mnemonic (post);
		gtk_label_set_mnemonic_widget (GTK_LABEL (label), use_pspec ? prefix : spin);
		gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
		gtk_widget_show (label);
	}

	mail_config_provider_page_handle_dependency (settings, entry, hbox);
}

static void
mail_config_provider_page_add_entry (GtkBox *main_box,
				     CamelSettings *settings,
				     CamelProviderConfEntry *entry)
{
	GtkWidget *hbox;
	GtkWidget *input;
	GtkWidget *label;

	g_return_if_fail (entry->text != NULL);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_set_spacing (GTK_BOX (hbox), 6);
	gtk_widget_set_margin_start (hbox, STANDARD_MARGIN);
	gtk_box_pack_start (main_box, hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);

	label = gtk_label_new_with_mnemonic (entry->text);
	gtk_label_set_xalign (GTK_LABEL (label), 0);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_widget_show (label);

	input = gtk_entry_new ();
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), input);
	gtk_box_pack_start (GTK_BOX (hbox), input, TRUE, TRUE, 0);
	gtk_widget_show (input);

	e_binding_bind_object_text_property (
		settings, entry->name,
		input, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		input, "sensitive",
		label, "sensitive",
		G_BINDING_SYNC_CREATE);

	mail_config_provider_page_handle_dependency (settings, entry, hbox);
}

static void
mail_config_provider_page_add_label (GtkBox *main_box,
				     CamelSettings *settings,
				     CamelProviderConfEntry *entry)
{
	GtkWidget *hbox;
	GtkWidget *label;

	g_return_if_fail (entry->text != NULL);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_set_spacing (GTK_BOX (hbox), 6);
	gtk_widget_set_margin_start (hbox, STANDARD_MARGIN);
	gtk_box_pack_start (main_box, hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);

	label = gtk_label_new (entry->text);
	gtk_label_set_xalign (GTK_LABEL (label), 0);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_widget_show (label);

	mail_config_provider_page_handle_dependency (settings, entry, hbox);
}

static void
mail_config_provider_page_add_options (GtkBox *main_box,
				       CamelProvider *provider,
				       CamelSettings *settings,
                                       CamelProviderConfEntry *entry)
{
	GtkCellRenderer *renderer;
	GtkListStore *store;
	GtkWidget *hbox;
	GtkWidget *combo;
	GtkWidget *label;
	gchar **tokens;
	guint length, ii;

	/* The 'value' string is of the format:
	 *
	 *   'nick0:caption0:nick1:caption1:...nickN:captionN'
	 *
	 * where 'nick' is the nickname a GEnumValue and 'caption'
	 * is the localized combo box item displayed to the user. */

	g_return_if_fail (entry->text != NULL);
	g_return_if_fail (entry->value != NULL);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_set_spacing (GTK_BOX (hbox), 6);
	gtk_widget_set_margin_start (hbox, STANDARD_MARGIN);
	gtk_box_pack_start (main_box, hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);

	label = gtk_label_new_with_mnemonic (entry->text);
	gtk_label_set_xalign (GTK_LABEL (label), 0);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_widget_show (label);

	/* 0: 'nick', 1: caption */
	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

	tokens = g_strsplit (entry->value, ":", -1);
	length = g_strv_length (tokens);

	/* Take the strings two at a time. */
	for (ii = 0; ii + 1 < length; ii += 2) {
		GtkTreeIter iter;
		const gchar *nick;
		const gchar *caption;

		nick = tokens[ii + 0];
		caption = tokens[ii + 1];

		/* Localize the caption. */
		caption = dgettext (provider->translation_domain, caption);

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, nick, 1, caption, -1);
	}

	g_strfreev (tokens);

	combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
	gtk_combo_box_set_id_column (GTK_COMBO_BOX (combo), 0);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
	gtk_box_pack_start (GTK_BOX (hbox), combo, FALSE, FALSE, 0);
	gtk_widget_show (combo);
	g_object_unref (store);

	e_binding_bind_property_full (
		settings, entry->name,
		combo, "active-id",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		e_binding_transform_enum_value_to_nick,
		e_binding_transform_enum_nick_to_value,
		NULL, (GDestroyNotify) NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
	gtk_cell_layout_set_attributes (
		GTK_CELL_LAYOUT (combo), renderer, "text", 1, NULL);

	mail_config_provider_page_handle_dependency (settings, entry, hbox);
}

static void
mail_config_provider_page_add_placeholder (GtkBox *main_box,
					   CamelSettings *settings,
					   CamelProviderConfEntry *entry)
{
	GtkWidget *hbox;

	/* The entry->name is used as an identifier of the placeholder,
	   which is used with e_mail_config_provider_page_get_placeholder(). */

	g_return_if_fail (entry->name && *(entry->name));

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_name (hbox, entry->name);
	gtk_box_set_spacing (GTK_BOX (hbox), 6);
	gtk_widget_set_margin_start (hbox, STANDARD_MARGIN);
	gtk_box_pack_start (main_box, hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);

	mail_config_provider_page_handle_dependency (settings, entry, hbox);
}

static GtkBox *
mail_config_provider_page_add_advanced_section (GtkBox *main_box,
						CamelSettings *settings,
						CamelProviderConfEntry *entry)
{
	GtkWidget *vbox, *expander, *widget;
	const gchar *label;

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_set_spacing (GTK_BOX (vbox), 6);
	gtk_widget_set_margin_start (vbox, STANDARD_MARGIN);
	gtk_widget_show (vbox);

	label = entry->text;

	if (!label || !*label)
		label = _("Advanced Options");

	expander = gtk_expander_new_with_mnemonic (label);
	widget = gtk_expander_get_label_widget (GTK_EXPANDER (expander));
	if (widget) {
		PangoAttrList *bold;

		bold = pango_attr_list_new ();
		pango_attr_list_insert (bold, pango_attr_weight_new (PANGO_WEIGHT_BOLD));

		gtk_label_set_attributes (GTK_LABEL (widget), bold);

		pango_attr_list_unref (bold);
	}
	gtk_expander_set_expanded (GTK_EXPANDER (expander), FALSE);

	gtk_widget_show (expander);

	gtk_box_pack_start (main_box, expander, FALSE, FALSE, 0);
	gtk_box_pack_start (main_box, vbox, FALSE, FALSE, 0);

	/* Because some themes can show the box in different style than the rest of the options */
	e_binding_bind_property (
		expander, "expanded",
		vbox, "visible",
		G_BINDING_SYNC_CREATE);

	mail_config_provider_page_handle_dependency (settings, entry, expander);
	mail_config_provider_page_handle_dependency (settings, entry, vbox);

	return GTK_BOX (vbox);
}

void
e_mail_config_provider_add_widgets (CamelProvider *provider,
				    CamelSettings *settings,
				    GtkBox *main_box,
				    gboolean skip_first_section_name)
{
	CamelProviderConfEntry *entries;
	GSList *section_boxes = NULL; /* GtkBox * */
	gboolean first_section = skip_first_section_name;
	gint ii;

	if (!provider || !provider->extra_conf)
		return;

	g_return_if_fail (CAMEL_IS_SETTINGS (settings));
	g_return_if_fail (GTK_IS_BOX (main_box));

	/* Note the "text" member of each CamelProviderConfEntry is
	 * already localized, so we can use it directly in widgets. */

	entries = provider->extra_conf;

	/* Loop until we see CAMEL_PROVIDER_CONF_END. */
	for (ii = 0; entries[ii].type != CAMEL_PROVIDER_CONF_END; ii++) {

		/* Skip entries with no name. */
		if (entries[ii].name == NULL && entries[ii].type != CAMEL_PROVIDER_CONF_ADVANCED_SECTION_START)
			continue;

		switch (entries[ii].type) {
			case CAMEL_PROVIDER_CONF_SECTION_START:
				/* Skip the first section start. */
				if (first_section) {
					first_section = FALSE;
					continue;
				}
				section_boxes = g_slist_prepend (section_boxes, main_box);
				mail_config_provider_page_add_section (
					main_box, provider, &entries[ii], skip_first_section_name);
				break;

			case CAMEL_PROVIDER_CONF_CHECKBOX:
				mail_config_provider_page_add_checkbox (
					main_box, settings, &entries[ii]);
				break;

			case CAMEL_PROVIDER_CONF_CHECKSPIN:
				mail_config_provider_page_add_checkspin (
					main_box, settings, &entries[ii]);
				break;

			case CAMEL_PROVIDER_CONF_ENTRY:
				mail_config_provider_page_add_entry (
					main_box, settings, &entries[ii]);
				break;

			case CAMEL_PROVIDER_CONF_LABEL:
				mail_config_provider_page_add_label (
					main_box, settings, &entries[ii]);
				break;

			case CAMEL_PROVIDER_CONF_OPTIONS:
				mail_config_provider_page_add_options (
					main_box, provider, settings, &entries[ii]);
				break;

			case CAMEL_PROVIDER_CONF_PLACEHOLDER:
				mail_config_provider_page_add_placeholder (
					main_box, settings, &entries[ii]);
				break;

			case CAMEL_PROVIDER_CONF_ADVANCED_SECTION_START:
				first_section = FALSE;

				section_boxes = g_slist_prepend (section_boxes, main_box);

				main_box = mail_config_provider_page_add_advanced_section (
					main_box, settings, &entries[ii]);
				break;

			case CAMEL_PROVIDER_CONF_SECTION_END:
				if (section_boxes) {
					main_box = section_boxes->data;
					section_boxes = g_slist_remove (section_boxes, main_box);
				}
				break;
			default:
				break;  /* skip it */
		}
	}

	g_slist_free (section_boxes);
}

static void
mail_config_provider_page_add_widgets (EMailConfigProviderPage *page,
				       GtkBox *main_box)
{
	EMailConfigServiceBackend *backend;
	CamelProvider *provider;
	CamelSettings *settings;
	GtkWidget *container;
	GtkWidget *widget;
	ESource *source;
	ESourceExtension *extension;
	const gchar *extension_name;
	const gchar *text;
	gchar *markup;

	/* XXX We begin the page with our own section header and refresh
	 *     interval setting, and then skip the CamelProvider's first
	 *     CAMEL_PROVIDER_CONF_SECTION_START entry.
	 *
	 *     This is all very brittle.  I'm convinced that generating
	 *     a user interface from an array of records like this is a
	 *     bad idea.  We already have EMailConfigServiceBackend for
	 *     building provider-specific "Receving Email" and "Sending
	 *     EMail" pages by hand.  We should do similarly here. */

	backend = e_mail_config_provider_page_get_backend (page);
	source = e_mail_config_service_backend_get_source (backend);
	settings = e_mail_config_service_backend_get_settings (backend);
	provider = e_mail_config_service_backend_get_provider (backend);
	g_return_if_fail (provider != NULL);

	/* XXX I guess refresh options go in the mail account source,
	 *     even if the source is part of a collection.  I did not
	 *     think about it too hard, so hopefully this is right. */
	extension_name = E_SOURCE_EXTENSION_REFRESH;
	extension = e_source_get_extension (source, extension_name);

	text = _("Checking for New Mail");
	markup = g_markup_printf_escaped ("<b>%s</b>", text);
	widget = gtk_label_new (markup);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_box_pack_start (main_box, widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);
	g_free (markup);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_margin_start (widget, STANDARD_MARGIN);
	gtk_box_pack_start (main_box, widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	text = _("Check for _new messages every");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	e_binding_bind_property (
		extension, "enabled",
		widget, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	widget = gtk_spin_button_new_with_range (1.0, 1440.0, 1.0);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	e_binding_bind_property (
		extension, "enabled",
		widget, "sensitive",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		extension, "interval-minutes",
		widget, "value",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	widget = gtk_label_new (_("minutes"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	e_mail_config_provider_add_widgets (provider, settings, main_box, TRUE);
}

static void
mail_config_provider_page_set_backend (EMailConfigProviderPage *page,
                                       EMailConfigServiceBackend *backend)
{
	g_return_if_fail (E_IS_MAIL_CONFIG_SERVICE_BACKEND (backend));
	g_return_if_fail (page->priv->backend == NULL);

	page->priv->backend = g_object_ref (backend);
}

static void
mail_config_provider_page_set_property (GObject *object,
                                        guint property_id,
                                        const GValue *value,
                                        GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_BACKEND:
			mail_config_provider_page_set_backend (
				E_MAIL_CONFIG_PROVIDER_PAGE (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_provider_page_get_property (GObject *object,
                                        guint property_id,
                                        GValue *value,
                                        GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_BACKEND:
			g_value_set_object (
				value,
				e_mail_config_provider_page_get_backend (
				E_MAIL_CONFIG_PROVIDER_PAGE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_provider_page_dispose (GObject *object)
{
	EMailConfigProviderPage *self = E_MAIL_CONFIG_PROVIDER_PAGE (object);

	g_clear_object (&self->priv->backend);

	/* Chain up parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_config_provider_page_parent_class)->
		dispose (object);
}

static void
mail_config_provider_page_constructed (GObject *object)
{
	EMailConfigProviderPage *page;
	EMailConfigServiceBackend *backend;
	CamelProvider *provider;
	GtkWidget *main_box;

	page = E_MAIL_CONFIG_PROVIDER_PAGE (object);

	/* Chain up parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_config_provider_page_parent_class)->constructed (object);

	main_box = e_mail_config_activity_page_get_internal_box (E_MAIL_CONFIG_ACTIVITY_PAGE (page));

	gtk_box_set_spacing (GTK_BOX (main_box), 6);

	backend = e_mail_config_provider_page_get_backend (page);
	provider = e_mail_config_service_backend_get_provider (backend);

	if (provider != NULL && provider->extra_conf != NULL)
		mail_config_provider_page_add_widgets (page, GTK_BOX (main_box));
	else
		page->priv->is_empty = TRUE;

	e_mail_config_page_set_content (E_MAIL_CONFIG_PAGE (page), main_box);

	e_extensible_load_extensions (E_EXTENSIBLE (page));
}

static void
e_mail_config_provider_page_class_init (EMailConfigProviderPageClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_config_provider_page_set_property;
	object_class->get_property = mail_config_provider_page_get_property;
	object_class->dispose = mail_config_provider_page_dispose;
	object_class->constructed = mail_config_provider_page_constructed;

	g_object_class_install_property (
		object_class,
		PROP_BACKEND,
		g_param_spec_object (
			"backend",
			"Backend",
			"Service backend to generate options from",
			E_TYPE_MAIL_CONFIG_SERVICE_BACKEND,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));
}

static void
e_mail_config_provider_page_interface_init (EMailConfigPageInterface *iface)
{
	iface->title = _("Receiving Options");
	iface->sort_order = E_MAIL_CONFIG_PROVIDER_PAGE_SORT_ORDER;
}

static void
e_mail_config_provider_page_init (EMailConfigProviderPage *page)
{
	page->priv = e_mail_config_provider_page_get_instance_private (page);
}

EMailConfigPage *
e_mail_config_provider_page_new (EMailConfigServiceBackend *backend)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_SERVICE_BACKEND (backend), NULL);

	return g_object_new (
		E_TYPE_MAIL_CONFIG_PROVIDER_PAGE,
		"backend", backend, NULL);
}

gboolean
e_mail_config_provider_page_is_empty (EMailConfigProviderPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_PROVIDER_PAGE (page), TRUE);

	return page->priv->is_empty;
}

EMailConfigServiceBackend *
e_mail_config_provider_page_get_backend (EMailConfigProviderPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_PROVIDER_PAGE (page), NULL);

	return page->priv->backend;
}

typedef struct _FindPlaceholderData {
	const gchar *name;
	GtkBox *box;
} FindPlaceholderData;

static void
mail_config_provider_page_find_placeholder (GtkWidget *widget,
					    gpointer user_data)
{
	FindPlaceholderData *fpd = user_data;

	g_return_if_fail (fpd != NULL);

	if (g_strcmp0 (fpd->name, gtk_widget_get_name (widget)) == 0) {
		if (fpd->box) {
			g_warning ("%s: Found multiple placeholders named '%s'", G_STRFUNC, fpd->name);
		} else {
			g_return_if_fail (GTK_IS_BOX (widget));

			fpd->box = GTK_BOX (widget);
		}
	}
}

GtkBox *
e_mail_config_provider_page_get_placeholder (EMailConfigProviderPage *page,
					     const gchar *name)
{
	FindPlaceholderData fpd;
	GtkWidget *widget;

	g_return_val_if_fail (E_IS_MAIL_CONFIG_PROVIDER_PAGE (page), NULL);
	g_return_val_if_fail (name && *name, NULL);

	fpd.name = name;
	fpd.box = NULL;

	widget = gtk_bin_get_child (GTK_BIN (page));
	if (GTK_IS_VIEWPORT (widget))
		widget = gtk_bin_get_child (GTK_BIN (widget));

	if (!GTK_IS_CONTAINER (widget))
		return NULL;

	gtk_container_foreach (GTK_CONTAINER (widget), mail_config_provider_page_find_placeholder, &fpd);

	return fpd.box;
}
