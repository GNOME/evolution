/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 *  Authors: Ettore Perazzoli <ettore@ximian.com>
 *           Jeffrey Stedfast <fejj@ximian.com>
 *	     Srinivasa Ragavan <sragavan@novell.com>
 *
 *  Copyright 1999-2005 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include <gdk/gdkkeysyms.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <glib/gi18n.h>
#include <libgnome/libgnome.h>

#include "e-attachment.h"
#include "e-attachment-bar.h"

#include <libedataserver/e-iconv.h>
#include <libedataserver/e-data-server-util.h>

#include <camel/camel-data-wrapper.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-stream-fs.h>
#include <camel/camel-stream-null.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-stream-filter.h>
#include <camel/camel-mime-filter-bestenc.h>
#include <camel/camel-mime-part.h>

#include "e-util/e-util.h"
#include "e-util/e-gui-utils.h"
#include "e-util/e-icon-factory.h"
#include "e-util/e-error.h"
#include "e-util/e-mktemp.h"

#define ICON_WIDTH 64
#define ICON_SEPARATORS " /-_"
#define ICON_SPACING 2
#define ICON_ROW_SPACING ICON_SPACING
#define ICON_COL_SPACING ICON_SPACING
#define ICON_BORDER 2
#define ICON_TEXT_SPACING 2


static GnomeIconListClass *parent_class = NULL;

struct _EAttachmentBarPrivate {
	GtkWidget *attach;	/* attachment file dialogue, if active */
	
	gboolean batch_unref;
	GPtrArray *attachments;
	char *path;
};


enum {
	CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


static void update (EAttachmentBar *bar);


static char *
size_to_string (gulong size)
{
	char *size_string;
	
	/* FIXME: The following should probably go into a separate module, as
           we might have to do the same thing in other places as well.  Also,
	   I am not sure this will be OK for all the languages.  */

	size_string = gnome_vfs_format_file_size_for_display (size);

	return size_string;
}

/* Attachment handling functions.  */

static void
attachment_destroy (EAttachmentBar *bar, EAttachment *attachment)
{
	if (bar->priv->batch_unref)
		return;
	
	if (g_ptr_array_remove (bar->priv->attachments, attachment)) {
		update (bar);
		g_signal_emit (bar, signals[CHANGED], 0);
	}
}

static void
attachment_changed_cb (EAttachment *attachment,
		       gpointer data)
{
	update (E_ATTACHMENT_BAR (data));
}

static void
add_common (EAttachmentBar *bar, EAttachment *attachment)
{
	g_return_if_fail (attachment != NULL);
	
	g_ptr_array_add (bar->priv->attachments, attachment);
	g_object_weak_ref ((GObject *) attachment, (GWeakNotify) attachment_destroy, bar);
	g_signal_connect (attachment, "changed", G_CALLBACK (attachment_changed_cb), bar);
	
	update (bar);
	
	g_signal_emit (bar, signals[CHANGED], 0);
}

static void
add_from_mime_part (EAttachmentBar *bar, CamelMimePart *part)
{
	add_common (bar, e_attachment_new_from_mime_part (part));
}

static void
add_from_file (EAttachmentBar *bar, const char *file_name, const char *disposition)
{
	EAttachment *attachment;
	CamelException ex;
	
	camel_exception_init (&ex);
	
	if ((attachment = e_attachment_new (file_name, disposition, &ex))) {
		add_common (bar, attachment);
	} else {
		/* FIXME: Avoid using error from mailer */
		e_error_run ((GtkWindow *) gtk_widget_get_toplevel ((GtkWidget *) bar), "mail-composer:no-attach",
			     file_name, camel_exception_get_description (&ex), NULL);
		camel_exception_clear (&ex);
	}
}


/* Icon list contents handling.  */

static void
calculate_height_width(EAttachmentBar *bar, int *new_width, int *new_height)
{
        int width, height, icon_width;
        PangoFontMetrics *metrics;
        PangoContext *context;
	
        context = gtk_widget_get_pango_context ((GtkWidget *) bar);
        metrics = pango_context_get_metrics (context, ((GtkWidget *) bar)->style->font_desc, pango_context_get_language (context));
        width = PANGO_PIXELS (pango_font_metrics_get_approximate_char_width (metrics)) * 15;
	/* This should be *2, but the icon list creates too much space above ... */
	height = PANGO_PIXELS (pango_font_metrics_get_ascent (metrics) + pango_font_metrics_get_descent (metrics)) * 3;
	pango_font_metrics_unref (metrics);
	icon_width = ICON_WIDTH + ICON_SPACING + ICON_BORDER + ICON_TEXT_SPACING;

	if (new_width)
		*new_width = MAX (icon_width, width);

	if (new_height)
		*new_height = ICON_WIDTH + ICON_SPACING + ICON_BORDER + ICON_TEXT_SPACING + height;

	return;
}

void
e_attachment_bar_create_attachment_cache (EAttachment *attachment)
{

	CamelContentType *content_type;

	if (!attachment->body)
		return;

	content_type = camel_mime_part_get_content_type (attachment->body);

	if (camel_content_type_is(content_type, "image", "*")) {
		CamelDataWrapper *wrapper;
		CamelStreamMem *mstream;
		GdkPixbufLoader *loader;
		gboolean error = TRUE;
		GdkPixbuf *pixbuf;
		
		wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (attachment->body));
		mstream = (CamelStreamMem *) camel_stream_mem_new ();
			
		camel_data_wrapper_decode_to_stream (wrapper, (CamelStream *) mstream);
			
		/* Stream image into pixbuf loader */
		loader = gdk_pixbuf_loader_new ();
		error = !gdk_pixbuf_loader_write (loader, mstream->buffer->data, mstream->buffer->len, NULL);
		gdk_pixbuf_loader_close (loader, NULL);
			
		if (!error) {
			int ratio, width, height;
			
			/* Shrink pixbuf */
			pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
			width = gdk_pixbuf_get_width (pixbuf);
			height = gdk_pixbuf_get_height (pixbuf);
			if (width >= height) {
				if (width > 48) {
					ratio = width / 48;
					width = 48;
					height = height / ratio;
					if (height == 0)
						height = 1;
				}
			} else {
				if (height > 48) {
					ratio = height / 48;
					height = 48;
					width = width / ratio;
					if (width == 0)
						width = 1;
				}
			}
			
			attachment->pixbuf_cache = gdk_pixbuf_scale_simple (pixbuf, width,height,GDK_INTERP_BILINEAR);
			pixbuf = attachment->pixbuf_cache;
			g_object_ref(pixbuf);
		} else {
			attachment->pixbuf_cache = NULL;
			g_warning ("GdkPixbufLoader Error");
		}
			
		/* Destroy everything */
		g_object_unref (loader);
		camel_object_unref (mstream);
	}
}

