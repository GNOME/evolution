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

#include <config.h>
#include <glib/gi18n-lib.h>

#include <libebackend/libebackend.h>

#include <e-util/e-util.h>

typedef ESourceConfigBackend EBookConfigGoogle;
typedef ESourceConfigBackendClass EBookConfigGoogleClass;

typedef struct _Context Context;

struct _Context {
	gint placeholder;
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
	Context *context;
	const gchar *uid;

	context = g_slice_new (Context);
	uid = e_source_get_uid (scratch_source);
	config = e_source_config_backend_get_config (backend);

	g_object_set_data_full (
		G_OBJECT (backend), uid, context,
		(GDestroyNotify) book_config_google_context_free);

	e_source_config_add_user_entry (config, scratch_source);

	e_source_config_add_refresh_interval (config, scratch_source);
}

static gboolean
book_config_google_check_complete (ESourceConfigBackend *backend,
                                   ESource *scratch_source)
{
	ESourceAuthentication *extension;
	const gchar *extension_name;
	const gchar *user;

	extension_name = E_SOURCE_EXTENSION_AUTHENTICATION;
	extension = e_source_get_extension (scratch_source, extension_name);
	user = e_source_authentication_get_user (extension);

	return (user != NULL && *user != '\0');
}

static void
book_config_google_commit_changes (ESourceConfigBackend *backend,
                                   ESource *scratch_source)
{
	ESource *collection_source;
	ESourceConfig *config;
	ESourceAuthentication *extension;
	const gchar *extension_name;
	const gchar *user;

	config = e_source_config_backend_get_config (backend);
	collection_source = e_source_config_get_collection_source (config);

	extension_name = E_SOURCE_EXTENSION_AUTHENTICATION;
	extension = e_source_get_extension (scratch_source, extension_name);

	if (!collection_source || (
	    !e_source_has_extension (collection_source, E_SOURCE_EXTENSION_GOA) &&
	    !e_source_has_extension (collection_source, E_SOURCE_EXTENSION_UOA))) {
		e_source_authentication_set_host (extension, "www.google.com");
		e_source_authentication_set_method (extension, "Google");
	}

	user = e_source_authentication_get_user (extension);
	g_return_if_fail (user != NULL);

	/* A user name without a domain implies '<user>@gmail.com'. */
	if (strchr (user, '@') == NULL) {
		gchar *full_user;

		full_user = g_strconcat (user, "@gmail.com", NULL);
		e_source_authentication_set_user (extension, full_user);
		g_free (full_user);
	}
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
	e_book_config_google_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
