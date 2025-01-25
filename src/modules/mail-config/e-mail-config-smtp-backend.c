/*
 * e-mail-config-smtp-backend.c
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

#include <mail/e-mail-config-auth-check.h>
#include <mail/e-mail-config-page.h>
#include <mail/e-mail-config-provider-page.h>
#include <mail/e-mail-config-service-page.h>

#include "e-mail-config-smtp-backend.h"

struct _EMailConfigSmtpBackendPrivate {
	GtkWidget *host_entry;			/* not referenced */
	GtkWidget *port_entry;			/* not referenced */
	GtkWidget *port_error_image;		/* not referenced */
	GtkWidget *user_entry;			/* not referenced */
	GtkWidget *forget_password_button;	/* not referenced */
	GtkWidget *security_combo_box;		/* not referenced */
	GtkWidget *auth_required_toggle;	/* not referenced */
	GtkWidget *auth_check;			/* not referenced */

	GCancellable *cancellable;
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (EMailConfigSmtpBackend, e_mail_config_smtp_backend, E_TYPE_MAIL_CONFIG_SERVICE_BACKEND, 0,
	G_ADD_PRIVATE_DYNAMIC (EMailConfigSmtpBackend))

static void
source_lookup_password_done (GObject *source,
			     GAsyncResult *result,
			     gpointer user_data)
{
	gchar *password = NULL;

	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (result != NULL);

	if (e_source_lookup_password_finish (E_SOURCE (source), result, &password, NULL)) {
		if (password && *password && E_IS_MAIL_CONFIG_SMTP_BACKEND (user_data)) {
			EMailConfigSmtpBackend *smtp_backend = user_data;

			gtk_widget_show (smtp_backend->priv->forget_password_button);
		}

		e_util_safe_free_string (password);
	}
}

static void
source_delete_password_done (GObject *source,
			     GAsyncResult *result,
			     gpointer user_data)
{
	GError *error = NULL;

	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (result != NULL);

	if (e_source_delete_password_finish (E_SOURCE (source), result, &error)) {
		if (E_IS_MAIL_CONFIG_SMTP_BACKEND (user_data)) {
			EMailConfigSmtpBackend *smtp_backend = user_data;

			gtk_widget_set_sensitive (smtp_backend->priv->forget_password_button, FALSE);
		}
	} else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_warning ("%s: Failed to forget password: %s", G_STRFUNC, error ? error->message : "Unknown error");
	}

	g_clear_error (&error);
}

static void
smtp_backend_forget_password_cb (GtkWidget *button,
				 EMailConfigSmtpBackend *smtp_backend)
{
	ESource *source;

	g_return_if_fail (E_IS_MAIL_CONFIG_SMTP_BACKEND (smtp_backend));

	source = e_mail_config_service_backend_get_source (E_MAIL_CONFIG_SERVICE_BACKEND (smtp_backend));

	e_source_delete_password (source, smtp_backend->priv->cancellable, source_delete_password_done, smtp_backend);
}

static void
server_requires_auth_toggled_cb (GtkToggleButton *toggle,
				 EMailConfigServiceBackend *backend)
{
	EMailConfigServicePage *page;

	g_return_if_fail (E_IS_MAIL_CONFIG_SERVICE_BACKEND (backend));

	page = e_mail_config_service_backend_get_page (backend);
	e_mail_config_page_changed (E_MAIL_CONFIG_PAGE (page));
}

