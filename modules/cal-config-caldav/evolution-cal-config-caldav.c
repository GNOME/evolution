/*
 * evolution-cal-config-caldav.c
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

#include <config.h>
#include <glib/gi18n-lib.h>

#include <libebackend/libebackend.h>

#include <e-util/e-util.h>

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

	GtkWidget *url_entry;
	GtkWidget *email_entry;
	GtkWidget *find_button;
	GtkWidget *auto_schedule_toggle;
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
	g_object_unref (context->url_entry);
	g_object_unref (context->email_entry);
	g_object_unref (context->find_button);
	g_object_unref (context->auto_schedule_toggle);

	g_slice_free (Context, context);
}

static GtkWindow *
caldav_config_get_dialog_parent_cb (ECredentialsPrompter *prompter,
				    GtkWindow *dialog)
{
	return dialog;
}

static void
cal_config_caldav_run_dialog (GtkButton *button,
                              Context *context)
{
	ESourceConfig *config;
	ESourceRegistry *registry;
	ECalClientSourceType source_type;
	ECredentialsPrompter *prompter;
	GtkWidget *dialog;
	GtkWidget *widget;
	gpointer parent;
	gulong handler_id;

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

	prompter = e_caldav_chooser_get_prompter (E_CALDAV_CHOOSER (widget));

	handler_id = g_signal_connect (prompter, "get-dialog-parent",
		G_CALLBACK (caldav_config_get_dialog_parent_cb), dialog);

	gtk_dialog_run (GTK_DIALOG (dialog));

	g_signal_handler_disconnect (prompter, handler_id);

	gtk_widget_destroy (dialog);
}

static gboolean
cal_config_caldav_uri_to_text (GBinding *binding,
                               const GValue *source_value,
                               GValue *target_value,
                               gpointer user_data)
{
	SoupURI *soup_uri;
	gchar *text;

	soup_uri = g_value_get_boxed (source_value);
	soup_uri_set_user (soup_uri, NULL);

	text = soup_uri_to_string (soup_uri, FALSE);
	g_value_take_string (target_value, text);

	return TRUE;
}

static gboolean
cal_config_caldav_text_to_uri (GBinding *binding,
                               const GValue *source_value,
                               GValue *target_value,
                               gpointer user_data)
{
	ESource *source;
	SoupURI *soup_uri;
	ESourceAuthentication *extension;
	const gchar *extension_name;
	const gchar *text;
	const gchar *user;

	text = g_value_get_string (source_value);
	soup_uri = soup_uri_new (text);

	if (soup_uri == NULL)
		return FALSE;

	source = E_SOURCE (user_data);
	extension_name = E_SOURCE_EXTENSION_AUTHENTICATION;
	extension = e_source_get_extension (source, extension_name);
	user = e_source_authentication_get_user (extension);

	soup_uri_set_user (soup_uri, user);

	g_value_take_boxed (target_value, soup_uri);

	return TRUE;
}

static void
cal_config_caldav_insert_widgets (ESourceConfigBackend *backend,
                                  ESource *scratch_source)
{
	ESourceConfig *config;
	ESource *collection_source;
	ESourceExtension *extension;
	ECalClientSourceType source_type;
	GtkWidget *widget;
	Context *context;
	const gchar *extension_name;
	const gchar *label;
	const gchar *uid;

	config = e_source_config_backend_get_config (backend);
	collection_source = e_source_config_get_collection_source (config);

	e_cal_source_config_add_offline_toggle (
		E_CAL_SOURCE_CONFIG (config), scratch_source);

	/* If this data source is a collection member,
	 * just add a refresh interval and skip the rest. */
	if (collection_source != NULL) {
		e_source_config_add_refresh_interval (config, scratch_source);
		return;
	}

	uid = e_source_get_uid (scratch_source);
	context = cal_config_caldav_context_new (backend, scratch_source);

	g_object_set_data_full (
		G_OBJECT (backend), uid, context,
		(GDestroyNotify) cal_config_caldav_context_free);

	widget = gtk_entry_new ();
	e_source_config_insert_widget (
		config, scratch_source, _("URL:"), widget);
	context->url_entry = g_object_ref (widget);
	gtk_widget_show (widget);

	e_source_config_add_secure_connection_for_webdav (
		config, scratch_source);

	e_source_config_add_user_entry (config, scratch_source);

	source_type = e_cal_source_config_get_source_type (
		E_CAL_SOURCE_CONFIG (config));

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

	e_binding_bind_object_text_property (
		extension, "email-address",
		context->email_entry, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	g_object_bind_property_full (
		extension, "soup-uri",
		context->url_entry, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		cal_config_caldav_uri_to_text,
		cal_config_caldav_text_to_uri,
		g_object_ref (scratch_source),
		(GDestroyNotify) g_object_unref);
}

static gboolean
cal_config_caldav_check_complete (ESourceConfigBackend *backend,
                                  ESource *scratch_source)
{
	ESourceConfig *config;
	ESource *collection_source;
	Context *context;
	const gchar *uid;
	const gchar *uri_string;
	SoupURI *soup_uri;
	gboolean complete;

	config = e_source_config_backend_get_config (backend);
	collection_source = e_source_config_get_collection_source (config);

	if (collection_source != NULL)
		return TRUE;

	uid = e_source_get_uid (scratch_source);
	context = g_object_get_data (G_OBJECT (backend), uid);
	g_return_val_if_fail (context != NULL, FALSE);

	uri_string = gtk_entry_get_text (GTK_ENTRY (context->url_entry));
	soup_uri = soup_uri_new (uri_string);

	if (!soup_uri) {
		complete = FALSE;
	} else {
		if (g_strcmp0 (soup_uri_get_scheme (soup_uri), "caldav") == 0)
			soup_uri_set_scheme (soup_uri, SOUP_URI_SCHEME_HTTP);

		complete = SOUP_URI_VALID_FOR_HTTP (soup_uri);
	}

	if (soup_uri != NULL)
		soup_uri_free (soup_uri);

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
