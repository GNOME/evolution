/*
 * e-settings-client-cache.c
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

#include "e-settings-client-cache.h"

#include <e-util/e-util.h>

#define E_SETTINGS_CLIENT_CACHE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SETTINGS_CLIENT_CACHE, ESettingsClientCachePrivate))

struct _ESettingsClientCachePrivate {
	gint placeholder;
};

G_DEFINE_DYNAMIC_TYPE (
	ESettingsClientCache,
	e_settings_client_cache,
	E_TYPE_EXTENSION)

static gboolean
settings_map_string_to_icaltimezone (GValue *value,
                                     GVariant *variant,
                                     gpointer user_data)
{
	GSettings *settings;
	const gchar *location = NULL;
	icaltimezone *timezone = NULL;

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	if (g_settings_get_boolean (settings, "use-system-timezone"))
		timezone = e_cal_util_get_system_timezone ();
	else
		location = g_variant_get_string (variant, NULL);

	if (location != NULL && *location != '\0')
		timezone = icaltimezone_get_builtin_timezone (location);

	if (timezone == NULL)
		timezone = icaltimezone_get_utc_timezone ();

	g_value_set_pointer (value, timezone);

	g_object_unref (settings);

	return TRUE;
}

static void
settings_client_cache_client_connected_cb (EClientCache *client_cache,
					   EClient *client)
{
	if (E_IS_CAL_CLIENT (client)) {
		GSettings *settings;

		settings = e_util_ref_settings ("org.gnome.evolution.calendar");

		g_settings_bind_with_mapping (
			settings, "timezone",
			client, "default-timezone",
			G_SETTINGS_BIND_GET,
			settings_map_string_to_icaltimezone,
			NULL,  /* one-way binding */
			NULL, (GDestroyNotify) NULL);

		g_object_unref (settings);
	}
}

static void
settings_client_cache_constructed (GObject *object)
{
	EExtension *extension;
	EExtensible *extensible;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_settings_client_cache_parent_class)->constructed (object);

	extension = E_EXTENSION (object);
	extensible = e_extension_get_extensible (extension);

	g_signal_connect (
		extensible, "client-connected",
		G_CALLBACK (settings_client_cache_client_connected_cb),
		NULL);
}

static void
e_settings_client_cache_class_init (ESettingsClientCacheClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	g_type_class_add_private (class, sizeof (ESettingsClientCachePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = settings_client_cache_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_CLIENT_CACHE;
}

static void
e_settings_client_cache_class_finalize (ESettingsClientCacheClass *class)
{
}

static void
e_settings_client_cache_init (ESettingsClientCache *extension)
{
	extension->priv = E_SETTINGS_CLIENT_CACHE_GET_PRIVATE (extension);
}

void
e_settings_client_cache_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_settings_client_cache_register_type (type_module);
}

