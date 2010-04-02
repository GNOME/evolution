/*
 * e-mail-store.h
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

#ifndef E_MAIL_STORE_H
#define E_MAIL_STORE_H

#include <glib.h>
#include <camel/camel.h>

G_BEGIN_DECLS

void		e_mail_store_init		(const gchar *data_dir);
void		e_mail_store_add		(CamelStore *store,
						 const gchar *display_name);
CamelStore *	e_mail_store_add_by_uri		(const gchar *uri,
						 const gchar *display_name);
void		e_mail_store_remove		(CamelStore *store);
void		e_mail_store_remove_by_uri	(const gchar *uri);
void		e_mail_store_foreach		(GHFunc func,
						 gpointer user_data);

G_END_DECLS

#endif /* E_MAIL_STORE_H */