static void
mail_config_smtp_backend_insert_widgets (EMailConfigServiceBackend *backend,
                                         GtkBox *parent)
{
	EMailConfigSmtpBackend *self = E_MAIL_CONFIG_SMTP_BACKEND (backend);
	CamelProvider *provider;
	CamelSettings *settings;
	ESource *source, *existing_source;
	ESourceBackend *extension;
	EMailConfigServicePage *page;
	EMailConfigServicePageClass *class;
	GtkLabel *label;
	GtkWidget *widget;
	GtkWidget *container;
	const gchar *backend_name;
	const gchar *extension_name;
	const gchar *mechanism;
	const gchar *text;
	guint16 port;
	gchar *markup;

	page = e_mail_config_service_backend_get_page (backend);
	source = e_mail_config_service_backend_get_source (backend);
	settings = e_mail_config_service_backend_get_settings (backend);
	existing_source = e_source_registry_ref_source (e_mail_config_service_page_get_registry (page), e_source_get_uid (source));

	class = E_MAIL_CONFIG_SERVICE_PAGE_GET_CLASS (page);
	extension_name = class->extension_name;
	extension = e_source_get_extension (source, extension_name);
	backend_name = e_source_backend_get_backend_name (extension);

	text = _("Configuration");
	markup = g_markup_printf_escaped ("<b>%s</b>", text);
	widget = gtk_label_new (markup);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_box_pack_start (GTK_BOX (parent), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);
	g_free (markup);

	widget = gtk_grid_new ();
	gtk_widget_set_margin_start (widget, 12);
	gtk_grid_set_row_spacing (GTK_GRID (widget), 6);
	gtk_grid_set_column_spacing (GTK_GRID (widget), 6);
	gtk_box_pack_start (GTK_BOX (parent), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_label_new_with_mnemonic (_("_Server:"));
	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_entry_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 0, 1, 1);
	self->priv->host_entry = widget;  /* do not reference */
	gtk_widget_show (widget);

	widget = gtk_label_new_with_mnemonic (_("_Port:"));
	gtk_grid_attach (GTK_GRID (container), widget, 2, 0, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = e_port_entry_new ();
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 3, 0, 1, 1);
	self->priv->port_entry = widget;  /* do not reference */
	gtk_widget_show (widget);

	widget = gtk_image_new_from_icon_name ("dialog-warning", GTK_ICON_SIZE_BUTTON);
	g_object_set (G_OBJECT (widget),
		"visible", FALSE,
		"has-tooltip", TRUE,
		"tooltip-text", _("Port number is not valid"),
		NULL);
	gtk_grid_attach (GTK_GRID (container), widget, 4, 0, 1, 1);
	self->priv->port_error_image = widget;  /* do not reference */

	text = _("Ser_ver requires authentication");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 1, 4, 1);
	self->priv->auth_required_toggle = widget;  /* do not reference */
	gtk_widget_show (widget);

	g_signal_connect_object (widget, "toggled",
		G_CALLBACK (server_requires_auth_toggled_cb), backend, 0);

	text = _("Security");
	markup = g_markup_printf_escaped ("<b>%s</b>", text);
	widget = gtk_label_new (markup);
	gtk_widget_set_margin_top (widget, 6);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_box_pack_start (GTK_BOX (parent), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);
	g_free (markup);

	widget = gtk_grid_new ();
	gtk_widget_set_margin_start (widget, 12);
	gtk_grid_set_row_spacing (GTK_GRID (widget), 6);
	gtk_grid_set_column_spacing (GTK_GRID (widget), 6);
	gtk_box_pack_start (GTK_BOX (parent), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_label_new_with_mnemonic (_("Encryption _method:"));
	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	/* The IDs correspond to the CamelNetworkSecurityMethod enum. */
	widget = gtk_combo_box_text_new ();
	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (widget),
		"none",
		_("No encryption"));
	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (widget),
		"starttls-on-standard-port",
		_("STARTTLS after connecting"));
	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (widget),
		"ssl-on-alternate-port",
		_("TLS on a dedicated port"));
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_widget_set_halign (widget, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 0, 1, 1);
	self->priv->security_combo_box = widget;  /* do not reference */
	gtk_widget_show (widget);

	provider = camel_provider_get (backend_name, NULL);
	if (provider != NULL && provider->port_entries != NULL)
		e_port_entry_set_camel_entries (
			E_PORT_ENTRY (self->priv->port_entry),
			provider->port_entries);

	text = _("Authentication");
	markup = g_markup_printf_escaped ("<b>%s</b>", text);
	widget = gtk_label_new (markup);
	gtk_widget_set_margin_top (widget, 6);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_box_pack_start (GTK_BOX (parent), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);
	g_free (markup);

	e_binding_bind_property (
		self->priv->auth_required_toggle, "active",
		widget, "sensitive",
		G_BINDING_SYNC_CREATE);

	widget = gtk_grid_new ();
	gtk_widget_set_margin_start (widget, 12);
	gtk_grid_set_row_spacing (GTK_GRID (widget), 6);
	gtk_grid_set_column_spacing (GTK_GRID (widget), 6);
	gtk_box_pack_start (GTK_BOX (parent), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	e_binding_bind_property (
		self->priv->auth_required_toggle, "active",
		widget, "sensitive",
		G_BINDING_SYNC_CREATE);

	container = widget;

	widget = gtk_label_new_with_mnemonic (_("T_ype:"));
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	/* We can't bind GSettings:auth-mechanism directly to this
	 * because the toggle button above influences the value we
	 * choose: "none" if the toggle button is inactive or else
	 * the active mechanism name from this widget. */
	widget = e_mail_config_auth_check_new (backend);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 0, 1, 1);
	self->priv->auth_check = widget;  /* do not reference */
	gtk_widget_show (widget);

	widget = gtk_label_new_with_mnemonic (_("User_name:"));
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 1, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_entry_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 1, 3, 1);
	self->priv->user_entry = widget;  /* do not reference */
	gtk_widget_show (widget);

	widget = gtk_button_new_with_mnemonic (_("_Forget password"));
	gtk_widget_set_halign (widget, GTK_ALIGN_START);
	gtk_widget_set_hexpand (widget, FALSE);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 2, 3, 1);
	self->priv->forget_password_button = widget; /* do not reference */
	gtk_widget_hide (widget);

	e_mail_config_provider_add_widgets (provider, settings, parent, FALSE);

	g_signal_connect (self->priv->forget_password_button, "clicked",
		G_CALLBACK (smtp_backend_forget_password_cb), backend);

	port = camel_network_settings_get_port (
		CAMEL_NETWORK_SETTINGS (settings));

	e_binding_bind_object_text_property (
		settings, "host",
		self->priv->host_entry, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property_full (
		settings, "security-method",
		self->priv->security_combo_box, "active-id",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		e_binding_transform_enum_value_to_nick,
		e_binding_transform_enum_nick_to_value,
		NULL, (GDestroyNotify) NULL);

	e_binding_bind_property (
		settings, "port",
		self->priv->port_entry, "port",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		settings, "security-method",
		self->priv->port_entry, "security-method",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_object_text_property (
		settings, "user",
		self->priv->user_entry, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	if (port != 0)
		g_object_set (G_OBJECT (self->priv->port_entry), "port", port, NULL);

	/* Enable the auth-required toggle button if
	 * we have an authentication mechanism name. */
	mechanism = camel_network_settings_get_auth_mechanism (
		CAMEL_NETWORK_SETTINGS (settings));
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (self->priv->auth_required_toggle),
		(mechanism != NULL && *mechanism != '\0'));

	if (!existing_source) {
		/* Default to TLS for new accounts */
		g_object_set (G_OBJECT (settings),
			"security-method", CAMEL_NETWORK_SECURITY_METHOD_SSL_ON_ALTERNATE_PORT,
			NULL);
	}

	g_clear_object (&existing_source);

	e_source_lookup_password (source, self->priv->cancellable, source_lookup_password_done, backend);
}

static gboolean
mail_config_smtp_backend_auto_configure (EMailConfigServiceBackend *backend,
					 EConfigLookup *config_lookup,
					 gint *out_priority,
					 gboolean *out_is_complete)
{
	EMailConfigSmtpBackend *self;
	CamelSettings *settings;
	const gchar *mechanism;

	if (!e_mail_config_service_backend_auto_configure_for_kind (backend, config_lookup,
		E_CONFIG_LOOKUP_RESULT_MAIL_SEND, NULL, NULL, out_priority, out_is_complete))
		return FALSE;

	/* XXX Need to set the authentication widgets to match the
	 *     auto-configured settings or else the auto-configured
	 *     settings will be overwritten in commit_changes().
	 *
	 *     It's not good enough to just set an auto-configured
	 *     flag and skip the widget checks in commit_changes()
	 *     if the flag is TRUE, since the user may revise the
	 *     SMTP settings before committing. */

	self = E_MAIL_CONFIG_SMTP_BACKEND (backend);

	settings = e_mail_config_service_backend_get_settings (backend);

	mechanism = camel_network_settings_get_auth_mechanism (
		CAMEL_NETWORK_SETTINGS (settings));

	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (self->priv->auth_required_toggle),
		(mechanism != NULL));

	if (mechanism != NULL)
		e_mail_config_auth_check_set_active_mechanism (
			E_MAIL_CONFIG_AUTH_CHECK (self->priv->auth_check),
			mechanism);

	return TRUE;
}

