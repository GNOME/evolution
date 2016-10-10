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

	g_settings_bind_with_mapping (
		settings, "timezone",
		extensible, "timezone",
		G_SETTINGS_BIND_GET,
		settings_map_string_to_icaltimezone,
		NULL, /* one-way binding */
		NULL, (GDestroyNotify) NULL);

	g_object_unref (settings);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_settings_meeting_store_parent_class)->constructed (object);
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

