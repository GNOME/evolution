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
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__LIBEMAIL_ENGINE_H_INSIDE__) && !defined (LIBEMAIL_ENGINE_COMPILATION)
#error "Only <libemail-engine/libemail-engine.h> should be included directly."
#endif

#ifndef _MAIL_MT
#define _MAIL_MT

#include <camel/camel.h>

#include <e-util/e-util.h>

typedef struct _MailMsg MailMsg;
typedef struct _MailMsgInfo MailMsgInfo;

typedef gchar *	(*MailMsgDescFunc)		(MailMsg *msg);
typedef void	(*MailMsgExecFunc)		(MailMsg *msg,
						 GCancellable *cancellable,
						 GError **error);
typedef void	(*MailMsgDoneFunc)		(MailMsg *msg);
typedef void	(*MailMsgFreeFunc)		(MailMsg *msg);
typedef void	(*MailMsgDispatchFunc)		(gpointer msg);

typedef void    (*MailMsgCreateActivityFunc)	(GCancellable *cancellable);
typedef void    (*MailMsgSubmitActivityFunc)	(GCancellable *cancellable);
typedef void    (*MailMsgFreeActivityFunc)	(GCancellable *cancellable);
typedef void    (*MailMsgCompleteActivityFunc)	(GCancellable *cancellable);
typedef void    (*MailMsgCancelActivityFunc)	(GCancellable *cancellable);
typedef void    (*MailMsgAlertErrorFunc)	(GCancellable *cancellable,
						 const gchar *what,
						 const gchar *message);
typedef EAlertSink *
		(*MailMsgGetAlertSinkFunc)	(void);

struct _MailMsg {
	MailMsgInfo *info;
	volatile gint ref_count;
	guint seq;			/* seq number for synchronisation */
	gint priority;			/* priority (default = 0) */
	GCancellable *cancellable;
	GError *error;			/* up to the caller to use this */
};

struct _MailMsgInfo {
	gsize size;
	MailMsgDescFunc desc;
	MailMsgExecFunc exec;
	MailMsgDoneFunc done;
	MailMsgFreeFunc free;
};

/* Just till we move this out to EDS */
EAlertSink *	mail_msg_get_alert_sink (void);

/* setup ports */
void mail_msg_init (void);
void mail_msg_register_activities (MailMsgCreateActivityFunc,
				   MailMsgSubmitActivityFunc,
				   MailMsgFreeActivityFunc,
				   MailMsgCompleteActivityFunc,
				   MailMsgCancelActivityFunc,
				   MailMsgAlertErrorFunc,
				   MailMsgGetAlertSinkFunc);

gboolean mail_in_main_thread (void);

/* allocate a new message */
gpointer mail_msg_new (MailMsgInfo *info);
gpointer mail_msg_new_with_cancellable (MailMsgInfo *info,
					GCancellable *cancellable);
gpointer mail_msg_ref (gpointer msg);
void mail_msg_unref (gpointer msg);
void mail_msg_check_error (gpointer msg);
void mail_msg_cancel (guint msgid);
gboolean mail_msg_active (void);

/* dispatch a message */
void mail_msg_main_loop_push (gpointer msg);
void mail_msg_unordered_push (gpointer msg);
void mail_msg_fast_ordered_push (gpointer msg);
void mail_msg_slow_ordered_push (gpointer msg);

/* Call a function in the GUI thread, wait for it to return, type is
 * the marshaller to use.  FIXME This thing is horrible, please put
 * it out of its misery. */
typedef enum {
	MAIL_CALL_p_p,
	MAIL_CALL_p_pp,
	MAIL_CALL_p_ppp,
	MAIL_CALL_p_pppp,
	MAIL_CALL_p_ppppp,
	MAIL_CALL_p_ppippp
} mail_call_t;

gpointer mail_call_main (mail_call_t type, GCallback func, ...);

#endif /* _MAIL_MT */
