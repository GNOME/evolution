/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-msg-composer-attachment-bar.c
 *
 * Copyright (C) 1999  Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
 * Author: Ettore Perazzoli
 */

#include <config.h>
#include <gnome.h>
#include <glade/glade.h>
#include <libgnomevfs/gnome-vfs-mime-info.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixbuf-loader.h>
#include <gdk-pixbuf/gnome-canvas-pixbuf.h>

#include "e-msg-composer.h"
#include "e-msg-composer-select-file.h"
#include "e-msg-composer-attachment.h"
#include "e-msg-composer-attachment-bar.h"

#include "e-icon-list.h"

#include "camel/camel-data-wrapper.h"
#include "camel/camel-stream-fs.h"
#include "camel/camel-stream-mem.h"
#include "camel/camel-mime-part.h"


#define ICON_WIDTH 64
#define ICON_SEPARATORS " /-_"
#define ICON_SPACING 2
#define ICON_ROW_SPACING ICON_SPACING
#define ICON_COL_SPACING ICON_SPACING
#define ICON_BORDER 2
#define ICON_TEXT_SPACING 2


static EIconListClass *parent_class = NULL;

struct _EMsgComposerAttachmentBarPrivate {
	GList *attachments;
	guint num_attachments;

	GtkWidget *context_menu;
	GtkWidget *icon_context_menu;
};


