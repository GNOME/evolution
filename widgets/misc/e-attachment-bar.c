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
#include <libgnome/gnome-i18n.h>

#include "e-attachment.h"
#include "e-attachment-bar.h"

#include <libedataserver/e-iconv.h>

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

	GList *attachments;
	guint num_attachments;
	gchar *path;
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
	
	if (size < 1e3L) {
		size_string = NULL;
	} else {
		gdouble displayed_size;
		
		if (size < 1e6L) {
			displayed_size = (gdouble) size / 1.0e3;
			size_string = g_strdup_printf (_("%.0fK"), displayed_size);
		} else if (size < 1e9L) {
			displayed_size = (gdouble) size / 1.0e6;
			size_string = g_strdup_printf (_("%.0fM"), displayed_size);
		} else {
			displayed_size = (gdouble) size / 1.0e9;
			size_string = g_strdup_printf (_("%.0fG"), displayed_size);
		}
	}
	
	return size_string;
}

/* Attachment handling functions.  */

static void
free_attachment_list (EAttachmentBar *bar)
{
	EAttachmentBarPrivate *priv;
	GList *p;
	
	priv = bar->priv;
	
	for (p = priv->attachments; p != NULL; p = p->next)
		g_object_unref (p->data);
	
	priv->attachments = NULL;
}

static void
attachment_changed_cb (EAttachment *attachment,
		       gpointer data)
{
	update (E_ATTACHMENT_BAR (data));
}

static void
add_common (EAttachmentBar *bar,
	    EAttachment *attachment)
{
	g_return_if_fail (attachment != NULL);
	
	g_signal_connect (attachment, "changed",
			  G_CALLBACK (attachment_changed_cb),
			  bar);
	
	bar->priv->attachments = g_list_append (bar->priv->attachments,
						attachment);
	bar->priv->num_attachments++;
	
	update (bar);
	
	g_signal_emit (bar, signals[CHANGED], 0);
}

static void
add_from_mime_part (EAttachmentBar *bar,
		    CamelMimePart *part)
{
	add_common (bar, e_attachment_new_from_mime_part (part));
}

static void
add_from_file (EAttachmentBar *bar,
	       const char *file_name,
	       const char *disposition)
{
	EAttachment *attachment;
	CamelException ex;
	
	camel_exception_init (&ex);
	attachment = e_attachment_new (file_name, disposition, &ex);
	if (attachment) {
		add_common (bar, attachment);
	} else {
		/* FIXME: Avoid using error from mailer */
		e_error_run((GtkWindow *)gtk_widget_get_toplevel((GtkWidget *)bar), "mail-composer:no-attach",
			    file_name, camel_exception_get_description(&ex), NULL);
		camel_exception_clear (&ex);
	}
}

static void
remove_attachment (EAttachmentBar *bar,
		   EAttachment *attachment)
{
	g_return_if_fail (E_IS_ATTACHMENT_BAR (bar));
	g_return_if_fail (g_list_find (bar->priv->attachments, attachment) != NULL);

	bar->priv->attachments = g_list_remove (bar->priv->attachments,
						attachment);
	bar->priv->num_attachments--;
	if (attachment->editor_gui != NULL) {
		GtkWidget *dialog = glade_xml_get_widget (attachment->editor_gui, "dialog");
		g_signal_emit_by_name (dialog, "response", GTK_RESPONSE_CLOSE);
	}
	
	g_object_unref(attachment);
	
	g_signal_emit (bar, signals[CHANGED], 0);
}


/* Icon list contents handling.  */

