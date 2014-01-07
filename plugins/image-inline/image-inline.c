/*
 * image-inline.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <gtkhtml/gtkhtml-embedded.h>
#include <gtkimageview/gtkimagescrollwin.h>

#include <mail/em-format-hook.h>
#include <mail/em-format-html-display.h>

static gint org_gnome_image_inline_classid;

/* Forward Declarations */
void org_gnome_image_inline_format (gpointer ep, EMFormatHookTarget *target);

typedef struct _ImageInlinePObject ImageInlinePObject;

struct _ImageInlinePObject {
	EMFormatHTMLPObject object;

	CamelMimePart *mime_part;
	GdkPixbuf *pixbuf;
	GtkWidget *widget;
};

static void
auto_rotate (ImageInlinePObject *image_object)
{
	GdkPixbuf *pixbuf;
	GdkPixbufRotation rotation;
	const gchar *orientation;
	gboolean flip;

	/* Check for an "orientation" pixbuf option and honor it. */

	pixbuf = image_object->pixbuf;
	orientation = gdk_pixbuf_get_option (pixbuf, "orientation");

	if (orientation == NULL)
		return;

	switch (strtol (orientation, NULL, 10)) {
		case 1: /* top - left */
			rotation = GDK_PIXBUF_ROTATE_NONE;
			flip = FALSE;
			break;

		case 2: /* top - right */
			rotation = GDK_PIXBUF_ROTATE_NONE;
			flip = TRUE;
			break;

		case 3: /* bottom - right */
			rotation = GDK_PIXBUF_ROTATE_UPSIDEDOWN;
			flip = FALSE;
			break;

		case 4: /* bottom - left */
			rotation = GDK_PIXBUF_ROTATE_UPSIDEDOWN;
			flip = TRUE;
			break;

		case 5: /* left/top */
			rotation = GDK_PIXBUF_ROTATE_CLOCKWISE;
			flip = TRUE;
			break;

		case 6: /* right/top */
			rotation = GDK_PIXBUF_ROTATE_CLOCKWISE;
			flip = FALSE;
			break;

		case 7: /* right/bottom */
			rotation = GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE;
			flip = TRUE;
			break;

		case 8: /* left/bottom */
			rotation = GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE;
			flip = FALSE;
			break;

		default:
			g_return_if_reached ();
	}

	if (rotation != GDK_PIXBUF_ROTATE_NONE) {
		pixbuf = gdk_pixbuf_rotate_simple (pixbuf, rotation);
		g_return_if_fail (pixbuf != NULL);
		g_object_unref (image_object->pixbuf);
		image_object->pixbuf = pixbuf;
	}

	if (flip) {
		pixbuf = gdk_pixbuf_flip (pixbuf, TRUE);
		g_return_if_fail (pixbuf != NULL);
		g_object_unref (image_object->pixbuf);
		image_object->pixbuf = pixbuf;
	}
}

static void
set_drag_source (GtkImageView *image_view)
{
	GtkTargetEntry *targets;
	GtkTargetList *list;
	gint n_targets;

	list = gtk_target_list_new (NULL, 0);
	gtk_target_list_add_uri_targets (list, 0);
	targets = gtk_target_table_new_from_list (list, &n_targets);

	gtk_drag_source_set (
		GTK_WIDGET (image_view), GDK_BUTTON1_MASK,
		targets, n_targets, GDK_ACTION_COPY);

	gtk_target_table_free (targets, n_targets);
	gtk_target_list_unref (list);
}

static gboolean
button_press_press_cb (GtkImageView *image_view,
                       GdkEvent *button_event,
                       ImageInlinePObject *image_object)
{
	if (event->type != GDK_2BUTTON_PRESS)
		return FALSE;

	if (gtk_image_view_get_zoom (image_view) < 1.0) {
		gtk_image_view_set_zoom (image_view, 1.0);
		gtk_drag_source_unset (GTK_WIDGET (image_view));
	} else {
		gtk_image_view_set_fitting (image_view, TRUE);
		set_drag_source (image_view);
	}

	return TRUE;
}

