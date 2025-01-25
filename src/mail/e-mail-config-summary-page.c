/*
 * e-mail-config-summary-page.c
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

#include <glib/gi18n-lib.h>

#include <camel/camel.h>
#include <libebackend/libebackend.h>
#include <e-util/e-util.h>

#include "e-mail-config-summary-page.h"

struct _EMailConfigSummaryPagePrivate {
	ESource *account_source;
	ESource *identity_source;
	ESource *transport_source;
	EMailConfigServiceBackend *account_backend;
	EMailConfigServiceBackend *transport_backend;

	gulong account_source_changed_id;
	gulong identity_source_changed_id;
	gulong transport_source_changed_id;

	/* Widgets (not referenced) */
	GtkBox *main_box;
	GtkLabel *name_label;
	GtkLabel *address_label;
	GtkLabel *recv_backend_label;
	GtkLabel *recv_host_label;
	GtkLabel *recv_user_label;
	GtkLabel *recv_security_label;
	GtkLabel *send_backend_label;
	GtkLabel *send_host_label;
	GtkLabel *send_user_label;
	GtkLabel *send_security_label;
	GtkEntry *account_name_entry;

	GBinding *account_name_binding;
};

enum {
	PROP_0,
	PROP_ACCOUNT_BACKEND,
	PROP_ACCOUNT_SOURCE,
	PROP_IDENTITY_SOURCE,
	PROP_TRANSPORT_BACKEND,
	PROP_TRANSPORT_SOURCE
};

enum {
	REFRESH,
	LAST_SIGNAL
};

static gulong signals[LAST_SIGNAL];

/* Forward Declarations */
static void	e_mail_config_summary_page_interface_init
					(EMailConfigPageInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EMailConfigSummaryPage, e_mail_config_summary_page, GTK_TYPE_SCROLLED_WINDOW,
	G_ADD_PRIVATE (EMailConfigSummaryPage)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL)
	G_IMPLEMENT_INTERFACE (E_TYPE_MAIL_CONFIG_PAGE, e_mail_config_summary_page_interface_init))

/* Helper for mail_config_summary_page_refresh() */
static void
mail_config_summary_page_refresh_auth_labels (ESource *source,
                                              GtkLabel *host_label,
                                              GtkLabel *user_label)
{
	ESourceAuthentication *extension;
	const gchar *extension_name;
	const gchar *value;

	extension_name = E_SOURCE_EXTENSION_AUTHENTICATION;
	if (!e_source_has_extension (source, extension_name))
		return;

	extension = e_source_get_extension (source, extension_name);

	value = e_source_authentication_get_host (extension);
	gtk_label_set_text (host_label, value);

	value = e_source_authentication_get_user (extension);
	gtk_label_set_text (user_label, value);
}

/* Helper for mail_config_summary_page_refresh() */
static void
mail_config_summary_page_refresh_security_label (ESource *source,
                                                 GtkLabel *security_label)
{
	GEnumClass *enum_class;
	GEnumValue *enum_value;
	ESourceSecurity *extension;
	const gchar *extension_name;
	const gchar *value;

	extension_name = E_SOURCE_EXTENSION_SECURITY;
	if (!e_source_has_extension (source, extension_name))
		return;

	extension = e_source_get_extension (source, extension_name);

	/* XXX This is a pain in the butt, but we want to avoid hard-coding
	 *     string values from the CamelNetworkSecurityMethod enum class
	 *     in case they change in the future. */
	enum_class = g_type_class_ref (CAMEL_TYPE_NETWORK_SECURITY_METHOD);
	value = e_source_security_get_method (extension);
	if (value != NULL)
		enum_value = g_enum_get_value_by_nick (enum_class, value);
	else
		enum_value = NULL;
	if (enum_value == NULL) {
		gtk_label_set_text (security_label, value);
	} else switch ((CamelNetworkSecurityMethod) enum_value->value) {
		case CAMEL_NETWORK_SECURITY_METHOD_NONE:
			gtk_label_set_text (security_label, _("None"));
			break;
		case CAMEL_NETWORK_SECURITY_METHOD_SSL_ON_ALTERNATE_PORT:
			gtk_label_set_text (security_label, _("TLS"));
			break;
		case CAMEL_NETWORK_SECURITY_METHOD_STARTTLS_ON_STANDARD_PORT:
			gtk_label_set_text (security_label, _("STARTTLS"));
			break;
	}
	g_type_class_unref (enum_class);
}

