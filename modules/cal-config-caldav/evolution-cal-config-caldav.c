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
#include <libedataserverui/libedataserverui.h>

#include <e-util/e-util.h>

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
	ESourceWebdav *webdav_extension;
	ECredentialsPrompter *prompter;
	SoupURI *uri;
	gchar *base_url;
	GtkDialog *dialog;
	gpointer parent;
	gulong handler_id;
	guint supports_filter = 0;
	const gchar *title = NULL;

	config = e_source_config_backend_get_config (context->backend);
	registry = e_source_config_get_registry (config);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (config));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	switch (e_cal_source_config_get_source_type (E_CAL_SOURCE_CONFIG (config))) {
	case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
		supports_filter = E_WEBDAV_DISCOVER_SUPPORTS_EVENTS;
		title = _("Choose a Calendar");
		break;
	case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
		supports_filter = E_WEBDAV_DISCOVER_SUPPORTS_MEMOS;
		title = _("Choose a Memo List");
		break;
	case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
		supports_filter = E_WEBDAV_DISCOVER_SUPPORTS_TASKS;
		title = _("Choose a Task List");
		break;
	default:
		g_return_if_reached ();
	}

	webdav_extension = e_source_get_extension (context->scratch_source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);

	uri = e_source_webdav_dup_soup_uri (webdav_extension);

	prompter = e_credentials_prompter_new (registry);
	e_credentials_prompter_set_auto_prompt (prompter, FALSE);
	base_url = soup_uri_to_string (uri, FALSE);

	dialog = e_webdav_discover_dialog_new (parent, title, prompter, context->scratch_source, base_url, supports_filter);

	if (parent != NULL)
		e_binding_bind_property (
			parent, "icon-name",
			dialog, "icon-name",
			G_BINDING_SYNC_CREATE);

	handler_id = g_signal_connect (prompter, "get-dialog-parent",
		G_CALLBACK (caldav_config_get_dialog_parent_cb), dialog);

	e_webdav_discover_dialog_refresh (dialog);

	if (gtk_dialog_run (dialog) == GTK_RESPONSE_ACCEPT) {
		gchar *href = NULL, *display_name = NULL, *color = NULL, *email;
		guint supports = 0;
		GtkWidget *content;

		content = e_webdav_discover_dialog_get_content (dialog);

		if (e_webdav_discover_content_get_selected (content, 0, &href, &supports, &display_name, &color)) {
			soup_uri_free (uri);
			uri = soup_uri_new (href);

			if (uri) {
				e_source_set_display_name (context->scratch_source, display_name);

				e_source_webdav_set_display_name (webdav_extension, display_name);
				e_source_webdav_set_soup_uri (webdav_extension, uri);

				if (color && *color) {
					ESourceSelectable *selectable_extension;
					const gchar *extension_name;

					switch (e_cal_source_config_get_source_type (E_CAL_SOURCE_CONFIG (config))) {
						case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
							extension_name = E_SOURCE_EXTENSION_CALENDAR;
							break;
						case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
							extension_name = E_SOURCE_EXTENSION_MEMO_LIST;
							break;
						case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
							extension_name = E_SOURCE_EXTENSION_TASK_LIST;
							break;
						default:
							g_return_if_reached ();
					}

					selectable_extension = e_source_get_extension (context->scratch_source, extension_name);

					e_source_selectable_set_color (selectable_extension, color);
				}
			}

			g_free (href);
			g_free (display_name);
			g_free (color);

			href = NULL;
			display_name = NULL;
			color = NULL;
		}

		email = e_webdav_discover_content_get_user_address (content);
		if (email && *email)
			e_source_webdav_set_email_address (webdav_extension, email);
		g_free (email);
	}

	g_signal_handler_disconnect (prompter, handler_id);

	gtk_widget_destroy (GTK_WIDGET (dialog));

	g_object_unref (prompter);
	if (uri)
		soup_uri_free (uri);
	g_free (base_url);
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
	 * just add a subset and skip the rest. */
	if (collection_source != NULL) {
		e_source_config_add_secure_connection_for_webdav (config, scratch_source);
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

	e_binding_bind_property (
		extension, "calendar-auto-schedule",
		context->auto_schedule_toggle, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_object_text_property (
		extension, "email-address",
		context->email_entry, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property_full (
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
	e_cal_config_caldav_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
