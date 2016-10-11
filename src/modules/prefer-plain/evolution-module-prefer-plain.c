/*
 * evolution-module-prefer-plain.c
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

#include "e-mail-parser-prefer-plain.h"
#include "e-mail-display-popup-prefer-plain.h"

#include <gmodule.h>
#include <gio/gio.h>

#include <e-util/e-util.h>

void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);
const gchar * g_module_check_init (GModule *module);

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	GSettings *settings;
	gchar **disabled_plugins;
	gint i = 0;

	settings = e_util_ref_settings ("org.gnome.evolution");
	disabled_plugins = g_settings_get_strv (settings, "disabled-eplugins");

	for (i = 0; disabled_plugins && disabled_plugins[i] != NULL; i++) {

		if (g_strcmp0 (
			disabled_plugins[i],
			"org.gnome.evolution.plugin.preferPlain") == 0) {

			g_strfreev (disabled_plugins);
			g_object_unref (settings);
			return;
		}

	}

	e_mail_parser_prefer_plain_type_register (type_module);
	e_mail_display_popup_prefer_plain_type_register (type_module);

	g_strfreev (disabled_plugins);
	g_object_unref (settings);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}

G_MODULE_EXPORT const gchar *
g_module_check_init (GModule *module)
{
	/* FIXME Until mail is split into a module library and a
	 *       reusable shared library, prevent the module from
	 *       being unloaded.  Unloading the module resets all
	 *       static variables, which screws up foo_get_type()
	 *       functions among other things. */
	g_module_make_resident (module);

	return NULL;
}
