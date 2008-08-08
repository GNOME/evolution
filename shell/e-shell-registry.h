/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-registry.h
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef E_SHELL_REGISTRY
#define E_SHELL_REGISTRY

#include "e-shell-common.h"
#include "e-shell-module.h"

G_BEGIN_DECLS

void		e_shell_registry_init			(void);
GList *		e_shell_registry_list_modules		(void);
GType *		e_shell_registry_get_view_types		(guint *n_types);
EShellModule *	e_shell_registry_get_module_by_name	(const gchar *name);
EShellModule *	e_shell_registry_get_module_by_schema	(const gchar *schema);

G_END_DECLS

#endif /* E_SHELL_REGISTRY */
