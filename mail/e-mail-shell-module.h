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

#include <camel/camel-store.h>
#include <e-util/e-signature-list.h>
#include <libedataserver/e-account-list.h>

G_BEGIN_DECLS

CamelStore *	e_mail_shell_module_load_store_by_uri
						(const gchar *uri,
						 const gchar *name);
EAccountList *	mail_config_get_accounts	(void);
void		mail_config_save_accounts	(void);
ESignatureList *mail_config_get_signatures	(void);
gchar *		em_uri_from_camel		(const gchar *curi);

G_END_DECLS

#endif /* E_MAIL_SHELL_MODULE_H */