static void
update (EAttachmentBar *bar)
{
	struct _EAttachmentBarPrivate *priv;
	GnomeIconList *icon_list;
	int bar_width, bar_height;
	int i;
	
	priv = bar->priv;
	icon_list = GNOME_ICON_LIST (bar);
	
	gnome_icon_list_freeze (icon_list);
	
	gnome_icon_list_clear (icon_list);
	
	/* FIXME could be faster, but we don't care.  */
	for (i = 0; i < priv->attachments->len; i++) {
		EAttachment *attachment;
		CamelContentType *content_type;
		char *size_string, *label;
		GdkPixbuf *pixbuf = NULL;
		const char *desc;
		
		attachment = priv->attachments->pdata[i];
		
		if (!attachment->is_available_local || !attachment->body) {
			/* stock_attach would be better, but its fugly scaled up */
			if ((pixbuf = e_icon_factory_get_icon("stock_unknown", E_ICON_SIZE_DIALOG))) {
				attachment->index = gnome_icon_list_append_pixbuf (icon_list, pixbuf, NULL, "");
				g_object_unref (pixbuf);
			}
			continue;
		}
		
		content_type = camel_mime_part_get_content_type (attachment->body);
		/* Get the image out of the attachment 
		   and create a thumbnail for it */
		if ((pixbuf = attachment->pixbuf_cache)) {
			g_object_ref(pixbuf);
		} else if (camel_content_type_is(content_type, "image", "*")) {
			CamelDataWrapper *wrapper;
			CamelStreamMem *mstream;
			GdkPixbufLoader *loader;
			gboolean error = TRUE;
			
			wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (attachment->body));
			mstream = (CamelStreamMem *) camel_stream_mem_new ();
			
			camel_data_wrapper_decode_to_stream (wrapper, (CamelStream *) mstream);
			
			/* Stream image into pixbuf loader */
			loader = gdk_pixbuf_loader_new ();
			error = !gdk_pixbuf_loader_write (loader, mstream->buffer->data, mstream->buffer->len, NULL);
			gdk_pixbuf_loader_close (loader, NULL);
			
			if (!error) {
				int ratio, width, height;
				
				/* Shrink pixbuf */
				pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
				width = gdk_pixbuf_get_width (pixbuf);
				height = gdk_pixbuf_get_height (pixbuf);
				if (width >= height) {
					if (width > 48) {
						ratio = width / 48;
						width = 48;
						height = height / ratio;
						if (height == 0)
							height = 1;
					}
				} else {
					if (height > 48) {
						ratio = height / 48;
						height = 48;
						width = width / ratio;
						if (width == 0)
							width = 1;
					}
				}
				
				attachment->pixbuf_cache = gdk_pixbuf_scale_simple (pixbuf, width, height,
										    GDK_INTERP_BILINEAR);
				pixbuf = attachment->pixbuf_cache;
				g_object_ref (pixbuf);
			} else {
				pixbuf = NULL;
				g_warning ("GdkPixbufLoader Error");
			}
			
			/* Destroy everything */
			g_object_unref (loader);
			camel_object_unref (mstream);
		}
		
		desc = camel_mime_part_get_description (attachment->body);
		if (!desc || *desc == '\0') {
			if (attachment->file_name)
				desc = attachment->file_name;
			else 
				desc = camel_mime_part_get_filename (attachment->body);
		}
		
		if (!desc)
			desc = _("attachment");
		
		if (attachment->size && (size_string = size_to_string (attachment->size))) {
			label = g_strdup_printf ("%s (%s)", desc, size_string);
			g_free (size_string);
		} else
			label = g_strdup (desc);
		
		if (pixbuf == NULL) {
			char *mime_type;
			
			mime_type = camel_content_type_simple (content_type);
			pixbuf = e_icon_for_mime_type (mime_type, 48);
			if (pixbuf == NULL) {
				g_warning("cannot find icon for mime type %s (installation problem?)", mime_type);
				/* stock_attach would be better, but its fugly scaled up */
				pixbuf = e_icon_factory_get_icon("stock_unknown", E_ICON_SIZE_DIALOG);
			}
			g_free (mime_type);
		}
		
		if (pixbuf) {
			GdkPixbuf *pixbuf_orig = pixbuf;
			pixbuf = gdk_pixbuf_add_alpha (pixbuf_orig, TRUE, 255, 255, 255);
			
			/* gdk_pixbuf_add_alpha returns a newly allocated pixbuf,
			   free the original one. 
			*/
			g_object_unref (pixbuf_orig);
			
			/* In case of a attachment bar, in a signed/encrypted part, display the status as a emblem*/
			if (attachment->sign) {
				/* Show the signature status at the right-bottom.*/
				GdkPixbuf *sign = NULL;
				int x, y;
				
				if (attachment->sign == CAMEL_CIPHER_VALIDITY_SIGN_BAD)
					sign = e_icon_factory_get_icon ("stock_signature-bad", E_ICON_SIZE_MENU);
				else if (attachment->sign == CAMEL_CIPHER_VALIDITY_SIGN_GOOD)
					sign = e_icon_factory_get_icon ("stock_signature-ok", E_ICON_SIZE_MENU);
				else
					sign = e_icon_factory_get_icon ("stock_signature", E_ICON_SIZE_MENU);
				
				x = gdk_pixbuf_get_width (pixbuf) - 17;
				y = gdk_pixbuf_get_height (pixbuf) - 17;
				
				gdk_pixbuf_copy_area (sign, 0, 0, 16, 16, pixbuf, x, y);
				g_object_unref (sign);
			}
			
			if (attachment->encrypt) {
				/* Show the encryption status at the top left.*/
				GdkPixbuf *encrypt = e_icon_factory_get_icon ("stock_lock-ok", E_ICON_SIZE_MENU);
				
				gdk_pixbuf_copy_area (encrypt, 0, 0, 16, 16, pixbuf, 1, 1);
				g_object_unref (encrypt);
			}
			
			gnome_icon_list_append_pixbuf (icon_list, pixbuf, NULL, label);
			g_object_unref (pixbuf);
		}
		
		g_free (label);
	}
	
	gnome_icon_list_thaw (icon_list);
	
	/* Resize */
	if (bar->expand) {
		gtk_widget_get_size_request ((GtkWidget *) bar, &bar_width, &bar_height);
		
		if (bar->priv->attachments->len) {
			int per_col, rows, height, width;
			
			calculate_height_width(bar, &width, &height);
			per_col = bar_width / width;
			per_col = (per_col ? per_col : 1);
			rows = (bar->priv->attachments->len + per_col -1) / per_col;
			gtk_widget_set_size_request ((GtkWidget *) bar, bar_width, rows * height);
		}
	}
}

