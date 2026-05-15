/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include "e-book-config-hook.h"

#include "e-book-shell-view.h"
#include "e-book-shell-backend.h"
#include "e-book-shell-content.h"
#include "e-book-shell-sidebar.h"

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	/* Register dynamically loaded types. */

	e_book_config_hook_register_type (type_module);

	e_book_shell_view_type_register (type_module);
	e_book_shell_backend_type_register (type_module);
	e_book_shell_content_type_register (type_module);
	e_book_shell_sidebar_type_register (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
