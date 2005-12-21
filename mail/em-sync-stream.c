/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *	     Michael Zucchi <notzed@ximian.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdio.h>
#include <camel/camel-stream.h>
#include <camel/camel-object.h>
#include <gtk/gtkmain.h>
#include "em-sync-stream.h"

#include "mail-mt.h"

#define LOG_STREAM

#define d(x) 

#define EMSS_CLASS(x) ((EMSyncStreamClass *)(((CamelObject *)(x))->klass))

struct _EMSyncStreamPrivate {
	/* FIXME: use a single data port/gui channel for all instances */
	/* TODO: possibly just use one of the mail-mt ports ... */
	struct _EMsgPort *data_port, *reply_port;
	struct _GIOChannel *gui_channel;
	guint gui_watch;

	char *buf_data;
	int buf_used;
	int buf_size;

#ifdef LOG_STREAM
	FILE *logfd;
#endif
};

#ifdef LOG_STREAM
int dolog;
#endif

/* Should probably expose messages to outside world ... so subclasses can extend */
enum _write_msg_t {
	EMSS_WRITE,
	EMSS_FLUSH,
	EMSS_CLOSE,
};

struct _write_msg {
	EMsg msg;

	enum _write_msg_t op;

	const char *data;
	size_t n;
};

static void em_sync_stream_class_init (EMSyncStreamClass *klass);
static void em_sync_stream_init (CamelObject *object);
static void em_sync_stream_finalize (CamelObject *object);

static ssize_t stream_write(CamelStream *stream, const char *buffer, size_t n);
static int stream_close(CamelStream *stream);
static int stream_flush(CamelStream *stream);

static CamelStreamClass *parent_class = NULL;

CamelType
em_sync_stream_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
#ifdef LOG_STREAM
		dolog = getenv("EVOLUTION_MAIL_LOG_HTML") != NULL;
#endif
		type = camel_type_register (CAMEL_STREAM_TYPE,
					    "EMSyncStream",
					    sizeof (EMSyncStream),
					    sizeof (EMSyncStreamClass),
					    (CamelObjectClassInitFunc) em_sync_stream_class_init,
					    NULL,
					    (CamelObjectInitFunc) em_sync_stream_init,
					    (CamelObjectFinalizeFunc) em_sync_stream_finalize);
	}
	
	return type;
}

static void
em_sync_stream_class_init (EMSyncStreamClass *klass)
{
	CamelStreamClass *stream_class = CAMEL_STREAM_CLASS (klass);
	
	parent_class = (CamelStreamClass *) CAMEL_STREAM_TYPE;
	
	/* virtual method overload */
	stream_class->write = stream_write;
	stream_class->flush = stream_flush;
	stream_class->close = stream_close;
}

static gboolean
emcs_gui_received(GIOChannel *source, GIOCondition cond, void *data)
{
	EMSyncStream *emss = data;
	struct _EMSyncStreamPrivate *p = emss->priv;
	struct _write_msg *msg;

	d(printf("%p: gui sync op job waiting\n", emss));

	msg = (struct _write_msg *)e_msgport_get(p->data_port);
	/* Should never happen ... */
	if (msg == NULL)
		return TRUE;

	d(printf("%p: running sync op %d\n", emss, msg->op));

	/* force out any pending data before doing anything else */
	if (p->buf_used > 0) {
		EMSS_CLASS(emss)->sync_write((CamelStream *)emss, p->buf_data, p->buf_used);
#ifdef LOG_STREAM
		if (p->logfd)
			fwrite(p->buf_data, 1, p->buf_used, p->logfd);
#endif
		p->buf_used = 0;
	}

	/* FIXME: need to handle return values */

	switch (msg->op) {
	case EMSS_WRITE:
		EMSS_CLASS(emss)->sync_write((CamelStream *)emss, msg->data, msg->n);
#ifdef LOG_STREAM
		if (p->logfd)
			fwrite(msg->data, 1, msg->n, p->logfd);
#endif
		break;
	case EMSS_FLUSH:
		EMSS_CLASS(emss)->sync_flush((CamelStream *)emss);
		break;
	case EMSS_CLOSE:
		EMSS_CLASS(emss)->sync_close((CamelStream *)emss);
#ifdef LOG_STREAM
		if (p->logfd) {
			fclose(p->logfd);
			p->logfd = NULL;
		}
#endif
		break;
	}
	
	e_msgport_reply((EMsg *)msg);
	d(printf("%p: gui sync op jobs done\n", emss));

	return TRUE;
}