static void
update_remote_file (EAttachment *attachment, EAttachmentBar *bar)
{
	GnomeIconList *icon_list;
	GnomeIconTextItem *item;
	char *msg, *base;

	if (attachment->percentage == -1) {
		update (bar);
		return;
	}
	
	base = g_path_get_basename(attachment->file_name);
	msg = g_strdup_printf("%s (%d%%)", base, attachment->percentage);
	g_free(base);

	icon_list = GNOME_ICON_LIST (bar);
	
	gnome_icon_list_freeze (icon_list);

	item = gnome_icon_list_get_icon_text_item (icon_list, attachment->index);
	if (!item->is_text_allocated)
		g_free (item->text);
		
	gnome_icon_text_item_configure (item, item->x, item->y, item->width, item->fontname, msg, item->is_editable, TRUE);

	gnome_icon_list_thaw (icon_list);
}

void
e_attachment_bar_remove_selected (EAttachmentBar *bar)
{
	struct _EAttachmentBarPrivate *priv;
	EAttachment *attachment;
	int id, left, nrem = 0;
	GList *items;
	GPtrArray *temp_arr;

	g_return_if_fail (E_IS_ATTACHMENT_BAR (bar));
	
	priv = bar->priv;
	
	if (!(items = gnome_icon_list_get_selection ((GnomeIconList *) bar)))
		return;
	
	temp_arr = g_ptr_array_new ();
	while (items != NULL) {
		if ((id = GPOINTER_TO_INT (items->data) - nrem) < priv->attachments->len) {
			attachment = E_ATTACHMENT(g_ptr_array_index (priv->attachments, id));
			g_ptr_array_add (temp_arr, (gpointer)attachment);
			g_ptr_array_remove_index (priv->attachments, id);
			nrem++;
		}
		
		items = items->next;
	}

	g_ptr_array_foreach (temp_arr, (GFunc)g_object_unref, NULL);
	g_ptr_array_free (temp_arr, TRUE);
	
	update (bar);

	g_signal_emit (bar, signals[CHANGED], 0);
	
	id++;
	
	if ((left = gnome_icon_list_get_num_icons ((GnomeIconList *) bar)) > 0)
		gnome_icon_list_focus_icon ((GnomeIconList *) bar, left > id ? id : left - 1);
}

