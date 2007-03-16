/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Authors: Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 2003 Ximian, Inc. (www.ximian.com)
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

/*
EMSyncStream - Abstract class.
A synchronous stream, that can be written from any thread, but whose
requests are always handled in the main gui thread in the correct order.
*/

#ifndef EM_SYNC_STREAM_H
#define EM_SYNC_STREAM_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define EM_SYNC_STREAM_TYPE     (em_sync_stream_get_type ())
#define EM_SYNC_STREAM(obj)     (CAMEL_CHECK_CAST((obj), EM_SYNC_STREAM_TYPE, EMSyncStream))
#define EM_SYNC_STREAM_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), EM_SYNC_STREAM_TYPE, EMSyncStreamClass))
#define EM_IS_SYNC_STREAM(o)    (CAMEL_CHECK_TYPE((o), EM_SYNC_STREAM_TYPE))

#include <glib.h>
#include <camel/camel-stream.h>

typedef struct _EMSyncStream {
	CamelStream parent_stream;

	struct _EMSyncStreamPrivate *priv;

	int cancel;
} EMSyncStream;

typedef struct {
	CamelStreamClass parent_class;

	ssize_t   (*sync_write)      (CamelStream *stream, const char *buffer, size_t n);
	int       (*sync_close)      (CamelStream *stream);
	int       (*sync_flush)      (CamelStream *stream);
	
} EMSyncStreamClass;

CamelType    em_sync_stream_get_type (void);
void em_sync_stream_set_buffer_size(EMSyncStream *, size_t size);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* EM_SYNC_STREAM_H */
