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
#include <string.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixbuf-loader.h>
#ifdef HAVE_LIBGNOMEUI_GNOME_THUMBNAIL_H
#include <libgnomeui/gnome-thumbnail.h>
#endif
#include <gtk/gtkimage.h>
#include "em-icon-stream.h"

#include "libedataserver/e-msgport.h"

#define d(x) 

/* fixed-point scale factor for scaled images in cache */
#define EMIS_SCALE (1024)

struct _emis_cache_node {
	EMCacheNode node;

	GdkPixbuf *pixbuf;
};

static void em_icon_stream_class_init (EMIconStreamClass *klass);
static void em_icon_stream_init (CamelObject *object);
static void em_icon_stream_finalize (CamelObject *object);

static ssize_t emis_sync_write(CamelStream *stream, const char *buffer, size_t n);
static int emis_sync_close(CamelStream *stream);
static int emis_sync_flush(CamelStream *stream);

static EMSyncStreamClass *parent_class = NULL;
static EMCache *emis_cache;

static void
emis_cache_free(void *data)
{
	struct _emis_cache_node *node = data;

	g_object_unref(node->pixbuf);
}

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

		emis_cache = em_cache_new(60, sizeof(struct _emis_cache_node), emis_cache_free);
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

	emis = emis;
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

	g_free(emis->key);
	emis->key = NULL;

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

static GdkPixbuf *
emis_fit(GdkPixbuf *pixbuf, int maxwidth, int maxheight, int *scale)
{
	GdkPixbuf *mini = NULL;
	int width, height;

	width = gdk_pixbuf_get_width(pixbuf);
	height = gdk_pixbuf_get_height(pixbuf);

	if ((maxwidth && width > maxwidth)
	    || (maxheight && height > maxheight)) {
		if (width >= height) {
			if (scale)
				*scale = maxwidth * EMIS_SCALE / width;
			height = height * maxwidth / width;
			width = maxwidth;
		} else {
			if (scale)
				*scale = maxheight * EMIS_SCALE / height;
			width = width * maxheight / height;
			height = maxheight;
		}

#ifdef HAVE_LIBGNOMEUI_GNOME_THUMBNAIL_H
		mini = gnome_thumbnail_scale_down_pixbuf(pixbuf, width, height);
#else
		mini = gdk_pixbuf_scale_simple(pixbuf, width, height, GDK_INTERP_BILINEAR);
#endif
	}

	return mini;
}

static int
emis_sync_close(CamelStream *stream)
{
	EMIconStream *emis = (EMIconStream *)stream;
	GdkPixbuf *pixbuf, *mini;
	struct _emis_cache_node *node;
	char *scalekey;
	int scale;

	if (emis->loader == NULL)
		return -1;

	gdk_pixbuf_loader_close(emis->loader, NULL);

	pixbuf = gdk_pixbuf_loader_get_pixbuf(emis->loader);
	if (pixbuf == NULL) {
		d(printf("couldn't get pixbuf from loader\n"));
		emis_cleanup(emis);
		return -1;
	}

	mini = emis_fit(pixbuf, emis->width, emis->height, &scale);
	gtk_image_set_from_pixbuf(emis->image, mini?mini:pixbuf);

	if (emis->keep) {
		node = (struct _emis_cache_node *)em_cache_node_new(emis_cache, emis->key);
		node->pixbuf = g_object_ref(pixbuf);
		em_cache_add(emis_cache, (EMCacheNode *)node);
	}

	if (!emis->keep || mini) {
		scalekey = g_alloca(strlen(emis->key) + 20);
		sprintf(scalekey, "%s.%x", emis->key, scale);
		node = (struct _emis_cache_node *)em_cache_node_new(emis_cache, scalekey);
		node->pixbuf = mini?mini:g_object_ref(pixbuf);
		em_cache_add(emis_cache, (EMCacheNode *)node);
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
em_icon_stream_new(GtkImage *image, const char *key, unsigned int maxwidth, unsigned int maxheight, int keep)
{
	EMIconStream *new;

	new = EM_ICON_STREAM(camel_object_new(EM_ICON_STREAM_TYPE));
	new->width = maxwidth;
	new->height = maxheight;
	new->image = image;
	new->keep = keep;
	new->destroy_id = g_signal_connect(image, "destroy", G_CALLBACK(emis_image_destroy), new);
	new->loader = gdk_pixbuf_loader_new();
	new->key = g_strdup(key);

	return (CamelStream *)new;
}

GdkPixbuf *
em_icon_stream_get_image(const char *key, unsigned int maxwidth, unsigned int maxheight)
{
	struct _emis_cache_node *node;
	GdkPixbuf *pb = NULL;

	/* forces the cache to be setup if not */
	em_icon_stream_get_type();	

	node = (struct _emis_cache_node *)em_cache_lookup(emis_cache, key);
	if (node) {
		int width, height;

		pb = node->pixbuf;
		g_object_ref(pb);
		em_cache_node_unref(emis_cache, (EMCacheNode *)node);

		width = gdk_pixbuf_get_width(pb);
		height = gdk_pixbuf_get_height(pb);

		if ((maxwidth && width > maxwidth)
		    || (maxheight && height > maxheight)) {
			unsigned int scale;
			char *realkey;

			if (maxheight == 0 || width >= height)
				scale = width * EMIS_SCALE / maxwidth;
			else
				scale = height * EMIS_SCALE / maxheight;

			realkey = g_alloca(strlen(key)+20);
			sprintf(realkey, "%s.%x", key, scale);
			node = (struct _emis_cache_node *)em_cache_lookup(emis_cache, realkey);
			if (node) {
				g_object_unref(pb);
				pb = node->pixbuf;
				g_object_ref(pb);
				em_cache_node_unref(emis_cache, (EMCacheNode *)node);
			} else {
				GdkPixbuf *mini = emis_fit(pb, maxwidth, maxheight, NULL);

				g_object_unref(pb);
				pb = mini;
				node = (struct _emis_cache_node *)em_cache_node_new(emis_cache, realkey);
				node->pixbuf = pb;
				g_object_ref(pb);
				em_cache_add(emis_cache, (EMCacheNode *)node);
			}
		}
	}

	return pb;
}

int
em_icon_stream_is_resized(const char *key, unsigned int maxwidth, unsigned int maxheight)
{
	int res = FALSE;
	struct _emis_cache_node *node;
	
	/* forces the cache to be setup if not */
	em_icon_stream_get_type();

	node = (struct _emis_cache_node *)em_cache_lookup(emis_cache, key);
	if (node) {
		res = (maxwidth && gdk_pixbuf_get_width(node->pixbuf) > maxwidth)
			|| (maxheight && gdk_pixbuf_get_width(node->pixbuf) > maxheight);

		em_cache_node_unref(emis_cache, (EMCacheNode *)node);
	}

	return res;
}

void
em_icon_stream_clear_cache(void)
{
	em_cache_clear(emis_cache);
}
