/*
 * e-settings-date-edit.c
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

#include "e-settings-date-edit.h"

#include <shell/e-shell.h>

#define E_SETTINGS_DATE_EDIT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SETTINGS_DATE_EDIT, ESettingsDateEditPrivate))

struct _ESettingsDateEditPrivate {
	gint placeholder;
};

G_DEFINE_DYNAMIC_TYPE (
	ESettingsDateEdit,
	e_settings_date_edit,
	E_TYPE_EXTENSION)

static void
settings_date_edit_constructed (GObject *object)
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
		shell_settings, "cal-show-week-numbers",
		extensible, "show-week-numbers",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		shell_settings, "cal-use-24-hour-format",
		extensible, "use-24-hour-format",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		shell_settings, "cal-week-start-day",
		extensible, "week-start-day",
		G_BINDING_SYNC_CREATE);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_settings_date_edit_parent_class)->
		constructed (object);
}

static void
e_settings_date_edit_class_init (ESettingsDateEditClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	g_type_class_add_private (class, sizeof (ESettingsDateEditPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = settings_date_edit_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_DATE_EDIT;
}

static void
e_settings_date_edit_class_finalize (ESettingsDateEditClass *class)
{
}

static void
e_settings_date_edit_init (ESettingsDateEdit *extension)
{
	extension->priv = E_SETTINGS_DATE_EDIT_GET_PRIVATE (extension);
}

void
e_settings_date_edit_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_settings_date_edit_register_type (type_module);
}

