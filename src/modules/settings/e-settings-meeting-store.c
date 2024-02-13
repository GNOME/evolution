/*
 * e-settings-meeting-store.c
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

#include "e-settings-meeting-store.h"

#include <calendar/gui/e-meeting-store.h>

struct _ESettingsMeetingStorePrivate {
	gint placeholder;
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (ESettingsMeetingStore, e_settings_meeting_store, E_TYPE_EXTENSION, 0,
	G_ADD_PRIVATE_DYNAMIC (ESettingsMeetingStore))

static void
settings_meeting_store_constructed (GObject *object)
{
	EExtension *extension;
	EExtensible *extensible;
	GSettings *settings;

	extension = E_EXTENSION (object);
	extensible = e_extension_get_extensible (extension);

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	g_settings_bind (
		settings, "default-reminder-interval",
		extensible, "default-reminder-interval",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "default-reminder-units",
		extensible, "default-reminder-units",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "publish-template",
		extensible, "free-busy-template",
		G_SETTINGS_BIND_GET);

	/* Do not bind the timezone property, the EMeetingStore
	   user is supposed to update it as needed (like from the GUI,
	   instead of the settings). */

	g_object_unref (settings);

	settings = e_util_ref_settings ("org.gnome.evolution.addressbook");

	g_settings_bind (
		settings, "completion-show-address",
		extensible, "show-address",
		G_SETTINGS_BIND_GET);

	g_object_unref (settings);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_settings_meeting_store_parent_class)->constructed (object);
}

static void
e_settings_meeting_store_class_init (ESettingsMeetingStoreClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

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
	extension->priv = e_settings_meeting_store_get_instance_private (extension);
}

void
e_settings_meeting_store_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_settings_meeting_store_register_type (type_module);
}

