/*
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

#include <glib/gi18n-lib.h>

#include <libedataserver/libedataserver.h>

#include "e-mail-config-page.h"
#include "e-mail-config-activity-page.h"
#include "em-composer-utils.h"

#include "e-mail-config-composing-page.h"

struct _EMailConfigComposingPagePrivate {
	ESource *identity_source;
};

enum {
	PROP_0,
	PROP_IDENTITY_SOURCE
};

/* Forward Declarations */
static void	e_mail_config_composing_page_interface_init
					(EMailConfigPageInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EMailConfigComposingPage, e_mail_config_composing_page, E_TYPE_MAIL_CONFIG_ACTIVITY_PAGE,
	G_ADD_PRIVATE (EMailConfigComposingPage)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL)
	G_IMPLEMENT_INTERFACE (E_TYPE_MAIL_CONFIG_PAGE, e_mail_config_composing_page_interface_init))

static gboolean
mail_config_composing_page_addrs_to_string (GBinding *binding,
                                           const GValue *source_value,
                                           GValue *target_value,
                                           gpointer unused)
{
	gchar **strv;

	strv = g_value_dup_boxed (source_value);

	if (strv != NULL) {
		gchar *string = g_strjoinv ("; ", strv);
		g_value_set_string (target_value, string);
		g_free (string);
	} else {
		g_value_set_string (target_value, "");
	}

	g_strfreev (strv);

	return TRUE;
}

static gboolean
mail_config_composing_page_string_to_addrs (GBinding *binding,
                                           const GValue *source_value,
                                           GValue *target_value,
                                           gpointer unused)
{
	CamelInternetAddress *address;
	const gchar *string;
	gchar **strv;
	gint n_addresses, ii;

	string = g_value_get_string (source_value);

	address = camel_internet_address_new ();
	n_addresses = camel_address_decode (CAMEL_ADDRESS (address), string);

	if (n_addresses < 0) {
		g_object_unref (address);
		return FALSE;

	} else if (n_addresses == 0) {
		g_value_set_boxed (target_value, NULL);
		g_object_unref (address);
		return TRUE;
	}

	strv = g_new0 (gchar *, n_addresses + 1);

	for (ii = 0; ii < n_addresses; ii++) {
		const gchar *name = NULL;
		const gchar *addr = NULL;

		g_warn_if_fail (camel_internet_address_get (address, ii, &name, &addr));
		strv[ii] = camel_internet_address_format_address (name, addr);
	}

	g_value_take_boxed (target_value, strv);

	return TRUE;
}

static void
mail_config_composing_fill_reply_style_combox (GtkComboBoxText *combo)
{
	struct _values {
		ESourceMailCompositionReplyStyle reply_style;
		const gchar *display_name;
	} values[] = {
		{ E_SOURCE_MAIL_COMPOSITION_REPLY_STYLE_DEFAULT,
		  NC_("ReplyForward", "Use global setting") },
		{ E_SOURCE_MAIL_COMPOSITION_REPLY_STYLE_ATTACH,
		  NC_("ReplyForward", "Attachment") },
		{ E_SOURCE_MAIL_COMPOSITION_REPLY_STYLE_OUTLOOK,
		  NC_("ReplyForward", "Inline (Outlook style)") },
		{ E_SOURCE_MAIL_COMPOSITION_REPLY_STYLE_QUOTED,
		  NC_("ReplyForward", "Quoted") },
		{ E_SOURCE_MAIL_COMPOSITION_REPLY_STYLE_DO_NOT_QUOTE,
		  NC_("ReplyForward", "Do Not Quote") }
	};
	GEnumClass *enum_class;
	GEnumValue *enum_value;
	gint ii;

	g_return_if_fail (GTK_IS_COMBO_BOX_TEXT (combo));

	enum_class = g_type_class_ref (E_TYPE_SOURCE_MAIL_COMPOSITION_REPLY_STYLE);
	g_return_if_fail (enum_class != NULL);

	g_warn_if_fail (enum_class->n_values == G_N_ELEMENTS (values));

	for (ii = 0; ii < G_N_ELEMENTS (values); ii++) {
		enum_value = g_enum_get_value (enum_class, values[ii].reply_style);
		g_warn_if_fail (enum_value != NULL);

		if (enum_value) {
			gtk_combo_box_text_append (combo,
				enum_value->value_name,
				g_dpgettext2 (GETTEXT_PACKAGE, "ReplyForward", values[ii].display_name));
		}
	}

	g_type_class_unref (enum_class);
}