static void
calculate_height_width(EAttachmentBar *bar, int *new_width, int *new_height)
{
        int width, height, icon_width;
        PangoFontMetrics *metrics;
        PangoContext *context;
        GnomeIconList *icon_list;

	icon_list = GNOME_ICON_LIST (bar);
			
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

static void
update (EAttachmentBar *bar)
{
	EAttachmentBarPrivate *priv;
	GnomeIconList *icon_list;
	GList *p;
	int bar_width, bar_height;
	
	priv = bar->priv;
	icon_list = GNOME_ICON_LIST (bar);
	
	gnome_icon_list_freeze (icon_list);
	
	gnome_icon_list_clear (icon_list);
	
	/* FIXME could be faster, but we don't care.  */
	for (p = priv->attachments; p != NULL; p = p->next) {
		EAttachment *attachment;
		CamelContentType *content_type;
		char *size_string, *label;
		GdkPixbuf *pixbuf=NULL;
		const char *desc;
		
		attachment = p->data;

		if (!attachment->is_available_local) {
			/* stock_attach would be better, but its fugly scaled up */
			pixbuf = e_icon_factory_get_icon("stock_unknown", E_ICON_SIZE_DIALOG);
			if (pixbuf) {
				attachment->index = gnome_icon_list_append_pixbuf (icon_list, pixbuf, NULL, "");
				g_object_unref(pixbuf);
			}
			continue;
		}
		content_type = camel_mime_part_get_content_type (attachment->body);
		/* Get the image out of the attachment 
		   and create a thumbnail for it */
		pixbuf = attachment->pixbuf_cache;
		if (pixbuf) {
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
					}
				} else {
					if (height > 48) {
						ratio = height / 48;
						height = 48;
						width = width / ratio;
					}
				}
				
				attachment->pixbuf_cache = gdk_pixbuf_scale_simple 
					(pixbuf,
					 width,
					 height,
					 GDK_INTERP_BILINEAR);
				pixbuf = attachment->pixbuf_cache;
				g_object_ref(pixbuf);
			} else {
				pixbuf = NULL;
				g_warning ("GdkPixbufLoader Error");
			}
			
			/* Destroy everything */
			g_object_unref (loader);
			camel_object_unref (mstream);
		}
		
		desc = camel_mime_part_get_description (attachment->body);
		if (!desc || *desc == '\0')
			desc = camel_mime_part_get_filename (attachment->body);
		
		if (!desc)
			desc = _("attachment");
		
		if (attachment->size
		    && (size_string = size_to_string (attachment->size))) {
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
			pixbuf = gdk_pixbuf_add_alpha (pixbuf, TRUE, 255, 255, 255);

			/* In case of a attachment bar, in a signed/encrypted part, display the status as a emblem*/
			if (attachment->sign) {
				/* Show the signature status at the right-bottom.*/
				GdkPixbuf *sign = NULL;
				int x,y;

				if (attachment->sign == CAMEL_CIPHER_VALIDITY_SIGN_BAD)
					sign = e_icon_factory_get_icon("stock_signature-bad", E_ICON_SIZE_MENU);
				else if (attachment->sign == CAMEL_CIPHER_VALIDITY_SIGN_GOOD)
					sign = e_icon_factory_get_icon("stock_signature-ok", E_ICON_SIZE_MENU);
				else
					sign = e_icon_factory_get_icon("stock_signature", E_ICON_SIZE_MENU);

				x = gdk_pixbuf_get_width(pixbuf) - 17;
				y = gdk_pixbuf_get_height(pixbuf) - 17;
				
				gdk_pixbuf_copy_area(sign, 0, 0, 16, 16, pixbuf, x, y);
				g_object_unref (sign);
			}

			if (attachment->encrypt) {
				/* Show the encryption status at the top left.*/
				GdkPixbuf *encrypt = e_icon_factory_get_icon("stock_lock-ok", E_ICON_SIZE_MENU);
				
				gdk_pixbuf_copy_area(encrypt, 0, 0, 16, 16, pixbuf, 1, 1);
				g_object_unref (encrypt);
			}

			gnome_icon_list_append_pixbuf (icon_list, pixbuf, NULL, label);
			g_object_unref(pixbuf);
		}
		
		g_free (label);
	}
	
	gnome_icon_list_thaw (icon_list);

	/* Resize */
	if (bar->expand) {
		gtk_widget_get_size_request ((GtkWidget *)bar, &bar_width, &bar_height);
	
		if (bar->priv->num_attachments) {
			int per_col, rows, height, width;
	
			calculate_height_width(bar, &width, &height);
			per_col = bar_width / width;
			rows = bar->priv->num_attachments / per_col;
			gtk_widget_set_size_request ((GtkWidget *)bar, bar_width, (rows+1) * height);
		}
	}
}

