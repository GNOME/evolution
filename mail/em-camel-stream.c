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
#include <gtkhtml/gtkhtml.h>
#include <gtkhtml/gtkhtml-stream.h>
#include <gtk/gtkmain.h>
#include "em-camel-stream.h"

#include "mail-mt.h"

#define EMCS_BUFFER_SIZE (4096)

#define LOG_STREAM 1

#define d(x) 

enum _write_msg_t {
	EMCS_WRITE,
	EMCS_FLUSH,
	EMCS_CLOSE_OK,
	EMCS_CLOSE_ERROR,
};

struct _write_msg {
	EMsg msg;

	enum _write_msg_t op;

	const char *data;
	size_t n;
};

static void em_camel_stream_class_init (EMCamelStreamClass *klass);
static void em_camel_stream_init (CamelObject *object);
static void em_camel_stream_finalize (CamelObject *object);

static ssize_t stream_write(CamelStream *stream, const char *buffer, size_t n);
static int stream_close(CamelStream *stream);
static int stream_flush(CamelStream *stream);

static CamelStreamClass *parent_class = NULL;

CamelType
em_camel_stream_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (CAMEL_STREAM_TYPE,
					    "EMCamelStream",
					    sizeof (EMCamelStream),
					    sizeof (EMCamelStreamClass),
					    (CamelObjectClassInitFunc) em_camel_stream_class_init,
					    NULL,
					    (CamelObjectInitFunc) em_camel_stream_init,
					    (CamelObjectFinalizeFunc) em_camel_stream_finalize);
	}
	
	return type;
}

static void
em_camel_stream_class_init (EMCamelStreamClass *klass)
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
	EMCamelStream *estream = data;
	struct _write_msg *msg;

	d(printf("%p: gui sync op job waiting\n", estream));

	msg = (struct _write_msg *)e_msgport_get(estream->data_port);
	/* Should never happen ... */
	if (msg == NULL)
		return TRUE;

	d(printf("%p: running sync op %d\n", estream, msg->op));

	/* force out any pending data before doing anything else */
	if (estream->used > 0) {
		d(printf("sync write %d\n", estream->used));
		
		if (estream->html_stream)
			gtk_html_stream_write(estream->html_stream, estream->buffer, estream->used);
		estream->used = 0;
	}

	switch (msg->op) {
	case EMCS_WRITE:
		d(printf("sync write %d\n", msg->n));
		if (estream->html_stream)
			gtk_html_stream_write(estream->html_stream, msg->data, msg->n);
		break;
	case EMCS_FLUSH:
		stream_flush((CamelStream *)estream);
		break;
	case EMCS_CLOSE_OK:
		if (estream->html_stream) {
			gtk_html_stream_close(estream->html_stream, GTK_HTML_STREAM_OK);
			estream->html_stream = NULL;
		}
		break;
	case EMCS_CLOSE_ERROR:
		if (estream->html_stream) {
			gtk_html_stream_close(estream->html_stream, GTK_HTML_STREAM_ERROR);
			estream->html_stream = NULL;
		}
		break;
	}
	
	e_msgport_reply((EMsg *)msg);
	d(printf("%p: gui sync op jobs done\n", estream));

	return TRUE;
}

static void
em_camel_stream_init (CamelObject *object)
{
	EMCamelStream *estream = (EMCamelStream *)object;

	estream->data_port = e_msgport_new();
	estream->reply_port = e_msgport_new();

#ifndef G_OS_WIN32
	estream->gui_channel = g_io_channel_unix_new(e_msgport_fd(estream->data_port));
#else
	estream->gui_channel = g_io_channel_win32_new_socket(e_msgport_fd(estream->data_port));
#endif
	estream->gui_watch = g_io_add_watch(estream->gui_channel, G_IO_IN, emcs_gui_received, estream);

	estream->used = 0;
	estream->buffer = g_malloc(EMCS_BUFFER_SIZE);

	d(printf("%p: new estream\n", estream));
}

static void
sync_op(EMCamelStream *estream, enum _write_msg_t op, const char *data, size_t n)
{
	struct _write_msg msg;

	d(printf("%p: launching sync op %d\n", estream, op));
	/* we do everything synchronous, we should never have any locks, and
	   this prevents overflow from banked up data */
	msg.msg.reply_port = estream->reply_port;
	msg.op = op;
	msg.data = data;
	msg.n = n;
	e_msgport_put(estream->data_port, &msg.msg);
	e_msgport_wait(estream->reply_port);
	g_assert(e_msgport_get(msg.msg.reply_port) == &msg.msg);
	d(printf("%p: returned sync op %d\n", estream, op));
}

