/*
 * e-cal-config-google.c
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

#include <libebackend/libebackend.h>

#include <e-util/e-util.h>

#include "e-google-chooser-button.h"
#include "module-cal-config-google.h"

#define GOOGLE_CALENDAR_URI	"https://calendar.google.com/calendar/syncselect"

typedef ESourceConfigBackend ECalConfigGoogle;
typedef ESourceConfigBackendClass ECalConfigGoogleClass;

typedef struct _Context Context;

struct _Context {
	GtkWidget *google_button;
	GtkWidget *user_entry;
};

/* Forward Declarations */
GType e_cal_config_google_get_type (void);

G_DEFINE_DYNAMIC_TYPE (
	ECalConfigGoogle,
	e_cal_config_google,
	E_TYPE_SOURCE_CONFIG_BACKEND)

static void
cal_config_google_context_free (Context *context)
{
	g_object_unref (context->google_button);
	g_object_unref (context->user_entry);

	g_slice_free (Context, context);
}

static gboolean
cal_config_google_allow_creation (ESourceConfigBackend *backend)
{
	ESourceConfig *config;
	ECalClientSourceType source_type;

	config = e_source_config_backend_get_config (backend);

	source_type = e_cal_source_config_get_source_type (
		E_CAL_SOURCE_CONFIG (config));

	/* XXX Google's CalDAV interface doesn't support tasks. */
	return (source_type == E_CAL_CLIENT_SOURCE_TYPE_EVENTS);
}

static void
cal_config_google_insert_widgets (ESourceConfigBackend *backend,
                                  ESource *scratch_source)
{
	ESourceConfig *config;
	GtkWidget *widget;
	Context *context;
	const gchar *uid;
	gchar *markup;

	context = g_slice_new0 (Context);
	uid = e_source_get_uid (scratch_source);
	config = e_source_config_backend_get_config (backend);

	g_object_set_data_full (
		G_OBJECT (backend), uid, context,
		(GDestroyNotify) cal_config_google_context_free);

	context->user_entry = g_object_ref (e_source_config_add_user_entry (config, scratch_source));

	widget = e_google_chooser_button_new (scratch_source, config);
	e_source_config_insert_widget (config, scratch_source, _("Calendar:"), widget);
	context->google_button = g_object_ref (widget);
	gtk_widget_show (widget);

	markup = g_markup_printf_escaped ("<a href=\"%s\">%s</a>", GOOGLE_CALENDAR_URI, _("Enable Calendars to synchronize"));

	widget = gtk_label_new (markup);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	e_source_config_insert_widget (config, scratch_source, NULL, widget);
	gtk_widget_show (widget);

	g_free (markup);

	e_source_config_add_refresh_interval (config, scratch_source);
	e_source_config_add_refresh_on_metered_network (config, scratch_source);
}

static void
cal_config_google_commit_changes (ESourceConfigBackend *backend,
                                  ESource *scratch_source)
{
	ESourceBackend *calendar_extension;
	ESourceWebdav *webdav_extension;
	ESourceAuthentication *authentication_extension;
	gboolean can_google_auth;
	GUri *guri;

	/* We need to hard-code a few settings. */

	calendar_extension = e_source_get_extension (
		scratch_source, E_SOURCE_EXTENSION_CALENDAR);

	webdav_extension = e_source_get_extension (
		scratch_source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);

	authentication_extension = e_source_get_extension (
		scratch_source, E_SOURCE_EXTENSION_AUTHENTICATION);

	can_google_auth = e_module_cal_config_google_is_supported (backend, NULL) &&
			  g_strcmp0 (e_source_authentication_get_method (authentication_extension), "OAuth2") != 0;

	/* The backend name is actually "caldav" even though the
	 * ESource is a child of the built-in "Google" source. */
	e_source_backend_set_backend_name (calendar_extension, "caldav");

	guri = e_source_webdav_dup_uri (webdav_extension);

	if (can_google_auth || g_strcmp0 (e_source_authentication_get_method (authentication_extension), "Google") == 0) {
		/* Prefer 'Google', aka internal OAuth2, authentication method, if available */
		e_source_authentication_set_method (authentication_extension, "Google");

		/* See https://developers.google.com/google-apps/calendar/caldav/v2/guide */
		e_util_change_uri_component (&guri, SOUP_URI_HOST, "apidata.googleusercontent.com");
	} else {
		e_util_change_uri_component (&guri, SOUP_URI_HOST, "www.google.com");
	}

	if (!g_uri_get_path (guri) || !*g_uri_get_path (guri) || g_strcmp0 (g_uri_get_path (guri), "/") == 0) {
		e_google_chooser_button_construct_default_uri (&guri,
			e_source_authentication_get_user (authentication_extension));
	}

	/* Google's CalDAV interface requires a secure connection. */
	e_util_change_uri_component (&guri, SOUP_URI_SCHEME, "https");

	e_source_webdav_set_uri (webdav_extension, guri);

	g_uri_unref (guri);
}

static gboolean
cal_config_google_check_complete (ESourceConfigBackend *backend,
                                  ESource *scratch_source)
{
	ESourceAuthentication *extension;
	Context *context;
	gboolean correct;
	const gchar *extension_name;
	const gchar *user;

	context = g_object_get_data (G_OBJECT (backend), e_source_get_uid (scratch_source));
	g_return_val_if_fail (context != NULL, FALSE);

	extension_name = E_SOURCE_EXTENSION_AUTHENTICATION;
	extension = e_source_get_extension (scratch_source, extension_name);
	user = e_source_authentication_get_user (extension);

	correct = (user != NULL);

	e_util_set_entry_issue_hint (context->user_entry, correct ?
		(camel_string_is_all_ascii (user) ? NULL : _("User name contains letters, which can prevent log in. Make sure the server accepts such written user name."))
		: _("User name cannot be empty"));

	return correct;
}

static void
e_cal_config_google_class_init (ESourceConfigBackendClass *class)
{
	EExtensionClass *extension_class;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_CAL_SOURCE_CONFIG;

	class->parent_uid = "google-stub";
	class->backend_name = "google";
	class->allow_creation = cal_config_google_allow_creation;
	class->insert_widgets = cal_config_google_insert_widgets;
	class->check_complete = cal_config_google_check_complete;
	class->commit_changes = cal_config_google_commit_changes;
}

static void
e_cal_config_google_class_finalize (ESourceConfigBackendClass *class)
{
}

static void
e_cal_config_google_init (ESourceConfigBackend *backend)
{
}

void
e_cal_config_google_type_register (GTypeModule *type_module)
{
	e_cal_config_google_register_type (type_module);
}
