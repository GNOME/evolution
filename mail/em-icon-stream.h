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
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef EM_ICON_STREAM_H
#define EM_ICON_STREAM_H

#include "mail/em-sync-stream.h"

#define EM_ICON_STREAM_TYPE     (em_icon_stream_get_type ())
#define EM_ICON_STREAM(obj)     (CAMEL_CHECK_CAST((obj), EM_ICON_STREAM_TYPE, EMIconStream))
#define EM_ICON_STREAM_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), EM_ICON_STREAM_TYPE, EMIconStreamClass))
#define EM_IS_ICON_STREAM(o)    (CAMEL_CHECK_TYPE((o), EM_ICON_STREAM_TYPE))

G_BEGIN_DECLS

typedef struct _EMIconStream {
	EMSyncStream sync;

	guint width, height;
	guint destroy_id;
	struct _GdkPixbufLoader *loader;
	GtkImage *image;
	gchar *key;

	guint keep:1;
} EMIconStream;

typedef struct {
	EMSyncStreamClass parent_class;
} EMIconStreamClass;

CamelType    em_icon_stream_get_type (void);
CamelStream *em_icon_stream_new(GtkImage *image, const gchar *key, guint maxwidth, guint maxheight, gint keep);

struct _GdkPixbuf *em_icon_stream_get_image(const gchar *key, guint maxwidth, guint maxheight);
gint em_icon_stream_is_resized(const gchar *key, guint maxwidth, guint maxheight);

void em_icon_stream_clear_cache(void);

G_END_DECLS

#endif /* EM_ICON_STREAM_H */
