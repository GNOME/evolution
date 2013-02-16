/*
 * e-settings-client-cache.c
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

#include "e-settings-client-cache.h"

#include <e-util/e-util.h>
#include <shell/e-shell.h>

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

static void
settings_client_cache_client_created_cb (EClientCache *client_cache,
                                         EClient *client)
{
	EShell *shell;
	EShellSettings *shell_settings;

	shell = e_shell_get_default ();
	shell_settings = e_shell_get_shell_settings (shell);

	if (E_IS_CAL_CLIENT (client)) {
		g_object_bind_property (
			shell_settings, "cal-timezone",
			client, "default-timezone",
			G_BINDING_SYNC_CREATE);
	}
}

static void
settings_client_cache_constructed (GObject *object)
{
	EExtension *extension;
	EExtensible *extensible;

	extension = E_EXTENSION (object);
	extensible = e_extension_get_extensible (extension);

	g_signal_connect (
		extensible, "client-created",
		G_CALLBACK (settings_client_cache_client_created_cb),
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

