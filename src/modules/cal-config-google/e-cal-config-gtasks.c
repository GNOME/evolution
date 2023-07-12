/*
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

#include <string.h>
#include <glib/gi18n-lib.h>

#include <libebackend/libebackend.h>

#include <e-util/e-util.h>

#include "module-cal-config-google.h"

typedef ESourceConfigBackend ECalConfigGTasks;
typedef ESourceConfigBackendClass ECalConfigGTasksClass;

typedef struct _Context Context;

struct _Context {
	GtkWidget *user_entry;
};

/* Forward Declarations */
GType e_cal_config_gtasks_get_type (void);

G_DEFINE_DYNAMIC_TYPE (
	ECalConfigGTasks,
	e_cal_config_gtasks,
	E_TYPE_SOURCE_CONFIG_BACKEND)

static void
cal_config_gtasks_context_free (Context *context)
{
	g_object_unref (context->user_entry);

	g_slice_free (Context, context);
}

static gboolean
cal_config_gtasks_allow_creation (ESourceConfigBackend *backend)
{
	ESourceConfig *config;
	ESourceTaskList *task_list;
	ESource *source;

	g_return_val_if_fail (E_IS_SOURCE_CONFIG_BACKEND (backend), FALSE);

	config = e_source_config_backend_get_config (backend);

	if (e_cal_source_config_get_source_type (E_CAL_SOURCE_CONFIG (config)) != E_CAL_CLIENT_SOURCE_TYPE_TASKS)
		return FALSE;

	source = e_source_config_get_original_source (config);
	if (!source && e_module_cal_config_google_is_supported (backend, NULL))
		return TRUE;

	if (!e_source_has_extension (source, E_SOURCE_EXTENSION_TASK_LIST))
		return FALSE;

	task_list = e_source_get_extension (source, E_SOURCE_EXTENSION_TASK_LIST);
	return g_strcmp0 (E_SOURCE_CONFIG_BACKEND_GET_CLASS (backend)->backend_name,
			  e_source_backend_get_backend_name (E_SOURCE_BACKEND (task_list))) == 0;
}

static void
cal_config_gtasks_insert_widgets (ESourceConfigBackend *backend,
                                  ESource *scratch_source)
{
	ESourceConfig *config;
	Context *context;

	config = e_source_config_backend_get_config (backend);
	context = g_slice_new0 (Context);

	g_object_set_data_full (
		G_OBJECT (backend), e_source_get_uid (scratch_source), context,
		(GDestroyNotify) cal_config_gtasks_context_free);

	context->user_entry = g_object_ref (e_source_config_add_user_entry (config, scratch_source));
	e_source_config_add_refresh_interval (config, scratch_source);
	e_source_config_add_refresh_on_metered_network (config, scratch_source);
}

static gboolean
cal_config_gtasks_check_complete (ESourceConfigBackend *backend,
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

	correct = user && *user;

	e_util_set_entry_issue_hint (context->user_entry, correct ?
		(camel_string_is_all_ascii (user) ? NULL : _("User name contains letters, which can prevent log in. Make sure the server accepts such written user name."))
		: _("User name cannot be empty"));

	return correct;
}

static void
cal_config_gtasks_commit_changes (ESourceConfigBackend *backend,
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

	e_source_authentication_set_host (extension, "www.google.com");

	if (!collection_source || (
	    !e_source_has_extension (collection_source, E_SOURCE_EXTENSION_GOA) &&
	    !e_source_has_extension (collection_source, E_SOURCE_EXTENSION_UOA))) {
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
e_cal_config_gtasks_class_init (ESourceConfigBackendClass *class)
{
	EExtensionClass *extension_class;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_CAL_SOURCE_CONFIG;

	class->parent_uid = "google-stub";
	class->backend_name = "gtasks";
	class->allow_creation = cal_config_gtasks_allow_creation;
	class->insert_widgets = cal_config_gtasks_insert_widgets;
	class->check_complete = cal_config_gtasks_check_complete;
	class->commit_changes = cal_config_gtasks_commit_changes;
}

static void
e_cal_config_gtasks_class_finalize (ESourceConfigBackendClass *class)
{
}

static void
e_cal_config_gtasks_init (ESourceConfigBackend *backend)
{
}

void
e_cal_config_gtasks_type_register (GTypeModule *type_module)
{
	e_cal_config_gtasks_register_type (type_module);
}
