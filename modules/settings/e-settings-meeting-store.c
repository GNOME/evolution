/*
 * e-settings-meeting-store.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-settings-meeting-store.h"

#include <shell/e-shell.h>
#include <calendar/gui/e-meeting-store.h>

#define E_SETTINGS_MEETING_STORE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SETTINGS_MEETING_STORE, ESettingsMeetingStorePrivate))

struct _ESettingsMeetingStorePrivate {
	gint placeholder;
};

G_DEFINE_DYNAMIC_TYPE (
	ESettingsMeetingStore,
	e_settings_meeting_store,
	E_TYPE_EXTENSION)

static void
settings_meeting_store_constructed (GObject *object)
{
	EExtension *extension;
	EExtensible *extensible;
	EShellSettings *shell_settings;
	EShell *shell;

	extension = E_EXTENSION (object);
	extensible = e_extension_get_extensible (extension);

	shell = e_shell_get_default ();
	shell_settings = e_shell_get_shell_settings (shell);

	g_object_bind_property (
		shell_settings, "cal-default-reminder-interval",
		extensible, "default-reminder-interval",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		shell_settings, "cal-default-reminder-units",
		extensible, "default-reminder-units",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		shell_settings, "cal-free-busy-template",
		extensible, "free-busy-template",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		shell_settings, "cal-timezone",
		extensible, "timezone",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		shell_settings, "cal-week-start-day",
		extensible, "week-start-day",
		G_BINDING_SYNC_CREATE);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_settings_meeting_store_parent_class)->
		constructed (object);
}

static void
e_settings_meeting_store_class_init (ESettingsMeetingStoreClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	g_type_class_add_private (
		class, sizeof (ESettingsMeetingStorePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = settings_meeting_store_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_MEETING_STORE;
}

static void
e_settings_meeting_store_class_finalize (ESettingsMeetingStoreClass *class)
{
}

static void
e_settings_meeting_store_init (ESettingsMeetingStore *extension)
{
	extension->priv = E_SETTINGS_MEETING_STORE_GET_PRIVATE (extension);
}

void
e_settings_meeting_store_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_settings_meeting_store_register_type (type_module);
}

