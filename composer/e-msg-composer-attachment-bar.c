/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 *  Authors: Ettore Perazzoli <ettore@ximian.com>
 *           Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 1999-2002 Ximian, Inc. (www.ximian.com)
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

#include "e-msg-composer.h"
#include "e-msg-composer-select-file.h"
#include "e-msg-composer-attachment.h"
#include "e-msg-composer-attachment-bar.h"

#include <gal/util/e-iconv.h>

#include <camel/camel-data-wrapper.h>
#include <camel/camel-stream-fs.h>
#include <camel/camel-stream-null.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-stream-filter.h>
#include <camel/camel-mime-filter-bestenc.h>
#include <camel/camel-mime-part.h>

#include "e-util/e-gui-utils.h"
#include "e-util/e-icon-factory.h"
#include "widgets/misc/e-error.h"
#include "mail/em-popup.h"

#define ICON_WIDTH 64
#define ICON_SEPARATORS " /-_"
#define ICON_SPACING 2
#define ICON_ROW_SPACING ICON_SPACING
#define ICON_COL_SPACING ICON_SPACING
#define ICON_BORDER 2
#define ICON_TEXT_SPACING 2


static GnomeIconListClass *parent_class = NULL;

struct _EMsgComposerAttachmentBarPrivate {
	GList *attachments;
	guint num_attachments;
};


