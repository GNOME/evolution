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
#include <camel/camel-object.h>
#include <libedataserver/e-flag.h>

#include "mail-mt.h"

#define EMSS_CLASS(x) ((EMSyncStreamClass *)(((CamelObject *)(x))->klass))

enum _write_msg_t {
	EMSS_WRITE,
	EMSS_FLUSH,
	EMSS_CLOSE
};

struct _write_msg {
	EMSyncStream *emss;
	EFlag *done;

	enum _write_msg_t op;

	const gchar *string;
	gsize len;
};

static CamelStreamClass *parent_class = NULL;

static gboolean
emss_process_message (struct _write_msg *msg)
{
	struct _EMSyncStream *emss = msg->emss;

	if (emss->cancel) {
		/* Do not pass data to the child if we are canceled. */
		e_flag_set (msg->done);

		return FALSE;
	}

	/* Force out any pending data before doing anything else. */
	if (emss->buffer != NULL && emss->buffer->len > 0) {
		EMSS_CLASS (emss)->sync_write (
			CAMEL_STREAM (emss), emss->buffer->str,
			emss->buffer->len);
		g_string_set_size (emss->buffer, 0);
	}

	switch (msg->op) {
		case EMSS_WRITE:
			EMSS_CLASS (emss)->sync_write (
				CAMEL_STREAM (emss), msg->string, msg->len);
			break;
		case EMSS_FLUSH:
			EMSS_CLASS (emss)->sync_flush (
				CAMEL_STREAM (emss));
			break;
		case EMSS_CLOSE:
			EMSS_CLASS (emss)->sync_close (
				CAMEL_STREAM (emss));
			break;
	}

	emss->idle_id = 0;
	e_flag_set (msg->done);

	return FALSE;
}

static void
emss_sync_op (EMSyncStream *emss, enum _write_msg_t op,
	      const gchar *string, gsize len)
{
	struct _write_msg msg;

	msg.done = e_flag_new ();
	msg.emss = emss;
	msg.op = op;
	msg.string = string;
	msg.len = len;

	camel_object_ref (emss);

	if (emss->idle_id)
		g_source_remove (emss->idle_id);
	emss->idle_id = g_idle_add ((GSourceFunc) emss_process_message, &msg);

	e_flag_wait (msg.done);
	e_flag_free (msg.done);

	camel_object_unref (emss);
}

static gssize
emss_stream_write (CamelStream *stream, const gchar *string, gsize len)
{
	EMSyncStream *emss = EM_SYNC_STREAM (stream);

	if (emss->cancel)
		return -1;

	if (mail_in_main_thread ()) {
		EMSS_CLASS (emss)->sync_write (stream, string, len);
	} else if (emss->buffer != NULL) {
		if (len < (emss->buffer->allocated_len - emss->buffer->len))
			g_string_append_len (emss->buffer, string, len);
		else
			emss_sync_op (emss, EMSS_WRITE, string, len);
	} else {
		emss_sync_op(emss, EMSS_WRITE, string, len);
	}

	return (gssize) len;
}

static gint
emss_stream_flush (CamelStream *stream)
{
	EMSyncStream *emss = EM_SYNC_STREAM (stream);

	if (emss->cancel)
		return -1;

	if (mail_in_main_thread ())
		return EMSS_CLASS (emss)->sync_flush (stream);
	else
		emss_sync_op (emss, EMSS_FLUSH, NULL, 0);

	return 0;
}

static gint
emss_stream_close (CamelStream *stream)
{
	EMSyncStream *emss = EM_SYNC_STREAM (stream);

	if (emss->cancel)
		return -1;

	emss->idle_id = 0;

	if (mail_in_main_thread ())
		return EMSS_CLASS (emss)->sync_close (stream);
	else
		emss_sync_op (emss, EMSS_CLOSE, NULL, 0);

	return 0;
}

static void
em_sync_stream_class_init (EMSyncStreamClass *class)
{
	CamelStreamClass *stream_class = CAMEL_STREAM_CLASS (class);

	parent_class = (CamelStreamClass *) CAMEL_STREAM_TYPE;

	stream_class->write = emss_stream_write;
	stream_class->flush = emss_stream_flush;
	stream_class->close = emss_stream_close;
}

static void
em_sync_stream_finalize (EMSyncStream *emss)
{
	if (emss->buffer != NULL)
		g_string_free (emss->buffer, TRUE);
	if (emss->idle_id)
		g_source_remove (emss->idle_id);
}

CamelType
em_sync_stream_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;

	if (G_UNLIKELY (type == CAMEL_INVALID_TYPE))
		type = camel_type_register (
			CAMEL_STREAM_TYPE,
			"EMSyncStream",
			sizeof (EMSyncStream),
			sizeof (EMSyncStreamClass),
			(CamelObjectClassInitFunc) em_sync_stream_class_init,
			NULL,
			(CamelObjectInitFunc) NULL,
			(CamelObjectFinalizeFunc) em_sync_stream_finalize);

	return type;
}

void
em_sync_stream_set_buffer_size (EMSyncStream *emss, gsize size)
{
	if (emss->buffer != NULL)
		g_string_free (emss->buffer, TRUE);
	emss->buffer = g_string_sized_new (size);
}
