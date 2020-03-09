/*
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
 * Authors:
 *		Michael Zucchi <NotZed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef MAIL_SEND_RECV_H
#define MAIL_SEND_RECV_H

#include <gtk/gtk.h>
#include <libemail-engine/libemail-engine.h>

G_BEGIN_DECLS

/* send/receive all CamelServices */
GtkWidget *	mail_send_receive		(GtkWindow *parent,
						 EMailSession *session);

GtkWidget *	mail_receive			(GtkWindow *parent,
						 EMailSession *session);

/* receive a single CamelService */
void		mail_receive_service		(CamelService *service);

void		mail_send			(EMailSession *session);
void		mail_send_immediately		(EMailSession *session);

G_END_DECLS

#endif /* MAIL_SEND_RECV_H */
