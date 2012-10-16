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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-mail-attachment-handler.h"

#include "e-mail-config-hook.h"
#include "e-mail-event-hook.h"

#include "e-mail-shell-backend.h"
#include "e-mail-shell-content.h"
#include "e-mail-shell-sidebar.h"
#include "e-mail-shell-view.h"

#include "e-mail-config-format-html.h"
#include "e-mail-config-reader.h"
#include "e-mail-config-web-view.h"

#include "em-account-prefs.h"

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

	e_mail_shell_backend_type_register (type_module);
	e_mail_shell_content_type_register (type_module);
	e_mail_shell_sidebar_type_register (type_module);
	e_mail_shell_view_register_type (type_module);

	e_mail_config_format_html_type_register (type_module);
	e_mail_config_reader_type_register (type_module);
	e_mail_config_web_view_type_register (type_module);

	em_account_prefs_type_register (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}