static gboolean
mail_config_smtp_backend_check_complete (EMailConfigServiceBackend *backend)
{
	EMailConfigSmtpBackend *self = E_MAIL_CONFIG_SMTP_BACKEND (backend);
	CamelSettings *settings;
	CamelNetworkSettings *network_settings;
	GtkToggleButton *toggle_button;
	EPortEntry *port_entry;
	const gchar *host;
	const gchar *user;
	gboolean correct, complete = TRUE;

	settings = e_mail_config_service_backend_get_settings (backend);

	network_settings = CAMEL_NETWORK_SETTINGS (settings);
	host = camel_network_settings_get_host (network_settings);
	user = camel_network_settings_get_user (network_settings);

	correct = (host != NULL && *host != '\0');
	complete = complete && correct;

	e_util_set_entry_issue_hint (self->priv->host_entry, correct ? NULL : _("Server address cannot be empty"));

	port_entry = E_PORT_ENTRY (self->priv->port_entry);

	correct = e_port_entry_is_valid (port_entry);
	complete = complete && correct;

	gtk_widget_set_visible (self->priv->port_error_image, !correct);

	toggle_button = GTK_TOGGLE_BUTTON (self->priv->auth_required_toggle);

	correct = TRUE;

	if (gtk_toggle_button_get_active (toggle_button))
		if (user == NULL || *user == '\0')
			correct = FALSE;

	complete = complete && correct;
	e_util_set_entry_issue_hint (self->priv->user_entry, correct ?
		((!gtk_toggle_button_get_active (toggle_button) || camel_string_is_all_ascii (user)) ? NULL :
		_("User name contains letters, which can prevent log in. Make sure the server accepts such written user name."))
		: _("User name cannot be empty"));

	return complete;
}