static void
mail_config_summary_page_source_changed (ESource *source,
                                         EMailConfigSummaryPage *page)
{
	e_mail_config_summary_page_refresh (page);
}

static void
mail_config_summary_page_set_property (GObject *object,
                                       guint property_id,
                                       const GValue *value,
                                       GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACCOUNT_BACKEND:
			e_mail_config_summary_page_set_account_backend (
				E_MAIL_CONFIG_SUMMARY_PAGE (object),
				g_value_get_object (value));
			return;

		case PROP_IDENTITY_SOURCE:
			e_mail_config_summary_page_set_identity_source (
				E_MAIL_CONFIG_SUMMARY_PAGE (object),
				g_value_get_object (value));
			return;

		case PROP_TRANSPORT_BACKEND:
			e_mail_config_summary_page_set_transport_backend (
				E_MAIL_CONFIG_SUMMARY_PAGE (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_summary_page_get_property (GObject *object,
                                       guint property_id,
                                       GValue *value,
                                       GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACCOUNT_BACKEND:
			g_value_set_object (
				value,
				e_mail_config_summary_page_get_account_backend (
				E_MAIL_CONFIG_SUMMARY_PAGE (object)));
			return;

		case PROP_ACCOUNT_SOURCE:
			g_value_set_object (
				value,
				e_mail_config_summary_page_get_account_source (
				E_MAIL_CONFIG_SUMMARY_PAGE (object)));
			return;

		case PROP_IDENTITY_SOURCE:
			g_value_set_object (
				value,
				e_mail_config_summary_page_get_identity_source (
				E_MAIL_CONFIG_SUMMARY_PAGE (object)));
			return;

		case PROP_TRANSPORT_BACKEND:
			g_value_set_object (
				value,
				e_mail_config_summary_page_get_transport_backend (
				E_MAIL_CONFIG_SUMMARY_PAGE (object)));
			return;

		case PROP_TRANSPORT_SOURCE:
			g_value_set_object (
				value,
				e_mail_config_summary_page_get_transport_source (
				E_MAIL_CONFIG_SUMMARY_PAGE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_summary_page_dispose (GObject *object)
{
	EMailConfigSummaryPage *self = E_MAIL_CONFIG_SUMMARY_PAGE (object);

	if (self->priv->account_source != NULL) {
		g_signal_handler_disconnect (
			self->priv->account_source,
			self->priv->account_source_changed_id);
		g_clear_object (&self->priv->account_source);
		self->priv->account_source_changed_id = 0;
	}

	if (self->priv->identity_source != NULL) {
		g_signal_handler_disconnect (
			self->priv->identity_source,
			self->priv->identity_source_changed_id);
		g_clear_object (&self->priv->identity_source);
	}

	if (self->priv->transport_source != NULL) {
		g_signal_handler_disconnect (
			self->priv->transport_source,
			self->priv->transport_source_changed_id);
		g_clear_object (&self->priv->transport_source);
		self->priv->transport_source_changed_id = 0;
	}

	g_clear_object (&self->priv->account_backend);
	g_clear_object (&self->priv->transport_backend);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_config_summary_page_parent_class)->dispose (object);
}

static void
mail_config_summary_page_constructed (GObject *object)
{
	EMailConfigSummaryPage *page;
	GtkLabel *label;
	GtkWidget *widget;
	GtkWidget *container;
	GtkWidget *main_box;
	GtkSizeGroup *size_group;
	const gchar *text;
	gchar *markup;

	page = E_MAIL_CONFIG_SUMMARY_PAGE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_config_summary_page_parent_class)->constructed (object);

	/* This page is dense with information,
	 * so put extra space between sections. */
	main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 24);

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	text = _("This is a summary of the settings which will be used "
		 "to access your mail.");
	widget = gtk_label_new (text);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_box_pack_start (GTK_BOX (main_box), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	/*** Account Information ***/

	widget = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (widget), 6);
	gtk_grid_set_column_spacing (GTK_GRID (widget), 6);
	gtk_box_pack_start (GTK_BOX (main_box), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	text = _("Account Information");
	markup = g_markup_printf_escaped ("<b>%s</b>", text);
	widget = gtk_label_new (markup);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 2, 1);
	gtk_widget_show (widget);
	g_free (markup);

	text = _("_Name:");
	widget = gtk_label_new_with_mnemonic (text);
	gtk_widget_set_margin_start (widget, 12);
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 1, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_entry_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 1, 1, 1);
	page->priv->account_name_entry = GTK_ENTRY (widget);
	gtk_widget_show (widget);

	/* This entry affects the "check-complete" result. */
	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (e_mail_config_page_changed), page);

	text = _("The above name will be used to identify this account.\n"
		 "Use for example, “Work” or “Personal”.");
	widget = gtk_label_new (text);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 2, 1, 1);
	gtk_widget_show (widget);

	/*** Details ***/

	widget = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (widget), 6);
	gtk_grid_set_column_spacing (GTK_GRID (widget), 12);
	gtk_box_pack_start (GTK_BOX (main_box), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	text = _("Personal Details");
	markup = g_markup_printf_escaped ("<b>%s</b>", text);
	widget = gtk_label_new (markup);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 3, 1);
	gtk_widget_show (widget);
	g_free (markup);

	text = _("Full Name:");
	widget = gtk_label_new (text);
	gtk_widget_set_margin_start (widget, 12);
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 1, 1, 1);
	gtk_widget_show (widget);

	widget = gtk_label_new (NULL);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_label_set_ellipsize (GTK_LABEL (widget), PANGO_ELLIPSIZE_END);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 1, 2, 1);
	page->priv->name_label = GTK_LABEL (widget);
	gtk_widget_show (widget);

	text = _("Email Address:");
	widget = gtk_label_new (text);
	gtk_widget_set_margin_start (widget, 12);
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 2, 1, 1);
	gtk_widget_show (widget);

	widget = gtk_label_new (NULL);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_label_set_ellipsize (GTK_LABEL (widget), PANGO_ELLIPSIZE_END);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 2, 2, 1);
	page->priv->address_label = GTK_LABEL (widget);
	gtk_widget_show (widget);

	text = _("Receiving");
	markup = g_markup_printf_escaped ("<b>%s</b>", text);
	widget = gtk_label_new (markup);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_widget_set_margin_top (widget, 6);
	gtk_size_group_add_widget (size_group, widget);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 3, 1, 1);
	gtk_widget_show (widget);
	g_free (markup);

	text = _("Sending");
	markup = g_markup_printf_escaped ("<b>%s</b>", text);
	widget = gtk_label_new (markup);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_widget_set_margin_top (widget, 6);
	gtk_size_group_add_widget (size_group, widget);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (GTK_GRID (container), widget, 2, 3, 1, 1);
	gtk_widget_show (widget);
	g_free (markup);

	text = _("Server Type:");
	widget = gtk_label_new (text);
	gtk_widget_set_margin_start (widget, 12);
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 4, 1, 1);
	gtk_widget_show (widget);

	widget = gtk_label_new (NULL);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_label_set_ellipsize (GTK_LABEL (widget), PANGO_ELLIPSIZE_END);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 4, 1, 1);
	page->priv->recv_backend_label = GTK_LABEL (widget);
	gtk_widget_show (widget);

	widget = gtk_label_new (NULL);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_label_set_ellipsize (GTK_LABEL (widget), PANGO_ELLIPSIZE_END);
	gtk_grid_attach (GTK_GRID (container), widget, 2, 4, 1, 1);
	page->priv->send_backend_label = GTK_LABEL (widget);
	gtk_widget_show (widget);

	text = _("Server:");
	widget = gtk_label_new (text);
	gtk_widget_set_margin_start (widget, 12);
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 5, 1, 1);
	gtk_widget_show (widget);

	widget = gtk_label_new (NULL);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_label_set_ellipsize (GTK_LABEL (widget), PANGO_ELLIPSIZE_END);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 5, 1, 1);
	page->priv->recv_host_label = GTK_LABEL (widget);
	gtk_widget_show (widget);

	widget = gtk_label_new (NULL);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_label_set_ellipsize (GTK_LABEL (widget), PANGO_ELLIPSIZE_END);
	gtk_grid_attach (GTK_GRID (container), widget, 2, 5, 1, 1);
	page->priv->send_host_label = GTK_LABEL (widget);
	gtk_widget_show (widget);

	text = _("Username:");
	widget = gtk_label_new (text);
	gtk_widget_set_margin_start (widget, 12);
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 6, 1, 1);
	gtk_widget_show (widget);

	widget = gtk_label_new (NULL);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_label_set_ellipsize (GTK_LABEL (widget), PANGO_ELLIPSIZE_END);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 6, 1, 1);
	page->priv->recv_user_label = GTK_LABEL (widget);
	gtk_widget_show (widget);

	widget = gtk_label_new (NULL);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_label_set_ellipsize (GTK_LABEL (widget), PANGO_ELLIPSIZE_END);
	gtk_grid_attach (GTK_GRID (container), widget, 2, 6, 1, 1);
	page->priv->send_user_label = GTK_LABEL (widget);
	gtk_widget_show (widget);

	text = _("Security:");
	widget = gtk_label_new (text);
	gtk_widget_set_margin_start (widget, 12);
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 7, 1, 1);
	gtk_widget_show (widget);

	widget = gtk_label_new (NULL);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_label_set_ellipsize (GTK_LABEL (widget), PANGO_ELLIPSIZE_END);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 7, 1, 1);
	page->priv->recv_security_label = GTK_LABEL (widget);
	gtk_widget_show (widget);

	widget = gtk_label_new (NULL);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_label_set_ellipsize (GTK_LABEL (widget), PANGO_ELLIPSIZE_END);
	gtk_grid_attach (GTK_GRID (container), widget, 2, 7, 1, 1);
	page->priv->send_security_label = GTK_LABEL (widget);
	gtk_widget_show (widget);

	g_object_unref (size_group);

	page->priv->main_box = GTK_BOX (main_box);

	e_mail_config_page_set_content (E_MAIL_CONFIG_PAGE (page), main_box);

	e_extensible_load_extensions (E_EXTENSIBLE (page));
}

