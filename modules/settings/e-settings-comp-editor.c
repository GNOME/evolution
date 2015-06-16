/*
 * e-settings-comp-editor.c
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

#include "e-settings-comp-editor.h"

#include <calendar/gui/dialogs/comp-editor.h>

#define E_SETTINGS_COMP_EDITOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SETTINGS_COMP_EDITOR, ESettingsCompEditorPrivate))

struct _ESettingsCompEditorPrivate {
	gint placeholder;
};

G_DEFINE_DYNAMIC_TYPE (
	ESettingsCompEditor,
	e_settings_comp_editor,
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
settings_comp_editor_constructed (GObject *object)
{
	EExtension *extension;
	EExtensible *extensible;
	GSettings *settings;

	extension = E_EXTENSION (object);
	extensible = e_extension_get_extensible (extension);

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	g_settings_bind_with_mapping (
		settings, "timezone",
		extensible, "timezone",
		G_SETTINGS_BIND_GET,
		settings_map_string_to_icaltimezone,
		NULL, /* one-way binding */
		NULL, (GDestroyNotify) NULL);

	g_settings_bind (
		settings, "use-24hour-format",
		extensible, "use-24-hour-format",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "week-start-day-name",
		extensible, "week-start-day",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-end-hour",
		extensible, "work-day-end-hour",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-end-minute",
		extensible, "work-day-end-minute",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-start-hour",
		extensible, "work-day-start-hour",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-start-minute",
		extensible, "work-day-start-minute",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-start-mon",
		extensible, "work-day-start-mon",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-end-mon",
		extensible, "work-day-end-mon",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-start-tue",
		extensible, "work-day-start-tue",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-end-tue",
		extensible, "work-day-end-tue",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-start-wed",
		extensible, "work-day-start-wed",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-end-wed",
		extensible, "work-day-end-wed",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-start-thu",
		extensible, "work-day-start-thu",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-end-thu",
		extensible, "work-day-end-thu",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-start-fri",
		extensible, "work-day-start-fri",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-end-fri",
		extensible, "work-day-end-fri",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-start-sat",
		extensible, "work-day-start-sat",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-end-sat",
		extensible, "work-day-end-sat",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-start-sun",
		extensible, "work-day-start-sun",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-end-sun",
		extensible, "work-day-end-sun",
		G_SETTINGS_BIND_GET);

	g_object_unref (settings);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_settings_comp_editor_parent_class)->constructed (object);
}

static void
e_settings_comp_editor_class_init (ESettingsCompEditorClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	g_type_class_add_private (class, sizeof (ESettingsCompEditorPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = settings_comp_editor_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = TYPE_COMP_EDITOR;
}

static void
e_settings_comp_editor_class_finalize (ESettingsCompEditorClass *class)
{
}

static void
e_settings_comp_editor_init (ESettingsCompEditor *extension)
{
	extension->priv = E_SETTINGS_COMP_EDITOR_GET_PRIVATE (extension);
}

void
e_settings_comp_editor_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_settings_comp_editor_register_type (type_module);
}

