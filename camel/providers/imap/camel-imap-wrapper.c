/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; -*- */
/* camel-imap-wrapper.c: data wrapper for offline IMAP data */

/*
 * Author: Dan Winship <danw@helixcode.com>
 *
 * Copyright 2000 Helix Code, Inc. (http://www.helixcode.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#include <config.h>

#include "camel-imap-wrapper.h"
#include "camel-imap-command.h"
#include "camel-imap-store.h"
#include "camel-imap-utils.h"
#include "camel-imap-private.h"
#include "camel-exception.h"
#include "camel-folder.h"
#include "camel-stream-mem.h"
#include "camel-stream-filter.h"
#include "camel-mime-filter-basic.h"
#include "camel-mime-filter-crlf.h"
#include "camel-mime-filter-charset.h"
#include "camel-mime-part.h"

#include <errno.h>
#include <string.h>

static CamelDataWrapperClass *parent_class = NULL;

/* Returns the class for a CamelDataWrapper */
#define CDW_CLASS(so) CAMEL_DATA_WRAPPER_CLASS (CAMEL_OBJECT_GET_CLASS(so))

static int write_to_stream (CamelDataWrapper *imap_wrapper, CamelStream *stream);

static void
camel_imap_wrapper_class_init (CamelImapWrapperClass *camel_imap_wrapper_class)
{
	CamelDataWrapperClass *camel_data_wrapper_class =
		CAMEL_DATA_WRAPPER_CLASS (camel_imap_wrapper_class);

	parent_class = CAMEL_DATA_WRAPPER_CLASS (camel_type_get_global_classfuncs (camel_data_wrapper_get_type ()));

	/* virtual method override */
	camel_data_wrapper_class->write_to_stream = write_to_stream;
}

static void
camel_imap_wrapper_finalize (CamelObject *object)
{
	CamelImapWrapper *imap_wrapper = CAMEL_IMAP_WRAPPER (object);

	if (imap_wrapper->folder)
		camel_object_unref (CAMEL_OBJECT (imap_wrapper->folder));
	if (imap_wrapper->uid)
		g_free (imap_wrapper->uid);
	if (imap_wrapper->part)
		g_free (imap_wrapper->part_spec);

#ifdef ENABLE_THREADS
	g_mutex_free (imap_wrapper->priv->lock);
#endif
	g_free (imap_wrapper->priv);
}

static void
camel_imap_wrapper_init (gpointer object, gpointer klass)
{
	CamelImapWrapper *imap_wrapper = CAMEL_IMAP_WRAPPER (object);

	imap_wrapper->priv = g_new0 (struct _CamelImapWrapperPrivate, 1);
#ifdef ENABLE_THREADS
	imap_wrapper->priv->lock = g_mutex_new ();
#endif
}

CamelType
camel_imap_wrapper_get_type (void)
{
	static CamelType camel_imap_wrapper_type = CAMEL_INVALID_TYPE;

	if (camel_imap_wrapper_type == CAMEL_INVALID_TYPE) {
		camel_imap_wrapper_type = camel_type_register (
			CAMEL_DATA_WRAPPER_TYPE, "CamelImapWrapper",
			sizeof (CamelImapWrapper),
			sizeof (CamelImapWrapperClass),
			(CamelObjectClassInitFunc) camel_imap_wrapper_class_init,
			NULL,
			(CamelObjectInitFunc) camel_imap_wrapper_init,
			(CamelObjectFinalizeFunc) camel_imap_wrapper_finalize);
	}

	return camel_imap_wrapper_type;
}