static void
drag_data_get_cb (GtkImageView *image_view,
                  GdkDragContext *context,
                  GtkSelectionData *selection,
                  guint info,
                  guint time,
                  ImageInlinePObject *image_object)
{
	EMFormatHTMLDisplay *html_display;
	EAttachmentStore *store;
	EAttachmentView *view;
	EAttachment *attachment = NULL;
	GtkTreeRowReference *reference;
	GtkTreePath *path;
	GList *list, *iter;

	/* XXX This illustrates the lack of integration between EMFormat
	 *     and EAttachment, in that we now have to search through the
	 *     attachment store to find an attachment whose CamelMimePart
	 *     matches ours.  This allows us to defer to EAttachmentView
	 *     for the drag-data-get implementation. */

	html_display = EM_FORMAT_HTML_DISPLAY (image_object->object.format);
	view = em_format_html_display_get_attachment_view (html_display);

	store = e_attachment_view_get_store (view);
	list = e_attachment_store_get_attachments (store);

	for (iter = list; iter != NULL; iter = iter->next) {
		CamelMimePart *mime_part;

		attachment = E_ATTACHMENT (iter->data);
		mime_part = e_attachment_get_mime_part (attachment);

		if (mime_part == image_object->mime_part) {
			g_object_ref (attachment);
			break;
		}

		attachment = NULL;
	}

	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);

	/* Make sure we found an EAttachment to select. */
	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	/* Now select its path in the attachment store. */

	reference = e_attachment_get_reference (attachment);
	g_return_if_fail (gtk_tree_row_reference_valid (reference));

	path = gtk_tree_row_reference_get_path (reference);

	e_attachment_view_unselect_all (view);
	e_attachment_view_select_path (view, path);

	gtk_tree_path_free (path);

	/* Let EAttachmentView handle the rest. */

	e_attachment_view_drag_data_get (
		view, context, selection, info, time);
}

static void
size_allocate_cb (GtkHTMLEmbedded *embedded,
                  GtkAllocation *allocation,
                  ImageInlinePObject *image_object)
{
	GtkAllocation image_allocation;
	EWebView *web_view;
	gint pixbuf_width;
	gint pixbuf_height;
	gint widget_width;
	gint widget_height;
	gdouble zoom = 1.0;

	web_view = em_format_html_get_web_view (image_object->object.format);
	gtk_widget_get_allocation (GTK_WIDGET (web_view), &image_allocation);
	widget_width = image_allocation.width - 12;

	pixbuf_width = gdk_pixbuf_get_width (image_object->pixbuf);
	pixbuf_height = gdk_pixbuf_get_height (image_object->pixbuf);

	if (pixbuf_width > widget_width)
		zoom = (gdouble) widget_width / pixbuf_width;

	widget_width = MIN (widget_width, pixbuf_width);
	widget_height = (gint) (zoom * pixbuf_height);

	gtk_widget_set_size_request (
		image_object->widget, widget_width, widget_height);
}

static void
mouse_wheel_scroll_cb (GtkWidget *image_view,
                       GdkScrollDirection direction,
                       ImageInlinePObject *image_object)
{
	GtkOrientation orientation;
	GtkScrollType scroll_type;
	EWebView *web_view;
	gint steps = 2;

	web_view = em_format_html_get_web_view (image_object->object.format);

	switch (direction) {
		case GDK_SCROLL_UP:
			orientation = GTK_ORIENTATION_VERTICAL;
			scroll_type = GTK_SCROLL_STEP_BACKWARD;
			break;

		case GDK_SCROLL_DOWN:
			orientation = GTK_ORIENTATION_VERTICAL;
			scroll_type = GTK_SCROLL_STEP_FORWARD;
			break;

		case GDK_SCROLL_LEFT:
			orientation = GTK_ORIENTATION_HORIZONTAL;
			scroll_type = GTK_SCROLL_STEP_BACKWARD;
			break;

		case GDK_SCROLL_RIGHT:
			orientation = GTK_ORIENTATION_HORIZONTAL;
			scroll_type = GTK_SCROLL_STEP_FORWARD;
			break;

		default:
			g_return_if_reached ();
	}

	while (steps > 0) {
		g_signal_emit_by_name (
			web_view, "scroll",
			orientation, scroll_type, 2.0, NULL);
		steps--;
	}
}

