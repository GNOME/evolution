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


#ifndef MAIL_DISPLAY_STREAM_H
#define MAIL_DISPLAY_STREAM_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <camel/camel-stream.h>
#include <gtkhtml/gtkhtml.h>

#define MAIL_DISPLAY_STREAM_TYPE     (mail_display_stream_get_type ())
#define MAIL_DISPLAY_STREAM(obj)     (CAMEL_CHECK_CAST((obj), MAIL_DISPLAY_STREAM_TYPE, MailDisplayStream))
#define MAIL_DISPLAY_STREAM_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), MAIL_DISPLAY_STREAM_TYPE, MailDisplayStreamClass))
#define MAIL_IS_DISPLAY_STREAM(o)    (CAMEL_CHECK_TYPE((o), MAIL_DISPLAY_STREAM_TYPE))

typedef struct _MailDisplayStream {
	CamelStream parent_stream;
	
	GtkHTML *html;
	GtkHTMLStream *html_stream;
} MailDisplayStream;

typedef struct {
	CamelStreamClass parent_class;
	
} MailDisplayStreamClass;


CamelType    mail_display_stream_get_type (void);

/* Note: stream does not ref these objects! */
CamelStream *mail_display_stream_new (GtkHTML *html, GtkHTMLStream *html_stream);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MAIL_DISPLAY_STREAM_H */
