/*
 * e-mail-config-identity-page.c
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

#include "e-mail-config-identity-page.h"

#include <config.h>
#include <glib/gi18n-lib.h>

#include <libebackend/libebackend.h>

#include "e-util/e-util.h"

#define E_MAIL_CONFIG_IDENTITY_PAGE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_CONFIG_IDENTITY_PAGE, EMailConfigIdentityPagePrivate))

struct _EMailConfigIdentityPagePrivate {
	ESource *identity_source;
	ESourceRegistry *registry;
	gboolean show_account_info;
	gboolean show_email_address;
	gboolean show_instructions;
	gboolean show_signatures;
	gboolean show_autodiscover_check;
	GtkWidget *autodiscover_check; /* not referenced */
};

enum {
	PROP_0,
	PROP_IDENTITY_SOURCE,
	PROP_REGISTRY,
	PROP_SHOW_ACCOUNT_INFO,
	PROP_SHOW_EMAIL_ADDRESS,
	PROP_SHOW_INSTRUCTIONS,
	PROP_SHOW_SIGNATURES,
	PROP_SHOW_AUTODISCOVER_CHECK
};

/* Forward Declarations */
static void	e_mail_config_identity_page_interface_init
					(EMailConfigPageInterface *iface);

G_DEFINE_TYPE_WITH_CODE (
	EMailConfigIdentityPage,
	e_mail_config_identity_page,
	GTK_TYPE_BOX,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_EXTENSIBLE, NULL)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_CONFIG_PAGE,
		e_mail_config_identity_page_interface_init))

static gboolean
mail_config_identity_page_is_email (const gchar *email_address)
{
	const gchar *cp;

	/* Make sure we have a '@' between a name and domain part. */
	cp = strchr (email_address, '@');

	return (cp != NULL && cp != email_address && *(cp + 1) != '\0');
}

static void
mail_config_identity_page_add_signature_cb (GtkButton *button,
                                            EMailConfigIdentityPage *page)
{
	ESourceRegistry *registry;
	GtkWidget *editor;

	registry = e_mail_config_identity_page_get_registry (page);

	editor = e_mail_signature_editor_new (registry, NULL);
	gtk_window_set_position (GTK_WINDOW (editor), GTK_WIN_POS_CENTER);
	gtk_widget_show (editor);
}

static void
mail_config_identity_page_set_registry (EMailConfigIdentityPage *page,
                                        ESourceRegistry *registry)
{
	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (page->priv->registry == NULL);

	page->priv->registry = g_object_ref (registry);
}

static void
mail_config_identity_page_set_identity_source (EMailConfigIdentityPage *page,
                                               ESource *identity_source)
{
	g_return_if_fail (E_IS_SOURCE (identity_source));
	g_return_if_fail (page->priv->identity_source == NULL);

	page->priv->identity_source = g_object_ref (identity_source);
}

