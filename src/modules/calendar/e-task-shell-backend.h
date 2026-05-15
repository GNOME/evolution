/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-FileCopyrightText: (C) 2014 Red Hat, Inc. (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_TASK_SHELL_BACKEND_H
#define E_TASK_SHELL_BACKEND_H

#include "e-cal-base-shell-backend.h"

/* Standard GObject macros */
#define E_TYPE_TASK_SHELL_BACKEND \
	(e_task_shell_backend_get_type ())
#define E_TASK_SHELL_BACKEND(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TASK_SHELL_BACKEND, ETaskShellBackend))
#define E_TASK_SHELL_BACKEND_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TASK_SHELL_BACKEND, ETaskShellBackendClass))
#define E_IS_TASK_SHELL_BACKEND(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TASK_SHELL_BACKEND))
#define E_IS_TASK_SHELL_BACKEND_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TASK_SHELL_BACKEND))
#define E_TASK_SHELL_BACKEND_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TASK_SHELL_BACKEND, ETaskShellBackendClass))

G_BEGIN_DECLS

typedef struct _ETaskShellBackend ETaskShellBackend;
typedef struct _ETaskShellBackendClass ETaskShellBackendClass;
typedef struct _ETaskShellBackendPrivate ETaskShellBackendPrivate;

struct _ETaskShellBackend {
	ECalBaseShellBackend parent;
	ETaskShellBackendPrivate *priv;
};

struct _ETaskShellBackendClass {
	ECalBaseShellBackendClass parent_class;
};

GType		e_task_shell_backend_get_type		(void);
void		e_task_shell_backend_type_register	(GTypeModule *type_module);

G_END_DECLS

#endif /* E_TASK_SHELL_BACKEND_H */