static void
mail_config_composing_fill_language_combox (GtkComboBoxText *combo)
{
	g_return_if_fail (GTK_IS_COMBO_BOX_TEXT (combo));

	gtk_combo_box_text_append (combo, NULL, _("Use global setting"));

	em_utils_add_installed_languages (combo);
}

static gboolean
mail_config_composing_page_reply_style_to_string (GBinding *binding,
						 const GValue *source_value,
						 GValue *target_value,
						 gpointer data)
{
	GEnumClass *enum_class;
	GEnumValue *enum_value;

	enum_class = g_type_class_ref (E_TYPE_SOURCE_MAIL_COMPOSITION_REPLY_STYLE);
	g_return_val_if_fail (enum_class != NULL, FALSE);

	enum_value = g_enum_get_value (enum_class, g_value_get_enum (source_value));
	g_return_val_if_fail (enum_value != NULL, FALSE);

	g_value_set_string (target_value, enum_value->value_name);

	g_type_class_unref (enum_class);

	return TRUE;
}

static gboolean
mail_config_composing_page_string_to_reply_style (GBinding *binding,
						 const GValue *source_value,
						 GValue *target_value,
						 gpointer data)
{
	GEnumClass *enum_class;
	GEnumValue *enum_value;
	const gchar *value_name;

	enum_class = g_type_class_ref (E_TYPE_SOURCE_MAIL_COMPOSITION_REPLY_STYLE);
	g_return_val_if_fail (enum_class != NULL, FALSE);

	value_name = g_value_get_string (source_value);
	if (!value_name || !*value_name) {
		enum_value = NULL;
	} else {
		enum_value = g_enum_get_value_by_name (enum_class, value_name);
	}
	if (!enum_value)
		g_value_set_enum (target_value, E_SOURCE_MAIL_COMPOSITION_REPLY_STYLE_DEFAULT);
	else
		g_value_set_enum (target_value, enum_value->value);

	g_warn_if_fail (enum_value != NULL);

	g_type_class_unref (enum_class);

	return TRUE;
}

static void
mail_config_composing_page_set_identity_source (EMailConfigComposingPage *page,
                                               ESource *identity_source)
{
	g_return_if_fail (E_IS_SOURCE (identity_source));
	g_return_if_fail (page->priv->identity_source == NULL);

	page->priv->identity_source = g_object_ref (identity_source);
}