static void
mail_config_identity_page_set_property (GObject *object,
                                        guint property_id,
                                        const GValue *value,
                                        GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_IDENTITY_SOURCE:
			mail_config_identity_page_set_identity_source (
				E_MAIL_CONFIG_IDENTITY_PAGE (object),
				g_value_get_object (value));
			return;

		case PROP_REGISTRY:
			mail_config_identity_page_set_registry (
				E_MAIL_CONFIG_IDENTITY_PAGE (object),
				g_value_get_object (value));
			return;

		case PROP_SHOW_ACCOUNT_INFO:
			e_mail_config_identity_page_set_show_account_info (
				E_MAIL_CONFIG_IDENTITY_PAGE (object),
				g_value_get_boolean (value));
			return;

		case PROP_SHOW_EMAIL_ADDRESS:
			e_mail_config_identity_page_set_show_email_address (
				E_MAIL_CONFIG_IDENTITY_PAGE (object),
				g_value_get_boolean (value));
			return;

		case PROP_SHOW_INSTRUCTIONS:
			e_mail_config_identity_page_set_show_instructions (
				E_MAIL_CONFIG_IDENTITY_PAGE (object),
				g_value_get_boolean (value));
			return;

		case PROP_SHOW_SIGNATURES:
			e_mail_config_identity_page_set_show_signatures (
				E_MAIL_CONFIG_IDENTITY_PAGE (object),
				g_value_get_boolean (value));
			return;

		case PROP_SHOW_AUTODISCOVER_CHECK:
			e_mail_config_identity_page_set_show_autodiscover_check (
				E_MAIL_CONFIG_IDENTITY_PAGE (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_identity_page_get_property (GObject *object,
                                        guint property_id,
                                        GValue *value,
                                        GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_IDENTITY_SOURCE:
			g_value_set_object (
				value,
				e_mail_config_identity_page_get_identity_source (
				E_MAIL_CONFIG_IDENTITY_PAGE (object)));
			return;

		case PROP_REGISTRY:
			g_value_set_object (
				value,
				e_mail_config_identity_page_get_registry (
				E_MAIL_CONFIG_IDENTITY_PAGE (object)));
			return;

		case PROP_SHOW_ACCOUNT_INFO:
			g_value_set_boolean (
				value,
				e_mail_config_identity_page_get_show_account_info (
				E_MAIL_CONFIG_IDENTITY_PAGE (object)));
			return;

		case PROP_SHOW_EMAIL_ADDRESS:
			g_value_set_boolean (
				value,
				e_mail_config_identity_page_get_show_email_address (
				E_MAIL_CONFIG_IDENTITY_PAGE (object)));
			return;

		case PROP_SHOW_INSTRUCTIONS:
			g_value_set_boolean (
				value,
				e_mail_config_identity_page_get_show_instructions (
				E_MAIL_CONFIG_IDENTITY_PAGE (object)));
			return;

		case PROP_SHOW_SIGNATURES:
			g_value_set_boolean (
				value,
				e_mail_config_identity_page_get_show_signatures (
				E_MAIL_CONFIG_IDENTITY_PAGE (object)));
			return;

		case PROP_SHOW_AUTODISCOVER_CHECK:
			g_value_set_boolean (
				value,
				e_mail_config_identity_page_get_show_autodiscover_check (
				E_MAIL_CONFIG_IDENTITY_PAGE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_identity_page_dispose (GObject *object)
{
	EMailConfigIdentityPagePrivate *priv;

	priv = E_MAIL_CONFIG_IDENTITY_PAGE_GET_PRIVATE (object);

	if (priv->identity_source != NULL) {
		g_object_unref (priv->identity_source);
		priv->identity_source = NULL;
	}

	if (priv->registry != NULL) {
		g_object_unref (priv->registry);
		priv->registry = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_config_identity_page_parent_class)->
		dispose (object);
}

static void
mail_config_identity_page_constructed (GObject *object)
{
	EMailConfigIdentityPage *page;
	ESource *source;
	ESourceRegistry *registry;
	ESourceMailIdentity *extension;
	GtkLabel *label;
	GtkWidget *widget;
	GtkWidget *container;
	GtkSizeGroup *size_group;
	const gchar *extension_name;
	const gchar *text;
	gchar *markup;

	page = E_MAIL_CONFIG_IDENTITY_PAGE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_config_identity_page_parent_class)->constructed (object);

	extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
	registry = e_mail_config_identity_page_get_registry (page);
	source = e_mail_config_identity_page_get_identity_source (page);
	extension = e_source_get_extension (source, extension_name);

	gtk_orientable_set_orientation (
		GTK_ORIENTABLE (page), GTK_ORIENTATION_VERTICAL);

	gtk_box_set_spacing (GTK_BOX (page), 12);
	gtk_widget_set_valign (GTK_WIDGET (page), GTK_ALIGN_FILL);
	gtk_widget_set_vexpand (GTK_WIDGET (page), TRUE);

	/* This keeps all mnemonic labels the same width. */
	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	text = _("Please enter your name and email address below. "
		 "The \"optional\" fields below do not need to be filled "
		 "in, unless you wish to include this information in email "
		 "you send.");
	widget = gtk_label_new (text);
	gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (page), widget, FALSE, FALSE, 0);

	g_object_bind_property (
		page, "show-instructions",
		widget, "visible",
		G_BINDING_SYNC_CREATE);

	/*** Account Information ***/

	widget = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (widget), 6);
	gtk_grid_set_column_spacing (GTK_GRID (widget), 6);
	gtk_box_pack_start (GTK_BOX (page), widget, FALSE, FALSE, 0);

	g_object_bind_property (
		page, "show-account-info",
		widget, "visible",
		G_BINDING_SYNC_CREATE);

	container = widget;

	text = _("Account Information");
	markup = g_markup_printf_escaped ("<b>%s</b>", text);
	widget = gtk_label_new (markup);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 2, 1);
	gtk_widget_show (widget);
	g_free (markup);

	text = _("Type the name by which you would like to refer to "
		 "this account.\nFor example, \"Work\" or \"Personal\".");
	widget = gtk_label_new (text);
	gtk_widget_set_margin_left (widget, 12);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 1, 2, 1);
	gtk_widget_show (widget);

	text = _("_Name:");
	widget = gtk_label_new_with_mnemonic (text);
	gtk_widget_set_margin_left (widget, 12);
	gtk_size_group_add_widget (size_group, widget);
	gtk_misc_set_alignment (GTK_MISC (widget), 1.0, 0.5);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 2, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_entry_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 2, 1, 1);
	gtk_widget_show (widget);

	e_binding_bind_object_text_property (
		source, "display-name",
		widget, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	/* This entry affects the "check-complete" result. */
	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (e_mail_config_page_changed), page);

	/*** Required Information ***/

	widget = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (widget), 6);
	gtk_grid_set_column_spacing (GTK_GRID (widget), 6);
	gtk_box_pack_start (GTK_BOX (page), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	text = _("Required Information");
	markup = g_markup_printf_escaped ("<b>%s</b>", text);
	widget = gtk_label_new (markup);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 2, 1);
	gtk_widget_show (widget);
	g_free (markup);

	text = _("Full Nam_e:");
	widget = gtk_label_new_with_mnemonic (text);
	gtk_widget_set_margin_left (widget, 12);
	gtk_size_group_add_widget (size_group, widget);
	gtk_misc_set_alignment (GTK_MISC (widget), 1.0, 0.5);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 1, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_entry_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 1, 1, 1);
	gtk_widget_show (widget);

	e_binding_bind_object_text_property (
		extension, "name",
		widget, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	/* This entry affects the "check-complete" result. */
	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (e_mail_config_page_changed), page);

	text = _("Email _Address:");
	widget = gtk_label_new_with_mnemonic (text);
	gtk_widget_set_margin_left (widget, 12);
	gtk_size_group_add_widget (size_group, widget);
	gtk_misc_set_alignment (GTK_MISC (widget), 1.0, 0.5);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 2, 1, 1);
	gtk_widget_show (widget);

	g_object_bind_property (
		page, "show-email-address",
		widget, "visible",
		G_BINDING_SYNC_CREATE);

	label = GTK_LABEL (widget);

	widget = gtk_entry_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 2, 1, 1);
	gtk_widget_show (widget);

	e_binding_bind_object_text_property (
		extension, "address",
		widget, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		page, "show-email-address",
		widget, "visible",
		G_BINDING_SYNC_CREATE);

	/* This entry affects the "check-complete" result. */
	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (e_mail_config_page_changed), page);

	/*** Optional Information ***/

	widget = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (widget), 6);
	gtk_grid_set_column_spacing (GTK_GRID (widget), 6);
	gtk_box_pack_start (GTK_BOX (page), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	text = _("Optional Information");
	markup = g_markup_printf_escaped ("<b>%s</b>", text);
	widget = gtk_label_new (markup);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 3, 1);
	gtk_widget_show (widget);
	g_free (markup);

	text = _("Re_ply-To:");
	widget = gtk_label_new_with_mnemonic (text);
	gtk_widget_set_margin_left (widget, 12);
	gtk_size_group_add_widget (size_group, widget);
	gtk_misc_set_alignment (GTK_MISC (widget), 1.0, 0.5);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 1, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_entry_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 1, 2, 1);
	gtk_widget_show (widget);

	e_binding_bind_object_text_property (
		extension, "reply-to",
		widget, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	/* This entry affects the "check-complete" result. */
	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (e_mail_config_page_changed), page);

	text = _("Or_ganization:");
	widget = gtk_label_new_with_mnemonic (text);
	gtk_widget_set_margin_left (widget, 12);
	gtk_size_group_add_widget (size_group, widget);
	gtk_misc_set_alignment (GTK_MISC (widget), 1.0, 0.5);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 2, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_entry_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 2, 2, 1);
	gtk_widget_show (widget);

	e_binding_bind_object_text_property (
		extension, "organization",
		widget, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	text = _("Si_gnature:");
	widget = gtk_label_new_with_mnemonic (text);
	gtk_widget_set_margin_left (widget, 12);
	gtk_size_group_add_widget (size_group, widget);
	gtk_misc_set_alignment (GTK_MISC (widget), 1.0, 0.5);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 3, 1, 1);
	gtk_widget_show (widget);

	g_object_bind_property (
		page, "show-signatures",
		widget, "visible",
		G_BINDING_SYNC_CREATE);

	label = GTK_LABEL (widget);

	widget = e_mail_signature_combo_box_new (registry);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_widget_set_halign (widget, GTK_ALIGN_START);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 3, 1, 1);
	gtk_widget_show (widget);

	g_object_bind_property (
		extension, "signature-uid",
		widget, "active-id",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		page, "show-signatures",
		widget, "visible",
		G_BINDING_SYNC_CREATE);

	text = _("Add Ne_w Signature...");
	widget = gtk_button_new_with_mnemonic (text);
	gtk_grid_attach (GTK_GRID (container), widget, 2, 3, 1, 1);
	gtk_widget_show (widget);

	g_object_bind_property (
		page, "show-signatures",
		widget, "visible",
		G_BINDING_SYNC_CREATE);

	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (mail_config_identity_page_add_signature_cb), page);

	g_object_unref (size_group);

	e_extensible_load_extensions (E_EXTENSIBLE (page));

	widget = gtk_check_button_new_with_mnemonic (_("Try _setup account automatically, based on Email Address"));
	g_object_set (G_OBJECT (widget),
		"valign", GTK_ALIGN_END,
		"vexpand", TRUE,
		"active", TRUE,
		NULL);

	g_object_bind_property (
		page, "show-autodiscover-check",
		widget, "visible",
		G_BINDING_SYNC_CREATE);

	page->priv->autodiscover_check = widget;

	gtk_container_add (GTK_CONTAINER (page), widget);
}