void
e_attachment_bar_set_width(EAttachmentBar *bar, int bar_width)
{
	int per_col, rows, height, width;

	calculate_height_width(bar, &width, &height);
	per_col = bar_width / width;
	per_col = (per_col ? per_col : 1);
	rows = (bar->priv->attachments->len + per_col - 1) / per_col;
	gtk_widget_set_size_request ((GtkWidget *)bar, bar_width, rows * height);
}

void
e_attachment_bar_edit_selected (EAttachmentBar *bar)
{
	struct _EAttachmentBarPrivate *priv;
	EAttachment *attachment;
	GList *items;
	int id;
	
	g_return_if_fail (E_IS_ATTACHMENT_BAR (bar));
	
	priv = bar->priv;
	
	items = gnome_icon_list_get_selection ((GnomeIconList *) bar);
	while (items != NULL) {
		if ((id = GPOINTER_TO_INT (items->data)) < priv->attachments->len) {
			attachment = priv->attachments->pdata[id];
			e_attachment_edit (attachment, GTK_WIDGET (bar));
		}
		
		items = items->next;
	}
}

GtkWidget **
e_attachment_bar_get_selector(EAttachmentBar *bar)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_BAR (bar), NULL);
	
	return &bar->priv->attach;
}


/**
 * e_attachment_bar_get_selected:
 * @bar: an #EAttachmentBar object
 *
 * Returns a newly allocated #GSList of ref'd #EAttachment objects
 * representing the selected items in the #EAttachmentBar Icon List.
 **/
GSList *
e_attachment_bar_get_selected (EAttachmentBar *bar)
{
	struct _EAttachmentBarPrivate *priv;
	GSList *attachments = NULL;
	EAttachment *attachment;
	GList *items;
	int id;
	
	g_return_val_if_fail (E_IS_ATTACHMENT_BAR (bar), NULL);
	
	priv = bar->priv;
	
	items = gnome_icon_list_get_selection ((GnomeIconList *) bar);
	
	while (items != NULL) {
		if ((id = GPOINTER_TO_INT (items->data)) < priv->attachments->len) {
			attachment = priv->attachments->pdata[id];
			attachments = g_slist_prepend (attachments, attachment);
			g_object_ref (attachment);
		}
		
		items = items->next;
	}
	
	attachments = g_slist_reverse (attachments);
	
	return attachments;
}

/* FIXME: Cleanup this, since there is a api to get selected attachments */
/**
 * e_attachment_bar_get_attachment:
 * @bar: an #EAttachmentBar object
 * @id: Index of the desired attachment or -1 to request all selected attachments
 *
 * Returns a newly allocated #GSList of ref'd #EAttachment objects
 * representing the requested item(s) in the #EAttachmentBar Icon
 * List.
 **/
GSList *
e_attachment_bar_get_attachment (EAttachmentBar *bar, int id)
{
	struct _EAttachmentBarPrivate *priv;
	EAttachment *attachment;
	GSList *attachments;
	
	g_return_val_if_fail (E_IS_ATTACHMENT_BAR (bar), NULL);
	
	priv = bar->priv;
	
	if (id == -1 || id > priv->attachments->len)
		return e_attachment_bar_get_selected (bar);
	
	attachment = priv->attachments->pdata[id];
	attachments = g_slist_prepend (NULL, attachment);
	g_object_ref (attachment);
	
	return attachments;
}


