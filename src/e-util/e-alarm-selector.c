/*
 * e-alarm-selector.c
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

#include "e-alarm-selector.h"

G_DEFINE_TYPE (
	EAlarmSelector,
	e_alarm_selector,
	E_TYPE_SOURCE_SELECTOR)

static gboolean
alarm_selector_get_source_selected (ESourceSelector *selector,
                                    ESource *source)
{
	ESourceAlarms *extension;
	const gchar *extension_name;

	/* Make sure this source is a calendar. */
	extension_name = e_source_selector_get_extension_name (selector);
	if (!e_source_has_extension (source, extension_name))
		return FALSE;

	extension_name = E_SOURCE_EXTENSION_ALARMS;
	extension = e_source_get_extension (source, extension_name);
	g_return_val_if_fail (E_IS_SOURCE_ALARMS (extension), FALSE);

	return e_source_alarms_get_include_me (extension);
}

static gboolean
alarm_selector_set_source_selected (ESourceSelector *selector,
                                    ESource *source,
                                    gboolean selected)
{
	ESourceAlarms *extension;
	const gchar *extension_name;

	/* Make sure this source is a calendar. */
	extension_name = e_source_selector_get_extension_name (selector);
	if (!e_source_has_extension (source, extension_name))
		return FALSE;

	extension_name = E_SOURCE_EXTENSION_ALARMS;
	extension = e_source_get_extension (source, extension_name);
	g_return_val_if_fail (E_IS_SOURCE_ALARMS (extension), FALSE);

	if (selected != e_source_alarms_get_include_me (extension)) {
		e_source_alarms_set_include_me (extension, selected);
		e_source_selector_queue_write (selector, source);

		return TRUE;
	}

	return FALSE;
}

static void
e_alarm_selector_class_init (EAlarmSelectorClass *class)
{
	ESourceSelectorClass *source_selector_class;

	source_selector_class = E_SOURCE_SELECTOR_CLASS (class);
	source_selector_class->get_source_selected =
					alarm_selector_get_source_selected;
	source_selector_class->set_source_selected =
					alarm_selector_set_source_selected;
}

static void
e_alarm_selector_init (EAlarmSelector *selector)
{
}

GtkWidget *
e_alarm_selector_new (ESourceRegistry *registry,
		      const gchar *extension_name)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);
	g_return_val_if_fail (extension_name != NULL, NULL);

	return g_object_new (
		E_TYPE_ALARM_SELECTOR,
		"extension-name", extension_name,
		"registry", registry, NULL);
}
