/*
 * html-stream.c: A CamelStream class that feeds data into a GtkHTML widget
 *
 * Author:
 *   Miguel de Icaza (miguel@helixcode.com)
 *
 * (C) 2000 Helix Code, Inc.
 */
#include <config.h>
#include "html-stream.h"
#include "e-util/e-util.h"

#define PARENT_TYPE camel_stream_get_type ()

static GtkObjectClass *html_stream_parent_class;

/*
 * CamelStream::read method
 *
 * Return 0 bytes read, as this is a write-only stream
 */
static gint
html_stream_read (CamelStream *stream, gchar *buffer, gint n)
{
	return 0;
}

/*
 * CamelStream::write method
 *
 * Writes @buffer into the HTML widget
 */
static gint
html_stream_write (CamelStream *stream, const gchar *buffer, gint n)
{
	HTMLStream *html_stream = HTML_STREAM (stream);
	
	gtk_html_write (html_stream->gtk_html, html_stream->gtk_html_stream, buffer, n);

	return n;
}

/*
 * CamelStream::available method
 *
 * Return 0, as this is only a write-stream
 */
static gint
html_stream_available (CamelStream *stream)
{
	return 0;
}

/*
 * CamelStream::eos method.
 *
 * We just return TRUE, as this is not a read-stream
 */
static gboolean
html_stream_eos (CamelStream *stream)
{
	return TRUE;
}

static void
html_stream_close (CamelStream *stream)
{
	HTMLStream *html_stream = HTML_STREAM (stream);
	
	gtk_html_end (html_stream->gtk_html, html_stream->gtk_html_stream, GTK_HTML_STREAM_OK);
}

static void
html_stream_destroy (GtkObject *object)
{
}

static void
html_stream_class_init (GtkObjectClass *object_class)
{
	CamelStreamClass *stream_class = (CamelStreamClass *) object_class;

	html_stream_parent_class = gtk_type_class (PARENT_TYPE);
	
	object_class->destroy = html_stream_destroy;
	
	stream_class->read = html_stream_read;
	stream_class->write = html_stream_write;
	stream_class->available = html_stream_available;
	stream_class->eos = html_stream_eos;
	stream_class->close = html_stream_close;
}

CamelStream *
html_stream_new (GtkHTML *html)
{
	HTMLStream *html_stream;

	g_return_val_if_fail (html != NULL, NULL);
	g_return_val_if_fail (GTK_IS_HTML (html), NULL);
	
	html_stream = gtk_type_new (html_stream_get_type ());

	gtk_object_ref (GTK_OBJECT (html));

	html_stream->gtk_html_stream = gtk_html_begin (html, NULL);
	gtk_html_parse (html);
	
	html_stream->gtk_html = html;

	return CAMEL_STREAM (html_stream);
}

E_MAKE_TYPE (html_stream, "HTMLStream", HTMLStream, html_stream_class_init, NULL, PARENT_TYPE);


