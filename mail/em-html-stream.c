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
 *		Jeffrey Stedfast <fejj@ximian.com>
 *	    Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include "em-html-stream.h"

#define d(x)

G_DEFINE_TYPE (EMHTMLStream, em_html_stream, EM_TYPE_SYNC_STREAM)

static void
html_stream_cleanup (EMHTMLStream *emhs)
{
	if (emhs->sync.cancel && emhs->html_stream)
		gtk_html_stream_close (
			emhs->html_stream, GTK_HTML_STREAM_ERROR);

	emhs->html_stream = NULL;
	emhs->sync.cancel = TRUE;
	g_signal_handler_disconnect (emhs->html, emhs->destroy_id);
	g_object_unref (emhs->html);
	emhs->html = NULL;
}

static void
html_stream_gtkhtml_destroy (GtkHTML *html,
                             EMHTMLStream *emhs)
{
	emhs->sync.cancel = TRUE;
	html_stream_cleanup (emhs);
}

static void
html_stream_dispose (GObject *object)
{
	EMHTMLStream *emhs = EM_HTML_STREAM (object);

	if (emhs->html_stream) {
		/* set 'in finalise' flag */
		camel_stream_close (CAMEL_STREAM (emhs), NULL);
	}
}

static gssize
html_stream_sync_write (CamelStream *stream,
                        const gchar *buffer,
                        gsize n,
                        GError **error)
{
	EMHTMLStream *emhs = EM_HTML_STREAM (stream);

	if (emhs->html == NULL) {
		g_set_error (
			error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			_("No HTML stream available"));
		return -1;
	}

	if (emhs->html_stream == NULL)
		emhs->html_stream = gtk_html_begin_full (
			emhs->html, NULL, NULL, emhs->flags);

	gtk_html_stream_write (emhs->html_stream, buffer, n);

	return (gssize) n;
}

static gint
html_stream_sync_flush (CamelStream *stream,
                        GError **error)
{
	EMHTMLStream *emhs = (EMHTMLStream *)stream;

	if (emhs->html_stream == NULL) {
		g_set_error (
			error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			_("No HTML stream available"));
		return -1;
	}

	gtk_html_flush (emhs->html);

	return 0;
}

static gint
html_stream_sync_close (CamelStream *stream,
                        GError **error)
{
	EMHTMLStream *emhs = (EMHTMLStream *)stream;

	if (emhs->html_stream == NULL) {
		g_set_error (
			error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			_("No HTML stream available"));
		return -1;
	}

	gtk_html_stream_close (emhs->html_stream, GTK_HTML_STREAM_OK);
	html_stream_cleanup (emhs);

	return 0;
}

static void
em_html_stream_class_init (EMHTMLStreamClass *class)
{
	GObjectClass *object_class;
	EMSyncStreamClass *sync_stream_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = html_stream_dispose;

	sync_stream_class = EM_SYNC_STREAM_CLASS (class);
	sync_stream_class->sync_write = html_stream_sync_write;
	sync_stream_class->sync_flush = html_stream_sync_flush;
	sync_stream_class->sync_close = html_stream_sync_close;
}

static void
em_html_stream_init (EMHTMLStream *emhs)
{
}

/* TODO: Could pass NULL for html_stream, and do a gtk_html_begin
   on first data -> less flashing */
CamelStream *
em_html_stream_new (GtkHTML *html,
                    GtkHTMLStream *html_stream)
{
	EMHTMLStream *new;

	g_return_val_if_fail (GTK_IS_HTML (html), NULL);

	new = g_object_new (EM_TYPE_HTML_STREAM, NULL);
	new->html_stream = html_stream;
	new->html = g_object_ref (html);
	new->flags = 0;
	new->destroy_id = g_signal_connect (
		html, "destroy",
		G_CALLBACK (html_stream_gtkhtml_destroy), new);

	em_sync_stream_set_buffer_size (&new->sync, 8192);

	return CAMEL_STREAM (new);
}

void
em_html_stream_set_flags (EMHTMLStream *emhs, GtkHTMLBeginFlags flags)
{
	g_return_if_fail (EM_IS_HTML_STREAM (emhs));

	emhs->flags = flags;
}
