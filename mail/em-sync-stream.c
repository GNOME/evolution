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
 *		Jeffrey Stedfast <fejj@ximian.com>
 *	    Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "em-sync-stream.h"

#include <stdio.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <libedataserver/e-flag.h>

#include "mail-mt.h"

enum _write_msg_t {
	EMSS_WRITE,
	EMSS_FLUSH,
	EMSS_CLOSE
};

struct _write_msg {
	EMSyncStream *emss;
	GError *error;
	EFlag *done;

	enum _write_msg_t op;

	const gchar *string;
	gsize len;
};

G_DEFINE_TYPE (EMSyncStream, em_sync_stream, CAMEL_TYPE_STREAM)

static gboolean
sync_stream_process_message (struct _write_msg *msg)
{
	struct _EMSyncStream *emss = msg->emss;

	if (emss->cancel) {
		/* Do not pass data to the child if we are canceled. */
		e_flag_set (msg->done);

		return FALSE;
	}

	/* Force out any pending data before doing anything else. */
	if (emss->buffer != NULL && emss->buffer->len > 0) {
		EM_SYNC_STREAM_GET_CLASS (emss)->sync_write (
			CAMEL_STREAM (emss), emss->buffer->str,
			emss->buffer->len, &msg->error);
		g_string_set_size (emss->buffer, 0);
	}

	switch (msg->op) {
		case EMSS_WRITE:
			EM_SYNC_STREAM_GET_CLASS (emss)->sync_write (
				CAMEL_STREAM (emss), msg->string,
				msg->len, &msg->error);
			break;
		case EMSS_FLUSH:
			EM_SYNC_STREAM_GET_CLASS (emss)->sync_flush (
				CAMEL_STREAM (emss), &msg->error);
			break;
		case EMSS_CLOSE:
			EM_SYNC_STREAM_GET_CLASS (emss)->sync_close (
				CAMEL_STREAM (emss), &msg->error);
			break;
	}

	emss->idle_id = 0;
	e_flag_set (msg->done);

	return FALSE;
}

static void
sync_stream_sync_op (EMSyncStream *emss,
                     enum _write_msg_t op,
                     const gchar *string,
                     gsize len,
                     GError **error)
{
	struct _write_msg msg;

	msg.done = e_flag_new ();
	msg.error = NULL;
	msg.emss = emss;
	msg.op = op;
	msg.string = string;
	msg.len = len;

	g_object_ref (emss);

	if (emss->idle_id)
		g_source_remove (emss->idle_id);
	emss->idle_id = g_idle_add ((GSourceFunc) sync_stream_process_message, &msg);

	e_flag_wait (msg.done);
	e_flag_free (msg.done);

	if (msg.error != NULL)
		g_propagate_error (error, msg.error);

	g_object_unref (emss);
}

static void
sync_stream_finalize (GObject *object)
{
	EMSyncStream *emss = EM_SYNC_STREAM (object);

	if (emss->buffer != NULL)
		g_string_free (emss->buffer, TRUE);
	if (emss->idle_id)
		g_source_remove (emss->idle_id);
}

static gssize
sync_stream_write (CamelStream *stream,
                   const gchar *string,
                   gsize len,
                   GError **error)
{
	EMSyncStream *emss = EM_SYNC_STREAM (stream);

	if (emss->cancel) {
		g_set_error (
			error, G_IO_ERROR, G_IO_ERROR_CANCELLED,
			_("Canceled"));
		return -1;
	}

	if (mail_in_main_thread ()) {
		EM_SYNC_STREAM_GET_CLASS (emss)->sync_write (stream, string, len, error);
	} else if (emss->buffer != NULL) {
		if (len < (emss->buffer->allocated_len - emss->buffer->len))
			g_string_append_len (emss->buffer, string, len);
		else
			sync_stream_sync_op (emss, EMSS_WRITE, string, len, error);
	} else {
		sync_stream_sync_op(emss, EMSS_WRITE, string, len, error);
	}

	return (gssize) len;
}

static gint
sync_stream_flush (CamelStream *stream,
                   GError **error)
{
	EMSyncStream *emss = EM_SYNC_STREAM (stream);

	if (emss->cancel) {
		g_set_error (
			error, G_IO_ERROR, G_IO_ERROR_CANCELLED,
			_("Canceled"));
		return -1;
	}

	if (mail_in_main_thread ())
		return EM_SYNC_STREAM_GET_CLASS (emss)->sync_flush (stream, error);
	else
		sync_stream_sync_op (emss, EMSS_FLUSH, NULL, 0, error);

	return 0;
}

static gint
sync_stream_close (CamelStream *stream,
                   GError **error)
{
	EMSyncStream *emss = EM_SYNC_STREAM (stream);

	if (emss->cancel) {
		g_set_error (
			error, G_IO_ERROR, G_IO_ERROR_CANCELLED,
			_("Canceled"));
		return -1;
	}

	emss->idle_id = 0;

	if (mail_in_main_thread ())
		return EM_SYNC_STREAM_GET_CLASS (emss)->sync_close (stream, error);
	else
		sync_stream_sync_op (emss, EMSS_CLOSE, NULL, 0, error);

	return 0;
}

static void
em_sync_stream_class_init (EMSyncStreamClass *class)
{
	GObjectClass *object_class;
	CamelStreamClass *stream_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = sync_stream_finalize;

	stream_class = CAMEL_STREAM_CLASS (class);
	stream_class->write = sync_stream_write;
	stream_class->flush = sync_stream_flush;
	stream_class->close = sync_stream_close;
}

static void
em_sync_stream_init (EMSyncStream *emss)
{
}

void
em_sync_stream_set_buffer_size (EMSyncStream *emss, gsize size)
{
	if (emss->buffer != NULL)
		g_string_free (emss->buffer, TRUE);
	emss->buffer = g_string_sized_new (size);
}