enum {
	CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


static void update (EMsgComposerAttachmentBar *bar);


static gchar *
size_to_string (gulong size)
{
	gchar *size_string;

	/* FIXME: The following should probably go into a separate module, as
           we might have to do the same thing in other places as well.  Also,
	   I am not sure this will be OK for all the languages.  */

	if (size < 1e3L) {
		if (size == 1)
			size_string = g_strdup (_("1 byte"));
		else
			size_string = g_strdup_printf (_("%u bytes"),
						       (guint) size);
	} else {
		gdouble displayed_size;

		if (size < 1e6L) {
			displayed_size = (gdouble) size / 1.0e3;
			size_string = g_strdup_printf (_("%.1fK"),
						       displayed_size);
		} else if (size < 1e9L) {
			displayed_size = (gdouble) size / 1.0e6;
			size_string = g_strdup_printf (_("%.1fM"),
						       displayed_size);
		} else {
			displayed_size = (gdouble) size / 1.0e9;
			size_string = g_strdup_printf (_("%.1fG"),
						       displayed_size);
		}
	}

	return size_string;
}

/* Attachment handling functions.  */

static void
free_attachment_list (EMsgComposerAttachmentBar *bar)
{
	EMsgComposerAttachmentBarPrivate *priv;
	GList *p;

	priv = bar->priv;

	for (p = priv->attachments; p != NULL; p = p->next)
		gtk_object_unref (GTK_OBJECT (p->data));
}

static void
attachment_changed_cb (EMsgComposerAttachment *attachment,
		       gpointer data)
{
	update (E_MSG_COMPOSER_ATTACHMENT_BAR (data));
}

static void
add_common (EMsgComposerAttachmentBar *bar,
	    EMsgComposerAttachment *attachment)
{
	g_return_if_fail (attachment != NULL);

	gtk_signal_connect (GTK_OBJECT (attachment), "changed",
			    GTK_SIGNAL_FUNC (attachment_changed_cb),
			    bar);

	bar->priv->attachments = g_list_append (bar->priv->attachments,
						attachment);
	bar->priv->num_attachments++;

	update (bar);

	gtk_signal_emit (GTK_OBJECT (bar), signals[CHANGED]);
}

static void
add_from_mime_part (EMsgComposerAttachmentBar *bar,
		    CamelMimePart *part)
{
	add_common (bar, e_msg_composer_attachment_new_from_mime_part (part));
}

static void
add_from_file (EMsgComposerAttachmentBar *bar,
	       const gchar *file_name)
{
	add_common (bar, e_msg_composer_attachment_new (file_name));
}

static void
remove_attachment (EMsgComposerAttachmentBar *bar,
		   EMsgComposerAttachment *attachment)
{
	bar->priv->attachments = g_list_remove (bar->priv->attachments,
						attachment);
	bar->priv->num_attachments--;

	gtk_object_unref (GTK_OBJECT (attachment));

	gtk_signal_emit (GTK_OBJECT (bar), signals[CHANGED]);
}


/* Icon list contents handling.  */

static GdkPixbuf *
pixbuf_for_mime_type (const char *mime_type)
{
	const char *icon_name;
	char *filename = NULL;
	GdkPixbuf *pixbuf;

	icon_name = gnome_vfs_mime_get_value (mime_type, "icon-filename");
	if (icon_name) {
		if (*icon_name == '/') {
			pixbuf = gdk_pixbuf_new_from_file (icon_name);
			if (pixbuf)
				return pixbuf;
		}
		
		filename = gnome_pixmap_file (icon_name);
		if (!filename) {
			char *fm_icon;
			
			fm_icon = g_strdup_printf ("nautilus/%s", icon_name);
			filename = gnome_pixmap_file (fm_icon);
			if (!filename) {
				fm_icon = g_strdup_printf ("mc/%s", icon_name);
				filename = gnome_pixmap_file (fm_icon);
			}
			g_free (fm_icon);
		}
	}
	
	if (!filename)
		filename = gnome_pixmap_file ("gnome-unknown.png");
	
	pixbuf = gdk_pixbuf_new_from_file (filename);
	g_free (filename);
	
	return pixbuf;
}

static void
update (EMsgComposerAttachmentBar *bar)
{
	EMsgComposerAttachmentBarPrivate *priv;
	EIconList *icon_list;
	GList *p;

	priv = bar->priv;
	icon_list = E_ICON_LIST (bar);

	e_icon_list_freeze (icon_list);

	e_icon_list_clear (icon_list);

	/* FIXME could be faster, but we don't care.  */
	for (p = priv->attachments; p != NULL; p = p->next) {
		EMsgComposerAttachment *attachment;
		const gchar *desc;
		gchar *size_string, *label, *mime_type;
		GMimeContentField *content_type;
		GdkPixbuf *pixbuf;
		gboolean image = FALSE;
		
		attachment = p->data;
		content_type = camel_mime_part_get_content_type (attachment->body);

		mime_type = g_strdup_printf ("%s/%s", content_type->type,
					     content_type->subtype);

		/* Get the image out of the attachment 
		   and create a thumbnail for it */
		if (strcmp (content_type->type, "image") == 0)
			image = TRUE;
		else
			image = FALSE;
		
		if (image && attachment->pixbuf_cache == NULL) {
			CamelDataWrapper *wrapper;
			CamelStream *mstream;
			GdkPixbufLoader *loader;
			gboolean error = TRUE;
			char tmp[4096];
			int t;

			wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (attachment->body));
			mstream = camel_stream_mem_new ();

			camel_data_wrapper_write_to_stream (wrapper, mstream);

			camel_stream_reset (mstream);

			/* Stream image into pixbuf loader */
			loader = gdk_pixbuf_loader_new ();
			do {
				t = camel_stream_read (mstream, tmp, 4096);
				if (t > 0) {
					error = !gdk_pixbuf_loader_write (loader,
									  tmp, t);
					if (error) {
						break;
					}
				} else {
					if (camel_stream_eos (mstream))
						break;
					error = TRUE;
					break;
				}
				
			} while (!camel_stream_eos (mstream));

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
			} else {
				g_warning ("GdkPixbufLoader Error");
				image = FALSE;
			}

			/* Destroy everything */
			gdk_pixbuf_loader_close (loader);
			gtk_object_destroy (GTK_OBJECT (loader));
			camel_stream_close (mstream);
		}
		
		desc = camel_mime_part_get_description (attachment->body);
		if (!desc || *desc == '\0')
			desc = camel_mime_part_get_filename (attachment->body);
		if (!desc)
			desc = _("attachment");
		
		if (attachment->size) {
			size_string = size_to_string (attachment->size);
			label = g_strdup_printf ("%s (%s)", desc, size_string);
			g_free (size_string);
		} else
			label = g_strdup (desc);

		if (image) {
			e_icon_list_append_pixbuf (icon_list, attachment->pixbuf_cache, NULL, label);
		} else {
			pixbuf = pixbuf_for_mime_type (mime_type);
			e_icon_list_append_pixbuf (icon_list, pixbuf, 
						   NULL, label);
			if (pixbuf)
				gdk_pixbuf_unref (pixbuf);
		}

