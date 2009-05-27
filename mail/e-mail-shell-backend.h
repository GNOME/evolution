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

#include <shell/e-shell-backend.h>

#include <camel/camel-folder.h>
#include <camel/camel-store.h>
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
	EShellBackend parent;
	EMailShellBackendPrivate *priv;
};

struct _EMailShellBackendClass {
	EShellBackendClass parent_class;
};

typedef enum {
	E_MAIL_FOLDER_INBOX,
	E_MAIL_FOLDER_DRAFTS,
	E_MAIL_FOLDER_OUTBOX,
	E_MAIL_FOLDER_SENT,
	E_MAIL_FOLDER_TEMPLATES,
	E_MAIL_FOLDER_LOCAL_INBOX
} EMailFolderType;

struct _EMFolderTreeModel;

/* Globally available shell backend.
 *
 * XXX I don't like having this globally available but passing it around
 *     to all the various utilities that need to access the backend's data
 *     directory and local folders is too much of a pain for now. */
extern EMailShellBackend *global_mail_shell_backend;

GType		e_mail_shell_backend_get_type	(void);
void		e_mail_shell_backend_register_type
					(GTypeModule *type_module);
CamelFolder *	e_mail_shell_backend_get_folder
					(EMailShellBackend *mail_shell_backend,
					 EMailFolderType folder_type);
const gchar *	e_mail_shell_backend_get_folder_uri
					(EMailShellBackend *mail_shell_backend,
					 EMailFolderType folder_type);
struct _EMFolderTreeModel *
		e_mail_shell_backend_get_folder_tree_model
					(EMailShellBackend *mail_shell_backend);
void		e_mail_shell_backend_add_store
					(EMailShellBackend *mail_shell_backend,
					 CamelStore *store,
					 const gchar *name);
CamelStore *	e_mail_shell_backend_get_local_store
					(EMailShellBackend *mail_shell_backend);
CamelStore *	e_mail_shell_backend_load_store_by_uri
					(EMailShellBackend *mail_shell_backend,
					 const gchar *uri,
					 const gchar *name);
void		e_mail_shell_backend_remove_store
					(EMailShellBackend *mail_shell_backend,
					 CamelStore *store);
void		e_mail_shell_backend_remove_store_by_uri
					(EMailShellBackend *mail_shell_backend,
					 const gchar *uri);
void		e_mail_shell_backend_stores_foreach
					(EMailShellBackend *mail_shell_backend,
					 GHFunc func,
					 gpointer user_data);

/* XXX Find a better place for this function. */
GSList *	e_mail_labels_get_filter_options(void);

G_END_DECLS

#endif /* E_MAIL_SHELL_BACKEND_H */
