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
 *		Michael Zucchi <NotZed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef MAIL_SEND_RECV_H
#define MAIL_SEND_RECV_H

#include <gtk/gtk.h>
#include "mail-config.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

struct _CamelSession;

/* send/receive all uri's */
GtkWidget *mail_send_receive(void);
/* receive a single uri */
void mail_receive_uri(const char *uri, int keep);
void mail_send (void);
/* setup auto receive stuff */
void mail_autoreceive_init (struct _CamelSession *session);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! MAIL_SEND_RECV_H */