static void
mail_config_smtp_backend_commit_changes (EMailConfigServiceBackend *backend)
{
	EMailConfigSmtpBackend *self = E_MAIL_CONFIG_SMTP_BACKEND (backend);
	GtkToggleButton *toggle_button;
	CamelSettings *settings;
	const gchar *mechanism = NULL;

	/* The authentication mechanism name depends on both the
	 * toggle button and the EMailConfigAuthCheck widget, so
	 * we have to set it manually here. */

	settings = e_mail_config_service_backend_get_settings (backend);

	toggle_button = GTK_TOGGLE_BUTTON (self->priv->auth_required_toggle);

	if (gtk_toggle_button_get_active (toggle_button))
		mechanism = e_mail_config_auth_check_get_active_mechanism (
			E_MAIL_CONFIG_AUTH_CHECK (self->priv->auth_check));

	camel_network_settings_set_auth_mechanism (
		CAMEL_NETWORK_SETTINGS (settings), mechanism);
}

static void
mail_config_smtp_backend_dispose (GObject *object)
{
	EMailConfigSmtpBackend *smtp_backend = E_MAIL_CONFIG_SMTP_BACKEND (object);

	if (smtp_backend->priv->cancellable) {
		g_cancellable_cancel (smtp_backend->priv->cancellable);
		g_clear_object (&smtp_backend->priv->cancellable);
	}

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_mail_config_smtp_backend_parent_class)->dispose (object);
}

static void
e_mail_config_smtp_backend_class_init (EMailConfigSmtpBackendClass *class)
{
	EMailConfigServiceBackendClass *backend_class;
	GObjectClass *object_class;

	backend_class = E_MAIL_CONFIG_SERVICE_BACKEND_CLASS (class);
	backend_class->backend_name = "smtp";
	backend_class->insert_widgets = mail_config_smtp_backend_insert_widgets;
	backend_class->auto_configure = mail_config_smtp_backend_auto_configure;
	backend_class->check_complete = mail_config_smtp_backend_check_complete;
	backend_class->commit_changes = mail_config_smtp_backend_commit_changes;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = mail_config_smtp_backend_dispose;
}

static void
e_mail_config_smtp_backend_class_finalize (EMailConfigSmtpBackendClass *class)
{
}

static void
e_mail_config_smtp_backend_init (EMailConfigSmtpBackend *backend)
{
	backend->priv = e_mail_config_smtp_backend_get_instance_private (backend);
	backend->priv->cancellable = g_cancellable_new ();
}

void
e_mail_config_smtp_backend_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_mail_config_smtp_backend_register_type (type_module);
}