static int
write_to_stream (CamelDataWrapper *data_wrapper, CamelStream *stream)
{
	CamelImapWrapper *imap_wrapper = CAMEL_IMAP_WRAPPER (data_wrapper);
	CamelImapStore *store;
	CamelImapResponse *response;
	CamelStream *memstream;
	CamelStreamFilter *filterstream;
	CamelMimeFilter *filter;
	CamelContentType *ct;
	char *result, *p, *body;
	int len;

	CAMEL_IMAP_WRAPPER_LOCK (imap_wrapper, lock);
	if (!data_wrapper->offline) {
		CAMEL_IMAP_WRAPPER_UNLOCK (imap_wrapper, lock);
		return parent_class->write_to_stream (data_wrapper, stream);
	}

	store = CAMEL_IMAP_STORE (imap_wrapper->folder->parent_store);
	CAMEL_IMAP_STORE_LOCK (store, command_lock);
	response = camel_imap_command (store, imap_wrapper->folder, NULL,
				       "UID FETCH %s BODY.PEEK[%s]",
				       imap_wrapper->uid,
				       imap_wrapper->part_spec);
	CAMEL_IMAP_STORE_UNLOCK (store, command_lock);
	if (!response)
		goto lose;

	result = camel_imap_response_extract (response, "FETCH", NULL);
	if (!result)
		goto lose;

	p = strchr (result, ']');
	if (!p) {
		g_free (result);
		goto lose;
	}
	p += 2;

	body = imap_parse_nstring (&p, &len);
	g_free (result);
	if (!body)
		goto lose;

	memstream = camel_stream_mem_new_with_buffer (body, len);
	g_free (body);
	filterstream = camel_stream_filter_new_with_stream (memstream);

	if (camel_mime_part_get_encoding (imap_wrapper->part) ==
	    CAMEL_MIME_PART_ENCODING_BASE64) {
		filter = (CamelMimeFilter *)camel_mime_filter_basic_new_type (CAMEL_MIME_FILTER_BASIC_BASE64_DEC);
		camel_stream_filter_add (filterstream, filter);
	} else if (camel_mime_part_get_encoding (imap_wrapper->part) ==
		   CAMEL_MIME_PART_ENCODING_QUOTEDPRINTABLE) {
		filter = (CamelMimeFilter *)camel_mime_filter_basic_new_type (CAMEL_MIME_FILTER_BASIC_QP_DEC);
		camel_stream_filter_add (filterstream, filter);
	} else
		filter = NULL;

	ct = camel_mime_part_get_content_type (imap_wrapper->part);
	if (header_content_type_is (ct, "text", "*")) {
		const char *charset;

		/* If we just did B64/QP, need to also do CRLF->LF */
		if (filter) {
			filter = camel_mime_filter_crlf_new (CAMEL_MIME_FILTER_CRLF_DECODE,
							     CAMEL_MIME_FILTER_CRLF_MODE_CRLF_ONLY);
			camel_stream_filter_add (filterstream, filter);
		}

		charset = header_content_type_param (ct, "charset");
		if (charset && !(strcasecmp (charset, "us-ascii") == 0
				 || strcasecmp (charset, "utf-8") == 0)) {
			filter = (CamelMimeFilter *)camel_mime_filter_charset_new_convert (charset, "UTF-8");
			if (filter)
				camel_stream_filter_add (filterstream, filter);
		}
	}

	data_wrapper->stream = CAMEL_STREAM (filterstream);
	data_wrapper->offline = FALSE;

	camel_object_unref (CAMEL_OBJECT (imap_wrapper->folder));
	imap_wrapper->folder = NULL;
	g_free (imap_wrapper->uid);
	imap_wrapper->uid = NULL;
	g_free (imap_wrapper->part_spec);
	imap_wrapper->part = NULL;

	CAMEL_IMAP_WRAPPER_UNLOCK (imap_wrapper, lock);

	return parent_class->write_to_stream (data_wrapper, stream);

 lose:
	CAMEL_IMAP_WRAPPER_UNLOCK (imap_wrapper, lock);
	errno = ENETUNREACH;
	return -1;
}


CamelDataWrapper *
camel_imap_wrapper_new (CamelFolder *folder, CamelContentType *type,
			const char *uid, const char *part_spec,
			CamelMimePart *part)
{
	CamelImapWrapper *imap_wrapper;

	imap_wrapper = (CamelImapWrapper *)camel_object_new(camel_imap_wrapper_get_type());

	camel_data_wrapper_set_mime_type_field (CAMEL_DATA_WRAPPER (imap_wrapper), type);
	((CamelDataWrapper *)imap_wrapper)->offline = TRUE;

	imap_wrapper->folder = folder;
	camel_object_ref (CAMEL_OBJECT (folder));
	imap_wrapper->uid = g_strdup (uid);
	imap_wrapper->part_spec = g_strdup (part_spec);

	/* Don't ref this, it's our parent. */
	imap_wrapper->part = part;

	return (CamelDataWrapper *)imap_wrapper;
}
