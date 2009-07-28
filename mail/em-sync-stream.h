/*
 *
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
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/*
EMSyncStream - Abstract class.
A synchronous stream, that can be written from any thread, but whose
requests are always handled in the main gui thread in the correct order.
*/

#ifndef EM_SYNC_STREAM_H
#define EM_SYNC_STREAM_H

#include <glib.h>
#include <camel/camel-stream.h>

#define EM_SYNC_STREAM_TYPE \
	(em_sync_stream_get_type ())
#define EM_SYNC_STREAM(obj) \
	(CAMEL_CHECK_CAST \
	((obj), EM_SYNC_STREAM_TYPE, EMSyncStream))
#define EM_SYNC_STREAM_CLASS(cls) \
	(CAMEL_CHECK_CLASS_CAST \
	((cls), EM_SYNC_STREAM_TYPE, EMSyncStreamClass))
#define EM_IS_SYNC_STREAM(obj) \
	(CAMEL_CHECK_TYPE ((obj), EM_SYNC_STREAM_TYPE))

G_BEGIN_DECLS

typedef struct _EMSyncStream EMSyncStream;
typedef struct _EMSyncStreamClass EMSyncStreamClass;

struct _EMSyncStream {
	CamelStream parent;
	GString *buffer;
	gboolean cancel;
	guint idle_id;
};

struct _EMSyncStreamClass {
	CamelStreamClass parent_class;

	gssize		(*sync_write)		(CamelStream *stream,
						 const gchar *string,
						 gsize len);
	gint		(*sync_close)		(CamelStream *stream);
	gint		(*sync_flush)		(CamelStream *stream);
};

CamelType	em_sync_stream_get_type		(void);
void		em_sync_stream_set_buffer_size	(EMSyncStream *stream,
						 gsize size);

G_END_DECLS

#endif /* EM_SYNC_STREAM_H */