static void
mail_config_composing_page_set_property (GObject *object,
                                        guint property_id,
                                        const GValue *value,
                                        GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_IDENTITY_SOURCE:
			mail_config_composing_page_set_identity_source (
				E_MAIL_CONFIG_COMPOSING_PAGE (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_composing_page_get_property (GObject *object,
                                        guint property_id,
                                        GValue *value,
                                        GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_IDENTITY_SOURCE:
			g_value_set_object (
				value,
				e_mail_config_composing_page_get_identity_source (
				E_MAIL_CONFIG_COMPOSING_PAGE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_composing_page_dispose (GObject *object)
{
	EMailConfigComposingPage *page = E_MAIL_CONFIG_COMPOSING_PAGE (object);

	g_clear_object (&page->priv->identity_source);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_mail_config_composing_page_parent_class)->dispose (object);
}

typedef struct _ThreeStateData {
	GObject *composition_ext;
	gchar *property_name;
	gulong handler_id;
} ThreeStateData;

static void
three_state_data_free (gpointer data,
		       GClosure *closure)
{
	ThreeStateData *tsd = data;

	if (tsd) {
		g_clear_object (&tsd->composition_ext);
		g_free (tsd->property_name);
		g_slice_free (ThreeStateData, tsd);
	}
}

static void
mail_config_composing_page_three_state_toggled_cb (GtkToggleButton *widget,
						   gpointer user_data)
{
	ThreeStateData *tsd = user_data;
	CamelThreeState set_to;

	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (widget));
	g_return_if_fail (tsd != NULL);

	g_signal_handler_block (widget, tsd->handler_id);

	if (gtk_toggle_button_get_inconsistent (widget) &&
	    gtk_toggle_button_get_active (widget)) {
		gtk_toggle_button_set_active (widget, FALSE);
		gtk_toggle_button_set_inconsistent (widget, FALSE);
		set_to = CAMEL_THREE_STATE_OFF;
	} else if (!gtk_toggle_button_get_active (widget)) {
		gtk_toggle_button_set_inconsistent (widget, TRUE);
		gtk_toggle_button_set_active (widget, FALSE);
		set_to = CAMEL_THREE_STATE_INCONSISTENT;
	} else {
		set_to = CAMEL_THREE_STATE_ON;
	}

	g_object_set (tsd->composition_ext, tsd->property_name, set_to, NULL);

	g_signal_handler_unblock (widget, tsd->handler_id);
}

static void
mail_config_composing_page_setup_three_state_value (ESourceMailComposition *composition_ext,
						    const gchar *property_name,
						    GtkWidget *check_button)
{
	ThreeStateData *tsd;
	gboolean set_inconsistent = FALSE, set_active = FALSE;
	EThreeState value = E_THREE_STATE_INCONSISTENT;

	g_return_if_fail (E_IS_SOURCE_MAIL_COMPOSITION (composition_ext));
	g_return_if_fail (property_name != NULL);
	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (check_button));

	tsd = g_slice_new0 (ThreeStateData);
	tsd->composition_ext = G_OBJECT (g_object_ref (composition_ext));
	tsd->property_name = g_strdup (property_name);

	g_object_get (tsd->composition_ext, tsd->property_name, &value, NULL);

	switch (value) {
		case CAMEL_THREE_STATE_ON:
			set_inconsistent = FALSE;
			set_active = TRUE;
			break;
		case CAMEL_THREE_STATE_OFF:
			set_inconsistent = FALSE;
			set_active = FALSE;
			break;
		case CAMEL_THREE_STATE_INCONSISTENT:
			set_inconsistent = TRUE;
			set_active = FALSE;
			break;
	}

	g_object_set (G_OBJECT (check_button),
		"inconsistent", set_inconsistent,
		"active", set_active,
		NULL);

	tsd->handler_id = g_signal_connect_data (check_button, "toggled",
		G_CALLBACK (mail_config_composing_page_three_state_toggled_cb),
		tsd, three_state_data_free, 0);
}

static void
mail_config_composing_page_constructed (GObject *object)
{
	EMailConfigComposingPage *page;
	ESource *source;
	ESourceMailComposition *composition_ext;
	ESourceMDN *mdn_ext;
	GtkLabel *label;
	GtkWidget *widget;
	GtkWidget *container;
	GtkWidget *main_box;
	GtkSizeGroup *size_group;
	GEnumClass *enum_class;
	GEnumValue *enum_value;
	EMdnResponsePolicy policy;
	const gchar *text;
	gchar *markup;

	page = E_MAIL_CONFIG_COMPOSING_PAGE (object);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_mail_config_composing_page_parent_class)->constructed (object);

	source = e_mail_config_composing_page_get_identity_source (page);

	composition_ext = e_source_get_extension (source, E_SOURCE_EXTENSION_MAIL_COMPOSITION);
	mdn_ext = e_source_get_extension (source, E_SOURCE_EXTENSION_MDN);

	main_box = e_mail_config_activity_page_get_internal_box (E_MAIL_CONFIG_ACTIVITY_PAGE (page));

	gtk_box_set_spacing (GTK_BOX (main_box), 12);

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	/*** Composing Messages ***/

	widget = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (widget), 6);
	gtk_grid_set_column_spacing (GTK_GRID (widget), 6);
	gtk_box_pack_start (GTK_BOX (main_box), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	text = _("Composing Messages");
	markup = g_markup_printf_escaped ("<b>%s</b>", text);
	widget = gtk_label_new (markup);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 2, 1);
	gtk_widget_show (widget);
	g_free (markup);

	text = _("Alway_s carbon-copy (cc) to:");
	widget = gtk_label_new_with_mnemonic (text);
	gtk_widget_set_margin_start (widget, 12);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 1, 2, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_entry_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_widget_set_margin_start (widget, 12);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 2, 2, 1);
	gtk_widget_show (widget);

	e_binding_bind_property_full (
		composition_ext, "cc",
		widget, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		mail_config_composing_page_addrs_to_string,
		mail_config_composing_page_string_to_addrs,
		NULL, (GDestroyNotify) NULL);

	text = _("Always _blind carbon-copy (bcc) to:");
	widget = gtk_label_new_with_mnemonic (text);
	gtk_widget_set_margin_start (widget, 12);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 3, 2, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_entry_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_widget_set_margin_start (widget, 12);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 4, 2, 1);
	gtk_widget_show (widget);

	e_binding_bind_property_full (
		composition_ext, "bcc",
		widget, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		mail_config_composing_page_addrs_to_string,
		mail_config_composing_page_string_to_addrs,
		NULL, (GDestroyNotify) NULL);

	text = _("Re_ply style:");
	widget = gtk_label_new_with_mnemonic (text);
	gtk_widget_set_hexpand (widget, FALSE);
	gtk_widget_set_margin_start (widget, 12);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 5, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_combo_box_text_new ();
	gtk_widget_set_halign (widget, GTK_ALIGN_START);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 5, 1, 1);
	gtk_widget_show (widget);

	mail_config_composing_fill_reply_style_combox (GTK_COMBO_BOX_TEXT (widget));

	e_binding_bind_property_full (
		composition_ext, "reply-style",
		widget, "active-id",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		mail_config_composing_page_reply_style_to_string,
		mail_config_composing_page_string_to_reply_style,
		NULL, (GDestroyNotify) NULL);

	widget = gtk_label_new_with_mnemonic (_("Lang_uage:"));
	gtk_widget_set_hexpand (widget, FALSE);
	gtk_widget_set_margin_start (widget, 12);
	gtk_widget_set_tooltip_text (widget, _("Language for Reply and Forward attribution text"));
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 6, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_combo_box_text_new ();
	gtk_widget_set_halign (widget, GTK_ALIGN_START);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_widget_set_tooltip_text (widget, _("Language for Reply and Forward attribution text"));
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 6, 1, 1);
	gtk_widget_show (widget);

	mail_config_composing_fill_language_combox (GTK_COMBO_BOX_TEXT (widget));

	e_binding_bind_property (
		composition_ext, "language",
		widget, "active-id",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	if (gtk_combo_box_get_active (GTK_COMBO_BOX (widget)) == -1)
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);

	widget = gtk_check_button_new_with_mnemonic (_("Start _typing at the bottom"));
	gtk_widget_set_margin_start (widget, 12);
	gtk_box_pack_start (GTK_BOX (main_box), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	mail_config_composing_page_setup_three_state_value (composition_ext, "start-bottom", widget);

	widget = gtk_check_button_new_with_mnemonic (_("_Keep signature above the original message"));
	gtk_widget_set_margin_start (widget, 12);
	gtk_box_pack_start (GTK_BOX (main_box), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	mail_config_composing_page_setup_three_state_value (composition_ext, "top-signature", widget);

	/*** Message Receipts ***/

	widget = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (widget), 6);
	gtk_grid_set_column_spacing (GTK_GRID (widget), 6);
	gtk_box_pack_start (GTK_BOX (main_box), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	text = _("Message Receipts");
	markup = g_markup_printf_escaped ("<b>%s</b>", text);
	widget = gtk_label_new (markup);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 2, 1);
	gtk_widget_show (widget);
	g_free (markup);

	text = _("S_end message receipts:");
	widget = gtk_label_new_with_mnemonic (text);
	gtk_widget_set_margin_start (widget, 12);
	gtk_size_group_add_widget (size_group, widget);
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 1, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_combo_box_text_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 1, 1, 1);
	gtk_widget_show (widget);

	/* XXX This is a pain in the butt, but we want to avoid hard-coding
	 *     string values from the EMdnResponsePolicy enum class in case
	 *     they change in the future. */
	enum_class = g_type_class_ref (E_TYPE_MDN_RESPONSE_POLICY);
	policy = E_MDN_RESPONSE_POLICY_NEVER;
	enum_value = g_enum_get_value (enum_class, policy);
	g_return_if_fail (enum_value != NULL);
	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (widget),
		enum_value->value_nick, _("Never"));
	policy = E_MDN_RESPONSE_POLICY_ALWAYS;
	enum_value = g_enum_get_value (enum_class, policy);
	g_return_if_fail (enum_value != NULL);
	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (widget),
		enum_value->value_nick, _("Always"));
	policy = E_MDN_RESPONSE_POLICY_ASK;
	enum_value = g_enum_get_value (enum_class, policy);
	g_return_if_fail (enum_value != NULL);
	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (widget),
		enum_value->value_nick, _("Ask for each message"));
	g_type_class_unref (enum_class);

	e_binding_bind_property_full (
		mdn_ext, "response-policy",
		widget, "active-id",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		e_binding_transform_enum_value_to_nick,
		e_binding_transform_enum_nick_to_value,
		NULL, (GDestroyNotify) NULL);

	g_object_unref (size_group);

	e_mail_config_page_set_content (E_MAIL_CONFIG_PAGE (page), main_box);

	e_extensible_load_extensions (E_EXTENSIBLE (page));
}