static void
org_gnome_image_inline_pobject_free (EMFormatHTMLPObject *object)
{
	ImageInlinePObject *image_object;

	image_object = (ImageInlinePObject *) object;

	if (image_object->mime_part != NULL) {
		g_object_unref (image_object->mime_part);
		image_object->mime_part = NULL;
	}

	if (image_object->pixbuf != NULL) {
		g_object_unref (image_object->pixbuf);
		image_object->pixbuf = NULL;
	}

	if (image_object->widget != NULL) {
		GtkWidget *parent;

		g_signal_handlers_disconnect_by_func (
			image_object->widget,
			button_press_press_cb, image_object);
		g_signal_handlers_disconnect_by_func (
			image_object->widget,
			drag_data_get_cb, image_object);
		g_signal_handlers_disconnect_by_func (
			image_object->widget,
			mouse_wheel_scroll_cb, image_object);

		parent = gtk_widget_get_parent (image_object->widget);
		if (parent != NULL)
			g_signal_handlers_disconnect_by_func (
				parent, size_allocate_cb, image_object);

		g_object_unref (image_object->widget);
		image_object->widget = NULL;
	}
}

static void
org_gnome_image_inline_decode (ImageInlinePObject *image_object)
{
	GdkPixbuf *pixbuf;
	GdkPixbufLoader *loader;
	CamelDataWrapper *data_wrapper;
	CamelMimePart *mime_part;
	CamelMedium *medium;
	CamelStream *stream;
	GByteArray *array;
	GError *error = NULL;

	array = g_byte_array_new ();
	mime_part = image_object->mime_part;
	medium = CAMEL_MEDIUM (mime_part);

	/* Stream takes ownership of the byte array. */
	stream = camel_stream_mem_new_with_byte_array (array);
	data_wrapper = camel_medium_get_content (medium);
	camel_data_wrapper_decode_to_stream_sync (
		data_wrapper, stream, NULL, NULL);

	/* Don't trust the content type in the MIME part.  It could
	 * be lying or it could be "application/octet-stream".  Let
	 * the GtkPixbufLoader figure it out. */
	loader = gdk_pixbuf_loader_new ();
	gdk_pixbuf_loader_write (loader, array->data, array->len, &error);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
		goto exit;
	}

	pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
	if (pixbuf != NULL) {
		image_object->pixbuf = g_object_ref (pixbuf);
		auto_rotate (image_object);
	}

	gdk_pixbuf_loader_close (loader, &error);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
		goto exit;
	}

exit:
	g_object_unref (stream);
	g_object_unref (loader);
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
	gtk_container_add (GTK_CONTAINER (container), widget);
	image_object->widget = g_object_ref (widget);
	gtk_widget_show (widget);

	image_view = GTK_IMAGE_VIEW (widget);

	gtk_image_view_set_pixbuf (
		image_view, image_object->pixbuf, TRUE);

	set_drag_source (image_view);

	g_signal_connect (
		image_view, "button-press-event",
		G_CALLBACK (button_press_press_cb), image_object);

	g_signal_connect (
		image_view, "drag-data-get",
		G_CALLBACK (drag_data_get_cb), image_object);

	g_signal_connect (
		embedded, "size-allocate",
		G_CALLBACK (size_allocate_cb), image_object);

	g_signal_connect (
		image_view, "mouse-wheel-scroll",
		G_CALLBACK (mouse_wheel_scroll_cb), image_object);

	return TRUE;
}

void
org_gnome_image_inline_format (gpointer ep,
                               EMFormatHookTarget *target)
{
	ImageInlinePObject *image_object;
	gchar *classid;
	gchar *content;

	classid = g_strdup_printf (
		"org-gnome-image-inline-display-%d",
		org_gnome_image_inline_classid++);

	image_object = (ImageInlinePObject *)
		em_format_html_add_pobject (
			EM_FORMAT_HTML (target->format),
			sizeof (ImageInlinePObject),
			classid, target->part,
			org_gnome_image_inline_embed);

	g_object_ref (target->part);
	image_object->mime_part = target->part;

	image_object->object.free = org_gnome_image_inline_pobject_free;
	org_gnome_image_inline_decode (image_object);

	content = g_strdup_printf ("<object classid=%s></object>", classid);
	camel_stream_write_string (target->stream, content, NULL, NULL);
	g_free (content);

	g_free (classid);
}
