/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2000 Helix Code, Inc. (www.helixcode.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#include <config.h>
#include "camel-imap-stream.h"
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>

static CamelStreamClass *parent_class = NULL;

/* Returns the class for a CamelImapStream */
#define CIS_CLASS(so) CAMEL_IMAP_STREAM_CLASS (CAMEL_OBJECT_GET_CLASS(so))

static ssize_t stream_read (CamelStream *stream, char *buffer, size_t n);
static int stream_reset (CamelStream *stream);
static gboolean stream_eos (CamelStream *stream);

static void finalize (CamelObject *object);

static void
camel_imap_stream_class_init (CamelImapStreamClass *camel_imap_stream_class)
{
	CamelStreamClass *camel_stream_class =
		CAMEL_STREAM_CLASS (camel_imap_stream_class);

	parent_class = CAMEL_STREAM_CLASS(camel_type_get_global_classfuncs (camel_stream_get_type ()));

	/* virtual method overload */
	camel_stream_class->read  = stream_read;
	camel_stream_class->reset = stream_reset;
	camel_stream_class->eos   = stream_eos;
}

static void
camel_imap_stream_init (gpointer object, gpointer klass)
{
	CamelImapStream *imap_stream = CAMEL_IMAP_STREAM (object);

	imap_stream->cache = NULL;
	imap_stream->cache_ptr = NULL;
}

CamelType
camel_imap_stream_get_type (void)
{
	static CamelType camel_imap_stream_type = CAMEL_INVALID_TYPE;

	if (camel_imap_stream_type == CAMEL_INVALID_TYPE) {
		camel_imap_stream_type = camel_type_register (camel_stream_get_type (), "CamelImapStream",
							      sizeof (CamelImapStream),
							      sizeof (CamelImapStreamClass),
							      (CamelObjectClassInitFunc) camel_imap_stream_class_init,
							      NULL,
							      (CamelObjectInitFunc) camel_imap_stream_init,
							      (CamelObjectFinalizeFunc) finalize);
	}

	return camel_imap_stream_type;
}

CamelStream *
camel_imap_stream_new (CamelImapFolder *folder, char *command)
{
	CamelImapStream *imap_stream;

	imap_stream = CAMEL_IMAP_STREAM(camel_object_new (camel_imap_stream_get_type ()));

	imap_stream->folder = folder;
	camel_object_ref (CAMEL_OBJECT (imap_stream->folder));
	
	imap_stream->command = g_strdup (command);
	
	return CAMEL_STREAM (imap_stream);
}

static void
finalize (CamelObject *object)
{
	CamelImapStream *imap_stream = CAMEL_IMAP_STREAM (object);

	g_free (imap_stream->cache);
	g_free (imap_stream->command);

	if (imap_stream->folder)
		camel_object_unref (CAMEL_OBJECT (imap_stream->folder));
}

static ssize_t
stream_read (CamelStream *stream, char *buffer, size_t n)
{
	ssize_t nread;
	
	/* do we want to do any IMAP specific parsing in here? If not, maybe rename to camel-stream-cache? */
	CamelImapStream *imap_stream = CAMEL_IMAP_STREAM (stream);

	if (!imap_stream->cache) {
		/* We need to send the IMAP command since this is our first fetch */
		CamelFolder *folder = CAMEL_FOLDER (imap_stream->folder);
		gchar *result, *p, *q;
		gint status, part_len;
		
		status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store),
						      CAMEL_FOLDER (imap_stream->folder),
						      &result, "%s\r\n",
						      imap_stream->command);
		
		if (!result || status != CAMEL_IMAP_OK) {
			/* we got an error, dump this stuff */
			g_free (result);
			imap_stream->cache = NULL;
			camel_object_unref (CAMEL_OBJECT (imap_stream->folder));
			
			return -1;
		}
		
		/* we don't need the folder anymore... */
		camel_object_unref (CAMEL_OBJECT (imap_stream->folder));

		/* parse out the message part */
		for (p = result; *p && *p != '{' && *p != '\n'; p++);
		if (*p != '{') {
			g_free (result);
			return -1;
		}
		
		part_len = atoi (p + 1);
		for ( ; *p && *p != '\n'; p++);
		if (*p != '\n') {
			g_free (result);
			return -1;
		}
		
		/* calculate the new part-length */
		for (q = p; *q && (q - p) <= part_len; q++) {
			if (*q == '\n')
				part_len--;
		}
		/* FIXME: This is a hack for IMAP daemons that send us a UID at the end of each FETCH */
		for (q--, part_len--; q > p && *(q-1) != '\n'; q--, part_len--);

		imap_stream->cache = g_strndup (p, part_len + 1);
		g_free (result);
		
		imap_stream->cache_ptr = imap_stream->cache;
	}
	
	/* we've already read this stream, so return whats in the cache */
	nread = MIN (n, strlen (imap_stream->cache_ptr));
	
	if (nread > 0) {
		memcpy (buffer, imap_stream->cache_ptr, nread);
		imap_stream->cache_ptr += nread;
	} else {
		nread = -1;
	}

	return nread;
}

static int
stream_reset (CamelStream *stream)
{
	CamelImapStream *imap_stream = CAMEL_IMAP_STREAM (stream);
	
	imap_stream->cache_ptr = imap_stream->cache;

	return 1;
}

static gboolean
stream_eos (CamelStream *stream)
{
	CamelImapStream *imap_stream = CAMEL_IMAP_STREAM (stream);

	return (imap_stream->cache_ptr && strlen (imap_stream->cache_ptr));
}
