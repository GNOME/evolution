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
   GObject to make it easier for the application to handle it.  For example,
   the "changed" signal is emitted whenever something changes in the
   attachment.  Also, this contains the code to let users edit the
   attachment manually. */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#include <camel/camel.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkdialog.h>
#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnome/gnome-i18n.h>

#include "e-util/e-mktemp.h"

#include "e-msg-composer.h"
#include "e-msg-composer-attachment.h"


enum {
	CHANGED,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;


static void
changed (EMsgComposerAttachment *attachment)
{
	g_signal_emit (attachment, signals[CHANGED], 0);
}


/* GtkObject methods.  */

static void
finalise(GObject *object)
{
	EMsgComposerAttachment *attachment;
	
	attachment = E_MSG_COMPOSER_ATTACHMENT (object);
	
	camel_object_unref (attachment->body);
	if (attachment->pixbuf_cache != NULL)
		g_object_unref (attachment->pixbuf_cache);
	
        G_OBJECT_CLASS (parent_class)->finalize (object);
}


/* Signals.  */

static void
real_changed (EMsgComposerAttachment *msg_composer_attachment)
{
	g_return_if_fail (E_IS_MSG_COMPOSER_ATTACHMENT (msg_composer_attachment));
}


static void
class_init (EMsgComposerAttachmentClass *klass)
{
	GObjectClass *object_class;
	
	object_class = (GObjectClass*) klass;
	parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = finalise;
	klass->changed = real_changed;
	
	signals[CHANGED] = g_signal_new ("changed",
					 E_TYPE_MSG_COMPOSER_ATTACHMENT,
					 G_SIGNAL_RUN_FIRST,
					 G_STRUCT_OFFSET (EMsgComposerAttachmentClass, changed),
					 NULL,
					 NULL,
					 g_cclosure_marshal_VOID__VOID,
					 G_TYPE_NONE, 0);
}

static void
init (EMsgComposerAttachment *msg_composer_attachment)
{
	msg_composer_attachment->editor_gui = NULL;
	msg_composer_attachment->body = NULL;
	msg_composer_attachment->size = 0;
	msg_composer_attachment->pixbuf_cache = NULL;
}

GType
e_msg_composer_attachment_get_type (void)
{
	static GType type = 0;
	
	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (EMsgComposerAttachmentClass),
			NULL,
			NULL,
			(GClassInitFunc) class_init,
			NULL,
			NULL,
			sizeof (EMsgComposerAttachment),
			0,
			(GInstanceInitFunc) init,
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "EMsgComposerAttachment", &info, 0);
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
	
	mime_type = e_msg_composer_guess_mime_type (file_name);
	if (mime_type) {
		if (!g_ascii_strcasecmp (mime_type, "message/rfc822")) {
			wrapper = (CamelDataWrapper *) camel_mime_message_new ();
		} else {
			wrapper = camel_data_wrapper_new ();
		}
		
		camel_data_wrapper_construct_from_stream (wrapper, stream);
		camel_data_wrapper_set_mime_type (wrapper, mime_type);
		g_free (mime_type);
	} else {
		wrapper = camel_data_wrapper_new ();
		camel_data_wrapper_construct_from_stream (wrapper, stream);
		camel_data_wrapper_set_mime_type (wrapper, "application/octet-stream");
	}
	
	camel_object_unref (stream);
	
	part = camel_mime_part_new ();
	camel_medium_set_content_object (CAMEL_MEDIUM (part), wrapper);
	camel_object_unref (wrapper);
	
	camel_mime_part_set_disposition (part, disposition);
	filename = g_path_get_basename (file_name);
	camel_mime_part_set_filename (part, filename);
	g_free (filename);
	
#if 0
	/* Note: Outlook 2002 is broken with respect to Content-Ids on
           non-multipart/related parts, so as an interoperability
           workaround, don't set a Content-Id on these parts. Fixes
           bug #10032 */
	/* set the Content-Id */
	content_id = camel_header_msgid_generate ();
	camel_mime_part_set_content_id (part, content_id);
	g_free (content_id);
#endif
	
	new = g_object_new (E_TYPE_MSG_COMPOSER_ATTACHMENT, NULL);
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
	
	new = g_object_new (E_TYPE_MSG_COMPOSER_ATTACHMENT, NULL);
	new->editor_gui = NULL;
	new->body = mime_part;
	new->guessed_type = FALSE;
	new->size = 0;
	
	return new;
}


/* The attachment property dialog.  */

