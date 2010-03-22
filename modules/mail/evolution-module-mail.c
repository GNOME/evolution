/*
 * evolution-module-mail.c
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-mail-attachment-handler.h"

#include "e-mail-config-hook.h"
#include "e-mail-event-hook.h"
#include "e-mail-junk-hook.h"

#include "e-mail-shell-backend.h"
#include "e-mail-shell-content.h"
#include "e-mail-shell-sidebar.h"
#include "e-mail-shell-view.h"

#include "e-mail-config-format-html.h"
#include "e-mail-config-web-view.h"

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);
const gchar * g_module_check_init (GModule *module);

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	/* Register dynamically loaded types. */

	e_mail_attachment_handler_register_type (type_module);

	e_mail_config_hook_register_type (type_module);
	e_mail_event_hook_register_type (type_module);
	e_mail_junk_hook_register_type (type_module);

	e_mail_shell_backend_register_type (type_module);
	e_mail_shell_content_register_type (type_module);
	e_mail_shell_sidebar_register_type (type_module);
	e_mail_shell_view_register_type (type_module);

	e_mail_config_format_html_register_type (type_module);
	e_mail_config_web_view_register_type (type_module);
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