static void
em_camel_stream_finalize (CamelObject *object)
{
	EMCamelStream *estream = (EMCamelStream *)object;

	d(printf("%p: finalising stream\n", object));
	if (estream->html_stream) {
		d(printf("%p: html stream still open - error\n", object));
		if (pthread_self() == mail_gui_thread)
			gtk_html_stream_close(estream->html_stream, GTK_HTML_STREAM_ERROR);
		else
			sync_op(estream, EMCS_CLOSE_ERROR, NULL, 0);
	}

	/* TODO: is this stuff safe to do in another thread? */
	g_source_remove(estream->gui_watch);
	g_io_channel_unref(estream->gui_channel);
	e_msgport_destroy(estream->data_port);
	estream->data_port = NULL;
	e_msgport_destroy(estream->reply_port);
	estream->reply_port = NULL;
	g_free(estream->buffer);
}

static ssize_t
stream_write (CamelStream *stream, const char *buffer, size_t n)
{
	EMCamelStream *estream = EM_CAMEL_STREAM (stream);

	if (estream->html_stream == NULL)
		return -1;

#ifdef LOG_STREAM
	if (estream->save)
		fwrite(buffer, sizeof(char), n, estream->save);
#endif

	if (pthread_self() == mail_gui_thread)
		gtk_html_stream_write(estream->html_stream, buffer, n);
	else {
#if 1
		size_t left = EMCS_BUFFER_SIZE-estream->used;

		/* A super-simple buffer, if we get too much to fit, just do a sync
		   write, which will implicitly clear our previous writes first */
		d(printf("thread write '%d'\n", n));

		if (n >= left) {
			sync_op(estream, EMCS_WRITE, buffer, n);
		} else {
			memcpy(estream->buffer + estream->used, buffer, n);
			estream->used += n;
		}
#else
		sync_op(estream, EMCS_WRITE, buffer, n);
#endif
	}	
	return (ssize_t) n;
}

static int
stream_flush(CamelStream *stream)
{
	EMCamelStream *estream = (EMCamelStream *)stream;

	if (estream->html_stream) {
		if (pthread_self() == mail_gui_thread) {
			/* FIXME: flush html stream via gtkhtml_stream_flush which doens't exist yet ... */
			while (gtk_events_pending ())
				gtk_main_iteration ();
		} else {
			sync_op(estream, EMCS_FLUSH, NULL, 0);
		}
	}

	return 0;
}

static int
stream_close(CamelStream *stream)
{
	EMCamelStream *estream = (EMCamelStream *)stream;

	d(printf("%p: closing stream\n", stream));

#ifdef LOG_STREAM
	if (estream->save) {
		fclose(estream->save);
		estream->save = NULL;
	}
#endif

	if (estream->html_stream) {
		if (pthread_self() == mail_gui_thread) {
			gtk_html_stream_close(estream->html_stream, GTK_HTML_STREAM_OK);
			estream->html_stream = NULL;
		} else {
			sync_op(estream, EMCS_CLOSE_OK, NULL, 0);
		}
	}

	return 0;
}

static void
emcs_gtkhtml_destroy(struct _GtkHTML *html, EMCamelStream *emcs)
{
	d(printf("%p: emcs gtkhtml destroy\n", emcs));
	emcs->html = NULL;
	emcs->html_stream = NULL;
}

/* TODO: Could pass NULL for html_stream, and do a gtk_html_begin
   on first data -> less flashing */
CamelStream *
em_camel_stream_new(struct _GtkHTML *html, struct _GtkHTMLStream *html_stream)
{
	EMCamelStream *new;
	
	new = EM_CAMEL_STREAM (camel_object_new (EM_CAMEL_STREAM_TYPE));
	new->html_stream = html_stream;
	g_signal_connect(html, "destroy", G_CALLBACK(emcs_gtkhtml_destroy), new);

#ifdef LOG_STREAM
	{
		static int count;
		char name[32];

		sprintf(name, "camel-stream.%d.html", count++);
		printf("saving raw html to '%s'\n", name);
		new->save = fopen(name, "w");
	}
#endif
	return CAMEL_STREAM (new);
}
