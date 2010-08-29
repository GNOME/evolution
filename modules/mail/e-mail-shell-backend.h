/*
 * e-mail-shell-backend.h
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

#ifndef E_MAIL_SHELL_BACKEND_H
#define E_MAIL_SHELL_BACKEND_H

#include <mail/e-mail-backend.h>

#include <e-util/e-signature-list.h>
#include <libedataserver/e-account-list.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_SHELL_BACKEND \
	(e_mail_shell_backend_get_type ())
#define E_MAIL_SHELL_BACKEND(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_SHELL_BACKEND, EMailShellBackend))
#define E_MAIL_SHELL_BACKEND_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_SHELL_BACKEND, EMailShellBackendClass))
#define E_IS_MAIL_SHELL_BACKEND(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_SHELL_BACKEND))
#define E_IS_MAIL_SHELL_BACKEND_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_SHELL_BACKEND))
#define E_MAIL_SHELL_BACKEND_GET_CLASS(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_SHELL_BACKEND, EMailShellBackendClass))

G_BEGIN_DECLS

typedef struct _EMailShellBackend EMailShellBackend;
typedef struct _EMailShellBackendClass EMailShellBackendClass;
typedef struct _EMailShellBackendPrivate EMailShellBackendPrivate;

struct _EMailShellBackend {
	EMailBackend parent;
	EMailShellBackendPrivate *priv;
};

struct _EMailShellBackendClass {
	EMailBackendClass parent_class;
};

GType		e_mail_shell_backend_get_type	(void);
void		e_mail_shell_backend_register_type
					(GTypeModule *type_module);

/* XXX Find a better place for this function. */
GSList *	e_mail_labels_get_filter_options (void);

G_END_DECLS

#endif /* E_MAIL_SHELL_BACKEND_H */
