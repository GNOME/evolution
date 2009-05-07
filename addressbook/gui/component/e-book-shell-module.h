/*
 * e-book-shell-module.h
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

#ifndef E_BOOK_SHELL_MODULE_H
#define E_BOOK_SHELL_MODULE_H

#include <shell/e-shell-module.h>
#include <libedataserver/e-source-list.h>

/* Standard GObject macros */
#define E_TYPE_BOOK_SHELL_MODULE \
	(e_book_shell_module_type)
#define E_BOOK_SHELL_MODULE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_BOOK_SHELL_MODULE, EBookShellModule))
#define E_BOOK_SHELL_MODULE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_BOOK_SHELL_MODULE, EBookShellModuleClass))
#define E_IS_BOOK_SHELL_MODULE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_BOOK_SHELL_MODULE))
#define E_IS_BOOK_SHELL_MODULE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_BOOK_SHELL_MODULE))
#define E_BOOK_SHELL_MODULE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_BOOK_SHELL_MODULE, EBookShellModuleClass))

G_BEGIN_DECLS

extern GType e_book_shell_module_type;

typedef struct _EBookShellModule EBookShellModule;
typedef struct _EBookShellModuleClass EBookShellModuleClass;
typedef struct _EBookShellModulePrivate EBookShellModulePrivate;

struct _EBookShellModule {
	EShellModule parent;
	EBookShellModulePrivate *priv;
};

struct _EBookShellModuleClass {
	EShellModuleClass parent_class;
};

GType		e_book_shell_module_get_type
					(GTypeModule *type_module);
ESourceList *	e_book_shell_module_get_source_list
					(EBookShellModule *book_shell_module);

G_END_DECLS

#endif /* E_BOOK_SHELL_MODULE_H */