static void
mail_config_summary_page_refresh (EMailConfigSummaryPage *page)
{
	EMailConfigSummaryPage *self;
	ESource *source;
	gboolean account_is_transport = FALSE;

	self = E_MAIL_CONFIG_SUMMARY_PAGE (page);

	/* Clear all labels. */
	gtk_label_set_text (self->priv->name_label, "");
	gtk_label_set_text (self->priv->address_label, "");
	gtk_label_set_text (self->priv->recv_backend_label, "");
	gtk_label_set_text (self->priv->recv_host_label, "");
	gtk_label_set_text (self->priv->recv_user_label, "");
	gtk_label_set_text (self->priv->recv_security_label, "");
	gtk_label_set_text (self->priv->send_backend_label, "");
	gtk_label_set_text (self->priv->send_host_label, "");
	gtk_label_set_text (self->priv->send_user_label, "");
	gtk_label_set_text (self->priv->send_security_label, "");

	source = e_mail_config_summary_page_get_identity_source (page);

	if (source != NULL) {
		ESourceMailIdentity *extension;
		const gchar *extension_name;
		const gchar *value;

		extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
		extension = e_source_get_extension (source, extension_name);

		value = e_source_mail_identity_get_name (extension);
		gtk_label_set_text (self->priv->name_label, value);

		value = e_source_mail_identity_get_address (extension);
		gtk_label_set_text (self->priv->address_label, value);
	}

	source = e_mail_config_summary_page_get_account_source (page);

	if (source != NULL) {
		ESourceBackend *extension;
		const gchar *extension_name;
		const gchar *value;

		extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;
		extension = e_source_get_extension (source, extension_name);

		value = e_source_backend_get_backend_name (extension);
		gtk_label_set_text (self->priv->recv_backend_label, value);

		mail_config_summary_page_refresh_auth_labels (
			source,
			self->priv->recv_host_label,
			self->priv->recv_user_label);

		mail_config_summary_page_refresh_security_label (
			source,
			self->priv->recv_security_label);

		extension_name = E_SOURCE_EXTENSION_MAIL_TRANSPORT;
		if (e_source_has_extension (source, extension_name))
			account_is_transport = TRUE;
	}

	if (account_is_transport)
		source = e_mail_config_summary_page_get_account_source (page);
	else
		source = e_mail_config_summary_page_get_transport_source (page);

	if (source != NULL) {
		ESourceBackend *extension;
		const gchar *extension_name;
		const gchar *value;

		extension_name = E_SOURCE_EXTENSION_MAIL_TRANSPORT;
		extension = e_source_get_extension (source, extension_name);

		value = e_source_backend_get_backend_name (extension);
		gtk_label_set_text (self->priv->send_backend_label, value);

		mail_config_summary_page_refresh_auth_labels (
			source,
			self->priv->send_host_label,
			self->priv->send_user_label);

		mail_config_summary_page_refresh_security_label (
			source,
			self->priv->send_security_label);
	}

	e_mail_config_page_changed (E_MAIL_CONFIG_PAGE (page));
}