typedef struct {
	GtkWidget *dialog;
	GtkEntry *file_name_entry;
	GtkEntry *description_entry;
	GtkEntry *mime_type_entry;
	GtkToggleButton *disposition_checkbox;
	EMsgComposerAttachment *attachment;
} DialogData;

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
set_entry (GladeXML *xml, const char *widget_name, const char *value)
{
	GtkEntry *entry;
	
	entry = GTK_ENTRY (glade_xml_get_widget (xml, widget_name));
	if (entry == NULL)
		g_warning ("Entry for `%s' not found.", widget_name);
	else
		gtk_entry_set_text (entry, value ? value : "");
}

static void
connect_widget (GladeXML *gui, const char *name, const char *signal_name,
		GCallback func, gpointer data)
{
	GtkWidget *widget;
	
	widget = glade_xml_get_widget (gui, name);
	g_signal_connect (widget, signal_name, func, data);
}

static void
close_cb (GtkWidget *widget, gpointer data)
{
	EMsgComposerAttachment *attachment;
	DialogData *dialog_data;
	
	dialog_data = (DialogData *) data;
	attachment = dialog_data->attachment;
	
	gtk_widget_destroy (dialog_data->dialog);
	g_object_unref (attachment->editor_gui);
	attachment->editor_gui = NULL;
	
	g_object_unref (attachment);
	
	destroy_dialog_data (dialog_data);
}

static void
ok_cb (GtkWidget *widget, gpointer data)
{
	DialogData *dialog_data;
	EMsgComposerAttachment *attachment;
	const char *str;
	
	dialog_data = (DialogData *) data;
	attachment = dialog_data->attachment;
	
	str = gtk_entry_get_text (dialog_data->file_name_entry);
	camel_mime_part_set_filename (attachment->body, str);
	
	str = gtk_entry_get_text (dialog_data->description_entry);
	camel_mime_part_set_description (attachment->body, str);
	
	str = gtk_entry_get_text (dialog_data->mime_type_entry);
	camel_mime_part_set_content_type (attachment->body, str);
	
	camel_data_wrapper_set_mime_type(camel_medium_get_content_object(CAMEL_MEDIUM (attachment->body)), str);
	
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
response_cb (GtkWidget *widget, gint response, gpointer data)
{
	if (response == GTK_RESPONSE_OK)
		ok_cb (widget, data);
	else
		close_cb (widget, data);
}

void
e_msg_composer_attachment_edit (EMsgComposerAttachment *attachment, GtkWidget *parent)
{
	CamelContentType *content_type;
	const char *disposition;
	DialogData *dialog_data;
	GladeXML *editor_gui;
	char *type;
	
	g_return_if_fail (attachment != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER_ATTACHMENT (attachment));
	
	if (attachment->editor_gui != NULL) {
		GtkWidget *window;
		
		window = glade_xml_get_widget (attachment->editor_gui,
					       "dialog");
		gdk_window_show (window->window);
		return;
	}
	
	editor_gui = glade_xml_new (EVOLUTION_GLADEDIR "/e-msg-composer-attachment.glade",
				    NULL, NULL);
	if (editor_gui == NULL) {
		g_warning ("Cannot load `e-msg-composer-attachment.glade'");
		return;
	}
	
	attachment->editor_gui = editor_gui;
	
	gtk_window_set_transient_for
		(GTK_WINDOW (glade_xml_get_widget (editor_gui, "dialog")),
		 GTK_WINDOW (gtk_widget_get_toplevel (parent)));
	
	dialog_data = g_new (DialogData, 1);
	g_object_ref (attachment);
	dialog_data->attachment = attachment;
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
	type = camel_content_type_simple (content_type);
	set_entry (editor_gui, "mime_type_entry", type);
	g_free (type);
	
	disposition = camel_mime_part_get_disposition (attachment->body);
	gtk_toggle_button_set_active (dialog_data->disposition_checkbox,
				      disposition && !g_ascii_strcasecmp (disposition, "inline"));
	
	connect_widget (editor_gui, "dialog", "response", (GCallback)response_cb, dialog_data);
#warning "signal connect while alive"	
	/* make sure that when the composer gets hidden/closed that our windows also close */
	parent = gtk_widget_get_toplevel (parent);
	gtk_signal_connect_while_alive (GTK_OBJECT (parent), "destroy", (GCallback)close_cb, dialog_data,
					GTK_OBJECT (dialog_data->dialog));
	gtk_signal_connect_while_alive (GTK_OBJECT (parent), "hide", (GCallback)close_cb, dialog_data,
					GTK_OBJECT (dialog_data->dialog));
}
