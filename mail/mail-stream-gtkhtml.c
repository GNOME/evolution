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

#include "mail-stream-gtkhtml.h"

static CamelStreamClass *parent_class = NULL;

static ssize_t stream_write (CamelStream *stream, const char *buffer, size_t n);

static void
mail_stream_gtkhtml_class_init (MailStreamGtkHTMLClass *mail_stream_gtkhtml_class)
{
	CamelStreamClass *camel_stream_class =
		CAMEL_STREAM_CLASS (mail_stream_gtkhtml_class);
	
	parent_class = CAMEL_STREAM_CLASS (camel_type_get_global_classfuncs (CAMEL_STREAM_TYPE));
	
	/* virtual method overload */
	camel_stream_class->write = stream_write;
}

static void
mail_stream_gtkhtml_init (CamelObject *object)
{
	;
}

static void
mail_stream_gtkhtml_finalize (CamelObject *object)
{
	;
}

CamelType
mail_stream_gtkhtml_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (CAMEL_STREAM_TYPE,
					    "MailStreamGtkHTML",
					    sizeof (MailStreamGtkHTML),
					    sizeof (MailStreamGtkHTMLClass),
					    (CamelObjectClassInitFunc) mail_stream_gtkhtml_class_init,
					    NULL,
					    (CamelObjectInitFunc) mail_stream_gtkhtml_init,
					    (CamelObjectFinalizeFunc) mail_stream_gtkhtml_finalize);
	}
	
	return type;
}


CamelStream *
mail_stream_gtkhtml_new (GtkHTML *html, GtkHTMLStream *html_stream)
{
	MailStreamGtkHTML *stream_gtkhtml;
	
	stream_gtkhtml = MAIL_STREAM_GTKHTML (camel_object_new (MAIL_STREAM_GTKHTML_TYPE));
	stream_gtkhtml->html = html;
	stream_gtkhtml->html_stream = html_stream;
	
	return CAMEL_STREAM (stream_gtkhtml);
}

static ssize_t
stream_write (CamelStream *stream, const char *buffer, size_t n)
{
	MailStreamGtkHTML *stream_gtkhtml = MAIL_STREAM_GTKHTML (stream);
	
	gtk_html_write (stream_gtkhtml->html, stream_gtkhtml->html_stream,
			buffer, n);
	return n;
}