static gboolean
mail_config_summary_page_check_complete (EMailConfigPage *page)
{
	EMailConfigSummaryPage *self;
	gchar *stripped_text;
	const gchar *text;
	gboolean complete;

	self = E_MAIL_CONFIG_SUMMARY_PAGE (page);

	/* Strip the account name of leading and trailing
	 * whitespace as e_source_set_display_name() does. */
	text = gtk_entry_get_text (self->priv->account_name_entry);
	stripped_text = g_strstrip (g_strdup ((text != NULL) ? text : ""));
	complete = (*stripped_text != '\0');
	g_free (stripped_text);

	e_util_set_entry_issue_hint (GTK_WIDGET (self->priv->account_name_entry), complete ? NULL : _("Account Name cannot be empty"));

	if (complete) {
		gboolean recv_is_none, send_is_none;

		recv_is_none = gtk_widget_get_visible (GTK_WIDGET (self->priv->recv_backend_label)) &&
				g_strcmp0 (gtk_label_get_text (self->priv->recv_backend_label), "none") == 0;

		send_is_none = gtk_widget_get_visible (GTK_WIDGET (self->priv->send_backend_label)) &&
				g_strcmp0 (gtk_label_get_text (self->priv->send_backend_label), "none") == 0;

		complete = !recv_is_none || !send_is_none;

		e_util_set_entry_issue_hint (GTK_WIDGET (self->priv->account_name_entry),
			complete ? NULL : _("Cannot have both receiving and sending parts set to None"));
	}

	return complete;
}

