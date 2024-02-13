/*
 * e-settings-meeting-time-selector.c
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

#include "e-settings-meeting-time-selector.h"

#include <calendar/gui/e-meeting-time-sel.h>

struct _ESettingsMeetingTimeSelectorPrivate {
	gint placeholder;
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (ESettingsMeetingTimeSelector, e_settings_meeting_time_selector, E_TYPE_EXTENSION, 0,
	G_ADD_PRIVATE_DYNAMIC (ESettingsMeetingTimeSelector))

static void
settings_meeting_time_selector_constructed (GObject *object)
{
	EExtension *extension;
	EExtensible *extensible;
	GSettings *settings;

	extension = E_EXTENSION (object);
	extensible = e_extension_get_extensible (extension);

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	g_settings_bind (
		settings, "use-24hour-format",
		extensible, "use-24-hour-format",
		G_SETTINGS_BIND_GET);

	g_object_unref (settings);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_settings_meeting_time_selector_parent_class)->constructed (object);
}

static void
e_settings_meeting_time_selector_class_init (ESettingsMeetingTimeSelectorClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = settings_meeting_time_selector_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_MEETING_TIME_SELECTOR;
}

static void
e_settings_meeting_time_selector_class_finalize (ESettingsMeetingTimeSelectorClass *class)
{
}

static void
e_settings_meeting_time_selector_init (ESettingsMeetingTimeSelector *extension)
{
	extension->priv = e_settings_meeting_time_selector_get_instance_private (extension);
}

void
e_settings_meeting_time_selector_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_settings_meeting_time_selector_register_type (type_module);
}

