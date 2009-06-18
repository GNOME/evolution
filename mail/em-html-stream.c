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
#include <gtkhtml/gtkhtml.h>
#include <gtkhtml/gtkhtml-stream.h>
#include <gtk/gtk.h>
#include "em-html-stream.h"

#define d(x)

static void em_html_stream_class_init (EMHTMLStreamClass *klass);
static void em_html_stream_init (CamelObject *object);
static void em_html_stream_finalize (CamelObject *object);

static gssize emhs_sync_write(CamelStream *stream, const gchar *buffer, gsize n);
static gint emhs_sync_close(CamelStream *stream);
static gint emhs_sync_flush(CamelStream *stream);

static EMSyncStreamClass *parent_class = NULL;

CamelType
em_html_stream_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;

	if (type == CAMEL_INVALID_TYPE) {
		parent_class = (EMSyncStreamClass *)em_sync_stream_get_type();
		type = camel_type_register (em_sync_stream_get_type(),
					    "EMHTMLStream",
					    sizeof (EMHTMLStream),
					    sizeof (EMHTMLStreamClass),
					    (CamelObjectClassInitFunc) em_html_stream_class_init,
					    NULL,
					    (CamelObjectInitFunc) em_html_stream_init,
					    (CamelObjectFinalizeFunc) em_html_stream_finalize);
	}

	return type;
}

static void
em_html_stream_class_init (EMHTMLStreamClass *klass)
{
	((EMSyncStreamClass *)klass)->sync_write = emhs_sync_write;
	((EMSyncStreamClass *)klass)->sync_flush = emhs_sync_flush;
	((EMSyncStreamClass *)klass)->sync_close = emhs_sync_close;
}

static void
em_html_stream_init (CamelObject *object)
{
	/*EMHTMLStream *emhs = (EMHTMLStream *)object;*/
}

static void
emhs_cleanup(EMHTMLStream *emhs)
{
	if (emhs->sync.cancel && emhs->html_stream)
		gtk_html_stream_close (emhs->html_stream, GTK_HTML_STREAM_ERROR);
	emhs->html_stream = NULL;
	emhs->sync.cancel = TRUE;
	g_signal_handler_disconnect(emhs->html, emhs->destroy_id);
	g_object_unref(emhs->html);
	emhs->html = NULL;
}

static void
em_html_stream_finalize (CamelObject *object)
{
	EMHTMLStream *emhs = (EMHTMLStream *)object;

	d(printf("%p: finalising stream\n", object));
	if (emhs->html_stream) {
		d(printf("%p: html stream still open - error\n", object));
		/* set 'in finalise' flag */
		camel_stream_close((CamelStream *)emhs);
	}
}

static gssize
emhs_sync_write(CamelStream *stream, const gchar *buffer, gsize n)
{
	EMHTMLStream *emhs = EM_HTML_STREAM (stream);

	if (emhs->html == NULL)
		return -1;

	if (emhs->html_stream == NULL)
		emhs->html_stream = gtk_html_begin_full (emhs->html, NULL, NULL, emhs->flags);

	gtk_html_stream_write(emhs->html_stream, buffer, n);

	return (gssize) n;
}

static gint
emhs_sync_flush(CamelStream *stream)
{
	EMHTMLStream *emhs = (EMHTMLStream *)stream;

	if (emhs->html_stream == NULL)
		return -1;

	gtk_html_flush (emhs->html);

	return 0;
}

static gint
emhs_sync_close(CamelStream *stream)
{
	EMHTMLStream *emhs = (EMHTMLStream *)stream;

	if (emhs->html_stream == NULL)
		return -1;

	gtk_html_stream_close(emhs->html_stream, GTK_HTML_STREAM_OK);
	emhs_cleanup(emhs);

	return 0;
}

static void
emhs_gtkhtml_destroy(GtkHTML *html, EMHTMLStream *emhs)
{
	d(printf("%p: emhs gtkhtml destroy\n", emhs));
	emhs->sync.cancel = TRUE;
	emhs_cleanup(emhs);
}

/* TODO: Could pass NULL for html_stream, and do a gtk_html_begin
   on first data -> less flashing */
CamelStream *
em_html_stream_new(GtkHTML *html, GtkHTMLStream *html_stream)
{
	EMHTMLStream *new;

	new = EM_HTML_STREAM (camel_object_new (EM_HTML_STREAM_TYPE));
	new->html_stream = html_stream;
	new->html = html;
	new->flags = 0;
	g_object_ref(html);
	new->destroy_id = g_signal_connect(html, "destroy", G_CALLBACK(emhs_gtkhtml_destroy), new);

	em_sync_stream_set_buffer_size(&new->sync, 8192);

	return (CamelStream *)new;
}

void
em_html_stream_set_flags (EMHTMLStream *emhs, GtkHTMLBeginFlags flags)
{
	emhs->flags = flags;
}