static void
mail_config_summary_page_commit_changes (EMailConfigPage *page,
                                         GQueue *source_queue)
{
	EMailConfigSummaryPage *self;
	EMailConfigServiceBackend *backend;
	ESource *account_source;
	ESource *identity_source;
	ESource *transport_source;
	ESource *collection_source;
	ESourceExtension *extension;
	const gchar *extension_name;
	const gchar *parent_uid;
	const gchar *text;

	self = E_MAIL_CONFIG_SUMMARY_PAGE (page);

	backend = e_mail_config_summary_page_get_account_backend (
		E_MAIL_CONFIG_SUMMARY_PAGE (page));
	account_source =
		e_mail_config_service_backend_get_source (backend);
	collection_source =
		e_mail_config_service_backend_get_collection (backend);

	/* The transport backend is NULL when the Sending Page is hidden. */
	backend = e_mail_config_summary_page_get_transport_backend (
		E_MAIL_CONFIG_SUMMARY_PAGE (page));
	transport_source = (backend != NULL) ?
		e_mail_config_service_backend_get_source (backend) : NULL;

	identity_source = e_mail_config_summary_page_get_identity_source (
		E_MAIL_CONFIG_SUMMARY_PAGE (page));

	/* This should propagate to the other sources through bindings. */
	text = gtk_entry_get_text (self->priv->account_name_entry);
	e_source_set_display_name (identity_source, text);

	/* Setup parent/child relationships and cross-references. */

	if (collection_source != NULL) {
		parent_uid = e_source_get_uid (collection_source);
		e_source_set_parent (account_source, parent_uid);
		e_source_set_parent (identity_source, parent_uid);
		if (transport_source != NULL)
			e_source_set_parent (transport_source, parent_uid);
	} else {
		parent_uid = e_source_get_uid (account_source);
		e_source_set_parent (identity_source, parent_uid);
		if (transport_source != NULL)
			e_source_set_parent (transport_source, parent_uid);
	}

	extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;
	extension = e_source_get_extension (account_source, extension_name);
	e_source_mail_account_set_identity_uid (
		E_SOURCE_MAIL_ACCOUNT (extension),
		e_source_get_uid (identity_source));

	extension_name = E_SOURCE_EXTENSION_MAIL_SUBMISSION;
	extension = e_source_get_extension (identity_source, extension_name);
	if (transport_source != NULL)
		e_source_mail_submission_set_transport_uid (
			E_SOURCE_MAIL_SUBMISSION (extension),
			e_source_get_uid (transport_source));
}

