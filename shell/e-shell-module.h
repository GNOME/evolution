/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-module.h
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

#ifndef E_SHELL_MODULE_H
#define E_SHELL_MODULE_H

#include "e-shell-common.h"
#include "e-shell.h"

/* Standard GObject macros */
#define E_TYPE_SHELL_MODULE \
	(e_shell_module_get_type ())
#define E_SHELL_MODULE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SHELL_MODULE, EShellModule))
#define E_SHELL_MODULE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SHELL_MODULE, EShellModuleClass))
#define E_IS_SHELL_MODULE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SHELL_MODULE))
#define E_IS_SHELL_MODULE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SHELL_MODULE))
#define E_SHELL_MODULE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SHELL_MODULE, EShellModuleClass))

G_BEGIN_DECLS

typedef struct _EShellModule EShellModule;
typedef struct _EShellModuleInfo EShellModuleInfo;
typedef struct _EShellModuleClass EShellModuleClass;
typedef struct _EShellModulePrivate EShellModulePrivate;

struct _EShellModuleInfo {
	const gchar *name;
	const gchar *aliases;   /* colon-separated list */
	const gchar *schemes;   /* colon-separated list */
	gint sort_order;

	gboolean	(*is_busy)		(EShellModule *shell_module);
	gboolean	(*shutdown)		(EShellModule *shell_module);
};

struct _EShellModule {
	GTypeModule parent;
	EShellModulePrivate *priv;
};

struct _EShellModuleClass {
	GTypeModuleClass parent_class;
};

GType		e_shell_module_get_type		(void);
EShellModule *	e_shell_module_new		(EShell *shell,
						 const gchar *filename);
gint		e_shell_module_compare		(EShellModule *shell_module_a,
						 EShellModule *shell_module_b);
const gchar *	e_shell_module_get_filename	(EShellModule *shell_module);
EShell *	e_shell_module_get_shell	(EShellModule *shell_module);
gboolean	e_shell_module_is_busy		(EShellModule *shell_module);
gboolean	e_shell_module_shutdown		(EShellModule *shell_module);
void		e_shell_module_set_info		(EShellModule *shell_module,
						 const EShellModuleInfo *info);

G_END_DECLS

#endif /* E_SHELL_MODULE_H */
