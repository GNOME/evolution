/*
 * evolution-module-addressbook.c
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

#include "e-book-config-hook.h"

#include "e-book-shell-backend.h"
#include "e-book-shell-content.h"
#include "e-book-shell-sidebar.h"
#include "e-book-shell-view.h"

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	/* Register dynamically loaded types. */

	e_book_config_hook_register_type (type_module);

	e_book_shell_backend_register_type (type_module);
	e_book_shell_content_register_type (type_module);
	e_book_shell_sidebar_register_type (type_module);
	e_book_shell_view_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