static gboolean
mail_config_identity_page_check_complete (EMailConfigPage *page)
{
	EMailConfigIdentityPage *id_page;
	ESource *source;
	ESourceMailIdentity *extension;
	const gchar *extension_name;
	const gchar *name;
	const gchar *address;
	const gchar *reply_to;
	const gchar *display_name;

	id_page = E_MAIL_CONFIG_IDENTITY_PAGE (page);
	extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
	source = e_mail_config_identity_page_get_identity_source (id_page);
	extension = e_source_get_extension (source, extension_name);

	name = e_source_mail_identity_get_name (extension);
	address = e_source_mail_identity_get_address (extension);
	reply_to = e_source_mail_identity_get_reply_to (extension);

	display_name = e_source_get_display_name (source);

	if (name == NULL)
		return FALSE;

	/* Only enforce when the email address is visible. */
	if (e_mail_config_identity_page_get_show_email_address (id_page)) {
		if (address == NULL)
			return FALSE;

		if (!mail_config_identity_page_is_email (address))
			return FALSE;
	}

	/* A NULL reply_to string is allowed. */
	if (reply_to != NULL && !mail_config_identity_page_is_email (reply_to))
		return FALSE;

	/* Only enforce when account information is visible. */
	if (e_mail_config_identity_page_get_show_account_info (id_page))
		if (display_name == NULL || *display_name == '\0')
			return FALSE;

	return TRUE;
}

