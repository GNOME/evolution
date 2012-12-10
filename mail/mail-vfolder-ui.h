/*
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
 * Authors:
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _MAIL_VFOLDER_UI_H
#define _MAIL_VFOLDER_UI_H

#include <camel/camel.h>

#include <libemail-engine/em-vfolder-rule.h>
#include <libemail-engine/mail-vfolder.h>

#include <mail/e-mail-backend.h>
#include <shell/e-shell-view.h>

void		vfolder_edit			(EMailBackend *backend,
						 GtkWindow *parent_window);
void		vfolder_edit_rule		(EMailSession *session,
						 const gchar *folder_uri,
						 EAlertSink *alert_sink);
EFilterRule *	vfolder_clone_rule		(EMailSession *session,
						 EFilterRule *in);
void		vfolder_gui_add_rule		(EMVFolderRule *rule);
void		vfolder_gui_add_from_message	(EMailSession *session,
						 CamelMimeMessage *message,
						 gint flags,
						 CamelFolder *folder);
void		vfolder_gui_add_from_address	(EMailSession *session,
						 CamelInternetAddress *addr,
						 gint flags,
						 CamelFolder *folder);
#endif
