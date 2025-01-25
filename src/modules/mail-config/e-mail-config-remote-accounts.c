/*
 * e-mail-config-remote-accounts.c
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
#include <mail/e-mail-config-service-page.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_CONFIG_REMOTE_BACKEND \
	(e_mail_config_remote_backend_get_type ())
#define E_MAIL_CONFIG_REMOTE_BACKEND(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CONFIG_REMOTE_BACKEND, EMailConfigRemoteBackend))
#define E_MAIL_CONFIG_REMOTE_BACKEND_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_CONFIG_REMOTE_BACKEND, EMailConfigRemoteBackendClass))
#define E_IS_MAIL_CONFIG_REMOTE_BACKEND(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CONFIG_REMOTE_BACKEND))
#define E_IS_MAIL_CONFIG_REMOTE_BACKEND_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_CONFIG_REMOTE_BACKEND))
#define E_MAIL_CONFIG_REMOTE_BACKEND_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_CONFIG_REMOTE_BACKEND, EMailConfigRemoteBackendClass))

typedef struct _EMailConfigRemoteBackend EMailConfigRemoteBackend;
typedef struct _EMailConfigRemoteBackendClass EMailConfigRemoteBackendClass;

typedef EMailConfigRemoteBackend EMailConfigPopBackend;
typedef EMailConfigRemoteBackendClass EMailConfigPopBackendClass;

typedef EMailConfigRemoteBackend EMailConfigNntpBackend;
typedef EMailConfigRemoteBackendClass EMailConfigNntpBackendClass;

typedef EMailConfigRemoteBackend EMailConfigImapxBackend;
typedef EMailConfigRemoteBackendClass EMailConfigImapxBackendClass;

struct _EMailConfigRemoteBackend {
	EMailConfigServiceBackend parent;

	GtkWidget *host_entry;		/* not referenced */
	GtkWidget *port_entry;		/* not referenced */
	GtkWidget *port_error_image;	/* not referenced */
	GtkWidget *user_entry;		/* not referenced */
	GtkWidget *forget_password_btn;	/* not referenced */
	GtkWidget *security_combo_box;	/* not referenced */
	GtkWidget *auth_check;		/* not referenced */

	GCancellable *cancellable;
};

struct _EMailConfigRemoteBackendClass {
	EMailConfigServiceBackendClass parent_class;
};

/* Forward Declarations */
void		e_mail_config_remote_accounts_register_types
						(GTypeModule *type_module);
GType		e_mail_config_remote_backend_get_type
						(void) G_GNUC_CONST;
GType		e_mail_config_pop_backend_get_type
						(void) G_GNUC_CONST;
GType		e_mail_config_nntp_backend_get_type
						(void) G_GNUC_CONST;
GType		e_mail_config_imapx_backend_get_type
						(void) G_GNUC_CONST;

G_DEFINE_DYNAMIC_TYPE_EXTENDED (
	EMailConfigRemoteBackend,
	e_mail_config_remote_backend,
	E_TYPE_MAIL_CONFIG_SERVICE_BACKEND,
	G_TYPE_FLAG_ABSTRACT,
	/* no custom code */)

G_DEFINE_DYNAMIC_TYPE (
	EMailConfigPopBackend,
	e_mail_config_pop_backend,
	E_TYPE_MAIL_CONFIG_REMOTE_BACKEND)

G_DEFINE_DYNAMIC_TYPE (
	EMailConfigNntpBackend,
	e_mail_config_nntp_backend,
	E_TYPE_MAIL_CONFIG_REMOTE_BACKEND)

G_DEFINE_DYNAMIC_TYPE (
	EMailConfigImapxBackend,
	e_mail_config_imapx_backend,
	E_TYPE_MAIL_CONFIG_REMOTE_BACKEND)

