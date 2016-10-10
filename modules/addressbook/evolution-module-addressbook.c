/*
 * evolution-module-addressbook.c
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
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
