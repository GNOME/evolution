/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*-
 *
 * Author:
 *  Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 1999, 2000 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#include <glib.h>

#include <camel/camel-stream-mem.h>

#include "camel-imapp-stream.h"
#include "camel-imapp-exception.h"

#define t(x) 
#define io(x) x

static CamelObjectClass *parent_class = NULL;

/* Returns the class for a CamelStream */
#define CS_CLASS(so) CAMEL_IMAPP_FETCH_STREAM_CLASS(CAMEL_OBJECT_GET_CLASS(so))

static ssize_t
stream_read(CamelStream *stream, char *buffer, size_t n)
{
	CamelIMAPPFetchStream *is = (CamelIMAPPFetchStream *)stream;
	ssize_t max;

	/* make sure we have all the data read in */
	while (camel_imapp_engine_iterate(is->engine, is->command)>0)
		;

	if (is->literal == 0 || n == 0)
		return 0;

	max = is->end - is->ptr;
	if (max > 0) {
		max = MIN(max, is->literal);
		max = MIN(max, n);
		memcpy(buffer, is->ptr, max);
		is->ptr += max;
	} else {
		max = MIN(is->literal, n);
		max = camel_stream_read(is->source, buffer, max);
		if (max <= 0)
			return max;
	}

	is->literal -= max;
	
	return max;
}

static ssize_t
stream_write(CamelStream *stream, const char *buffer, size_t n)
{
	CamelIMAPPFetchStream *is = (CamelIMAPPFetchStream *)stream;

	return camel_stream_write(is->source, buffer, n);
}

static int
stream_close(CamelStream *stream)
{
	/* nop? */
	return 0;
}

static int
stream_flush(CamelStream *stream)
{
	/* nop? */
	return 0;
}

static gboolean
stream_eos(CamelStream *stream)
{
	CamelIMAPPFetchStream *is = (CamelIMAPPFetchStream *)stream;

	return is->literal == 0;
}

static int
stream_reset(CamelStream *stream)
{
	/* nop?  reset literal mode? */
	return 0;
}

static void
camel_imapp_fetch_stream_class_init (CamelStreamClass *camel_imapp_fetch_stream_class)
{
	CamelStreamClass *camel_stream_class = (CamelStreamClass *)camel_imapp_fetch_stream_class;

	parent_class = camel_type_get_global_classfuncs( CAMEL_OBJECT_TYPE );

	/* virtual method definition */
	camel_stream_class->read = stream_read;
	camel_stream_class->write = stream_write;
	camel_stream_class->close = stream_close;
	camel_stream_class->flush = stream_flush;
	camel_stream_class->eos = stream_eos;
	camel_stream_class->reset = stream_reset;
}

static void
camel_imapp_fetch_stream_init(CamelIMAPPFetchStream *is, CamelIMAPPFetchStreamClass *isclass)
{
	;
}

static void
camel_imapp_fetch_stream_finalise(CamelIMAPPFetchStream *is)
{
	if (is->engine)
		camel_object_unref(is->engine);
}

CamelType
camel_imapp_fetch_stream_get_type (void)
{
	static CamelType camel_imapp_fetch_stream_type = CAMEL_INVALID_TYPE;

	if (camel_imapp_fetch_stream_type == CAMEL_INVALID_TYPE) {
		setup_table();
		camel_imapp_fetch_stream_type = camel_type_register( camel_stream_get_type(),
							    "CamelIMAPPFetchStream",
							    sizeof( CamelIMAPPFetchStream ),
							    sizeof( CamelIMAPPFetchStreamClass ),
							    (CamelObjectClassInitFunc) camel_imapp_fetch_stream_class_init,
							    NULL,
							    (CamelObjectInitFunc) camel_imapp_fetch_stream_init,
							    (CamelObjectFinalizeFunc) camel_imapp_fetch_stream_finalise );
	}

	return camel_imapp_fetch_stream_type;
}

/**
 * camel_imapp_fetch_stream_new:
 *
 * Return value: the stream
 **/
CamelStream *
camel_imapp_fetch_stream_new(CamelIMAPPEngine *ie, const char *uid, const char *body)
{
	CamelIMAPPFetchStream *is;

	is = (CamelIMAPPFetchStream *)camel_object_new(camel_imapp_fetch_stream_get_type ());
	is->engine = ie;
	camel_object_ref(ie);

	is->command = camel_imapp_engine_command_new(ie, "FETCH", NULL, "FETCH %t (BODY[%t]<0.4096>", uid, body);
	camel_imapp_engine_command_queue(ie, command);

	return (CamelStream *)is;
}