static void
update_remote_file (EAttachment *attachment, EAttachmentBar *bar)
{
	EAttachmentBarPrivate *priv;
	GnomeIconList *icon_list;
	GnomeIconTextItem *item;
	char *msg, *base;
	priv = bar->priv;

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
	GnomeIconList *icon_list;
	EAttachment *attachment;
	GList *attachment_list, *p;
	int num = 0, left, dlen;

	g_return_if_fail (bar != NULL);
	g_return_if_fail (E_IS_ATTACHMENT_BAR (bar));
	
	icon_list = GNOME_ICON_LIST (bar);
	
	/* Weee!  I am especially proud of this piece of cheesy code: it is
           truly awful.  But unless one attaches a huge number of files, it
           will not be as greedy as intended.  FIXME of course.  */
	
	attachment_list = NULL;
	p = gnome_icon_list_get_selection (icon_list);
	dlen = g_list_length (p);
	for ( ; p != NULL; p = p->next) {
		num = GPOINTER_TO_INT (p->data);
		attachment = E_ATTACHMENT (g_list_nth_data (bar->priv->attachments, num));

		/* We need to check if there are duplicated index in the return list of 
		   gnome_icon_list_get_selection() because of gnome bugzilla bug #122356.
		   FIXME in the future. */

		if (g_list_find (attachment_list, attachment) == NULL) {
			attachment_list = g_list_prepend (attachment_list, attachment);
		}
	}
	
	for (p = attachment_list; p != NULL; p = p->next)
		remove_attachment (bar, E_ATTACHMENT (p->data));
	
	g_list_free (attachment_list);
	
	update (bar);
	
	left = gnome_icon_list_get_num_icons (icon_list);
	num = num - dlen + 1;
	if (left > 0)
		gnome_icon_list_focus_icon (icon_list, left > num ? num : left - 1);
}

void
e_attachment_bar_refresh (EAttachmentBar *bar)
{
	update (bar);
}

void
e_attachment_bar_edit_selected (EAttachmentBar *bar)
{
	GnomeIconList *icon_list;
	GList *selection, *attach;
	int num;

	g_return_if_fail (bar != NULL);
	g_return_if_fail (E_IS_ATTACHMENT_BAR (bar));
	
	icon_list = GNOME_ICON_LIST (bar);
	
	selection = gnome_icon_list_get_selection (icon_list);
	if (selection) {
		num = GPOINTER_TO_INT (selection->data);
		attach = g_list_nth (bar->priv->attachments, num);
		if (attach)
			e_attachment_edit ((EAttachment *)attach->data, GTK_WIDGET (bar));
	}
}

GtkWidget **
e_attachment_bar_get_selector(EAttachmentBar *bar)
{
	g_return_val_if_fail (bar != NULL, 0);
	g_return_val_if_fail (E_IS_ATTACHMENT_BAR (bar), 0);
	
	return &bar->priv->attach;
}

GSList *
e_attachment_bar_get_selected (EAttachmentBar *bar)
{
	GSList *attachments = NULL;
	GList *p;

	g_return_val_if_fail (bar != NULL, 0);
	g_return_val_if_fail (E_IS_ATTACHMENT_BAR (bar), 0);

	p = gnome_icon_list_get_selection((GnomeIconList *)bar);
	for ( ; p != NULL; p = p->next) {
		int num = GPOINTER_TO_INT(p->data);
		EAttachment *attachment = g_list_nth_data(bar->priv->attachments, num);
			
		if (attachment && g_slist_find(attachments, attachment) == NULL) {
			g_object_ref(attachment);
			attachments = g_slist_prepend(attachments, attachment);
		}
	}
	attachments = g_slist_reverse(attachments);
	
	return attachments;
}

