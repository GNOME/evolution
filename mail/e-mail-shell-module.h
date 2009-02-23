/*
 * e-mail-shell-module.h
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

#ifndef E_MAIL_SHELL_MODULE_H
#define E_MAIL_SHELL_MODULE_H

#include <shell/e-shell-module.h>

#include <camel/camel-folder.h>
#include <camel/camel-store.h>
#include <e-util/e-signature-list.h>
#include <libedataserver/e-account-list.h>

G_BEGIN_DECLS

/* Globally available shell module. 
 *
 * XXX I don't like having this globally available but passing it around
 *     to all the various utilities that need to access the module's data
 *     directory and local folders is too much of a pain for now. */
extern EShellModule *mail_shell_module;

typedef enum {
	E_MAIL_FOLDER_INBOX,
	E_MAIL_FOLDER_DRAFTS,
	E_MAIL_FOLDER_OUTBOX,
	E_MAIL_FOLDER_SENT,
	E_MAIL_FOLDER_TEMPLATES,
	E_MAIL_FOLDER_LOCAL_INBOX
} EMailFolderType;

struct _EMFolderTreeModel;

CamelFolder *	e_mail_shell_module_get_folder	(EShellModule *shell_module,
						 EMailFolderType folder_type);
const gchar *	e_mail_shell_module_get_folder_uri
						(EShellModule *shell_module,
						 EMailFolderType folder_type);
struct _EMFolderTreeModel *
		e_mail_shell_module_get_folder_tree_model
						(EShellModule *shell_module);
void		e_mail_shell_module_add_store	(EShellModule *shell_module,
						 CamelStore *store,
						 const gchar *name);
CamelStore *	e_mail_shell_module_get_local_store
						(EShellModule *shell_module);
CamelStore *	e_mail_shell_module_load_store_by_uri
						(EShellModule *shell_module,
						 const gchar *uri,
						 const gchar *name);
void		e_mail_shell_module_remove_store(EShellModule *shell_module,
						 CamelStore *store);
void		e_mail_shell_module_remove_store_by_uri
						(EShellModule *shell_module,
						 const gchar *uri);
void		e_mail_shell_module_stores_foreach
						(EShellModule *shell_module,
						 GHFunc func,
						 gpointer user_data);

/* XXX Find a better place for this function. */
GSList *	e_mail_labels_get_filter_options(void);

G_END_DECLS

#endif /* E_MAIL_SHELL_MODULE_H */
