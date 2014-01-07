/*
 * e-book-shell-backend.h
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

#ifndef E_BOOK_SHELL_BACKEND_H
#define E_BOOK_SHELL_BACKEND_H

#include <shell/e-shell-backend.h>

/* Standard GObject macros */
#define E_TYPE_BOOK_SHELL_BACKEND \
	(e_book_shell_backend_get_type ())
#define E_BOOK_SHELL_BACKEND(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_BOOK_SHELL_BACKEND, EBookShellBackend))
#define E_BOOK_SHELL_BACKEND_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_BOOK_SHELL_BACKEND, EBookShellBackendClass))
#define E_IS_BOOK_SHELL_BACKEND(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_BOOK_SHELL_BACKEND))
#define E_IS_BOOK_SHELL_BACKEND_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_BOOK_SHELL_BACKEND))
#define E_BOOK_SHELL_BACKEND_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_BOOK_SHELL_BACKEND, EBookShellBackendClass))

G_BEGIN_DECLS

typedef struct _EBookShellBackend EBookShellBackend;
typedef struct _EBookShellBackendClass EBookShellBackendClass;
typedef struct _EBookShellBackendPrivate EBookShellBackendPrivate;

struct _EBookShellBackend {
	EShellBackend parent;
	EBookShellBackendPrivate *priv;
};

struct _EBookShellBackendClass {
	EShellBackendClass parent_class;
};

GType		e_book_shell_backend_get_type	(void);
void		e_book_shell_backend_type_register
					(GTypeModule *type_module);

G_END_DECLS

#endif /* E_BOOK_SHELL_BACKEND_H */
