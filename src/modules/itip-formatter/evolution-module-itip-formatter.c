/*
 * evolution-module-itip-formatter.c
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

#include "e-mail-formatter-itip.h"
#include "e-mail-parser-itip.h"
#include "e-mail-part-itip.h"

#include <gmodule.h>
#include <gio/gio.h>

void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);
const gchar * g_module_check_init (GModule *module);

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_mail_part_itip_type_register (type_module);
	e_mail_parser_itip_type_register (type_module);
	e_mail_formatter_itip_type_register (type_module);
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