static void
source_lookup_password_done (GObject *source,
			     GAsyncResult *result,
			     gpointer user_data)
{
	gchar *password = NULL;

	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (result != NULL);

	if (e_source_lookup_password_finish (E_SOURCE (source), result, &password, NULL)) {
		if (password && *password && E_IS_MAIL_CONFIG_REMOTE_BACKEND (user_data)) {
			EMailConfigRemoteBackend *remote_backend = user_data;

			gtk_widget_show (remote_backend->forget_password_btn);
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
		if (E_IS_MAIL_CONFIG_REMOTE_BACKEND (user_data)) {
			EMailConfigRemoteBackend *remote_backend = user_data;

			gtk_widget_set_sensitive (remote_backend->forget_password_btn, FALSE);
		}
	} else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_warning ("%s: Failed to forget password: %s", G_STRFUNC, error ? error->message : "Unknown error");
	}

	g_clear_error (&error);
}

static void
remote_backend_forget_password_cb (GtkWidget *button,
				   EMailConfigRemoteBackend *remote_backend)
{
	ESource *source;

	g_return_if_fail (E_IS_MAIL_CONFIG_REMOTE_BACKEND (remote_backend));

	source = e_mail_config_service_backend_get_source (E_MAIL_CONFIG_SERVICE_BACKEND (remote_backend));

	e_source_delete_password (source, remote_backend->cancellable, source_delete_password_done, remote_backend);
}