static void
em_sync_stream_init (CamelObject *object)
{
	EMSyncStream *emss = (EMSyncStream *)object;
	struct _EMSyncStreamPrivate *p;

	p = emss->priv = g_malloc0(sizeof(*p));

	p->data_port = e_msgport_new();
	p->reply_port = e_msgport_new();

#ifndef G_OS_WIN32
	p->gui_channel = g_io_channel_unix_new(e_msgport_fd(p->data_port));
#else
	p->gui_channel = g_io_channel_win32_new_socket(e_msgport_fd(p->data_port));
#endif
	p->gui_watch = g_io_add_watch(p->gui_channel, G_IO_IN, emcs_gui_received, emss);

#ifdef LOG_STREAM
	if (dolog) {
		char name[32];
		static int count;

		sprintf(name, "sync-stream.%d.html", count++);
		printf("Saving raw data stream to '%s'\n", name);
		p->logfd = fopen(name, "w");
	}
#endif

	d(printf("%p: new emss\n", emss));
}

static void
sync_op(EMSyncStream *emss, enum _write_msg_t op, const char *data, size_t n)
{
	struct _EMSyncStreamPrivate *p = emss->priv;
	struct _write_msg msg;

	d(printf("%p: launching sync op %d\n", emss, op));

	/* we do everything synchronous, we should never have any locks, and
	   this prevents overflow from banked up data */

	msg.msg.reply_port = p->reply_port;
	msg.op = op;
	msg.data = data;
	msg.n = n;

	e_msgport_put(p->data_port, &msg.msg);
	e_msgport_wait(p->reply_port);

	g_assert(e_msgport_get(msg.msg.reply_port) == &msg.msg);
	d(printf("%p: returned sync op %d\n", emss, op));
}

static void
em_sync_stream_finalize (CamelObject *object)
{
	EMSyncStream *emss = (EMSyncStream *)object;
	struct _EMSyncStreamPrivate *p = emss->priv;

	/* TODO: is this stuff safe to do in another thread? */
	g_source_remove(p->gui_watch);
	g_io_channel_unref(p->gui_channel);

	e_msgport_destroy(p->data_port);
	e_msgport_destroy(p->reply_port);

	p->data_port = NULL;
	p->reply_port = NULL;

	g_free(p->buf_data);

#ifdef LOG_STREAM
	if (p->logfd)
		fclose(p->logfd);
#endif

	g_free(p);
}

static ssize_t
stream_write (CamelStream *stream, const char *buffer, size_t n)
{
	EMSyncStream *emss = EM_SYNC_STREAM (stream);
	struct _EMSyncStreamPrivate *p = emss->priv;

	if (emss->cancel)
		return -1;

	if (pthread_equal(pthread_self(), mail_gui_thread)) {
		EMSS_CLASS(emss)->sync_write(stream, buffer, n);
#ifdef LOG_STREAM
		if (p->logfd)
			fwrite(buffer, 1, n, p->logfd);
#endif
	} else if (p->buf_size > 0) {
		size_t left = p->buf_size-p->buf_used;

		if (n >= left) {
			sync_op(emss, EMSS_WRITE, buffer, n);
		} else {
			memcpy(p->buf_data + p->buf_used, buffer, n);
			p->buf_used += n;
		}
	} else {
		sync_op(emss, EMSS_WRITE, buffer, n);
	}

	return (ssize_t) n;
}

static int
stream_flush(CamelStream *stream)
{
	EMSyncStream *emss = (EMSyncStream *)stream;

	if (emss->cancel)
		return -1;

	if (pthread_equal(pthread_self(), mail_gui_thread))
		return ((EMSyncStreamClass *)(((CamelObject *)emss)->klass))->sync_flush(stream);
	else
		sync_op(emss, EMSS_FLUSH, NULL, 0);

	return 0;
}

static int
stream_close(CamelStream *stream)
{
	EMSyncStream *emss = (EMSyncStream *)stream;

	if (emss->cancel)
		return -1;

	d(printf("%p: closing stream\n", stream));

	if (pthread_equal(pthread_self(), mail_gui_thread)) {
#ifdef LOG_STREAM
		if (emss->priv->logfd) {
			fclose(emss->priv->logfd);
			emss->priv->logfd = NULL;
		}
#endif
		return ((EMSyncStreamClass *)(((CamelObject *)emss)->klass))->sync_close(stream);
	} else
		sync_op(emss, EMSS_CLOSE, NULL, 0);

	return 0;
}

void
em_sync_stream_set_buffer_size(EMSyncStream *emss, size_t size)
{
	struct _EMSyncStreamPrivate *p = emss->priv;

	g_free(p->buf_data);
	p->buf_data = g_malloc(size);
	p->buf_size = size;
	p->buf_used = 0;
}