/**
 * e_attachment_bar_get_all_attachments:
 * @bar: an #EAttachmentBar object
 *
 * Returns a newly allocated #GSList of ref'd #EAttachment objects.
 **/
GSList *
e_attachment_bar_get_all_attachments (EAttachmentBar *bar)
{
	struct _EAttachmentBarPrivate *priv;
	GSList *attachments = NULL;
	EAttachment *attachment;
	int i;
	
	g_return_val_if_fail (E_IS_ATTACHMENT_BAR (bar), NULL);
	
	priv = bar->priv;
	
	for (i = priv->attachments->len - 1; i >= 0; i--) {
		attachment = priv->attachments->pdata[i];
		if (attachment->is_available_local) {
			attachments = g_slist_prepend (attachments, attachment);
			g_object_ref (attachment);
		}
	}
	
	return attachments;
}

/* Just the GSList has to be freed by the caller */
GSList *
e_attachment_bar_get_parts (EAttachmentBar *bar)
{
	struct _EAttachmentBarPrivate *priv;
	EAttachment *attachment;
	GSList *parts = NULL;
	int i;
	
	g_return_val_if_fail (E_IS_ATTACHMENT_BAR (bar), NULL);
	
	priv = bar->priv;
	
	for (i = 0; i < priv->attachments->len; i++) {
		attachment = priv->attachments->pdata[i];
		if (attachment->is_available_local)
			parts = g_slist_prepend (parts, attachment->body);
	}
	
        return parts;
}

/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	EAttachmentBar *bar = (EAttachmentBar *) object;
	struct _EAttachmentBarPrivate *priv = bar->priv;
	EAttachment *attachment;
	int i;
	
	if ((priv = bar->priv)) {
		priv->batch_unref = TRUE;
		for (i = 0; i < priv->attachments->len; i++) {
			attachment = priv->attachments->pdata[i];
			g_object_weak_unref ((GObject *) attachment, (GWeakNotify) attachment_destroy, bar);
			g_object_unref (attachment);
		}
		g_ptr_array_free (priv->attachments, TRUE);
		
		if (priv->attach)
			gtk_widget_destroy (priv->attach);
		
		if (priv->path)
			g_free (priv->path);
		
		g_free (priv);
		bar->priv = NULL;
	}
	
	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static char *
temp_save_part (CamelMimePart *part, gboolean readonly)
{
	const char *filename;
	char *tmpdir, *path, *mfilename = NULL, *utf8_mfilename = NULL;
	CamelStream *stream;
	CamelDataWrapper *wrapper;
	
	if (!(tmpdir = e_mkdtemp ("evolution-tmp-XXXXXX")))
		return NULL;
	
	if (!(filename = camel_mime_part_get_filename (part))) {
		/* This is the default filename used for temporary file creation */
		filename = _("Unknown");
	} else {
		utf8_mfilename = g_strdup (filename);
		e_filename_make_safe (utf8_mfilename);
		mfilename = g_filename_from_utf8 ((const char *) utf8_mfilename, -1, NULL, NULL, NULL);
		g_free (utf8_mfilename);
		filename = (const char *) mfilename;
	}
	
	path = g_build_filename (tmpdir, filename, NULL);
	g_free (tmpdir);
	g_free (mfilename);
	
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (part));
	if (readonly)
		stream = camel_stream_fs_new_with_name (path, O_RDWR|O_CREAT|O_TRUNC, 0444);
	else 
		stream = camel_stream_fs_new_with_name (path, O_RDWR|O_CREAT|O_TRUNC, 0644);
	
	if (!stream) {
		/* TODO handle error conditions */
		g_message ("DEBUG: could not open the file to write\n");
		g_free (path);
		return NULL;
	}

	if (camel_data_wrapper_decode_to_stream (wrapper, (CamelStream *) stream) == -1) {
		g_free (path);
		camel_stream_close (stream);
		camel_object_unref (stream);
		g_message ("DEBUG: could not write to file\n");
		return NULL;
	}

	camel_stream_close(stream);
	camel_object_unref(stream);

	return path;
}