static void
e_mail_config_composing_page_class_init (EMailConfigComposingPageClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_config_composing_page_set_property;
	object_class->get_property = mail_config_composing_page_get_property;
	object_class->dispose = mail_config_composing_page_dispose;
	object_class->constructed = mail_config_composing_page_constructed;

	g_object_class_install_property (
		object_class,
		PROP_IDENTITY_SOURCE,
		g_param_spec_object (
			"identity-source",
			"Identity Source",
			"Mail identity source being edited",
			E_TYPE_SOURCE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));
}

static void
e_mail_config_composing_page_interface_init (EMailConfigPageInterface *iface)
{
	iface->title = _("Composing Messages");
	iface->sort_order = E_MAIL_CONFIG_COMPOSING_PAGE_SORT_ORDER;
}

static void
e_mail_config_composing_page_init (EMailConfigComposingPage *page)
{
	page->priv = e_mail_config_composing_page_get_instance_private (page);
}

EMailConfigPage *
e_mail_config_composing_page_new (ESource *identity_source)
{
	g_return_val_if_fail (E_IS_SOURCE (identity_source), NULL);

	return g_object_new (E_TYPE_MAIL_CONFIG_COMPOSING_PAGE,
		"identity-source", identity_source,
		NULL);
}

ESource *
e_mail_config_composing_page_get_identity_source (EMailConfigComposingPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_COMPOSING_PAGE (page), NULL);

	return page->priv->identity_source;
}
