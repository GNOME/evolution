/*
 * e-mail-config-imap-headers-page.c
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

#include "e-mail-config-imap-headers-page.h"

#include <config.h>
#include <glib/gi18n-lib.h>

#include <libedataserver/e-source-camel.h>
#include <libedataserver/e-source-mail-account.h>

#include <mail/e-mail-config-security-page.h>

#include "e-mail-config-header-manager.h"

#define E_MAIL_CONFIG_IMAP_HEADERS_PAGE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_CONFIG_IMAP_HEADERS_PAGE, EMailConfigImapHeadersPagePrivate))

#define E_MAIL_CONFIG_IMAP_HEADERS_PAGE_SORT_ORDER \
	(E_MAIL_CONFIG_SECURITY_PAGE_SORT_ORDER + 10)

struct _EMailConfigImapHeadersPagePrivate {
	ESource *account_source;
	CamelSettings *settings;

	GtkToggleButton *fetch_all_headers;  /* not referenced */
	GtkToggleButton *fetch_bas_headers;  /* not referenced */
	GtkToggleButton *fetch_bml_headers;  /* not referenced */
};

enum {
	PROP_0,
	PROP_ACCOUNT_SOURCE
};

/* Forward Declarations */
static void	e_mail_config_imap_headers_page_interface_init
					(EMailConfigPageInterface *interface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (
	EMailConfigImapHeadersPage,
	e_mail_config_imap_headers_page,
	GTK_TYPE_BOX,
	0,
	G_IMPLEMENT_INTERFACE_DYNAMIC (
		E_TYPE_MAIL_CONFIG_PAGE,
		e_mail_config_imap_headers_page_interface_init))

static void
mail_config_imap_headers_page_all_toggled (GtkToggleButton *toggle_button,
                                           EMailConfigImapHeadersPage *page)
{
	if (gtk_toggle_button_get_active (toggle_button)) {
		CamelSettings *settings;
		CamelFetchHeadersType fetch_headers;

		settings = page->priv->settings;
		fetch_headers = CAMEL_FETCH_HEADERS_ALL;
		g_object_set (settings, "fetch-headers", fetch_headers, NULL);
	}
}

static void
mail_config_imap_headers_page_bas_toggled (GtkToggleButton *toggle_button,
                                           EMailConfigImapHeadersPage *page)
{
	if (gtk_toggle_button_get_active (toggle_button)) {
		CamelSettings *settings;
		CamelFetchHeadersType fetch_headers;

		settings = page->priv->settings;
		fetch_headers = CAMEL_FETCH_HEADERS_BASIC;
		g_object_set (settings, "fetch-headers", fetch_headers, NULL);
	}
}

static void
mail_config_imap_headers_page_bml_toggled (GtkToggleButton *toggle_button,
                                           EMailConfigImapHeadersPage *page)
{
	if (gtk_toggle_button_get_active (toggle_button)) {
		CamelSettings *settings;
		CamelFetchHeadersType fetch_headers;

		settings = page->priv->settings;
		fetch_headers = CAMEL_FETCH_HEADERS_BASIC_AND_MAILING_LIST;
		g_object_set (settings, "fetch-headers", fetch_headers, NULL);
	}
}

static void
mail_config_imap_headers_page_set_account_source (EMailConfigImapHeadersPage *page,
                                                  ESource *account_source)
{
	g_return_if_fail (E_IS_SOURCE (account_source));
	g_return_if_fail (page->priv->account_source == NULL);

	page->priv->account_source = g_object_ref (account_source);
}

static void
mail_config_imap_headers_page_set_property (GObject *object,
                                            guint property_id,
                                            const GValue *value,
                                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACCOUNT_SOURCE:
			mail_config_imap_headers_page_set_account_source (
				E_MAIL_CONFIG_IMAP_HEADERS_PAGE (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_imap_headers_page_get_property (GObject *object,
                                            guint property_id,
                                            GValue *value,
                                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACCOUNT_SOURCE:
			g_value_set_object (
				value,
				e_mail_config_imap_headers_page_get_account_source (
				E_MAIL_CONFIG_IMAP_HEADERS_PAGE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_imap_headers_page_dispose (GObject *object)
{
	EMailConfigImapHeadersPagePrivate *priv;

	priv = E_MAIL_CONFIG_IMAP_HEADERS_PAGE_GET_PRIVATE (object);

	if (priv->account_source != NULL) {
		g_object_unref (priv->account_source);
		priv->account_source = NULL;
	}

	if (priv->settings != NULL) {
		g_object_unref (priv->settings);
		priv->settings = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_config_imap_headers_page_parent_class)->
		dispose (object);
}

static void
mail_config_imap_headers_page_constructed (GObject *object)
{
	EMailConfigImapHeadersPage *page;
	ESource *source;
	ESourceCamel *camel_ext;
	ESourceBackend *backend_ext;
	CamelSettings *settings;
	CamelFetchHeadersType fetch_headers;
	GtkWidget *widget;
	GtkWidget *container;
	GtkToggleButton *toggle_button;
	GSList *group = NULL;
	const gchar *backend_name;
	const gchar *extension_name;
	const gchar *text;
	gchar *markup;

	page = E_MAIL_CONFIG_IMAP_HEADERS_PAGE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_config_imap_headers_page_parent_class)->
		constructed (object);

	source = e_mail_config_imap_headers_page_get_account_source (page);

	extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;
	backend_ext = e_source_get_extension (source, extension_name);
	backend_name = e_source_backend_get_backend_name (backend_ext);

	extension_name = e_source_camel_get_extension_name (backend_name);
	camel_ext = e_source_get_extension (source, extension_name);
	settings = e_source_camel_get_settings (camel_ext);

	page->priv->settings = g_object_ref (settings);

	gtk_orientable_set_orientation (
		GTK_ORIENTABLE (page), GTK_ORIENTATION_VERTICAL);

	gtk_box_set_spacing (GTK_BOX (page), 12);

	/*** IMAP Headers ***/

	/* Use row-spacing=0 so we can pack the "Basic Headers" hint
	 * label closer to its radio button.  Unfortunately this means
	 * we have to remember to set a top margin on all the children. */
	widget = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (widget), 0);
	gtk_grid_set_column_spacing (GTK_GRID (widget), 6);
	gtk_box_pack_start (GTK_BOX (page), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	text = _("IMAP Headers");
	markup = g_markup_printf_escaped ("<b>%s</b>", text);
	widget = gtk_label_new (markup);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 1, 1);
	gtk_widget_show (widget);

	text = _("Select a predefined set of IMAP headers to fetch.\n"
		 "Note, larger sets of headers take longer to download.");
	widget = gtk_label_new (text);
	gtk_widget_set_margin_top (widget, 6);
	gtk_widget_set_margin_left (widget, 12);
	gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 1, 1, 1);
	gtk_widget_show (widget);

	text = _("_Fetch All Headers");
	widget = gtk_radio_button_new_with_mnemonic (group, text);
	gtk_widget_set_margin_top (widget, 6);
	gtk_widget_set_margin_left (widget, 12);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 2, 1, 1);
	page->priv->fetch_all_headers = GTK_TOGGLE_BUTTON (widget);
	gtk_widget_show (widget);

	g_signal_connect (
		widget, "toggled",
		G_CALLBACK (mail_config_imap_headers_page_all_toggled), page);

	group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget));

	text = _("_Basic Headers (fastest)");
	widget = gtk_radio_button_new_with_mnemonic (group, text);
	gtk_widget_set_margin_top (widget, 6);
	gtk_widget_set_margin_left (widget, 12);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 3, 1, 1);
	page->priv->fetch_bas_headers = GTK_TOGGLE_BUTTON (widget);
	gtk_widget_show (widget);

	g_signal_connect (
		widget, "toggled",
		G_CALLBACK (mail_config_imap_headers_page_bas_toggled), page);

	group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget));

	text = _("Use this if you are not filtering any mailing lists.");
	markup = g_markup_printf_escaped ("<small>%s</small>", text);
	widget = gtk_label_new (markup);
	gtk_widget_set_margin_left (widget, 36);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 4, 1, 1);
	gtk_widget_show (widget);
	g_free (markup);

	text = _("Basic and _Mailing List Headers (default)");
	widget = gtk_radio_button_new_with_mnemonic (group, text);
	gtk_widget_set_margin_top (widget, 6);
	gtk_widget_set_margin_left (widget, 12);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 5, 1, 1);
	page->priv->fetch_bml_headers = GTK_TOGGLE_BUTTON (widget);
	gtk_widget_show (widget);

	g_signal_connect (
		widget, "toggled",
		G_CALLBACK (mail_config_imap_headers_page_bml_toggled), page);

	/* Pick an initial radio button. */

	g_object_get (settings, "fetch-headers", &fetch_headers, NULL);

	switch (fetch_headers) {
		case CAMEL_FETCH_HEADERS_ALL:
			toggle_button = page->priv->fetch_all_headers;
			break;

		case CAMEL_FETCH_HEADERS_BASIC:
			toggle_button = page->priv->fetch_bas_headers;
			break;

		case CAMEL_FETCH_HEADERS_BASIC_AND_MAILING_LIST:
		default:
			toggle_button = page->priv->fetch_bml_headers;
			break;
	}

	gtk_toggle_button_set_active (toggle_button, TRUE);

	/*** Custom Headers ***/

	widget = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (widget), 6);
	gtk_grid_set_column_spacing (GTK_GRID (widget), 6);
	gtk_box_pack_start (GTK_BOX (page), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	g_object_bind_property (
		page->priv->fetch_all_headers, "active",
		widget, "sensitive",
		G_BINDING_SYNC_CREATE |
		G_BINDING_INVERT_BOOLEAN);

	container = widget;

	text = _("Custom Headers");
	markup = g_markup_printf_escaped ("<b>%s</b>", text);
	widget = gtk_label_new (markup);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 1, 1);
	gtk_widget_show (widget);

	text = _("Specify any extra headers to fetch in addition "
		 "to the predefined set of headers selected above.");
	widget = gtk_label_new (text);
	gtk_widget_set_margin_left (widget, 12);
	gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 1, 1, 1);
	gtk_widget_show (widget);

	widget = e_mail_config_header_manager_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_widget_set_vexpand (widget, TRUE);
	gtk_widget_set_margin_left (widget, 12);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 2, 1, 1);
	gtk_widget_show (widget);

	g_object_bind_property (
		settings, "fetch-headers-extra",
		widget, "headers",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);
}