		g_free (mime_type);
		g_free (label);
	}

	e_icon_list_thaw (icon_list);
}

static void
remove_selected (EMsgComposerAttachmentBar *bar)
{
	EIconList *icon_list;
	EMsgComposerAttachment *attachment;
	GList *attachment_list;
	GList *p;
	gint num;

	icon_list = E_ICON_LIST (bar);

	/* Weee!  I am especially proud of this piece of cheesy code: it is
           truly awful.  But unless one attaches a huge number of files, it
           will not be as greedy as intended.  FIXME of course.  */

	attachment_list = NULL;
	p = e_icon_list_get_selection (icon_list);
	for (; p != NULL; p = p->next) {
		num = GPOINTER_TO_INT (p->data);
		attachment = E_MSG_COMPOSER_ATTACHMENT
			(g_list_nth (bar->priv->attachments, num)->data);
		attachment_list = g_list_prepend (attachment_list, attachment);
	}

	for (p = attachment_list; p != NULL; p = p->next)
		remove_attachment (bar, E_MSG_COMPOSER_ATTACHMENT (p->data));

	g_list_free (attachment_list);

	update (bar);
}

static void
edit_selected (EMsgComposerAttachmentBar *bar)
{
	EIconList *icon_list;
	EMsgComposerAttachment *attachment;
	GList *selection;
	gint num;

	icon_list = E_ICON_LIST (bar);

	selection = e_icon_list_get_selection (icon_list);
	num = GPOINTER_TO_INT (selection->data);
	attachment = g_list_nth (bar->priv->attachments, num)->data;

	e_msg_composer_attachment_edit (attachment, GTK_WIDGET (bar));
}


/* "Attach" dialog.  */

static void
add_from_user (EMsgComposerAttachmentBar *bar)
{
	EMsgComposer *composer;
	char *file_name;
	
	composer = E_MSG_COMPOSER (gtk_widget_get_toplevel (GTK_WIDGET (bar)));
	
	file_name = e_msg_composer_select_file (composer, _("Attach a file"));
	
	add_from_file (bar, file_name);
	
	g_free (file_name);
}


/* Callbacks.  */

static void
add_cb (GtkWidget *widget,
	gpointer data)
{
	g_return_if_fail (E_IS_MSG_COMPOSER_ATTACHMENT_BAR (data));

	add_from_user (E_MSG_COMPOSER_ATTACHMENT_BAR (data));
}

static void
properties_cb (GtkWidget *widget,
	       gpointer data)
{
	EMsgComposerAttachmentBar *bar;

	g_return_if_fail (E_IS_MSG_COMPOSER_ATTACHMENT_BAR (data));

	bar = E_MSG_COMPOSER_ATTACHMENT_BAR (data);
	edit_selected (data);
}

static void
remove_cb (GtkWidget *widget,
	   gpointer data)
{
	EMsgComposerAttachmentBar *bar;

	g_return_if_fail (E_IS_MSG_COMPOSER_ATTACHMENT_BAR (data));

	bar = E_MSG_COMPOSER_ATTACHMENT_BAR (data);
	remove_selected (bar);
}


/* Popup menu handling.  */

static GnomeUIInfo icon_context_menu_info[] = {
	GNOMEUIINFO_ITEM (N_("Remove"),
			  N_("Remove selected items from the attachment list"),
			  remove_cb, NULL),
	GNOMEUIINFO_MENU_PROPERTIES_ITEM (properties_cb, NULL),
	GNOMEUIINFO_END
};

static GtkWidget *
get_icon_context_menu (EMsgComposerAttachmentBar *bar)
{
	EMsgComposerAttachmentBarPrivate *priv;

	priv = bar->priv;
	if (priv->icon_context_menu == NULL)
		priv->icon_context_menu = gnome_popup_menu_new
			(icon_context_menu_info);

	return priv->icon_context_menu;
}

static void
popup_icon_context_menu (EMsgComposerAttachmentBar *bar,
			 gint num,
			 GdkEventButton *event)
{
	GtkWidget *menu;

	menu = get_icon_context_menu (bar);
	gnome_popup_menu_do_popup (menu, NULL, NULL, event, bar);
}

