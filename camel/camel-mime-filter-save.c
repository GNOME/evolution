/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
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


#ifndef HAVE_CONFIG_H
#include <config.h>
#endif

#include "camel-mime-filter-save.h"
#include "camel-stream-mem.h"

static void filter (CamelMimeFilter *f, char *in, size_t len, size_t prespace,
		    char **out, size_t *outlen, size_t *outprespace);
static void complete (CamelMimeFilter *f, char *in, size_t len,
		      size_t prespace, char **out, size_t *outlen,
		      size_t *outprespace);
static void reset (CamelMimeFilter *f);


static void
camel_mime_filter_save_class_init (CamelMimeFilterSaveClass *klass)
{
	CamelMimeFilterClass *mime_filter_class =
		(CamelMimeFilterClass *) klass;
	
	mime_filter_class->filter = filter;
	mime_filter_class->complete = complete;
	mime_filter_class->reset = reset;
}

CamelType
camel_mime_filter_save_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_mime_filter_get_type(), "CamelMimeFilterSave",
					    sizeof (CamelMimeFilterSave),
					    sizeof (CamelMimeFilterSaveClass),
					    (CamelObjectClassInitFunc) camel_mime_filter_save_class_init,
					    NULL,
					    NULL,
					    NULL);
	}
	
	return type;
}

static void
filter (CamelMimeFilter *f, char *in, size_t len, size_t prespace,
	char **out, size_t *outlen, size_t *outprespace)
{
	CamelMimeFilterSave *save = (CamelMimeFilterSave *) f;
	
	if (save->stream)
		camel_stream_write (save->stream, in, len);
	
	*out = in;
	*outlen = len;
	*outprespace = f->outpre;
}

static void 
complete (CamelMimeFilter *f, char *in, size_t len, size_t prespace,
	  char **out, size_t *outlen, size_t *outprespace)
{
	if (len)
		filter (f, in, len, prespace, out, outlen, outprespace);
}

static void
reset (CamelMimeFilter *f)
{
	/* no-op */
}

CamelMimeFilter *
camel_mime_filter_save_new (void)
{
	CamelMimeFilterSave *save = CAMEL_MIME_FILTER_SAVE (camel_object_new (CAMEL_MIME_FILTER_SAVE_TYPE));
	
	save->stream = camel_stream_mem_new ();
	
	return (CamelMimeFilter *) save;
}

CamelMimeFilter *
camel_mime_filter_save_new_with_stream (CamelStream *stream)
{
	CamelMimeFilterSave *save = CAMEL_MIME_FILTER_SAVE (camel_object_new (CAMEL_MIME_FILTER_SAVE_TYPE));
	
	save->stream = stream;
	camel_object_ref (stream);
	
	return (CamelMimeFilter *) save;
}
