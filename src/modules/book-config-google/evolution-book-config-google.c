/*
 * evolution-book-config-google.c
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

#include "e-google-book-chooser-button.h"

typedef ESourceConfigBackend EBookConfigGoogle;
typedef ESourceConfigBackendClass EBookConfigGoogleClass;

typedef struct _Context Context;

struct _Context {
	GtkWidget *user_entry; /* not referenced */
};

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

/* Forward Declarations */
GType e_book_config_google_get_type (void);

G_DEFINE_DYNAMIC_TYPE (
	EBookConfigGoogle,
	e_book_config_google,
	E_TYPE_SOURCE_CONFIG_BACKEND)

static void
book_config_google_context_free (Context *context)
{
	g_slice_free (Context, context);
}

static void
book_config_google_insert_widgets (ESourceConfigBackend *backend,
                                   ESource *scratch_source)
{
	ESourceConfig *config;
	GtkWidget *widget;
	Context *context;
	const gchar *uid;

	context = g_slice_new (Context);
	uid = e_source_get_uid (scratch_source);
	config = e_source_config_backend_get_config (backend);

	g_object_set_data_full (
		G_OBJECT (backend), uid, context,
		(GDestroyNotify) book_config_google_context_free);

	context->user_entry = e_source_config_add_user_entry (config, scratch_source);

	widget = e_google_book_chooser_button_new (scratch_source, config);
	e_source_config_insert_widget (config, scratch_source, _("Address Book:"), widget);
	gtk_widget_show (widget);

	e_source_config_add_refresh_interval (config, scratch_source);
	e_source_config_add_refresh_on_metered_network (config, scratch_source);
}

static gboolean
book_config_google_check_complete (ESourceConfigBackend *backend,
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

	correct = user != NULL && *user != '\0';

	e_util_set_entry_issue_hint (context->user_entry, correct ?
		(camel_string_is_all_ascii (user) ? NULL : _("User name contains letters, which can prevent log in. Make sure the server accepts such written user name."))
		: _("User name cannot be empty"));

	return correct;
}

static void
book_config_google_commit_changes (ESourceConfigBackend *backend,
                                   ESource *scratch_source)
{
	ESource *collection_source;
	ESourceConfig *config;
	ESourceBackend *addressbook_extension;
	ESourceWebdav *webdav_extension;
	ESourceAuthentication *extension;
	GUri *guri;
	const gchar *extension_name;
	const gchar *user;

	config = e_source_config_backend_get_config (backend);
	collection_source = e_source_config_get_collection_source (config);

	addressbook_extension = e_source_get_extension (scratch_source, E_SOURCE_EXTENSION_ADDRESS_BOOK);
	webdav_extension = e_source_get_extension (scratch_source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);

	extension_name = E_SOURCE_EXTENSION_AUTHENTICATION;
	extension = e_source_get_extension (scratch_source, extension_name);

	if (!collection_source ||
	    !e_source_authentication_get_is_external (extension)) {
		e_source_authentication_set_host (extension, "www.googleapis.com");
		e_source_authentication_set_method (extension, "Google");
	}

	/* The backend name is "carddav" even though the ESource is
	 * a child of the built-in "Google" source. */
	e_source_backend_set_backend_name (addressbook_extension, "carddav");

	user = e_source_authentication_get_user (extension);
	g_return_if_fail (user != NULL);

	/* A user name without a domain implies '<user>@gmail.com'. */
	if (strchr (user, '@') == NULL) {
		gchar *full_user;

		full_user = g_strconcat (user, "@gmail.com", NULL);
		e_source_authentication_set_user (extension, full_user);
		g_free (full_user);
	}

	guri = e_source_webdav_dup_uri (webdav_extension);

	if (!g_uri_get_path (guri) || !*g_uri_get_path (guri) || g_strcmp0 (g_uri_get_path (guri), "/") == 0) {
		e_google_book_chooser_button_construct_default_uri (&guri, e_source_authentication_get_user (extension));
	}

	/* Google's CalDAV interface requires a secure connection. */
	e_util_change_uri_component (&guri, SOUP_URI_SCHEME, "https");

	e_source_webdav_set_uri (webdav_extension, guri);

	g_uri_unref (guri);
}

static void
e_book_config_google_class_init (ESourceConfigBackendClass *class)
{
	EExtensionClass *extension_class;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_BOOK_SOURCE_CONFIG;

	class->parent_uid = "google-stub";
	class->backend_name = "google";
	class->insert_widgets = book_config_google_insert_widgets;
	class->check_complete = book_config_google_check_complete;
	class->commit_changes = book_config_google_commit_changes;
}

static void
e_book_config_google_class_finalize (ESourceConfigBackendClass *class)
{
}

static void
e_book_config_google_init (ESourceConfigBackend *backend)
{
}

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_google_book_chooser_button_type_register (type_module);
	e_book_config_google_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
