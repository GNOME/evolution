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


/* This is the object representing an email attachment.  It is implemented as a
   GtkObject to make it easier for the application to handle it.  For example,
   the "changed" signal is emitted whenever something changes in the
   attachment.  Also, this contains the code to let users edit the
   attachment manually. */

#include <sys/stat.h>
#include <errno.h>

#include <gtk/gtknotebook.h>
#include <gtk/gtktogglebutton.h>
#include <camel/camel.h>
#include <gal/widgets/e-unicode.h>
#include <libgnomevfs/gnome-vfs-mime.h>

#include "e-msg-composer.h"
#include "e-msg-composer-attachment.h"


enum {
	CHANGED,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL] = { 0 };

static GtkObjectClass *parent_class = NULL;


static void
changed (EMsgComposerAttachment *attachment)
{
	gtk_signal_emit (GTK_OBJECT (attachment), signals[CHANGED]);
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	EMsgComposerAttachment *attachment;

	attachment = E_MSG_COMPOSER_ATTACHMENT (object);
	
	camel_object_unref (CAMEL_OBJECT (attachment->body));
	if (attachment->pixbuf_cache != NULL)
		gdk_pixbuf_unref (attachment->pixbuf_cache);
}


/* Signals.  */

static void
real_changed (EMsgComposerAttachment *msg_composer_attachment)
{
	g_return_if_fail (msg_composer_attachment != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER_ATTACHMENT (msg_composer_attachment));
}


static void
class_init (EMsgComposerAttachmentClass *klass)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*) klass;

	parent_class = gtk_type_class (gtk_object_get_type ());

	object_class->destroy = destroy;

	signals[CHANGED] = gtk_signal_new ("changed",
					   GTK_RUN_FIRST,
					   object_class->type,
					   GTK_SIGNAL_OFFSET
					   	(EMsgComposerAttachmentClass,
						 changed),
					   gtk_marshal_NONE__NONE,
					   GTK_TYPE_NONE, 0);


	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

	klass->changed = real_changed;
}

static void
init (EMsgComposerAttachment *msg_composer_attachment)
{
	msg_composer_attachment->editor_gui = NULL;
	msg_composer_attachment->body = NULL;
	msg_composer_attachment->size = 0;
	msg_composer_attachment->pixbuf_cache = NULL;
}

GtkType
e_msg_composer_attachment_get_type (void)
{
	static GtkType type = 0;

	if (type == 0) {
		static const GtkTypeInfo info = {
			"EMsgComposerAttachment",
			sizeof (EMsgComposerAttachment),
			sizeof (EMsgComposerAttachmentClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		type = gtk_type_unique (gtk_object_get_type (), &info);
	}

	return type;
}


/**
 * e_msg_composer_attachment_new:
 * @file_name: filename to attach
 * @disposition: Content-Disposition of the attachment
 * @ex: exception
 *
 * Return value: the new attachment, or %NULL on error
 **/
EMsgComposerAttachment *
e_msg_composer_attachment_new (const char *file_name,
			       const char *disposition,
			       CamelException *ex)
{
	EMsgComposerAttachment *new;
	CamelMimePart *part;
	CamelDataWrapper *wrapper;
	CamelStream *stream;
	struct stat statbuf;
	char *mime_type;
	char *filename;
	
	g_return_val_if_fail (file_name != NULL, NULL);
	
	if (stat (file_name, &statbuf) < 0) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot attach file %s: %s"),
				      file_name, g_strerror (errno));
		return NULL;
	}
	
	/* return if it's not a regular file */
	if (!S_ISREG (statbuf.st_mode)) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot attach file %s: not a regular file"),
				      file_name);
		return NULL;
	}
	
	stream = camel_stream_fs_new_with_name (file_name, O_RDONLY, 0);
	if (!stream) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot attach file %s: %s"),
				      file_name, g_strerror (errno));
		return NULL;
	}
	
	wrapper = camel_data_wrapper_new ();
	camel_data_wrapper_construct_from_stream (wrapper, stream);
	
	mime_type = e_msg_composer_guess_mime_type (file_name);
	if (mime_type) {
		if (!strcasecmp (mime_type, "message/rfc822")) {
			camel_object_unref (wrapper);
			wrapper = (CamelDataWrapper *) camel_mime_message_new ();
			
			camel_stream_reset (stream);
			camel_data_wrapper_construct_from_stream (wrapper, stream);
		}
		
		camel_data_wrapper_set_mime_type (wrapper, mime_type);
		g_free (mime_type);
	} else
		camel_data_wrapper_set_mime_type (wrapper, "application/octet-stream");
	
	camel_object_unref (CAMEL_OBJECT (stream));
	
	part = camel_mime_part_new ();
	camel_medium_set_content_object (CAMEL_MEDIUM (part), wrapper);
	camel_object_unref (CAMEL_OBJECT (wrapper));
	
	camel_mime_part_set_disposition (part, disposition);
	filename = e_utf8_from_locale_string (g_basename (file_name));
	camel_mime_part_set_filename (part, filename);
	g_free (filename);
	