enum {
	CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


static void update (EMsgComposerAttachmentBar *bar);


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
free_attachment_list (EMsgComposerAttachmentBar *bar)
{
	EMsgComposerAttachmentBarPrivate *priv;
	GList *p;
	
	priv = bar->priv;
	
	for (p = priv->attachments; p != NULL; p = p->next)
		g_object_unref (p->data);
	
	priv->attachments = NULL;
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
add_from_mime_part (EMsgComposerAttachmentBar *bar,
		    CamelMimePart *part)
{
	add_common (bar, e_msg_composer_attachment_new_from_mime_part (part));
}

static void
add_from_file (EMsgComposerAttachmentBar *bar,
	       const char *file_name,
	       const char *disposition)
{
	EMsgComposerAttachment *attachment;
	CamelException ex;
	
	camel_exception_init (&ex);
	attachment = e_msg_composer_attachment_new (file_name, disposition, &ex);
	if (attachment) {
		add_common (bar, attachment);
	} else {
		e_error_run((GtkWindow *)gtk_widget_get_toplevel((GtkWidget *)bar), "mail-composer:no-attach",
			    file_name, camel_exception_get_description(&ex), NULL);
		camel_exception_clear (&ex);
	}
}

static void
remove_attachment (EMsgComposerAttachmentBar *bar,
		   EMsgComposerAttachment *attachment)
{
	g_return_if_fail (E_IS_MSG_COMPOSER_ATTACHMENT_BAR (bar));
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
update (EMsgComposerAttachmentBar *bar)
{
	EMsgComposerAttachmentBarPrivate *priv;
	GnomeIconList *icon_list;
	GList *p;
	
	priv = bar->priv;
	icon_list = GNOME_ICON_LIST (bar);
	
	gnome_icon_list_freeze (icon_list);
	
	gnome_icon_list_clear (icon_list);
	
	/* FIXME could be faster, but we don't care.  */
	for (p = priv->attachments; p != NULL; p = p->next) {
		EMsgComposerAttachment *attachment;
		CamelContentType *content_type;
		char *size_string, *label;
		GdkPixbuf *pixbuf;
		const char *desc;
		
		attachment = p->data;
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
			gnome_icon_list_append_pixbuf (icon_list, pixbuf, NULL, label);
			g_object_unref(pixbuf);
		}
		
		g_free (label);
	}
	
	gnome_icon_list_thaw (icon_list);
}

static void
remove_selected (EMsgComposerAttachmentBar *bar)
{
	GnomeIconList *icon_list;
	EMsgComposerAttachment *attachment;
	GList *attachment_list, *p;
	int num = 0, left, dlen;
	
	icon_list = GNOME_ICON_LIST (bar);
	
	/* Weee!  I am especially proud of this piece of cheesy code: it is
           truly awful.  But unless one attaches a huge number of files, it
           will not be as greedy as intended.  FIXME of course.  */
	
	attachment_list = NULL;
	p = gnome_icon_list_get_selection (icon_list);
	dlen = g_list_length (p);
	for ( ; p != NULL; p = p->next) {
		num = GPOINTER_TO_INT (p->data);
		attachment = E_MSG_COMPOSER_ATTACHMENT (g_list_nth_data (bar->priv->attachments, num));

		/* We need to check if there are duplicated index in the return list of 
		   gnome_icon_list_get_selection() because of gnome bugzilla bug #122356.
		   FIXME in the future. */

		if (g_list_find (attachment_list, attachment) == NULL) {
			attachment_list = g_list_prepend (attachment_list, attachment);
		}
	}
	
	for (p = attachment_list; p != NULL; p = p->next)
		remove_attachment (bar, E_MSG_COMPOSER_ATTACHMENT (p->data));
	
	g_list_free (attachment_list);
	
	update (bar);
	
	left = gnome_icon_list_get_num_icons (icon_list);
	num = num - dlen + 1;
	if (left > 0)
		gnome_icon_list_focus_icon (icon_list, left > num ? num : left - 1);
}

static void
edit_selected (EMsgComposerAttachmentBar *bar)
{
	GnomeIconList *icon_list;
	GList *selection, *attach;
	int num;
	
	icon_list = GNOME_ICON_LIST (bar);
	
	selection = gnome_icon_list_get_selection (icon_list);
	if (selection) {
		num = GPOINTER_TO_INT (selection->data);
		attach = g_list_nth (bar->priv->attachments, num);
		if (attach)
			e_msg_composer_attachment_edit ((EMsgComposerAttachment *)attach->data, GTK_WIDGET (bar));
	}
}

/* "Attach" dialog.  */

static void
add_from_user (EMsgComposerAttachmentBar *bar)
{
	EMsgComposer *composer;
	GPtrArray *file_list;
	gboolean is_inline = FALSE;
	int i;
	
	composer = E_MSG_COMPOSER (gtk_widget_get_toplevel (GTK_WIDGET (bar)));
	
	file_list = e_msg_composer_select_file_attachments (composer, &is_inline);
	if (!file_list)
		return;
	
	for (i = 0; i < file_list->len; i++) {
		add_from_file (bar, file_list->pdata[i], is_inline ? "inline" : "attachment");
		g_free (file_list->pdata[i]);
	}
	
	g_ptr_array_free (file_list, TRUE);
}


/* Callbacks.  */

static void
emcab_add(EPopup *ep, EPopupItem *item, void *data)
{
	EMsgComposerAttachmentBar *bar = data;

	add_from_user(bar);
}

static void
emcab_properties(EPopup *ep, EPopupItem *item, void *data)
{
	EMsgComposerAttachmentBar *bar = data;
	
	edit_selected(bar);
}

static void
emcab_remove(EPopup *ep, EPopupItem *item, void *data)
{
	EMsgComposerAttachmentBar *bar = data;

	remove_selected(bar);
}

/* Popup menu handling.  */
static EPopupItem emcab_popups[] = {
	{ E_POPUP_ITEM, "10.attach", N_("_Remove"), emcab_remove, NULL, GTK_STOCK_REMOVE, EM_POPUP_ATTACHMENTS_MANY },
	{ E_POPUP_ITEM, "20.attach", N_("_Properties"), emcab_properties, NULL, GTK_STOCK_PROPERTIES, EM_POPUP_ATTACHMENTS_ONE },
	{ E_POPUP_BAR, "30.attach.00", NULL, NULL, NULL, NULL, EM_POPUP_ATTACHMENTS_MANY|EM_POPUP_ATTACHMENTS_ONE },
	{ E_POPUP_ITEM, "30.attach.01", N_("_Add attachment..."), emcab_add, NULL, GTK_STOCK_ADD, 0 },
};

static void
emcab_popup_position(GtkMenu *menu, int *x, int *y, gboolean *push_in, gpointer user_data)
{
	EMsgComposerAttachmentBar *bar = user_data;
	GnomeIconList *icon_list = user_data;
	GList *selection;
	GnomeCanvasPixbuf *image;
	
	gdk_window_get_origin (((GtkWidget*) bar)->window, x, y);
	
	selection = gnome_icon_list_get_selection (icon_list);
	if (selection == NULL)
		return;
	
	image = gnome_icon_list_get_icon_pixbuf_item (icon_list, (gint)selection->data);
	if (image == NULL)
		return;
	
	/* Put menu to the center of icon. */
	*x += (int)(image->item.x1 + image->item.x2) / 2;
	*y += (int)(image->item.y1 + image->item.y2) / 2;
}

static void
emcab_popups_free(EPopup *ep, GSList *l, void *data)
{
	g_slist_free(l);
}

/* if id != -1, then use it as an index for target of the popup */
static void
emcab_popup(EMsgComposerAttachmentBar *bar, GdkEventButton *event, int id)
{
	GList *p;
	GSList *attachments = NULL, *menus = NULL;
	int i;
	EMPopup *emp;
	EMPopupTargetAttachments *t;
	GtkMenu *menu;
	EMsgComposerAttachment *attachment;

	/* We need to check if there are duplicated index in the return list of 
	   gnome_icon_list_get_selection() because of gnome bugzilla bug #122356.
	   FIXME in the future. */

	if (id == -1
	    || (attachment = g_list_nth_data(bar->priv->attachments, id)) == NULL) {
		p = gnome_icon_list_get_selection((GnomeIconList *)bar);
		for ( ; p != NULL; p = p->next) {
			int num = GPOINTER_TO_INT(p->data);
			EMsgComposerAttachment *attachment = g_list_nth_data(bar->priv->attachments, num);
			
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

	for (i=0;i<sizeof(emcab_popups)/sizeof(emcab_popups[0]);i++)
		menus = g_slist_prepend(menus, &emcab_popups[i]);

	emp = em_popup_new("org.gnome.evolution.mail.composer.attachmentBar");
	e_popup_add_items((EPopup *)emp, menus, emcab_popups_free, bar);
	t = em_popup_target_new_attachments(emp, attachments);
	t->target.widget = (GtkWidget *)bar;
	menu = e_popup_create_menu_once((EPopup *)emp, (EPopupTarget *)t, 0);

	if (event == NULL)
		gtk_menu_popup(menu, NULL, NULL, emcab_popup_position, bar, 0, gtk_get_current_event_time());
	else
		gtk_menu_popup(menu, NULL, NULL, NULL, NULL, event->button, event->time);
}

/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	EMsgComposerAttachmentBar *bar;
	
	bar = E_MSG_COMPOSER_ATTACHMENT_BAR (object);
	
	if (bar->priv) {
		free_attachment_list (bar);
		g_free (bar->priv);
		bar->priv = NULL;
	}
	
	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/* GtkWidget methods.  */

static gboolean 
popup_menu_event (GtkWidget *widget)
{
	emcab_popup((EMsgComposerAttachmentBar *)widget, NULL, -1);
	return TRUE;
}


static int
button_press_event (GtkWidget *widget, GdkEventButton *event)
{
	EMsgComposerAttachmentBar *bar = (EMsgComposerAttachmentBar *)widget;
	GnomeIconList *icon_list = GNOME_ICON_LIST(widget);
	int icon_number;
	
	if (event->button != 3)
		return GTK_WIDGET_CLASS (parent_class)->button_press_event (widget, event);
	
	icon_number = gnome_icon_list_get_icon_at (icon_list, event->x, event->y);
	if (icon_number >= 0) {
		gnome_icon_list_unselect_all(icon_list);
		gnome_icon_list_select_icon (icon_list, icon_number);
	}

	emcab_popup(bar, event, icon_number);
	
	return TRUE;
}

static gint
key_press_event(GtkWidget *widget, GdkEventKey *event)
{
        EMsgComposerAttachmentBar *bar = E_MSG_COMPOSER_ATTACHMENT_BAR(widget);

        if (event->keyval == GDK_Delete) {
                remove_selected (bar);
                return TRUE;
        }
                                                                                
        return GTK_WIDGET_CLASS (parent_class)->key_press_event (widget, event);
}


/* Initialization.  */

static void
class_init (EMsgComposerAttachmentBarClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GnomeIconListClass *icon_list_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);
	icon_list_class = GNOME_ICON_LIST_CLASS (klass);
	
	parent_class = g_type_class_ref (gnome_icon_list_get_type ());
	
	object_class->destroy = destroy;
	
	widget_class->button_press_event = button_press_event;
	widget_class->popup_menu = popup_menu_event;
	widget_class->key_press_event = key_press_event;

	
	/* Setup signals.  */
	
	signals[CHANGED] =
		g_signal_new ("changed",
			      E_TYPE_MSG_COMPOSER_ATTACHMENT_BAR,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EMsgComposerAttachmentBarClass, changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

static void
init (EMsgComposerAttachmentBar *bar)
{
	EMsgComposerAttachmentBarPrivate *priv;
	
	priv = g_new (EMsgComposerAttachmentBarPrivate, 1);
	
	priv->attachments = NULL;
	priv->num_attachments = 0;
	
	bar->priv = priv;
}


GType
e_msg_composer_attachment_bar_get_type (void)
{
	static GType type = 0;
	
	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (EMsgComposerAttachmentBarClass),
			NULL, NULL,
			(GClassInitFunc) class_init,
			NULL, NULL,
			sizeof (EMsgComposerAttachmentBar),
			0,
			(GInstanceInitFunc) init,
		};
		
		type = g_type_register_static (GNOME_TYPE_ICON_LIST, "EMsgComposerAttachmentBar", &info, 0);
	}
	
	return type;
}

GtkWidget *
e_msg_composer_attachment_bar_new (GtkAdjustment *adj)
{
	EMsgComposerAttachmentBar *new;
	GnomeIconList *icon_list;
	int width, height, icon_width, window_height;
	PangoFontMetrics *metrics;
	PangoContext *context;
	
	new = g_object_new (e_msg_composer_attachment_bar_get_type (), NULL);
	
	icon_list = GNOME_ICON_LIST (new);
	
	context = gtk_widget_get_pango_context ((GtkWidget *) new);
	metrics = pango_context_get_metrics (context, ((GtkWidget *) new)->style->font_desc, pango_context_get_language (context));
	width = PANGO_PIXELS (pango_font_metrics_get_approximate_char_width (metrics)) * 15;
	/* This should be *2, but the icon list creates too much space above ... */
	height = PANGO_PIXELS (pango_font_metrics_get_ascent (metrics) + pango_font_metrics_get_descent (metrics)) * 3;
	pango_font_metrics_unref (metrics);
	
	icon_width = ICON_WIDTH + ICON_SPACING + ICON_BORDER + ICON_TEXT_SPACING;
	icon_width = MAX (icon_width, width);
	
	gnome_icon_list_construct (icon_list, icon_width, adj, 0);
	
	window_height = ICON_WIDTH + ICON_SPACING + ICON_BORDER + ICON_TEXT_SPACING + height;
	gtk_widget_set_size_request (GTK_WIDGET (new), icon_width * 4, window_height);
	
	gnome_icon_list_set_separators (icon_list, ICON_SEPARATORS);
	gnome_icon_list_set_row_spacing (icon_list, ICON_ROW_SPACING);
	gnome_icon_list_set_col_spacing (icon_list, ICON_COL_SPACING);
	gnome_icon_list_set_icon_border (icon_list, ICON_BORDER);
	gnome_icon_list_set_text_spacing (icon_list, ICON_TEXT_SPACING);
	gnome_icon_list_set_selection_mode (icon_list, GTK_SELECTION_MULTIPLE);
	
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
		     EMsgComposerAttachment *attachment,
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
e_msg_composer_attachment_bar_to_multipart (EMsgComposerAttachmentBar *bar,
					    CamelMultipart *multipart,
					    const char *default_charset)
{
	EMsgComposerAttachmentBarPrivate *priv;
	GList *p;
	
	g_return_if_fail (E_IS_MSG_COMPOSER_ATTACHMENT_BAR (bar));
	g_return_if_fail (CAMEL_IS_MULTIPART (multipart));
	
	priv = bar->priv;
	
	for (p = priv->attachments; p != NULL; p = p->next) {
		EMsgComposerAttachment *attachment;
		
		attachment = E_MSG_COMPOSER_ATTACHMENT (p->data);
		attach_to_multipart (multipart, attachment, default_charset);
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
		add_from_file (bar, file_name, "attachment");
}

void
e_msg_composer_attachment_bar_attach_mime_part (EMsgComposerAttachmentBar *bar,
						CamelMimePart *part)
{
	g_return_if_fail (E_IS_MSG_COMPOSER_ATTACHMENT_BAR (bar));
	
	add_from_mime_part (bar, part);
}