static void
e_mail_config_identity_page_class_init (EMailConfigIdentityPageClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (
		class, sizeof (EMailConfigIdentityPagePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_config_identity_page_set_property;
	object_class->get_property = mail_config_identity_page_get_property;
	object_class->dispose = mail_config_identity_page_dispose;
	object_class->constructed = mail_config_identity_page_constructed;

	g_object_class_install_property (
		object_class,
		PROP_REGISTRY,
		g_param_spec_object (
			"registry",
			"Registry",
			"Registry of data sources",
			E_TYPE_SOURCE_REGISTRY,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

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

	g_object_class_install_property (
		object_class,
		PROP_SHOW_ACCOUNT_INFO,
		g_param_spec_boolean (
			"show-account-info",
			"Show Account Info",
			"Show the \"Account Information\" section",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_EMAIL_ADDRESS,
		g_param_spec_boolean (
			"show-email-address",
			"Show Email Address",
			"Show the \"Email Address\" field",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_INSTRUCTIONS,
		g_param_spec_boolean (
			"show-instructions",
			"Show Instructions",
			"Show helpful instructions",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_SIGNATURES,
		g_param_spec_boolean (
			"show-signatures",
			"Show Signatures",
			"Show mail signature options",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_AUTODISCOVER_CHECK,
		g_param_spec_boolean (
			"show-autodiscover-check",
			"Show Autodiscover Check",
			"Show check button to allow autodiscover based on Email Address",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));
}

static void
e_mail_config_identity_page_interface_init (EMailConfigPageInterface *iface)
{
	iface->title = _("Identity");
	iface->sort_order = E_MAIL_CONFIG_IDENTITY_PAGE_SORT_ORDER;
	iface->check_complete = mail_config_identity_page_check_complete;
}

static void
e_mail_config_identity_page_init (EMailConfigIdentityPage *page)
{
	page->priv = E_MAIL_CONFIG_IDENTITY_PAGE_GET_PRIVATE (page);
}

EMailConfigPage *
e_mail_config_identity_page_new (ESourceRegistry *registry,
                                 ESource *identity_source)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);
	g_return_val_if_fail (E_IS_SOURCE (identity_source), NULL);

	return g_object_new (
		E_TYPE_MAIL_CONFIG_IDENTITY_PAGE,
		"registry", registry,
		"identity-source", identity_source,
		NULL);
}

ESourceRegistry *
e_mail_config_identity_page_get_registry (EMailConfigIdentityPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_IDENTITY_PAGE (page), NULL);

	return page->priv->registry;
}

ESource *
e_mail_config_identity_page_get_identity_source (EMailConfigIdentityPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_IDENTITY_PAGE (page), NULL);

	return page->priv->identity_source;
}

gboolean
e_mail_config_identity_page_get_show_account_info (EMailConfigIdentityPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_IDENTITY_PAGE (page), FALSE);

	return page->priv->show_account_info;
}

void
e_mail_config_identity_page_set_show_account_info (EMailConfigIdentityPage *page,
                                                   gboolean show_account_info)
{
	g_return_if_fail (E_IS_MAIL_CONFIG_IDENTITY_PAGE (page));

	if (page->priv->show_account_info == show_account_info)
		return;

	page->priv->show_account_info = show_account_info;

	g_object_notify (G_OBJECT (page), "show-account-info");
}

gboolean
e_mail_config_identity_page_get_show_email_address (EMailConfigIdentityPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_IDENTITY_PAGE (page), FALSE);

	return page->priv->show_email_address;
}

void
e_mail_config_identity_page_set_show_email_address (EMailConfigIdentityPage *page,
                                                    gboolean show_email_address)
{
	g_return_if_fail (E_IS_MAIL_CONFIG_IDENTITY_PAGE (page));

	if (page->priv->show_email_address == show_email_address)
		return;

	page->priv->show_email_address = show_email_address;

	g_object_notify (G_OBJECT (page), "show-email-address");
}

gboolean
e_mail_config_identity_page_get_show_instructions (EMailConfigIdentityPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_IDENTITY_PAGE (page), FALSE);

	return page->priv->show_instructions;
}

void
e_mail_config_identity_page_set_show_instructions (EMailConfigIdentityPage *page,
                                                   gboolean show_instructions)
{
	g_return_if_fail (E_IS_MAIL_CONFIG_IDENTITY_PAGE (page));

	if (page->priv->show_instructions == show_instructions)
		return;

	page->priv->show_instructions = show_instructions;

	g_object_notify (G_OBJECT (page), "show-instructions");
}

gboolean
e_mail_config_identity_page_get_show_signatures (EMailConfigIdentityPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_IDENTITY_PAGE (page), FALSE);

	return page->priv->show_signatures;
}