static GnomeUIInfo context_menu_info[] = {
	GNOMEUIINFO_ITEM (N_("Add attachment..."),
			  N_("Attach a file to the message"),
			  add_cb, NULL),
	GNOMEUIINFO_END
};

static GtkWidget *
get_context_menu (EMsgComposerAttachmentBar *bar)
{
	EMsgComposerAttachmentBarPrivate *priv;

	priv = bar->priv;
	if (priv->context_menu == NULL)
		priv->context_menu = gnome_popup_menu_new (context_menu_info);

	return priv->context_menu;
}

static void
popup_context_menu (EMsgComposerAttachmentBar *bar,
		    GdkEventButton *event)
{
	GtkWidget *menu;

	menu = get_context_menu (bar);
	gnome_popup_menu_do_popup (menu, NULL, NULL, event, bar);
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	EMsgComposerAttachmentBar *bar;

	bar = E_MSG_COMPOSER_ATTACHMENT_BAR (object);

	free_attachment_list (bar);

	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


/* GtkWidget methods.  */

static gint
button_press_event (GtkWidget *widget,
		    GdkEventButton *event)
{
	EMsgComposerAttachmentBar *bar;
	EIconList *icon_list;
	gint icon_number;

	bar = E_MSG_COMPOSER_ATTACHMENT_BAR (widget);
	icon_list = E_ICON_LIST (widget);

	if (event->button != 3)
		return GTK_WIDGET_CLASS (parent_class)->button_press_event
			(widget, event);

	icon_number = e_icon_list_get_icon_at (icon_list,
					       event->x, event->y);

	if (icon_number >= 0) {
		e_icon_list_select_icon (icon_list, icon_number);
		popup_icon_context_menu (bar, icon_number, event);
	} else {
		popup_context_menu (bar, event);
	}

	return TRUE;
}


/* Initialization.  */

static void
class_init (EMsgComposerAttachmentBarClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	EIconListClass *icon_list_class;

	object_class = GTK_OBJECT_CLASS (class);
	widget_class = GTK_WIDGET_CLASS (class);
	icon_list_class = E_ICON_LIST_CLASS (class);

	parent_class = gtk_type_class (e_icon_list_get_type ());

	object_class->destroy = destroy;

	widget_class->button_press_event = button_press_event;

	/* Setup signals.  */

	signals[CHANGED] =
		gtk_signal_new ("changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EMsgComposerAttachmentBarClass,
						   changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
init (EMsgComposerAttachmentBar *bar)
{
	EMsgComposerAttachmentBarPrivate *priv;
	guint icon_size, icon_height;
	GdkFont *font;

	priv = g_new (EMsgComposerAttachmentBarPrivate, 1);

	priv->attachments = NULL;
	priv->context_menu = NULL;
	priv->icon_context_menu = NULL;

	priv->num_attachments = 0;

	bar->priv = priv;

	/* FIXME partly hardcoded.  We should compute height from the font, and
           allow at least 2 lines for every item.  */
	icon_size = ICON_WIDTH + ICON_SPACING + ICON_BORDER + ICON_TEXT_SPACING;

	font = GTK_WIDGET (bar)->style->font;
	icon_height = icon_size + ((font->ascent + font->descent) * 2);
	icon_size += 24;
	
	gtk_widget_set_usize (GTK_WIDGET (bar), icon_size * 4, icon_height);
}


GtkType
e_msg_composer_attachment_bar_get_type (void)
{
	static GtkType type = 0;

	if (type == 0) {
		static const GtkTypeInfo info = {
			"EMsgComposerAttachmentBar",
			sizeof (EMsgComposerAttachmentBar),
			sizeof (EMsgComposerAttachmentBarClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		type = gtk_type_unique (e_icon_list_get_type (), &info);
	}

	return type;
}

GtkWidget *
e_msg_composer_attachment_bar_new (GtkAdjustment *adj)
{
	EMsgComposerAttachmentBar *new;
	EIconList *icon_list;

	gdk_rgb_init ();
	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());
	new = gtk_type_new (e_msg_composer_attachment_bar_get_type ());
	gtk_widget_pop_visual ();
	gtk_widget_pop_colormap ();

	icon_list = E_ICON_LIST (new);

	e_icon_list_construct (icon_list, ICON_WIDTH, 0);

	e_icon_list_set_separators (icon_list, ICON_SEPARATORS);
	e_icon_list_set_row_spacing (icon_list, ICON_ROW_SPACING);
	e_icon_list_set_col_spacing (icon_list, ICON_COL_SPACING);
	e_icon_list_set_icon_border (icon_list, ICON_BORDER);
	e_icon_list_set_text_spacing (icon_list, ICON_TEXT_SPACING);
	e_icon_list_set_selection_mode (icon_list, GTK_SELECTION_MULTIPLE);

	return GTK_WIDGET (new);
}

/* FIXME: is_8bit() and best_encoding() should really be shared
   between e-msg-composer.c and this file. */
static gboolean
is_8bit (const guchar *text)
{
	guchar *c;
	
	for (c = (guchar *) text; *c; c++)
		if (*c > (guchar) 127)
			return TRUE;
	
	return FALSE;
}

static int
best_encoding (const guchar *text)
{
	guchar *ch;
	int count = 0;
	int total;
	
	for (ch = (guchar *) text; *ch; ch++)
		if (*ch > (guchar) 127)
			count++;
	
	total = (int) (ch - text);
	
	if ((float) count <= total * 0.17)
		return CAMEL_MIME_PART_ENCODING_QUOTEDPRINTABLE;
	else
		return CAMEL_MIME_PART_ENCODING_BASE64;
}

static void
attach_to_multipart (CamelMultipart *multipart,
		     EMsgComposerAttachment *attachment)
{
	GMimeContentField *content_type;
	
	content_type = camel_mime_part_get_content_type (attachment->body);
	
	if (!g_strcasecmp (content_type->type, "text")) {
		CamelStream *stream;
		GByteArray *array;
		guchar *text;
		
		array = g_byte_array_new ();
		stream = camel_stream_mem_new_with_byte_array (array);
		camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (attachment->body), stream);
		g_byte_array_append (array, "", 1);
		text = array->data;
		
		if (is_8bit (text))
			camel_mime_part_set_encoding (attachment->body, best_encoding (text));
		else
			camel_mime_part_set_encoding (attachment->body, CAMEL_MIME_PART_ENCODING_7BIT);
		
		camel_object_unref (CAMEL_OBJECT (stream));
	} else if (g_strcasecmp (content_type->type, "message") != 0) {
		camel_mime_part_set_encoding (attachment->body,
					      CAMEL_MIME_PART_ENCODING_BASE64);
	}
	
	camel_multipart_add_part (multipart, attachment->body);
}

void
e_msg_composer_attachment_bar_to_multipart (EMsgComposerAttachmentBar *bar,
					    CamelMultipart *multipart)
{
	EMsgComposerAttachmentBarPrivate *priv;
	GList *p;

	g_return_if_fail (bar != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER_ATTACHMENT_BAR (bar));
	g_return_if_fail (multipart != NULL);
	g_return_if_fail (CAMEL_IS_MULTIPART (multipart));

	priv = bar->priv;

	for (p = priv->attachments; p != NULL; p = p->next) {
		EMsgComposerAttachment *attachment;

		attachment = E_MSG_COMPOSER_ATTACHMENT (p->data);
		attach_to_multipart (multipart, attachment);
	}
}


guint
e_msg_composer_attachment_bar_get_num_attachments (EMsgComposerAttachmentBar *bar)
{
	g_return_val_if_fail (bar != NULL, 0);
	g_return_val_if_fail (E_IS_MSG_COMPOSER_ATTACHMENT_BAR (bar), 0);

	return bar->priv->num_attachments;
}


void
e_msg_composer_attachment_bar_attach (EMsgComposerAttachmentBar *bar,
				      const gchar *file_name)
{
	g_return_if_fail (E_IS_MSG_COMPOSER_ATTACHMENT_BAR (bar));

	if (file_name == NULL)
		add_from_user (bar);
	else
		add_from_file (bar, file_name);
}

void
e_msg_composer_attachment_bar_attach_mime_part (EMsgComposerAttachmentBar *bar,
						CamelMimePart *part)
{
	g_return_if_fail (E_IS_MSG_COMPOSER_ATTACHMENT_BAR (bar));

	add_from_mime_part (bar, part);
}