static void
eab_drag_data_get(EAttachmentBar *bar, GdkDragContext *drag, GtkSelectionData *data, guint info, guint time)
{
	struct _EAttachmentBarPrivate *priv = bar->priv;
	EAttachment *attachment;
	char *path, **uris;
	int len, n, i = 0;
	CamelURL *url;
	GList *items;
	
	if (info)
		return;
	
	items = gnome_icon_list_get_selection (GNOME_ICON_LIST (bar));
	len = g_list_length (items);
	
	uris = g_malloc0 (sizeof (char *) * (len + 1));
	
	for ( ; items != NULL; items = items->next) {
		if (!((n = GPOINTER_TO_INT (items->data)) < priv->attachments->len))
			continue;
		
		attachment = priv->attachments->pdata[n];
		
		if (!attachment->is_available_local)
			continue;
		
		if (attachment->store_uri) {
			uris[i++] = attachment->store_uri;
			continue;
		}
		
		/* If we are not able to save, ignore it */
		if (!(path = temp_save_part (attachment->body, FALSE)))
			continue;
		
		url = camel_url_new ("file://", NULL);
		camel_url_set_path (url, path);
		attachment->store_uri = camel_url_to_string (url, 0);
		camel_url_free (url);
		g_free (path);
		
		uris[i++] = attachment->store_uri;
	}
	
	uris[i] = NULL;
	
	gtk_selection_data_set_uris (data, uris);
	
	g_free (uris);
	
	return;
}

static gboolean
eab_button_release_event(EAttachmentBar *bar, GdkEventButton *event, gpointer dummy)
{
	GnomeIconList *icon_list = GNOME_ICON_LIST(bar);
	GList *selected;
	int length;
	GtkTargetEntry drag_types[] = {
		{ "text/uri-list", 0, 0 },
	};	

	if (event && event->button == 1) {
		selected = gnome_icon_list_get_selection(icon_list);
		length = g_list_length (selected);
		if (length)
			gtk_drag_source_set((GtkWidget *)bar, GDK_BUTTON1_MASK, drag_types, G_N_ELEMENTS(drag_types), GDK_ACTION_COPY);
		else
			gtk_drag_source_unset((GtkWidget *)bar);
	}

	return FALSE;
}

static gboolean
eab_button_press_event(EAttachmentBar *bar, GdkEventButton *event, gpointer dummy)
{
	GnomeIconList *icon_list = GNOME_ICON_LIST(bar);
	GList *selected = NULL, *tmp;
	int length, icon_number;
	gboolean take_selected = FALSE;
	GtkTargetEntry drag_types[] = {
		{ "text/uri-list", 0, 0 },
	};	

	selected = gnome_icon_list_get_selection(icon_list);
	length = g_list_length (selected);

	if (event) {
		icon_number = gnome_icon_list_get_icon_at(icon_list, event->x, event->y);
		if (icon_number < 0) { 
			/* When nothing is selected, deselect all */
			gnome_icon_list_unselect_all (icon_list);
			length = 0;
			selected = NULL;
		}
		
		if (event->button == 1) {
			/* If something is selected, then allow drag or else help to select */
			if (length)
				gtk_drag_source_set((GtkWidget *)bar, GDK_BUTTON1_MASK, drag_types, G_N_ELEMENTS(drag_types), GDK_ACTION_COPY);
			else
				gtk_drag_source_unset((GtkWidget *)bar);
			return FALSE;
		}
		
		/* If not r-click dont progress any more.*/
		if (event->button != 3)
			return FALSE;
		
		/* When a r-click on something, if it is in the already selected list, consider a r-click of multiple things
		 * or deselect all and select only this for r-click 
		 */
		if (icon_number >= 0) {
			for (tmp = selected; tmp; tmp = tmp->next) {
				if (GPOINTER_TO_INT(tmp->data) == icon_number)
					take_selected = TRUE;
			}
			
			if (!take_selected) {
				gnome_icon_list_unselect_all(icon_list);
				gnome_icon_list_select_icon(icon_list, icon_number);
			}
		}
	}
	
	return FALSE;
}

static gboolean
eab_icon_clicked_cb (EAttachmentBar *bar, GdkEvent *event, gpointer *dummy)
{
	EAttachment *attachment;
	GError *error = NULL;
	gboolean ret = FALSE;
	CamelURL *url;
	char *path;
	GSList *p;
	
	if (E_IS_ATTACHMENT_BAR (bar) && event->type == GDK_2BUTTON_PRESS) {
		p = e_attachment_bar_get_selected (bar);
		if (p && p->next == NULL) {
			attachment = p->data;
			
			/* Check if the file is stored already */
			if (!attachment->store_uri) {
				path = temp_save_part (attachment->body, TRUE);				
				url = camel_url_new ("file://", NULL);
				camel_url_set_path (url, path);
				attachment->store_uri = camel_url_to_string (url, 0);
				camel_url_free (url);
				g_free (path);
			}

			/* launch the url now */
			gnome_url_show (attachment->store_uri, &error);
			if (error) {
				g_message ("DEBUG: Launch failed: %s\n", error->message);
				g_error_free (error);
				error = NULL;
			}
			
			ret = TRUE;
		}
		
		if (p) {
			g_slist_foreach (p, (GFunc) g_object_unref, NULL);
			g_slist_free (p);
		}
	}

	return ret;
}

