/*
 * image-inline.c
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <camel/camel-medium.h>
#include <camel/camel-mime-part.h>
#include <camel/camel-stream-mem.h>
#include <gtkhtml/gtkhtml-embedded.h>
#include <gtkimageview/gtkimagescrollwin.h>

#include "mail/em-format-hook.h"
#include "mail/em-format-html.h"

static gint org_gnome_image_inline_classid;

/* Forward Declarations */
void org_gnome_image_inline_format (gpointer ep, EMFormatHookTarget *target);

typedef struct _ImageInlinePObject ImageInlinePObject;

struct _ImageInlinePObject {
	EMFormatHTMLPObject object;

	GdkPixbuf *pixbuf;
	GtkWidget *widget;
};

static void
size_allocate_cb (GtkHTMLEmbedded *embedded,
                  GtkAllocation *event,
                  ImageInlinePObject *image_object)
{
	GtkWidget *widget;
	gint pixbuf_width;
	gint pixbuf_height;
	gint widget_width;
	gint widget_height;
	gdouble zoom;

	widget = GTK_WIDGET (image_object->object.format->html);
	widget_width = widget->allocation.width - 12;

	pixbuf_width = gdk_pixbuf_get_width (image_object->pixbuf);
	pixbuf_height = gdk_pixbuf_get_height (image_object->pixbuf);

	if (pixbuf_width <= widget_width)
		zoom = 1.0;
	else
		zoom = (gdouble) widget_width / pixbuf_width;

	widget_width = MIN (widget_width, pixbuf_width);
	widget_height = (gint) (zoom * pixbuf_height);

	gtk_widget_set_size_request (
		image_object->widget, widget_width, widget_height);
}

static void
org_gnome_image_inline_pobject_free (EMFormatHTMLPObject *object)
{
	ImageInlinePObject *image_object;

	image_object = (ImageInlinePObject *) object;

	if (image_object->pixbuf != NULL) {
		g_object_unref (image_object->pixbuf);
		image_object->pixbuf = NULL;
	}

	if (image_object->widget != NULL) {
		g_object_unref (image_object->widget);
		image_object->widget = NULL;
	}
}

static void
org_gnome_image_inline_decode (ImageInlinePObject *image_object,
                               CamelMimePart *mime_part)
{
	GdkPixbuf *pixbuf;
	GdkPixbufLoader *loader;
	CamelContentType *content_type;
	CamelDataWrapper *data_wrapper;
	CamelMedium *medium;
	CamelStream *stream;
	GByteArray *array;
	gchar *mime_type;
	GError *error = NULL;

	array = g_byte_array_new ();
	medium = CAMEL_MEDIUM (mime_part);

	/* Stream takes ownership of the byte array. */
	stream = camel_stream_mem_new_with_byte_array (array);
	data_wrapper = camel_medium_get_content_object (medium);
	camel_data_wrapper_decode_to_stream (data_wrapper, stream);

	content_type = camel_mime_part_get_content_type (mime_part);
	mime_type = camel_content_type_simple (content_type);
	loader = gdk_pixbuf_loader_new_with_mime_type (mime_type, &error);
	g_free (mime_type);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
		goto exit;
	}

	gdk_pixbuf_loader_write (loader, array->data, array->len, &error);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
		goto exit;
	}

	pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
	if (pixbuf != NULL)
		image_object->pixbuf = g_object_ref (pixbuf);

	gdk_pixbuf_loader_close (loader, &error);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
		goto exit;
	}

exit:
	camel_object_unref (mime_part);
	camel_object_unref (stream);
}

static gboolean
org_gnome_image_inline_embed (EMFormatHTML *format,
                              GtkHTMLEmbedded *embedded,
                              EMFormatHTMLPObject *object)
{
	ImageInlinePObject *image_object;
	GtkImageView *image_view;
	GtkWidget *container;
	GtkWidget *widget;

	image_object = (ImageInlinePObject *) object;

	if (image_object->pixbuf == NULL)
		return FALSE;

	container = GTK_WIDGET (embedded);

	widget = gtk_image_view_new ();
	image_view = GTK_IMAGE_VIEW (widget);
	gtk_widget_show (widget);

	widget = gtk_image_scroll_win_new (image_view);
	gtk_container_add (GTK_CONTAINER (container), widget);
	image_object->widget = g_object_ref (widget);
	gtk_widget_show (widget);

	gtk_image_view_set_pixbuf (
		image_view, image_object->pixbuf, TRUE);

	g_signal_connect (
		embedded, "size-allocate",
		G_CALLBACK (size_allocate_cb), image_object);

	return TRUE;
}

void
org_gnome_image_inline_format (gpointer ep, EMFormatHookTarget *target)
{
	ImageInlinePObject *image_object;
	gchar *classid;

	classid = g_strdup_printf (
		"org-gnome-image-inline-display-%d",
		org_gnome_image_inline_classid++);

	image_object = (ImageInlinePObject *)
		em_format_html_add_pobject (
			EM_FORMAT_HTML (target->format),
			sizeof (ImageInlinePObject),
			classid, target->part,
			org_gnome_image_inline_embed);

	camel_object_ref (target->part);

	image_object->object.free = org_gnome_image_inline_pobject_free;
	org_gnome_image_inline_decode (image_object, target->part);

	camel_stream_printf (
		target->stream, "<object classid=%s></object>", classid);

	g_free (classid);
}
