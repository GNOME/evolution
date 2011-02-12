/*
 * e-cal-config-meeting-store.c
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

#include "e-cal-config-meeting-store.h"

#include <shell/e-shell.h>
#include <e-util/e-extension.h>
#include <calendar/gui/e-meeting-store.h>

static gpointer parent_class;

static void
cal_config_meeting_store_constructed (GObject *object)
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
	G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
cal_config_meeting_store_class_init (EExtensionClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = cal_config_meeting_store_constructed;

	class->extensible_type = E_TYPE_MEETING_STORE;
}

void
e_cal_config_meeting_store_register_type (GTypeModule *type_module)
{
	static const GTypeInfo type_info = {
		sizeof (EExtensionClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) cal_config_meeting_store_class_init,
		(GClassFinalizeFunc) NULL,
		NULL,  /* class_data */
		sizeof (EExtension),
		0,     /* n_preallocs */
		(GInstanceInitFunc) NULL,
		NULL   /* value_table */
	};

	g_type_module_register_type (
		type_module, E_TYPE_EXTENSION,
		"ECalConfigMeetingStore", &type_info, 0);
}