/* Initialization.  */

static void
class_init (EAttachmentBarClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (gnome_icon_list_get_type ());
	
	object_class->destroy = destroy;
	
	/* Setup signals.  */
	
	signals[CHANGED] =
		g_signal_new ("changed",
			      E_TYPE_ATTACHMENT_BAR,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAttachmentBarClass, changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

static void
init (EAttachmentBar *bar)
{
	struct _EAttachmentBarPrivate *priv;
	
	priv = g_new (struct _EAttachmentBarPrivate, 1);
	
	priv->attach = NULL;
	priv->batch_unref = FALSE;
	priv->attachments = g_ptr_array_new ();
	priv->path = NULL;
	
	bar->priv = priv;
	bar->expand = FALSE;
}


GType
e_attachment_bar_get_type (void)
{
	static GType type = 0;
	
	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (EAttachmentBarClass),
			NULL, NULL,
			(GClassInitFunc) class_init,
			NULL, NULL,
			sizeof (EAttachmentBar),
			0,
			(GInstanceInitFunc) init,
		};
		
		type = g_type_register_static (GNOME_TYPE_ICON_LIST, "EAttachmentBar", &info, 0);
	}
	
	return type;
}

GtkWidget *
e_attachment_bar_new (GtkAdjustment *adj)
{
	EAttachmentBar *new;
	GnomeIconList *icon_list;
	int icon_width, window_height;
	
	new = g_object_new (e_attachment_bar_get_type (), NULL);
	
	icon_list = GNOME_ICON_LIST (new);
	
	calculate_height_width (new, &icon_width, &window_height);
	
	gnome_icon_list_construct (icon_list, icon_width, adj, 0);
	
	gtk_widget_set_size_request (GTK_WIDGET (new), icon_width * 4, window_height);

        GTK_WIDGET_SET_FLAGS (new, GTK_CAN_FOCUS);
	
	gnome_icon_list_set_separators (icon_list, ICON_SEPARATORS);
	gnome_icon_list_set_row_spacing (icon_list, ICON_ROW_SPACING);
	gnome_icon_list_set_col_spacing (icon_list, ICON_COL_SPACING);
	gnome_icon_list_set_icon_border (icon_list, ICON_BORDER);
	gnome_icon_list_set_text_spacing (icon_list, ICON_TEXT_SPACING);
	gnome_icon_list_set_selection_mode (icon_list, GTK_SELECTION_MULTIPLE);

	atk_object_set_name (gtk_widget_get_accessible (GTK_WIDGET (new)), 
			     _("Attachment Bar"));
	
	g_signal_connect (new, "button_release_event", G_CALLBACK(eab_button_release_event), NULL);
	g_signal_connect (new, "button_press_event", G_CALLBACK(eab_button_press_event), NULL);
	g_signal_connect (new, "drag-data-get", G_CALLBACK(eab_drag_data_get), NULL);
	g_signal_connect (icon_list, "event", G_CALLBACK (eab_icon_clicked_cb), NULL);
	
	return GTK_WIDGET (new);
}

static char *
get_default_charset (void)
{
	GConfClient *gconf;
	const char *locale;
	char *charset;
	
	gconf = gconf_client_get_default ();
	charset = gconf_client_get_string (gconf, "/apps/evolution/mail/composer/charset", NULL);
	
	if (!charset || charset[0] == '\0') {
		g_free (charset);
		charset = gconf_client_get_string (gconf, "/apps/evolution/mail/format/charset", NULL);
		if (charset && charset[0] == '\0') {
			g_free (charset);
			charset = NULL;
		}
	}
	
	g_object_unref (gconf);
	
	if (!charset && (locale = e_iconv_locale_charset ()))
		charset = g_strdup (locale);
	
	return charset ? charset : g_strdup ("us-ascii");
}

