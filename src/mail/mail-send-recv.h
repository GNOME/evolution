/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Michael Zucchi <NotZed@ximian.com>
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
