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

#include <gnome.h>
#include <glade/glade.h>

#include "e-msg-composer-attachment.h"
#include "e-msg-composer-attachment-bar.h"
#include "camel/camel-data-wrapper.h"
#include "camel/camel-stream-fs.h"
#include "camel/camel-mime-part.h"


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
		const gchar *icon_name, *desc;
		gchar *size_string, *label, *mime_type;
		GMimeContentField *content_type;

		attachment = p->data;
		content_type = camel_mime_part_get_content_type (attachment->body);
		mime_type = g_strdup_printf ("%s/%s", content_type->type,
					     content_type->subtype);
		icon_name = gnome_mime_get_value (mime_type, "icon-filename");
		g_free (mime_type);

		/* FIXME we need some better default icon.  */
		if (icon_name == NULL)
			icon_name = gnome_mime_get_value ("text/plain",
							  "icon-filename");

		desc = camel_mime_part_get_description (attachment->body);
		if (!desc)
			desc = camel_mime_part_get_filename (attachment->body);
		if (!desc)
			desc = "attachment";

		if (attachment->size) {
			size_string = size_to_string (attachment->size);
			label = g_strdup_printf ("%s (%s)", desc, size_string);
			g_free (size_string);
		} else
			label = g_strdup (desc);

		gnome_icon_list_append (icon_list, icon_name, label);
		g_free (label);
	}

	gnome_icon_list_thaw (icon_list);
}

static void
remove_selected (EMsgComposerAttachmentBar *bar)
{
	GnomeIconList *icon_list;
	EMsgComposerAttachment *attachment;
	GList *attachment_list;
	GList *p;
	gint num;

	icon_list = GNOME_ICON_LIST (bar);

	/* Weee!  I am especially proud of this piece of cheesy code: it is
           truly awful.  But unless one attaches a huge number of files, it
           will not be as greedy as intended.  FIXME of course.  */

	attachment_list = NULL;
	for (p = icon_list->selection; p != NULL; p = p->next) {
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
	GnomeIconList *icon_list;
	EMsgComposerAttachment *attachment;
	gint num;

	icon_list = GNOME_ICON_LIST (bar);

	num = GPOINTER_TO_INT (icon_list->selection->data);
	attachment = g_list_nth (bar->priv->attachments, num)->data;

	e_msg_composer_attachment_edit (attachment, GTK_WIDGET (bar));
}


/* "Attach" dialog.  */

static void
attach_cb (GtkWidget *widget,
	   gpointer data)
{
	EMsgComposerAttachmentBar *bar;
	GtkWidget *file_selection;
	const gchar *file_name;

	file_selection = gtk_widget_get_toplevel (widget);
	bar = E_MSG_COMPOSER_ATTACHMENT_BAR (data);

	file_name = gtk_file_selection_get_filename
		                          (GTK_FILE_SELECTION (file_selection));
	add_from_file (bar, file_name);

	gtk_widget_hide (file_selection);
}

static void
add_from_user (EMsgComposerAttachmentBar *bar)
{
	static GtkWidget *fs;

	if (!fs) {
		GtkWidget *cancel_button;
		GtkWidget *ok_button;

		fs = gtk_file_selection_new (_("Add attachment"));

		ok_button = GTK_FILE_SELECTION (fs)->ok_button;
		gtk_signal_connect (GTK_OBJECT (ok_button),
				    "clicked", GTK_SIGNAL_FUNC (attach_cb),
				    bar);

		cancel_button = GTK_FILE_SELECTION (fs)->cancel_button;
		gtk_signal_connect_object (GTK_OBJECT (cancel_button),
					   "clicked",
					   GTK_SIGNAL_FUNC (gtk_widget_hide),
					   GTK_OBJECT (fs));

		gtk_signal_connect (GTK_OBJECT (fs), "delete_event",
				    GTK_SIGNAL_FUNC (gtk_widget_hide), NULL);
	} else
		gtk_file_selection_set_filename (GTK_FILE_SELECTION (fs), "");

	gtk_window_set_position (GTK_WINDOW (fs), GTK_WIN_POS_MOUSE);

	gtk_widget_show (GTK_WIDGET (fs));
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
	GnomeIconList *icon_list;
	gint icon_number;

	bar = E_MSG_COMPOSER_ATTACHMENT_BAR (widget);
	icon_list = GNOME_ICON_LIST (widget);

	if (event->button != 3)
		return GTK_WIDGET_CLASS (parent_class)->button_press_event
							        (widget, event);

	icon_number = gnome_icon_list_get_icon_at (icon_list,
						   event->x, event->y);

	if (icon_number >= 0) {
		gnome_icon_list_select_icon (icon_list, icon_number);
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
	GnomeIconListClass *icon_list_class;

	object_class = GTK_OBJECT_CLASS (class);
	widget_class = GTK_WIDGET_CLASS (class);
	icon_list_class = GNOME_ICON_LIST_CLASS (class);

	parent_class = gtk_type_class (gnome_icon_list_get_type ());

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
	guint icon_size;

	priv = g_new (EMsgComposerAttachmentBarPrivate, 1);

	priv->attachments = NULL;
	priv->context_menu = NULL;
	priv->icon_context_menu = NULL;

	priv->num_attachments = 0;

	bar->priv = priv;

	/* FIXME partly hardcoded.  We should compute height from the font, and
           allow at least 2 lines for every item.  */
	icon_size = ICON_WIDTH + ICON_SPACING + ICON_BORDER + ICON_TEXT_SPACING;
	icon_size += 24;

	gtk_widget_set_usize (GTK_WIDGET (bar), icon_size * 4, icon_size);
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

		type = gtk_type_unique (gnome_icon_list_get_type (), &info);
	}

	return type;
}

GtkWidget *
e_msg_composer_attachment_bar_new (GtkAdjustment *adj)
{
	EMsgComposerAttachmentBar *new;
	GnomeIconList *icon_list;

	gtk_widget_push_visual (gdk_imlib_get_visual ());
	gtk_widget_push_colormap (gdk_imlib_get_colormap ());
	new = gtk_type_new (e_msg_composer_attachment_bar_get_type ());
	gtk_widget_pop_visual ();
	gtk_widget_pop_colormap ();

	icon_list = GNOME_ICON_LIST (new);

	gnome_icon_list_construct (icon_list, ICON_WIDTH, adj, 0);

	gnome_icon_list_set_separators (icon_list, ICON_SEPARATORS);
	gnome_icon_list_set_row_spacing (icon_list, ICON_ROW_SPACING);
	gnome_icon_list_set_col_spacing (icon_list, ICON_COL_SPACING);
	gnome_icon_list_set_icon_border (icon_list, ICON_BORDER);
	gnome_icon_list_set_text_spacing (icon_list, ICON_TEXT_SPACING);
	gnome_icon_list_set_selection_mode (icon_list, GTK_SELECTION_MULTIPLE);

	return GTK_WIDGET (new);
}


static void
attach_to_multipart (CamelMultipart *multipart,
		     EMsgComposerAttachment *attachment)
{
	GMimeContentField *content_type;

	content_type = camel_mime_part_get_content_type (attachment->body);

	/* Kludge a bit on CTE. For now, we set QP for text and B64
	 * for all else except message (which must be 7bit, 8bit, or
	 * binary). FIXME.
	 */
	if (!g_strcasecmp (content_type->type, "text")) {
		camel_mime_part_set_encoding (attachment->body,
					      CAMEL_MIME_PART_ENCODING_QUOTEDPRINTABLE);
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
