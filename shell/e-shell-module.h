/*
 * e-shell-module.h
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

/**
 * SECTION: e-shell-module
 * @short_description: dynamically loaded capabilities
 * @include: shell/e-shell-module.h
 **/

#ifndef E_SHELL_MODULE_H
#define E_SHELL_MODULE_H

#include <shell/e-shell-common.h>
#include <widgets/misc/e-activity.h>

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

/* Avoid including <e-shell.h> */
struct _EShell;

typedef struct _EShellModule EShellModule;
typedef struct _EShellModuleInfo EShellModuleInfo;
typedef struct _EShellModuleClass EShellModuleClass;
typedef struct _EShellModulePrivate EShellModulePrivate;

/**
 * EShellModuleInfo:
 * @name:	The name of the module.  Also becomes the name of
 * 		the corresponding #EShellView subclass that the
 * 		module will register.
 * @aliases:	Colon-separated list of aliases that can be used
 * 		when referring to a module by name.
 * @schemes:	Colon-separated list of URI schemes.  The #EShell
 * 		will forward command-line URIs to the appropriate
 * 		module based on this list.
 * @searches:	Base name of the XML file containing predefined
 * 		search rules for this module.  These show up as
 * 		options in the search entry drop-down.
 * @sort_order:	Used to determine the order of modules listed in
 * 		the main menu and in the switcher.  See
 * 		e_shell_module_compare().
 * @is_busy:	Callback for querying whether the module has
 * 		operations in progress that cannot be cancelled
 * 		or finished immediately.  Returning %TRUE prevents
 * 		the application from shutting down.
 * @shutdown:	Callback for notifying the module to begin
 * 		shutting down.  Returning %FALSE indicates there
 * 		are still unfinished operations and the #EShell
 * 		should check back shortly.
 **/
struct _EShellModuleInfo {
	const gchar *name;
	const gchar *aliases;
	const gchar *schemes;
	const gchar *searches;
	gint sort_order;

	gboolean	(*is_busy)		(EShellModule *shell_module);
	gboolean	(*shutdown)		(EShellModule *shell_module);
};

/**
 * EShellModule:
 *
 * Contains only private data that should be read and manipulated using the
 * functions below.
 **/
struct _EShellModule {
	GTypeModule parent;
	EShellModulePrivate *priv;
};

struct _EShellModuleClass {
	GTypeModuleClass parent_class;
};

GType		e_shell_module_get_type		(void);
EShellModule *	e_shell_module_new		(struct _EShell *shell,
						 const gchar *filename);
gint		e_shell_module_compare		(EShellModule *shell_module_a,
						 EShellModule *shell_module_b);
const gchar *	e_shell_module_get_config_dir	(EShellModule *shell_module);
const gchar *	e_shell_module_get_data_dir	(EShellModule *shell_module);
const gchar *	e_shell_module_get_filename	(EShellModule *shell_module);
const gchar *	e_shell_module_get_searches	(EShellModule *shell_module);
struct _EShell *e_shell_module_get_shell	(EShellModule *shell_module);
void		e_shell_module_add_activity	(EShellModule *shell_module,
						 EActivity *activity);
gboolean	e_shell_module_is_busy		(EShellModule *shell_module);
gboolean	e_shell_module_shutdown		(EShellModule *shell_module);
void		e_shell_module_set_info		(EShellModule *shell_module,
						 const EShellModuleInfo *info);

G_END_DECLS

#endif /* E_SHELL_MODULE_H */
