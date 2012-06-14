/*
 * evolution-cal-config-caldav.c
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

#include <config.h>
#include <glib/gi18n-lib.h>

#include <libebackend/libebackend.h>

#include <misc/e-cal-source-config.h>
#include <misc/e-interval-chooser.h>
#include <misc/e-source-config-backend.h>

#include "e-caldav-chooser.h"
#include "e-caldav-chooser-dialog.h"

#define HTTP_PORT 80
#define HTTPS_PORT 443

typedef ESourceConfigBackend ECalConfigCalDAV;
typedef ESourceConfigBackendClass ECalConfigCalDAVClass;

typedef struct _Context Context;

struct _Context {
	ESourceConfigBackend *backend;		/* not referenced */
	ESource *scratch_source;		/* not referenced */

	GtkWidget *server_entry;
	GtkWidget *path_entry;
	GtkWidget *email_entry;
	GtkWidget *find_button;
	GtkWidget *auto_schedule_toggle;

	GSocketConnectable *connectable;
};

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

/* Forward Declarations */
GType e_cal_config_caldav_get_type (void);

G_DEFINE_DYNAMIC_TYPE (
	ECalConfigCalDAV,
	e_cal_config_caldav,
	E_TYPE_SOURCE_CONFIG_BACKEND)

static Context *
cal_config_caldav_context_new (ESourceConfigBackend *backend,
                               ESource *scratch_source)
{
	Context *context;

	context = g_slice_new0 (Context);
	context->backend = backend;
	context->scratch_source = scratch_source;

	return context;
}

static void
cal_config_caldav_context_free (Context *context)
{
	g_object_unref (context->server_entry);
	g_object_unref (context->path_entry);
	g_object_unref (context->email_entry);
	g_object_unref (context->find_button);
	g_object_unref (context->auto_schedule_toggle);

	if (context->connectable != NULL)
		g_object_unref (context->connectable);

	g_slice_free (Context, context);
}

static gchar *
cal_config_caldav_get_server (ESource *scratch_source)
{
	ESourceAuthentication *authentication_extension;
	ESourceSecurity *security_extension;
	const gchar *host;
	gboolean secure;
	guint16 default_port;
	guint16 port;

	authentication_extension = e_source_get_extension (
		scratch_source, E_SOURCE_EXTENSION_AUTHENTICATION);
	host = e_source_authentication_get_host (authentication_extension);
	port = e_source_authentication_get_port (authentication_extension);

	security_extension = e_source_get_extension (
		scratch_source, E_SOURCE_EXTENSION_SECURITY);
	secure = e_source_security_get_secure (security_extension);
	default_port = secure ? HTTPS_PORT: HTTP_PORT;

	if (port == 0)
		port = default_port;

	if (host == NULL || *host == '\0')
		return NULL;

	if (port == default_port)
		return g_strdup (host);

	return g_strdup_printf ("%s:%u", host, port);
}

static void
cal_config_caldav_server_changed_cb (GtkEntry *entry,
                                     Context *context)
{
	ESourceAuthentication *authentication_extension;
	ESourceSecurity *security_extension;
	const gchar *host_and_port;
	const gchar *host;
	gboolean secure;
	guint16 default_port;
	guint16 port;

	if (context->connectable != NULL) {
		g_object_unref (context->connectable);
		context->connectable = NULL;
	}

	authentication_extension = e_source_get_extension (
		context->scratch_source, E_SOURCE_EXTENSION_AUTHENTICATION);

	security_extension = e_source_get_extension (
		context->scratch_source, E_SOURCE_EXTENSION_SECURITY);

	host_and_port = gtk_entry_get_text (entry);
	secure = e_source_security_get_secure (security_extension);
	default_port = secure ? HTTPS_PORT : HTTP_PORT;

	if (host_and_port != NULL && *host_and_port != '\0')
		context->connectable = g_network_address_parse (
			host_and_port, default_port, NULL);

	if (context->connectable != NULL) {
		GNetworkAddress *address;

		address = G_NETWORK_ADDRESS (context->connectable);
		host = g_network_address_get_hostname (address);
		port = g_network_address_get_port (address);
	} else {
		host = NULL;
		port = 0;
	}

	e_source_authentication_set_host (authentication_extension, host);
	e_source_authentication_set_port (authentication_extension, port);
}