void
e_mail_config_identity_page_set_show_signatures (EMailConfigIdentityPage *page,
                                                 gboolean show_signatures)
{
	g_return_if_fail (E_IS_MAIL_CONFIG_IDENTITY_PAGE (page));

	if (page->priv->show_signatures == show_signatures)
		return;

	page->priv->show_signatures = show_signatures;

	g_object_notify (G_OBJECT (page), "show-signatures");
}

void
e_mail_config_identity_page_set_show_autodiscover_check (EMailConfigIdentityPage *page,
							 gboolean show_autodiscover)
{
	g_return_if_fail (E_IS_MAIL_CONFIG_IDENTITY_PAGE (page));

	if ((page->priv->show_autodiscover_check ? 1 : 0) == (show_autodiscover ? 1 : 0))
		return;

	page->priv->show_autodiscover_check = show_autodiscover;

	g_object_notify (G_OBJECT (page), "show-autodiscover-check");
}

gboolean
e_mail_config_identity_page_get_show_autodiscover_check (EMailConfigIdentityPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_IDENTITY_PAGE (page), FALSE);

	return page->priv->show_autodiscover_check;
}

GtkWidget *
e_mail_config_identity_page_get_autodiscover_check (EMailConfigIdentityPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_IDENTITY_PAGE (page), NULL);

	return page->priv->autodiscover_check;
}
