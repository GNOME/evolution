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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mail-display-stream.h"


static void mail_display_stream_class_init (MailDisplayStreamClass *klass);
static void mail_display_stream_init (CamelObject *object);
static void mail_display_stream_finalize (CamelObject *object);

static ssize_t stream_write (CamelStream *stream, const char *buffer, size_t n);


static CamelStreamClass *parent_class = NULL;


CamelType
mail_display_stream_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (CAMEL_STREAM_TYPE,
					    "MailDisplayStream",
					    sizeof (MailDisplayStream),
					    sizeof (MailDisplayStreamClass),
					    (CamelObjectClassInitFunc) mail_display_stream_class_init,
					    NULL,
					    (CamelObjectInitFunc) mail_display_stream_init,
					    (CamelObjectFinalizeFunc) mail_display_stream_finalize);
	}
	
	return type;
}

static void
mail_display_stream_class_init (MailDisplayStreamClass *klass)
{
	CamelStreamClass *stream_class = CAMEL_STREAM_CLASS (klass);
	
	parent_class = (CamelStreamClass *) CAMEL_STREAM_TYPE;
	
	/* virtual method overload */
	stream_class->write = stream_write;
}

static void
mail_display_stream_init (CamelObject *object)
{
	;
}

static void
mail_display_stream_finalize (CamelObject *object)
{
	;
}

static ssize_t
stream_write (CamelStream *stream, const char *buffer, size_t n)
{
	MailDisplayStream *dstream = MAIL_DISPLAY_STREAM (stream);
	
	gtk_html_write (dstream->html, dstream->html_stream, buffer, n);
	
	return (ssize_t) n;
}


CamelStream *
mail_display_stream_new (GtkHTML *html, GtkHTMLStream *html_stream)
{
	MailDisplayStream *new;
	
	new = MAIL_DISPLAY_STREAM (camel_object_new (MAIL_DISPLAY_STREAM_TYPE));
	new->html = html;
	new->html_stream = html_stream;
	
	return CAMEL_STREAM (new);
}