static void
e_mail_config_summary_page_class_init (EMailConfigSummaryPageClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_config_summary_page_set_property;
	object_class->get_property = mail_config_summary_page_get_property;
	object_class->dispose = mail_config_summary_page_dispose;
	object_class->constructed = mail_config_summary_page_constructed;

	class->refresh = mail_config_summary_page_refresh;

	g_object_class_install_property (
		object_class,
		PROP_ACCOUNT_BACKEND,
		g_param_spec_object (
			"account-backend",
			"Account Backend",
			"Active mail account service backend",
			E_TYPE_MAIL_CONFIG_SERVICE_BACKEND,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_ACCOUNT_SOURCE,
		g_param_spec_object (
			"account-source",
			"Account Source",
			"Mail account source being edited",
			E_TYPE_SOURCE,
			G_PARAM_READABLE |
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
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_TRANSPORT_BACKEND,
		g_param_spec_object (
			"transport-backend",
			"Transport Backend",
			"Active mail transport service backend",
			E_TYPE_MAIL_CONFIG_SERVICE_BACKEND,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_TRANSPORT_SOURCE,
		g_param_spec_object (
			"transport-source",
			"Transport Source",
			"Mail transport source being edited",
			E_TYPE_SOURCE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	signals[REFRESH] = g_signal_new (
		"refresh",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailConfigSummaryPageClass, refresh),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_mail_config_summary_page_interface_init (EMailConfigPageInterface *iface)
{
	iface->title = _("Account Summary");
	iface->sort_order = E_MAIL_CONFIG_SUMMARY_PAGE_SORT_ORDER;
	iface->check_complete = mail_config_summary_page_check_complete;
	iface->commit_changes = mail_config_summary_page_commit_changes;
}

static void
e_mail_config_summary_page_init (EMailConfigSummaryPage *page)
{
	page->priv = e_mail_config_summary_page_get_instance_private (page);
}

EMailConfigPage *
e_mail_config_summary_page_new (void)
{
	return g_object_new (E_TYPE_MAIL_CONFIG_SUMMARY_PAGE, NULL);
}

GtkBox *
e_mail_config_summary_page_get_internal_box (EMailConfigSummaryPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_SUMMARY_PAGE (page), NULL);

	return page->priv->main_box;
}

const gchar *
e_mail_config_summary_page_get_account_name (EMailConfigSummaryPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_SUMMARY_PAGE (page), NULL);

	return gtk_entry_get_text (page->priv->account_name_entry);
}

void
e_mail_config_summary_page_refresh (EMailConfigSummaryPage *page)
{
	g_return_if_fail (E_IS_MAIL_CONFIG_SUMMARY_PAGE (page));

	g_signal_emit (page, signals[REFRESH], 0);
}

EMailConfigServiceBackend *
e_mail_config_summary_page_get_account_backend (EMailConfigSummaryPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_SUMMARY_PAGE (page), NULL);

	return page->priv->account_backend;
}

void
e_mail_config_summary_page_set_account_backend (EMailConfigSummaryPage *page,
                                                EMailConfigServiceBackend *backend)
{
	g_return_if_fail (E_IS_MAIL_CONFIG_SUMMARY_PAGE (page));

	if (backend != NULL) {
		g_return_if_fail (E_IS_MAIL_CONFIG_SERVICE_BACKEND (backend));
		g_object_ref (backend);
	}

	if (page->priv->account_backend != NULL)
		g_object_unref (page->priv->account_backend);

	page->priv->account_backend = backend;

	if (page->priv->account_source != NULL) {
		g_signal_handler_disconnect (
			page->priv->account_source,
			page->priv->account_source_changed_id);
		g_object_unref (page->priv->account_source);
		page->priv->account_source = NULL;
		page->priv->account_source_changed_id = 0;
	}

	if (backend != NULL) {
		ESource *source;
		gulong handler_id;

		source = e_mail_config_service_backend_get_source (backend);

		handler_id = g_signal_connect (
			source, "changed",
			G_CALLBACK (mail_config_summary_page_source_changed),
			page);

		page->priv->account_source = g_object_ref (source);
		page->priv->account_source_changed_id = handler_id;
	}

	g_object_freeze_notify (G_OBJECT (page));
	g_object_notify (G_OBJECT (page), "account-backend");
	g_object_notify (G_OBJECT (page), "account-source");
	g_object_thaw_notify (G_OBJECT (page));

	e_mail_config_summary_page_refresh (page);
}

ESource *
e_mail_config_summary_page_get_account_source (EMailConfigSummaryPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_SUMMARY_PAGE (page), NULL);

	return page->priv->account_source;
}

ESource *
e_mail_config_summary_page_get_identity_source (EMailConfigSummaryPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_SUMMARY_PAGE (page), NULL);

	return page->priv->identity_source;
}

void
e_mail_config_summary_page_set_identity_source (EMailConfigSummaryPage *page,
                                                ESource *identity_source)
{
	g_return_if_fail (E_IS_MAIL_CONFIG_SUMMARY_PAGE (page));

	if (page->priv->identity_source == identity_source)
		return;

	if (identity_source != NULL) {
		g_return_if_fail (E_IS_SOURCE (identity_source));
		g_object_ref (identity_source);
	}

	if (page->priv->identity_source != NULL) {
		g_signal_handler_disconnect (
			page->priv->identity_source,
			page->priv->identity_source_changed_id);
		g_object_unref (page->priv->identity_source);
	}

	page->priv->identity_source = identity_source;
	page->priv->identity_source_changed_id = 0;

	g_clear_pointer (&page->priv->account_name_binding, g_binding_unbind);

	if (identity_source != NULL) {
		gulong handler_id;

		handler_id = g_signal_connect (
			identity_source, "changed",
			G_CALLBACK (mail_config_summary_page_source_changed),
			page);

		page->priv->identity_source_changed_id = handler_id;

		page->priv->account_name_binding =
			e_binding_bind_property (identity_source, "display-name",
				page->priv->account_name_entry, "text",
				G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
	}

	g_object_notify (G_OBJECT (page), "identity-source");

	e_mail_config_summary_page_refresh (page);
}

EMailConfigServiceBackend *
e_mail_config_summary_page_get_transport_backend (EMailConfigSummaryPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_SUMMARY_PAGE (page), NULL);

	return page->priv->transport_backend;
}

void
e_mail_config_summary_page_set_transport_backend (EMailConfigSummaryPage *page,
                                                  EMailConfigServiceBackend *backend)
{
	g_return_if_fail (E_IS_MAIL_CONFIG_SUMMARY_PAGE (page));

	if (backend != NULL) {
		g_return_if_fail (E_IS_MAIL_CONFIG_SERVICE_BACKEND (backend));
		g_object_ref (backend);
	}

	if (page->priv->transport_backend != NULL)
		g_object_unref (page->priv->transport_backend);

	page->priv->transport_backend = backend;

	if (page->priv->transport_source != NULL) {
		g_signal_handler_disconnect (
			page->priv->transport_source,
			page->priv->transport_source_changed_id);
		g_object_unref (page->priv->transport_source);
		page->priv->transport_source = NULL;
		page->priv->transport_source_changed_id = 0;
	}

	if (backend != NULL) {
		ESource *source;
		gulong handler_id;

		source = e_mail_config_service_backend_get_source (backend);

		handler_id = g_signal_connect (
			source, "changed",
			G_CALLBACK (mail_config_summary_page_source_changed),
			page);

		page->priv->transport_source = g_object_ref (source);
		page->priv->transport_source_changed_id = handler_id;
	}

	g_object_freeze_notify (G_OBJECT (page));
	g_object_notify (G_OBJECT (page), "transport-backend");
	g_object_notify (G_OBJECT (page), "transport-source");
	g_object_thaw_notify (G_OBJECT (page));

	e_mail_config_summary_page_refresh (page);
}

ESource *
e_mail_config_summary_page_get_transport_source (EMailConfigSummaryPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_SUMMARY_PAGE (page), NULL);

	return page->priv->transport_source;
}

