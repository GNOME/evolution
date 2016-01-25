/*
 * e-settings-calendar-view.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-settings-calendar-view.h"

#include <calendar/gui/e-day-view.h>
#include <calendar/gui/e-week-view.h>

#define E_SETTINGS_CALENDAR_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SETTINGS_CALENDAR_VIEW, ESettingsCalendarViewPrivate))

struct _ESettingsCalendarViewPrivate {
	gint placeholder;
};

G_DEFINE_DYNAMIC_TYPE (
	ESettingsCalendarView,
	e_settings_calendar_view,
	E_TYPE_EXTENSION)

static void
settings_calendar_view_constructed (GObject *object)
{
	EExtension *extension;
	EExtensible *extensible;
	GSettings *settings;

	extension = E_EXTENSION (object);
	extensible = e_extension_get_extensible (extension);

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	g_settings_bind (
		settings, "time-divisions",
		extensible, "time-divisions",
		G_SETTINGS_BIND_DEFAULT);

	/*** EDayView ***/

	if (E_IS_DAY_VIEW (extensible)) {

		g_settings_bind (
			settings, "show-week-numbers",
			E_DAY_VIEW (extensible)->week_number_label, "visible",
			G_SETTINGS_BIND_GET);

		g_settings_bind (
			settings, "marcus-bains-line",
			extensible, "marcus-bains-show-line",
			G_SETTINGS_BIND_GET);

		g_settings_bind (
			settings, "marcus-bains-color-dayview",
			extensible, "marcus-bains-day-view-color",
			G_SETTINGS_BIND_GET);

		g_settings_bind (
			settings, "marcus-bains-color-timebar",
			extensible, "marcus-bains-time-bar-color",
			G_SETTINGS_BIND_GET);
	}

	/*** EWeekView ***/

	if (E_IS_WEEK_VIEW (extensible)) {

		g_settings_bind (
			settings, "compress-weekend",
			extensible, "compress-weekend",
			G_SETTINGS_BIND_GET);

		g_settings_bind (
			settings, "show-event-end",
			extensible, "show-event-end-times",
			G_SETTINGS_BIND_GET);

		g_settings_bind (
			settings, "show-icons-month-view",
			extensible, "show-icons-month-view",
			G_SETTINGS_BIND_GET);
	}

	g_object_unref (settings);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_settings_calendar_view_parent_class)->constructed (object);
}

static void
e_settings_calendar_view_class_init (ESettingsCalendarViewClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	g_type_class_add_private (class, sizeof (ESettingsCalendarViewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = settings_calendar_view_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_CALENDAR_VIEW;
}

static void
e_settings_calendar_view_class_finalize (ESettingsCalendarViewClass *class)
{
}

static void
e_settings_calendar_view_init (ESettingsCalendarView *extension)
{
	extension->priv = E_SETTINGS_CALENDAR_VIEW_GET_PRIVATE (extension);
}

void
e_settings_calendar_view_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_settings_calendar_view_register_type (type_module);
}