static void
cal_config_caldav_run_dialog (GtkButton *button,
                              Context *context)
{
	ESourceConfig *config;
	ESourceRegistry *registry;
	ECalClientSourceType source_type;
	GtkWidget *dialog;
	GtkWidget *widget;
	gpointer parent;

	config = e_source_config_backend_get_config (context->backend);
	registry = e_source_config_get_registry (config);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (config));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	source_type = e_cal_source_config_get_source_type (
		E_CAL_SOURCE_CONFIG (config));

	widget = e_caldav_chooser_new (
		registry, context->scratch_source, source_type);

	dialog = e_caldav_chooser_dialog_new (
		E_CALDAV_CHOOSER (widget), parent);

	if (parent != NULL)
		g_object_bind_property (
			parent, "icon-name",
			dialog, "icon-name",
			G_BINDING_SYNC_CREATE);

	gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);
}

static void
cal_config_caldav_insert_widgets (ESourceConfigBackend *backend,
                                  ESource *scratch_source)
{
	ESourceConfig *config;
	ESourceExtension *extension;
	ECalClientSourceType source_type;
	GtkWidget *widget;
	Context *context;
	gchar *text;
	const gchar *extension_name;
	const gchar *label;
	const gchar *uid;

	context = cal_config_caldav_context_new (backend, scratch_source);
	uid = e_source_get_uid (scratch_source);
	config = e_source_config_backend_get_config (backend);

	g_object_set_data_full (
		G_OBJECT (backend), uid, context,
		(GDestroyNotify) cal_config_caldav_context_free);

	source_type = e_cal_source_config_get_source_type (
		E_CAL_SOURCE_CONFIG (config));

	e_cal_source_config_add_offline_toggle (
		E_CAL_SOURCE_CONFIG (config), scratch_source);

	widget = gtk_entry_new ();
	e_source_config_insert_widget (
		config, scratch_source, _("Server:"), widget);
	context->server_entry = g_object_ref (widget);
	gtk_widget_show (widget);

	/* Connect the signal before initializing the entry text. */
	g_signal_connect (
		widget, "changed",
		G_CALLBACK (cal_config_caldav_server_changed_cb), context);

	text = cal_config_caldav_get_server (scratch_source);
	if (text != NULL) {
		gtk_entry_set_text (GTK_ENTRY (context->server_entry), text);
		g_free (text);
	}

	e_source_config_add_secure_connection_for_webdav (
		config, scratch_source);

	e_source_config_add_user_entry (config, scratch_source);

	switch (source_type) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			label = _("Find Calendars");
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			label = _("Find Memo Lists");
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			label = _("Find Task Lists");
			break;
		default:
			g_return_if_reached ();
	}

	widget = gtk_button_new_with_label (label);
	e_source_config_insert_widget (
		config, scratch_source, NULL, widget);
	context->find_button = g_object_ref (widget);
	gtk_widget_show (widget);

	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (cal_config_caldav_run_dialog), context);

	widget = gtk_entry_new ();
	e_source_config_insert_widget (
		config, scratch_source, _("Path:"), widget);
	context->path_entry = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_entry_new ();
	e_source_config_insert_widget (
		config, scratch_source, _("Email:"), widget);
	context->email_entry = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_check_button_new_with_label (
		_("Server handles meeting invitations"));
	e_source_config_insert_widget (
		config, scratch_source, NULL, widget);
	context->auto_schedule_toggle = g_object_ref (widget);
	gtk_widget_show (widget);

	e_source_config_add_refresh_interval (config, scratch_source);

	extension_name = E_SOURCE_EXTENSION_WEBDAV_BACKEND;
	extension = e_source_get_extension (scratch_source, extension_name);

	g_object_bind_property (
		extension, "calendar-auto-schedule",
		context->auto_schedule_toggle, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		extension, "email-address",
		context->email_entry, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		extension, "resource-path",
		context->path_entry, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);
}

static gboolean
cal_config_caldav_check_complete (ESourceConfigBackend *backend,
                                  ESource *scratch_source)
{
	Context *context;
	const gchar *uid;
	gboolean complete;

	uid = e_source_get_uid (scratch_source);
	context = g_object_get_data (G_OBJECT (backend), uid);
	g_return_val_if_fail (context != NULL, FALSE);

	complete = (context->connectable != NULL);

	gtk_widget_set_sensitive (context->find_button, complete);

	return complete;
}

static void
e_cal_config_caldav_class_init (ESourceConfigBackendClass *class)
{
	EExtensionClass *extension_class;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_CAL_SOURCE_CONFIG;

	class->parent_uid = "caldav-stub";
	class->backend_name = "caldav";
	class->insert_widgets = cal_config_caldav_insert_widgets;
	class->check_complete = cal_config_caldav_check_complete;
}

static void
e_cal_config_caldav_class_finalize (ESourceConfigBackendClass *class)
{
}

static void
e_cal_config_caldav_init (ESourceConfigBackend *backend)
{
}

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_caldav_chooser_type_register (type_module);
	e_caldav_chooser_dialog_type_register (type_module);
	e_cal_config_caldav_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
