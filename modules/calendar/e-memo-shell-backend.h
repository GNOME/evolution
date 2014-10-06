/*
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 2014 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef E_MEMO_SHELL_BACKEND_H
#define E_MEMO_SHELL_BACKEND_H

#include "e-cal-base-shell-backend.h"

/* Standard GObject macros */
#define E_TYPE_MEMO_SHELL_BACKEND \
	(e_memo_shell_backend_get_type ())
#define E_MEMO_SHELL_BACKEND(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MEMO_SHELL_BACKEND, EMemoShellBackend))
#define E_MEMO_SHELL_BACKEND_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MEMO_SHELL_BACKEND, EMemoShellBackendClass))
#define E_IS_MEMO_SHELL_BACKEND(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MEMO_SHELL_BACKEND))
#define E_IS_MEMO_SHELL_BACKEND_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MEMO_SHELL_BACKEND))
#define E_MEMO_SHELL_BACKEND_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MEMO_SHELL_BACKEND, EMemoShellBackendClass))

G_BEGIN_DECLS

typedef struct _EMemoShellBackend EMemoShellBackend;
typedef struct _EMemoShellBackendClass EMemoShellBackendClass;
typedef struct _EMemoShellBackendPrivate EMemoShellBackendPrivate;

struct _EMemoShellBackend {
	ECalBaseShellBackend parent;
	EMemoShellBackendPrivate *priv;
};

struct _EMemoShellBackendClass {
	ECalBaseShellBackendClass parent_class;
};

GType		e_memo_shell_backend_get_type		(void);
void		e_memo_shell_backend_type_register	(GTypeModule *type_module);

G_END_DECLS

#endif /* E_MEMO_SHELL_BACKEND_H */