#if 0
	/* Note: Outlook 2002 is broken with respect to Content-Ids on
           non-multipart/related parts, so as an interoperability
           workwaround, don't set a Content-Id on these parts. Fixes
           bug #10032 */
	/* set the Content-Id */
	content_id = header_msgid_generate ();
	camel_mime_part_set_content_id (part, content_id);
	g_free (content_id);
#endif
	
	new = gtk_type_new (e_msg_composer_attachment_get_type ());
	new->editor_gui = NULL;
	new->body = part;
	new->size = statbuf.st_size;
	new->guessed_type = TRUE;
	
	return new;
}


/**
 * e_msg_composer_attachment_new_from_mime_part:
 * @part: a CamelMimePart
 * 
 * Return value: a new EMsgComposerAttachment based on the mime part
 **/
EMsgComposerAttachment *
e_msg_composer_attachment_new_from_mime_part (CamelMimePart *part)
{
	EMsgComposerAttachment *new;
	CamelMimePart *mime_part;
	CamelStream *stream;
	
	g_return_val_if_fail (CAMEL_IS_MIME_PART (part), NULL);
	
	stream = camel_stream_mem_new ();
	if (camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (part), stream) == -1) {
		camel_object_unref (stream);
		return NULL;
	}
	
	camel_stream_reset (stream);
	mime_part = camel_mime_part_new ();
	
	if (camel_data_wrapper_construct_from_stream (CAMEL_DATA_WRAPPER (mime_part), stream) == -1) {
		camel_object_unref (mime_part);
		camel_object_unref (stream);
		return NULL;
	}
	
	camel_object_unref (stream);
	
	new = gtk_type_new (e_msg_composer_attachment_get_type ());
	new->editor_gui = NULL;
	new->body = mime_part;
	new->guessed_type = FALSE;
	new->size = 0;
	
	return new;
}


/* The attachment property dialog.  */

struct _DialogData {
	GtkWidget *dialog;
	GtkEntry *file_name_entry;
	GtkEntry *description_entry;
	GtkEntry *mime_type_entry;
	GtkToggleButton *disposition_checkbox;
	EMsgComposerAttachment *attachment;
};
typedef struct _DialogData DialogData;

static void
destroy_dialog_data (DialogData *data)
{
	g_free (data);
}

/*
 * fixme: I am converting EVERYTHING to/from UTF-8, although mime types
 * are in ASCII. This is not strictly necessary, but we want to be
 * consistent and possibly check for errors somewhere.
 */

static void
update_mime_type (DialogData *data)
{
	const gchar *mime_type;
	gchar *file_name;

	if (!data->attachment->guessed_type)
		return;

	file_name = e_utf8_gtk_entry_get_text (data->file_name_entry);
	mime_type = gnome_vfs_mime_type_from_name_or_default (file_name, NULL);
	g_free (file_name);

	if (mime_type)
		e_utf8_gtk_entry_set_text (data->mime_type_entry, mime_type);
}

static void
set_entry (GladeXML *xml,
	   const gchar *widget_name,
	   const gchar *value)
{
	GtkEntry *entry;

	entry = GTK_ENTRY (glade_xml_get_widget (xml, widget_name));
	if (entry == NULL)
		g_warning ("Entry for `%s' not found.", widget_name);
	else
		e_utf8_gtk_entry_set_text (entry, value ? value : "");
}

static void
connect_widget (GladeXML *gui,
		const gchar *name,
		const gchar *signal_name,
		GtkSignalFunc func,
		gpointer data)
{
	GtkWidget *widget;

	widget = glade_xml_get_widget (gui, name);
	gtk_signal_connect (GTK_OBJECT (widget), signal_name, func, data);
}

static void
close_cb (GtkWidget *widget, gpointer data)
{
	EMsgComposerAttachment *attachment;
	DialogData *dialog_data;
	
	dialog_data = (DialogData *) data;
	attachment = dialog_data->attachment;
	
	gtk_widget_destroy (dialog_data->dialog);
	gtk_object_unref (GTK_OBJECT (attachment->editor_gui));
	attachment->editor_gui = NULL;
	
	gtk_object_unref (GTK_OBJECT (attachment));
	
	destroy_dialog_data (dialog_data);
}