static void
e_mail_config_imap_headers_page_class_init (EMailConfigImapHeadersPageClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (
		class, sizeof (EMailConfigImapHeadersPagePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_config_imap_headers_page_set_property;
	object_class->get_property = mail_config_imap_headers_page_get_property;
	object_class->dispose = mail_config_imap_headers_page_dispose;
	object_class->constructed = mail_config_imap_headers_page_constructed;

	g_object_class_install_property (
		object_class,
		PROP_ACCOUNT_SOURCE,
		g_param_spec_object (
			"account-source",
			"Account Source",
			"Mail account source being edited",
			E_TYPE_SOURCE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
e_mail_config_imap_headers_page_class_finalize (EMailConfigImapHeadersPageClass *class)
{
}

static void
e_mail_config_imap_headers_page_interface_init (EMailConfigPageInterface *interface)
{
	interface->title = _("IMAP Headers");
	interface->sort_order = E_MAIL_CONFIG_IMAP_HEADERS_PAGE_SORT_ORDER;
}

static void
e_mail_config_imap_headers_page_init (EMailConfigImapHeadersPage *page)
{
	page->priv = E_MAIL_CONFIG_IMAP_HEADERS_PAGE_GET_PRIVATE (page);
}

void
e_mail_config_imap_headers_page_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_mail_config_imap_headers_page_register_type (type_module);
}

EMailConfigPage *
e_mail_config_imap_headers_page_new (ESource *account_source)
{
	g_return_val_if_fail (E_IS_SOURCE (account_source), NULL);

	return g_object_new (
		E_TYPE_MAIL_CONFIG_IMAP_HEADERS_PAGE,
		"account-source", account_source, NULL);
}

ESource *
e_mail_config_imap_headers_page_get_account_source (EMailConfigImapHeadersPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_IMAP_HEADERS_PAGE (page), NULL);

	return page->priv->account_source;
}

