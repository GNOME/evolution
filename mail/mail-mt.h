/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Authors: Michael Zucchi <notzed@helixcode.com>
 *
 * Copyright 2000, Helix Code, Inc. (http://www.helixcode.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef _MAIL_MT
#define _MAIL_MT

#include "camel/camel-exception.h"
#include "e-util/e-msgport.h"
#include "camel/camel-object.h"
#include "camel/camel-session.h"

typedef struct _mail_msg {
	EMsg msg;		/* parent type */
	struct _mail_msg_op *ops; /* operation functions */
	unsigned int seq;	/* seq number for synchronisation */
	CamelCancel *cancel;	/* a cancellation handle */
	CamelException ex;	/* an initialised camel exception, upto the caller to use this */
} mail_msg_t;

/* callback functions for thread message */
typedef struct _mail_msg_op {
	char *(*describe_msg)(struct _mail_msg *msg, int complete);

	void (*receive_msg)(struct _mail_msg *msg);	/* message received */
	void (*reply_msg)(struct _mail_msg *msg);	/* message replied */
	void (*destroy_msg)(struct _mail_msg *msg);	/* finalise message */
} mail_msg_op_t;

/* setup ports */
void mail_msg_init(void);

/* allocate a new message */
void *mail_msg_new(mail_msg_op_t *ops, EMsgPort *reply_port, size_t size);
void mail_msg_free(void *msg);
void mail_msg_check_error(void *msg);
void mail_msg_cancel(unsigned int msgid);
void mail_msg_wait(unsigned int msgid);

/* set the status-bar message */
/* start/end a new op */
void mail_status_start(const char *msg);
void mail_status_end(void);
/* set a status during an op */
void mail_statusf(const char *fmt, ...);
void mail_status(const char *msg);

/* request a string/password */
char *mail_get_password(char *prompt, gboolean secret);

/* forward a camel event (or other call) to the gui thread */
int mail_proxy_event(CamelObjectEventHookFunc func, CamelObject *o, void *event_data, void *data);

/* a message port that receives messages in the gui thread, used for sending port */
extern EMsgPort *mail_gui_port;
/* a message port that receives messages in the gui thread, used for the reply port */
extern EMsgPort *mail_gui_reply_port;

/* some globally available threads */
extern EThread *mail_thread_queued;	/* for operations that can (or should) be queued */
extern EThread *mail_thread_new;	/* for operations that should run in a new thread each time */

#endif /* ! _MAIL_MT */
