/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include "e-mail-attachment-handler.h"

#include "e-mail-shell-backend.h"
#include "e-mail-shell-content.h"
#include "e-mail-shell-sidebar.h"
#include "e-mail-shell-view.h"

#include "em-account-prefs.h"

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	/* Register dynamically loaded types. */

	e_mail_attachment_handler_type_register (type_module);

	e_mail_shell_view_type_register (type_module);
	e_mail_shell_backend_type_register (type_module);
	e_mail_shell_content_type_register (type_module);
	e_mail_shell_sidebar_type_register (type_module);

	em_account_prefs_type_register (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}

