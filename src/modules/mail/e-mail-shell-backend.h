/*
 * e-mail-shell-backend.h
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

#ifndef E_MAIL_SHELL_BACKEND_H
#define E_MAIL_SHELL_BACKEND_H

#include <e-util/e-util.h>
#include <mail/e-mail-backend.h>

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

	/* Signals */
	GtkWidget *	(* new_account)		(EMailShellBackend *mail_shell_backend,
						 GtkWindow *parent);
	void		(* edit_account)	(EMailShellBackend *mail_shell_backend,
						 GtkWindow *parent,
						 ESource *mail_account);
};

GType		e_mail_shell_backend_get_type	(void);
void		e_mail_shell_backend_type_register
						(GTypeModule *type_module);

GtkWidget *	e_mail_shell_backend_new_account
						(EMailShellBackend *mail_shell_backend,
						 GtkWindow *parent);
void		e_mail_shell_backend_edit_account
						(EMailShellBackend *mail_shell_backend,
						 GtkWindow *parent,
						 ESource *mail_account);

/* XXX Find a better place for this function. */
GSList *	e_mail_labels_get_filter_options
						(void);
GSList *	e_mail_labels_get_filter_options_without_none
						(void);
GSList *	e_mail_labels_get_filter_options_with_all
						(void);
void		e_mail_labels_get_filter_code	(EFilterElement *element,
						 GString *out,
						 EFilterPart *part);
void		e_mail_labels_get_unset_filter_code
						(EFilterPart *part,
						 GString *out);
GSList *	e_mail_addressbook_get_filter_options
						(void);

G_END_DECLS

#endif /* E_MAIL_SHELL_BACKEND_H */