/* FIXME: Cleanup this, since there is a api to get selected attachments */
/* if id != -1, then use it as an index for target of the popup */
GSList *
e_attachment_bar_get_attachment (EAttachmentBar *bar, int id)
{
	GSList *attachments = NULL;
	GList *p;
	EAttachment *attachment;

	g_return_val_if_fail (bar != NULL, 0);
	g_return_val_if_fail (E_IS_ATTACHMENT_BAR (bar), 0);

	/* We need to check if there are duplicated index in the return list of 
	   gnome_icon_list_get_selection() because of gnome bugzilla bug #122356.
	   FIXME in the future. */

	if (id == -1
	    || (attachment = g_list_nth_data(bar->priv->attachments, id)) == NULL) {
		p = gnome_icon_list_get_selection((GnomeIconList *)bar);
		for ( ; p != NULL; p = p->next) {
			int num = GPOINTER_TO_INT(p->data);
			EAttachment *attachment = g_list_nth_data(bar->priv->attachments, num);
			
			if (attachment && g_slist_find(attachments, attachment) == NULL) {
				g_object_ref(attachment);
				attachments = g_slist_prepend(attachments, attachment);
			}
		}
		attachments = g_slist_reverse(attachments);
	} else {
		g_object_ref(attachment);
		attachments = g_slist_prepend(attachments, attachment);
	}
	
	return attachments;
}

/* Just the GSList has to be freed by the caller */
GSList *
e_attachment_bar_get_parts (EAttachmentBar *bar)
{
        EAttachment *attachment;
        GList *p = NULL;
	GSList *part_list = NULL;

	g_return_val_if_fail (bar != NULL, 0);
	g_return_val_if_fail (E_IS_ATTACHMENT_BAR (bar), 0);

        for ( p = bar->priv->attachments; p!= NULL; p = p->next) {
                attachment = p->data;
                if (attachment && attachment->is_available_local)
                        part_list = g_slist_prepend(part_list, attachment->body);
        }
	
        return part_list;
}

/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	EAttachmentBar *bar;
	
	bar = E_ATTACHMENT_BAR (object);
	
	if (bar->priv) {
		free_attachment_list (bar);

		if (bar->priv->attach)
			gtk_widget_destroy(bar->priv->attach);

		if (bar->priv->path)
			g_free (bar->priv->path);

		g_free (bar->priv);
		bar->priv = NULL;
	}
	
	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static char *
