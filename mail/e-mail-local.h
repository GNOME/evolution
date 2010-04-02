/*
 * e-mail-local.h
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

#ifndef E_MAIL_LOCAL_H
#define E_MAIL_LOCAL_H

#include <glib.h>
#include <camel/camel.h>

G_BEGIN_DECLS

/* XXX E_MAIL_FOLDER_TEMPLATES is a prime example of why templates
 *     should be a core feature: the mailer now has to know about
 *     this specific plugin, which defeats the purpose of plugins. */
typedef enum {
	E_MAIL_FOLDER_INBOX,
	E_MAIL_FOLDER_DRAFTS,
	E_MAIL_FOLDER_OUTBOX,
	E_MAIL_FOLDER_SENT,
	E_MAIL_FOLDER_TEMPLATES,
	E_MAIL_FOLDER_LOCAL_INBOX,
	E_MAIL_NUM_LOCAL_FOLDERS
} EMailLocalFolder;

void		e_mail_local_init		(const gchar *data_dir);
CamelFolder *	e_mail_local_get_folder		(EMailLocalFolder type);
const gchar *	e_mail_local_get_folder_uri	(EMailLocalFolder type);
CamelStore *	e_mail_local_get_store		(void);

G_END_DECLS

#endif /* E_MAIL_LOCAL_H */
