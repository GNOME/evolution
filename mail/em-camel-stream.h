/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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

#ifndef EM_CAMEL_STREAM_H
#define EM_CAMEL_STREAM_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define EM_CAMEL_STREAM_TYPE     (em_camel_stream_get_type ())
#define EM_CAMEL_STREAM(obj)     (CAMEL_CHECK_CAST((obj), EM_CAMEL_STREAM_TYPE, EMCamelStream))
#define EM_CAMEL_STREAM_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), EM_CAMEL_STREAM_TYPE, EMCamelStreamClass))
#define MAIL_IS_DISPLAY_STREAM(o)    (CAMEL_CHECK_TYPE((o), EM_CAMEL_STREAM_TYPE))

struct _GtkHTML;
struct _GtkHTMLStream;

#include <camel/camel-stream.h>
#include "libedataserver/e-msgport.h"

typedef struct _EMCamelStream {
	CamelStream parent_stream;

	struct _GtkHTML *html;
	struct _GtkHTMLStream *html_stream;

	struct _EMsgPort *data_port, *reply_port;
	struct _GIOChannel *gui_channel;
	guint gui_watch;
	char *buffer;
	int used;
	void *save;
} EMCamelStream;

typedef struct {
	CamelStreamClass parent_class;
	
} EMCamelStreamClass;


CamelType    em_camel_stream_get_type (void);

/* the html_stream is closed when we are finalised (with an error), or closed (ok) */
CamelStream *em_camel_stream_new(struct _GtkHTML *html, struct _GtkHTMLStream *html_stream);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* EM_CAMEL_STREAM_H */