static void
attach_to_multipart (CamelMultipart *multipart,
		     EAttachment *attachment,
		     const char *default_charset)
{
	CamelContentType *content_type;
	CamelDataWrapper *content;
	
	if (!attachment->body)
		return;

	content_type = camel_mime_part_get_content_type (attachment->body);
	content = camel_medium_get_content_object (CAMEL_MEDIUM (attachment->body));
	
	if (!CAMEL_IS_MULTIPART (content)) {
		if (camel_content_type_is (content_type, "text", "*")) {
			CamelTransferEncoding encoding;
			CamelStreamFilter *filter_stream;
			CamelMimeFilterBestenc *bestenc;
			CamelStream *stream;
			const char *charset;
			char *buf = NULL;
			char *type;
			
			charset = camel_content_type_param (content_type, "charset");
			
			stream = camel_stream_null_new ();
			filter_stream = camel_stream_filter_new_with_stream (stream);
			bestenc = camel_mime_filter_bestenc_new (CAMEL_BESTENC_GET_ENCODING);
			camel_stream_filter_add (filter_stream, CAMEL_MIME_FILTER (bestenc));
			camel_object_unref (stream);
			
			camel_data_wrapper_decode_to_stream (content, CAMEL_STREAM (filter_stream));
			camel_object_unref (filter_stream);
			
			encoding = camel_mime_filter_bestenc_get_best_encoding (bestenc, CAMEL_BESTENC_8BIT);
			camel_mime_part_set_encoding (attachment->body, encoding);
			
			if (encoding == CAMEL_TRANSFER_ENCODING_7BIT) {
				/* the text fits within us-ascii so this is safe */
				/* FIXME: check that this isn't iso-2022-jp? */
				default_charset = "us-ascii";
			} else if (!charset) {
				if (!default_charset)
					default_charset = buf = get_default_charset ();
				
				/* FIXME: We should really check that this fits within the
                                   default_charset and if not find one that does and/or
				   allow the user to specify? */
			}
			
			if (!charset) {
				/* looks kinda nasty, but this is how ya have to do it */
				camel_content_type_set_param (content_type, "charset", default_charset);
				type = camel_content_type_format (content_type);
				camel_mime_part_set_content_type (attachment->body, type);
				g_free (type);
				g_free (buf);
			}
			
			camel_object_unref (bestenc);
		} else if (!CAMEL_IS_MIME_MESSAGE (content)) {
			camel_mime_part_set_encoding (attachment->body, CAMEL_TRANSFER_ENCODING_BASE64);
		}
	}
	
	camel_multipart_add_part (multipart, attachment->body);
}

void
e_attachment_bar_to_multipart (EAttachmentBar *bar, CamelMultipart *multipart, const char *default_charset)
{
	struct _EAttachmentBarPrivate *priv;
	EAttachment *attachment;
	int i;
	
	g_return_if_fail (E_IS_ATTACHMENT_BAR (bar));
	g_return_if_fail (CAMEL_IS_MULTIPART (multipart));
	
	priv = bar->priv;
	
	for (i = 0; i < priv->attachments->len; i++) {
		attachment = priv->attachments->pdata[i];
		if (attachment->is_available_local)
			attach_to_multipart (multipart, attachment, default_charset);
	}
}


guint
e_attachment_bar_get_num_attachments (EAttachmentBar *bar)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_BAR (bar), 0);
	
	return bar->priv->attachments->len;
}


void
e_attachment_bar_attach (EAttachmentBar *bar, const char *file_name, const char *disposition)
{
	g_return_if_fail (E_IS_ATTACHMENT_BAR (bar));
	g_return_if_fail (file_name != NULL && disposition != NULL);
	
	add_from_file (bar, file_name, disposition);
}

void
e_attachment_bar_add_attachment (EAttachmentBar *bar, EAttachment *attachment)
{
	g_return_if_fail (E_IS_ATTACHMENT_BAR (bar));
	
	add_common (bar, attachment);
}

int 
e_attachment_bar_get_download_count (EAttachmentBar *bar)
{
	struct _EAttachmentBarPrivate *priv;
	EAttachment *attachment;
	int i, n = 0;
	
	g_return_val_if_fail (E_IS_ATTACHMENT_BAR (bar), 0);
	
	priv = bar->priv;
	
	for (i = 0; i < priv->attachments->len; i++) {
		attachment = priv->attachments->pdata[i];
		if (!attachment->is_available_local)
			n++;
	}
	
	return n;
}

void
e_attachment_bar_attach_remote_file (EAttachmentBar *bar, const char *url, const char *disposition)
{
	EAttachment *attachment;
	CamelException ex;
	
	g_return_if_fail (E_IS_ATTACHMENT_BAR (bar));
	
	if (!bar->priv->path)
		bar->priv->path = e_mkdtemp ("attach-XXXXXX");
	
	camel_exception_init (&ex);
	if ((attachment = e_attachment_new_remote_file (url, disposition, bar->priv->path, &ex))) {
		add_common (bar, attachment);
		g_signal_connect (attachment, "update", G_CALLBACK (update_remote_file), bar);
	} else {
		e_error_run ((GtkWindow *) gtk_widget_get_toplevel ((GtkWidget *) bar), "mail-composer:no-attach",
			     attachment->file_name, camel_exception_get_description (&ex), NULL);
		camel_exception_clear (&ex);
	}
}

void
e_attachment_bar_attach_mime_part (EAttachmentBar *bar, CamelMimePart *part)
{
	g_return_if_fail (E_IS_ATTACHMENT_BAR (bar));
	
	add_from_mime_part (bar, part);
}
