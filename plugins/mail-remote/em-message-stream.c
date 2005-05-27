/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Michael Zucchi <notzed@novel.com>
 *
 *  Copyright 2005 Novell, Inc. (www.novell.com)
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
#include <errno.h>
#include <camel/camel-stream.h>
#include "em-message-stream.h"

#define d(x) 

#define EMSS_CLASS(x) ((EMMessageStreamClass *)(((CamelObject *)(x))->klass))

static CamelStreamClass *parent_class = NULL;

static ssize_t
emms_read(CamelStream *stream, char *buffer, size_t n)
{
	EMMessageStream *emms = (EMMessageStream *)stream;
	ssize_t len;
	Evolution_Mail_Buffer *buf;
	CORBA_Environment ev = { 0 };

	/* To avoid all of the rount-trip overhead, this could always fire off
	   one request in advance, to pipeline the data.  Using another thread. */

	if (emms->source == CORBA_OBJECT_NIL) {
		errno = EBADF;
		return -1;
	}

	buf = Evolution_Mail_MessageStream_next(emms->source, n, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free(&ev);
		Evolution_Mail_MessageStream_dispose(emms->source, &ev);
		emms->source = CORBA_OBJECT_NIL;
		stream->eos = TRUE;
		errno = EBADF;
		return -1;
	}

	if (buf->_length == 0)
		stream->eos = TRUE;

	len = buf->_length;
	memcpy(buffer, buf->_buffer, buf->_length);
	CORBA_free(buf);

	return len;
}

static void
em_message_stream_init (CamelObject *object)
{
	/*EMMessageStream *emss = (EMMessageStream *)object;*/
}

static void
em_message_stream_finalize (CamelObject *object)
{
	EMMessageStream *emms = (EMMessageStream *)object;

	printf("EMMessageStream.finalise()\n");

	if (emms->source) {
		CORBA_Environment ev = { 0 };

		Evolution_Mail_MessageStream_dispose(emms->source, &ev);
		if (ev._major != CORBA_NO_EXCEPTION)
			CORBA_exception_free(&ev);

		CORBA_Object_release(emms->source, &ev);
		if (ev._major != CORBA_NO_EXCEPTION)
			CORBA_exception_free(&ev);
	}
}

static void
em_message_stream_class_init (EMMessageStreamClass *klass)
{
	CamelStreamClass *stream_class = CAMEL_STREAM_CLASS (klass);

	parent_class = (CamelStreamClass *) CAMEL_STREAM_TYPE;

	stream_class->read = emms_read;
}

CamelType
em_message_stream_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (CAMEL_STREAM_TYPE,
					    "EMMessageStream",
					    sizeof (EMMessageStream),
					    sizeof (EMMessageStreamClass),
					    (CamelObjectClassInitFunc) em_message_stream_class_init,
					    NULL,
					    (CamelObjectInitFunc) em_message_stream_init,
					    (CamelObjectFinalizeFunc) em_message_stream_finalize);
	}
	
	return type;
}

CamelStream *
em_message_stream_new(const Evolution_Mail_MessageStream source)
{
	EMMessageStream *ems = (EMMessageStream *)camel_object_new(em_message_stream_get_type());
	CORBA_Environment ev = { 0 };

	ems->source = CORBA_Object_duplicate(source, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free(&ev);
		camel_object_unref(ems);
		ems = NULL;
	}

	return (CamelStream *)ems;
}
