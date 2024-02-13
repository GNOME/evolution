/*
 * e-settings-weekday-chooser.c
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

#include <e-util/e-util.h>

#include "e-settings-weekday-chooser.h"

#include <calendar/gui/e-weekday-chooser.h>

struct _ESettingsWeekdayChooserPrivate {
	gint placeholder;
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (ESettingsWeekdayChooser, e_settings_weekday_chooser, E_TYPE_EXTENSION, 0,
	G_ADD_PRIVATE_DYNAMIC (ESettingsWeekdayChooser))

static void
settings_weekday_chooser_constructed (GObject *object)
{
	EExtension *extension;
	EExtensible *extensible;
	GSettings *settings;

	extension = E_EXTENSION (object);
	extensible = e_extension_get_extensible (extension);

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	g_settings_bind (
		settings, "week-start-day-name",
		extensible, "week-start-day",
		G_SETTINGS_BIND_GET);

	g_object_unref (settings);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_settings_weekday_chooser_parent_class)->constructed (object);
}

static void
e_settings_weekday_chooser_class_init (ESettingsWeekdayChooserClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = settings_weekday_chooser_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_WEEKDAY_CHOOSER;
}

static void
e_settings_weekday_chooser_class_finalize (ESettingsWeekdayChooserClass *class)
{
}

static void
e_settings_weekday_chooser_init (ESettingsWeekdayChooser *extension)
{
	extension->priv = e_settings_weekday_chooser_get_instance_private (extension);
}

void
e_settings_weekday_chooser_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_settings_weekday_chooser_register_type (type_module);
}