temp_save_part(CamelMimePart *part)
{
	const char *filename;
	char *tmpdir, *path, *mfilename = NULL;
	CamelStream *stream;
	CamelDataWrapper *wrapper;

	tmpdir = e_mkdtemp("evolution-tmp-XXXXXX");
	if (tmpdir == NULL) {
		return NULL;
	}

	filename = camel_mime_part_get_filename (part);
	if (filename == NULL) {
		/* This is the default filename used for temporary file creation */
		filename = _("Unknown");
	} else {
		mfilename = g_strdup(filename);
		e_filename_make_safe(mfilename);
		filename = mfilename;
	}

	path = g_build_filename(tmpdir, filename, NULL);
	g_free(tmpdir);
	g_free(mfilename);

	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (part));
	stream = camel_stream_fs_new_with_name (path, O_RDWR|O_CREAT|O_TRUNC, 0600);

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
	char *path;
	GList *tmp;
	gchar **uris;
	int length, i=0;

	if (info)
		return;
	
	tmp = gnome_icon_list_get_selection (GNOME_ICON_LIST(bar));
	length = g_list_length (tmp);

	uris = g_malloc0(sizeof(bar) * (length+1));

	for (; tmp; tmp = tmp->next) {
		int num = GPOINTER_TO_INT(tmp->data);
		EAttachment *attachment = g_list_nth_data(bar->priv->attachments, num);	
		char *uri;

		if (!attachment->is_available_local)
			continue;

		uri = g_object_get_data((GObject *)attachment, "e-drag-uri");
		if (uri) {
			uris[i] = uri;
			i++;
			continue;
		}
		path = temp_save_part(attachment->body);
		/* If we are not able to save, ignore it*/
		if (path == NULL) 
			continue;
		
		uri = g_strdup_printf("file://%s\r\n", path);
		g_free(path);
		g_object_set_data_full((GObject *)attachment, "e-drag-uri", uri, g_free);
		uris[i] = uri;
		i++;
	}
	uris[i]=0;
	gtk_selection_data_set_uris(data, uris);
	
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
	length = g_list_length(selected);

	if (event) {
		icon_number = gnome_icon_list_get_icon_at(icon_list, event->x, event->y);
		if (icon_number < 0) { 
			/* When nothing is selected deselect all*/
			gnome_icon_list_unselect_all(icon_list);
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

/* Initialization.  */

static void
class_init (EAttachmentBarClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GnomeIconListClass *icon_list_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);
	icon_list_class = GNOME_ICON_LIST_CLASS (klass);
	
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
	EAttachmentBarPrivate *priv;
	
	priv = g_new (EAttachmentBarPrivate, 1);
	
	priv->attach = NULL;
	priv->attachments = NULL;
	priv->num_attachments = 0;
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
e_attachment_bar_to_multipart (EAttachmentBar *bar,
			       CamelMultipart *multipart,
			       const char *default_charset)
{
	EAttachmentBarPrivate *priv;
	GList *p;
	
	g_return_if_fail (E_IS_ATTACHMENT_BAR (bar));
	g_return_if_fail (CAMEL_IS_MULTIPART (multipart));
	
	priv = bar->priv;
	
	for (p = priv->attachments; p != NULL; p = p->next) {
		EAttachment *attachment;
		
		attachment = E_ATTACHMENT (p->data);
		if (attachment->is_available_local)
			attach_to_multipart (multipart, attachment, default_charset);
	}
}


guint
e_attachment_bar_get_num_attachments (EAttachmentBar *bar)
{
	g_return_val_if_fail (bar != NULL, 0);
	g_return_val_if_fail (E_IS_ATTACHMENT_BAR (bar), 0);
	
	return bar->priv->num_attachments;
}


void
e_attachment_bar_attach (EAttachmentBar *bar,
			 const gchar *file_name,
			 char *disposition)
{
	g_return_if_fail (E_IS_ATTACHMENT_BAR (bar));
	g_return_if_fail ( file_name != NULL && disposition != NULL);
	
	add_from_file (bar, file_name, disposition);
}

void
e_attachment_bar_add_attachment (EAttachmentBar *bar,
				 EAttachment *attachment)
{
	g_return_if_fail (bar != NULL);
	g_return_if_fail (E_IS_ATTACHMENT_BAR (bar));
	
	add_common (bar, attachment);
}

int 
e_attachment_bar_get_download_count (EAttachmentBar *bar)
{
	EAttachmentBarPrivate *priv;
	GList *p;
	int count=0;
	
	g_return_val_if_fail (bar != NULL, 0);
	g_return_val_if_fail (E_IS_ATTACHMENT_BAR (bar), 0);
	
	priv = bar->priv;
	
	for (p = priv->attachments; p != NULL; p = p->next) {
		EAttachment *attachment;
		
		attachment = p->data;
		if (!attachment->is_available_local)
			count++;
	}

	return count;
}

void 
e_attachment_bar_attach_remote_file (EAttachmentBar *bar,
				     const gchar *url)
{
	EAttachment *attachment;
	CamelException ex;

	g_return_if_fail ( bar!=NULL );
	g_return_if_fail (E_IS_ATTACHMENT_BAR (bar));

	if (!bar->priv->path)
		bar->priv->path = e_mkdtemp("attach-XXXXXX");

	camel_exception_init (&ex);
	attachment = e_attachment_new_remote_file (url, "attachment", bar->priv->path, &ex);
	g_signal_connect (attachment, "update", G_CALLBACK(update_remote_file), bar);
	if (attachment) {
		add_common (bar, attachment);
	} else {
		e_error_run((GtkWindow *)gtk_widget_get_toplevel((GtkWidget *)bar), "mail-composer:no-attach",
			    attachment->file_name, camel_exception_get_description(&ex), NULL);
		camel_exception_clear (&ex);
	}
}

void
e_attachment_bar_attach_mime_part (EAttachmentBar *bar,
				   CamelMimePart *part)
{
	g_return_if_fail ( bar!=NULL );
	g_return_if_fail (E_IS_ATTACHMENT_BAR (bar));
	
	add_from_mime_part (bar, part);
}