static void
ok_cb (GtkWidget *widget, gpointer data)
{
	DialogData *dialog_data;
	EMsgComposerAttachment *attachment;
	char *str;
	
	dialog_data = (DialogData *) data;
	attachment = dialog_data->attachment;
	
	str = e_utf8_gtk_entry_get_text (dialog_data->file_name_entry);
	camel_mime_part_set_filename (attachment->body, str);
	g_free (str);
	
	str = e_utf8_gtk_entry_get_text (dialog_data->description_entry);
	camel_mime_part_set_description (attachment->body, str);
	g_free (str);
	
	str = e_utf8_gtk_entry_get_text (dialog_data->mime_type_entry);
	camel_mime_part_set_content_type (attachment->body, str);
	
	camel_data_wrapper_set_mime_type (
		camel_medium_get_content_object (CAMEL_MEDIUM (attachment->body)), str);
	g_free (str);
	
	switch (gtk_toggle_button_get_active (dialog_data->disposition_checkbox)) {
	case 0:
		camel_mime_part_set_disposition (attachment->body, "attachment");
		break;
	case 1:
		camel_mime_part_set_disposition (attachment->body, "inline");
		break;
	default:
		/* Hmmmm? */
		break;
	}
	
	changed (attachment);
	close_cb (widget, data);
}

static void
file_name_focus_out_cb (GtkWidget *widget,
			GdkEventFocus *event,
			gpointer data)
{
	DialogData *dialog_data;
	
	dialog_data = (DialogData *) data;
	update_mime_type (dialog_data);
}


void
e_msg_composer_attachment_edit (EMsgComposerAttachment *attachment,
				GtkWidget *parent)
{
	CamelContentType *content_type;
	DialogData *dialog_data;
	const char *disposition;
	GladeXML *editor_gui;
	char *type;
	
	g_return_if_fail (E_IS_MSG_COMPOSER_ATTACHMENT (attachment));
	
	if (attachment->editor_gui != NULL) {
		GtkWidget *window;
		
		window = glade_xml_get_widget (attachment->editor_gui,
					       "dialog");
		gdk_window_show (window->window);
		return;
	}
	
	editor_gui = glade_xml_new (E_GLADEDIR "/e-msg-composer-attachment.glade",
				    NULL);
	if (editor_gui == NULL) {
		g_warning ("Cannot load `e-msg-composer-attachment.glade'");
		return;
	}
	
	attachment->editor_gui = editor_gui;
	
	gtk_window_set_transient_for
		(GTK_WINDOW (glade_xml_get_widget (editor_gui, "dialog")),
		 GTK_WINDOW (gtk_widget_get_toplevel (parent)));
	
	dialog_data = g_new (DialogData, 1);
	dialog_data->attachment = attachment;
	gtk_object_ref (GTK_OBJECT (attachment));
	dialog_data->dialog = glade_xml_get_widget (editor_gui, "dialog");
	dialog_data->file_name_entry = GTK_ENTRY (
		glade_xml_get_widget (editor_gui, "file_name_entry"));
	dialog_data->description_entry = GTK_ENTRY (
		glade_xml_get_widget (editor_gui, "description_entry"));
	dialog_data->mime_type_entry = GTK_ENTRY (
		glade_xml_get_widget (editor_gui, "mime_type_entry"));
	dialog_data->disposition_checkbox = GTK_TOGGLE_BUTTON (
		glade_xml_get_widget (editor_gui, "disposition_checkbox"));
	
	set_entry (editor_gui, "file_name_entry",
		   camel_mime_part_get_filename (attachment->body));
	set_entry (editor_gui, "description_entry",
		   camel_mime_part_get_description (attachment->body));
	content_type = camel_mime_part_get_content_type (attachment->body);
	type = header_content_type_simple (content_type);
	set_entry (editor_gui, "mime_type_entry", type);
	g_free (type);
	
	disposition = camel_mime_part_get_disposition (attachment->body);
	gtk_toggle_button_set_active (dialog_data->disposition_checkbox,
				      disposition && !g_strcasecmp (disposition, "inline"));
	
	connect_widget (editor_gui, "ok_button", "clicked", ok_cb, dialog_data);
	connect_widget (editor_gui, "close_button", "clicked", close_cb, dialog_data);
	
	connect_widget (editor_gui, "file_name_entry", "focus_out_event",
			file_name_focus_out_cb, dialog_data);
	
	/* make sure that when the composer gets hidden/closed that our windows also close */
	parent = gtk_widget_get_toplevel (parent);
	gtk_signal_connect_while_alive (GTK_OBJECT (parent), "destroy", close_cb, dialog_data,
					GTK_OBJECT (dialog_data->dialog));
	gtk_signal_connect_while_alive (GTK_OBJECT (parent), "hide", close_cb, dialog_data,
					GTK_OBJECT (dialog_data->dialog));
}