static void
mail_config_remote_backend_insert_widgets (EMailConfigServiceBackend *backend,
                                           GtkBox *parent)
{
	EMailConfigRemoteBackend *remote_backend;
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
	const gchar *text;
	gchar *markup;

	remote_backend = E_MAIL_CONFIG_REMOTE_BACKEND (backend);

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
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_entry_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 0, 1, 1);
	remote_backend->host_entry = widget;  /* do not reference */
	gtk_widget_show (widget);

	widget = gtk_label_new_with_mnemonic (_("_Port:"));
	gtk_grid_attach (GTK_GRID (container), widget, 2, 0, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = e_port_entry_new ();
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 3, 0, 1, 1);
	remote_backend->port_entry = widget;  /* do not reference */
	gtk_widget_show (widget);

	widget = gtk_image_new_from_icon_name ("dialog-warning", GTK_ICON_SIZE_BUTTON);
	g_object_set (G_OBJECT (widget),
		"visible", FALSE,
		"has-tooltip", TRUE,
		"tooltip-text", _("Port number is not valid"),
		NULL);
	gtk_grid_attach (GTK_GRID (container), widget, 4, 0, 1, 1);
	remote_backend->port_error_image = widget;  /* do not reference */

	widget = gtk_label_new_with_mnemonic (_("User_name:"));
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 1, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_entry_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 1, 4, 1);
	remote_backend->user_entry = widget;  /* do not reference */
	gtk_widget_show (widget);

	widget = gtk_button_new_with_mnemonic (_("_Forget password"));
	gtk_widget_set_halign (widget, GTK_ALIGN_START);
	gtk_widget_set_hexpand (widget, FALSE);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 2, 4, 1);
	remote_backend->forget_password_btn = widget; /* do not reference */
	gtk_widget_hide (widget);

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
	remote_backend->security_combo_box = widget;  /* do not reference */
	gtk_widget_show (widget);

	provider = camel_provider_get (backend_name, NULL);
	if (provider != NULL && provider->port_entries != NULL)
		e_port_entry_set_camel_entries (
			E_PORT_ENTRY (remote_backend->port_entry),
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

	widget = e_mail_config_auth_check_new (backend);
	gtk_widget_set_margin_start (widget, 12);
	gtk_box_pack_start (GTK_BOX (parent), widget, FALSE, FALSE, 0);
	remote_backend->auth_check = widget;  /* do not reference */
	gtk_widget_show (widget);

	g_signal_connect (remote_backend->forget_password_btn, "clicked",
		G_CALLBACK (remote_backend_forget_password_cb), remote_backend);

	e_binding_bind_object_text_property (
		settings, "host",
		remote_backend->host_entry, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property_full (
		settings, "security-method",
		remote_backend->security_combo_box, "active-id",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		e_binding_transform_enum_value_to_nick,
		e_binding_transform_enum_nick_to_value,
		NULL, (GDestroyNotify) NULL);

	e_binding_bind_property (
		settings, "port",
		remote_backend->port_entry, "port",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		settings, "security-method",
		remote_backend->port_entry, "security-method",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_object_text_property (
		settings, "user",
		remote_backend->user_entry, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	/* Don't use G_BINDING_SYNC_CREATE here since the widget
	 * chooses its initial mechanism more intelligently than
	 * a simple property binding would. */
	e_binding_bind_property (
		settings, "auth-mechanism",
		remote_backend->auth_check, "active-mechanism",
		G_BINDING_BIDIRECTIONAL);

	if (!existing_source) {
		/* Default to TLS for new accounts */
		g_object_set (G_OBJECT (settings),
			"security-method", CAMEL_NETWORK_SECURITY_METHOD_SSL_ON_ALTERNATE_PORT,
			NULL);
	}

	g_clear_object (&existing_source);

	e_source_lookup_password (source, remote_backend->cancellable, source_lookup_password_done, remote_backend);
}

static gboolean
mail_config_remote_backend_check_complete (EMailConfigServiceBackend *backend)
{
	EMailConfigRemoteBackend *remote_backend;
	CamelSettings *settings;
	CamelNetworkSettings *network_settings;
	CamelProvider *provider;
	EPortEntry *port_entry;
	const gchar *host;
	const gchar *user;
	gboolean correct, complete = TRUE;

	remote_backend = E_MAIL_CONFIG_REMOTE_BACKEND (backend);

	settings = e_mail_config_service_backend_get_settings (backend);
	provider = e_mail_config_service_backend_get_provider (backend);

	g_return_val_if_fail (provider != NULL, FALSE);

	network_settings = CAMEL_NETWORK_SETTINGS (settings);
	host = camel_network_settings_get_host (network_settings);
	user = camel_network_settings_get_user (network_settings);

	correct = TRUE;

	if (CAMEL_PROVIDER_NEEDS (provider, CAMEL_URL_PART_HOST) &&
	    (host == NULL || *host == '\0'))
		correct = FALSE;

	complete = complete && correct;
	e_util_set_entry_issue_hint (remote_backend->host_entry, correct ? NULL : _("Server address cannot be empty"));

	correct = TRUE;

	port_entry = E_PORT_ENTRY (remote_backend->port_entry);
	if (CAMEL_PROVIDER_NEEDS (provider, CAMEL_URL_PART_PORT) &&
	    !e_port_entry_is_valid (port_entry))
		correct = FALSE;

	complete = complete && correct;
	gtk_widget_set_visible (remote_backend->port_error_image, !correct);

	correct = TRUE;

	if (CAMEL_PROVIDER_NEEDS (provider, CAMEL_URL_PART_USER) &&
	    (user == NULL || *user == '\0'))
		correct = FALSE;

	complete = complete && correct;
	e_util_set_entry_issue_hint (remote_backend->user_entry, correct ?
		(camel_string_is_all_ascii (user) ? NULL : _("User name contains letters, which can prevent log in. Make sure the server accepts such written user name."))
		: _("User name cannot be empty"));

	return complete;
}

static void
mail_config_remote_backend_commit_changes (EMailConfigServiceBackend *backend)
{
	/* All CamelNetworkSettings properties are already up-to-date,
	 * and these properties are bound to ESourceExtension properties,
	 * so nothing to do here. */
}

static void
mail_config_remote_backend_dispose (GObject *object)
{
	EMailConfigRemoteBackend *remote_backend = E_MAIL_CONFIG_REMOTE_BACKEND (object);

	if (remote_backend->cancellable) {
		g_cancellable_cancel (remote_backend->cancellable);
		g_clear_object (&remote_backend->cancellable);
	}

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_mail_config_remote_backend_parent_class)->dispose (object);
}

static void
e_mail_config_remote_backend_class_init (EMailConfigRemoteBackendClass *class)
{
	EMailConfigServiceBackendClass *backend_class;
	GObjectClass *object_class;

	backend_class = E_MAIL_CONFIG_SERVICE_BACKEND_CLASS (class);
	backend_class->insert_widgets = mail_config_remote_backend_insert_widgets;
	backend_class->check_complete = mail_config_remote_backend_check_complete;
	backend_class->commit_changes = mail_config_remote_backend_commit_changes;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = mail_config_remote_backend_dispose;
}

static void
e_mail_config_remote_backend_class_finalize (EMailConfigRemoteBackendClass *class)
{
}

static void
e_mail_config_remote_backend_init (EMailConfigRemoteBackend *backend)
{
	backend->cancellable = g_cancellable_new ();
}

static gboolean
mail_config_pop_backend_auto_configure (EMailConfigServiceBackend *backend,
					EConfigLookup *config_lookup,
					gint *out_priority,
					gboolean *out_is_complete)
{
	return e_mail_config_service_backend_auto_configure_for_kind (backend, config_lookup,
		E_CONFIG_LOOKUP_RESULT_MAIL_RECEIVE, NULL, NULL, out_priority, out_is_complete);
}

static void
e_mail_config_pop_backend_class_init (EMailConfigRemoteBackendClass *class)
{
	EMailConfigServiceBackendClass *backend_class;

	backend_class = E_MAIL_CONFIG_SERVICE_BACKEND_CLASS (class);
	backend_class->backend_name = "pop";
	backend_class->auto_configure = mail_config_pop_backend_auto_configure;
}

static void
e_mail_config_pop_backend_class_finalize (EMailConfigRemoteBackendClass *class)
{
}

static void
e_mail_config_pop_backend_init (EMailConfigRemoteBackend *backend)
{
}

static void
e_mail_config_nntp_backend_class_init (EMailConfigRemoteBackendClass *class)
{
	EMailConfigServiceBackendClass *backend_class;

	backend_class = E_MAIL_CONFIG_SERVICE_BACKEND_CLASS (class);
	backend_class->backend_name = "nntp";
}

static void
e_mail_config_nntp_backend_class_finalize (EMailConfigRemoteBackendClass *class)
{
}

static void
e_mail_config_nntp_backend_init (EMailConfigRemoteBackend *backend)
{
}

static gboolean
mail_config_imapx_backend_auto_configure (EMailConfigServiceBackend *backend,
					  EConfigLookup *config_lookup,
					  gint *out_priority,
					  gboolean *out_is_complete)
{
	return e_mail_config_service_backend_auto_configure_for_kind (backend, config_lookup,
		E_CONFIG_LOOKUP_RESULT_MAIL_RECEIVE, NULL, NULL, out_priority, out_is_complete);
}

static void
e_mail_config_imapx_backend_class_init (EMailConfigRemoteBackendClass *class)
{
	EMailConfigServiceBackendClass *backend_class;

	backend_class = E_MAIL_CONFIG_SERVICE_BACKEND_CLASS (class);
	backend_class->backend_name = "imapx";
	backend_class->auto_configure = mail_config_imapx_backend_auto_configure;
}

static void
e_mail_config_imapx_backend_class_finalize (EMailConfigRemoteBackendClass *class)
{
}

static void
e_mail_config_imapx_backend_init (EMailConfigRemoteBackend *backend)
{
}

void
e_mail_config_remote_accounts_register_types (GTypeModule *type_module)
{
	/* Abstract base type */
	e_mail_config_remote_backend_register_type (type_module);

	/* Concrete sub-types */
	e_mail_config_pop_backend_register_type (type_module);
	e_mail_config_nntp_backend_register_type (type_module);
	e_mail_config_imapx_backend_register_type (type_module);
}

