/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors: Jeffrey Stedfast <fejj@ximian.com>
 *	    Michael Zucchi <notzed@ximian.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixbuf-loader.h>
#include <gtk/gtkimage.h>
#include "em-icon-stream.h"

#define d(x) 

static void em_icon_stream_class_init (EMIconStreamClass *klass);
static void em_icon_stream_init (CamelObject *object);
static void em_icon_stream_finalize (CamelObject *object);

static ssize_t emis_sync_write(CamelStream *stream, const char *buffer, size_t n);
static int emis_sync_close(CamelStream *stream);
static int emis_sync_flush(CamelStream *stream);

static EMSyncStreamClass *parent_class = NULL;

CamelType
em_icon_stream_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		parent_class = (EMSyncStreamClass *)em_sync_stream_get_type();
		type = camel_type_register (em_sync_stream_get_type(),
					    "EMIconStream",
					    sizeof (EMIconStream),
					    sizeof (EMIconStreamClass),
					    (CamelObjectClassInitFunc) em_icon_stream_class_init,
					    NULL,
					    (CamelObjectInitFunc) em_icon_stream_init,
					    (CamelObjectFinalizeFunc) em_icon_stream_finalize);
	}
	
	return type;
}

static void
em_icon_stream_class_init (EMIconStreamClass *klass)
{
	((EMSyncStreamClass *)klass)->sync_write = emis_sync_write;
	((EMSyncStreamClass *)klass)->sync_flush = emis_sync_flush;
	((EMSyncStreamClass *)klass)->sync_close = emis_sync_close;
}

static void
em_icon_stream_init (CamelObject *object)
{
	EMIconStream *emis = (EMIconStream *)object;

	emis->width = 24;
	emis->height = 24;
}

static void
emis_cleanup(EMIconStream *emis)
{
	if (emis->loader) {
		gdk_pixbuf_loader_close(emis->loader, NULL);
		g_object_unref(emis->loader);
		emis->loader = NULL;
	}

	if (emis->destroy_id) {
		g_signal_handler_disconnect(emis->image, emis->destroy_id);
		emis->destroy_id = 0;
	}

	emis->image = NULL;
	emis->sync.cancel = TRUE;
}

static void
em_icon_stream_finalize(CamelObject *object)
{
	EMIconStream *emis = (EMIconStream *)object;

	emis_cleanup(emis);
}

static ssize_t
emis_sync_write(CamelStream *stream, const char *buffer, size_t n)
{
	EMIconStream *emis = EM_ICON_STREAM (stream);

	if (emis->loader == NULL)
		return -1;

	if (!gdk_pixbuf_loader_write(emis->loader, buffer, n, NULL)) {
		emis_cleanup(emis);
		return -1;
	}

	return (ssize_t) n;
}

static int
emis_sync_flush(CamelStream *stream)
{
	return 0;
}

static int
emis_sync_close(CamelStream *stream)
{
	EMIconStream *emis = (EMIconStream *)stream;
	int width, height, ratio;
	GdkPixbuf *pixbuf, *mini;

	if (emis->loader == NULL)
		return -1;

	gdk_pixbuf_loader_close(emis->loader, NULL);

	pixbuf = gdk_pixbuf_loader_get_pixbuf(emis->loader);
	if (pixbuf == NULL) {
		printf("couldn't get pixbuf from loader\n");
		emis_cleanup(emis);
		return -1;
	}

	width = gdk_pixbuf_get_width(pixbuf);
	height = gdk_pixbuf_get_height(pixbuf);

	if (width != emis->width || height != emis->height) {
		if (width >= height) {
			if (width > emis->width) {
				ratio = width / emis->width;
				width = emis->width;
				height /= ratio;
			}
		} else {
			if (height > emis->height) {
				ratio = height / emis->height;
				height = emis->height;
				width /= ratio;
			}
		}

		mini = gdk_pixbuf_scale_simple(pixbuf, width, height, GDK_INTERP_BILINEAR);
		gtk_image_set_from_pixbuf(emis->image, mini);
		g_object_unref(mini);
	} else {
		gtk_image_set_from_pixbuf(emis->image, pixbuf);
	}

	g_object_unref(emis->loader);
	emis->loader = NULL;

	g_signal_handler_disconnect(emis->image, emis->destroy_id);
	emis->destroy_id = 0;

	return 0;
}

static void
emis_image_destroy(struct _GtkImage *image, EMIconStream *emis)
{
	emis_cleanup(emis);
}

CamelStream *
em_icon_stream_new(GtkImage *image)
{
	EMIconStream *new;
	
	new = EM_ICON_STREAM(camel_object_new(EM_ICON_STREAM_TYPE));
	new->image = image;
	new->destroy_id = g_signal_connect(image, "destroy", G_CALLBACK(emis_image_destroy), new);
	new->loader = gdk_pixbuf_loader_new();

	return (CamelStream *)new;
}
