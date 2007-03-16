/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Michael Zucchi <NotZed@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef MAIL_SEND_RECV_H
#define MAIL_SEND_RECV_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include "mail-config.h"

/* send/receive all uri's */
GtkWidget *mail_send_receive(void);
/* receive a single uri */
void mail_receive_uri(const char *uri, int keep);
void mail_send (void);
/* setup auto receive stuff */
void mail_autoreceive_init(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! MAIL_SEND_RECV_H */
