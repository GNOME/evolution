/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-msg-composer-attachment.c
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

/* This is the object representing an email attachment.  It is implemented as a
   GtkObject to make it easier for the application to handle it.  For example,
   the "changed" signal is emitted whenever something changes in the
   attachment.  Also, this contains the code to let users edit the
   attachment manually. */

#include <sys/stat.h>

#include <gnome.h>
#include <camel/camel.h>

#include "e-msg-composer-attachment.h"


enum {
	CHANGED,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL] = { 0 };

static GtkObjectClass *parent_class = NULL;


/* Utility functions.  */

static const gchar *
get_mime_type (const gchar *file_name)
{
	const gchar *mime_type;

	mime_type = gnome_mime_type_of_file (file_name);
	if (mime_type == NULL)
		mime_type = "application/octet-stream";

	return mime_type;
}

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
 * @file_name: 
 * 
 * Return value: 
 **/
EMsgComposerAttachment *
e_msg_composer_attachment_new (const gchar *file_name)
{
	EMsgComposerAttachment *new;
	CamelMimePart *part;
	CamelDataWrapper *wrapper;
	CamelStream *data;
	struct stat statbuf;

	g_return_val_if_fail (file_name != NULL, NULL);

	data = camel_stream_fs_new_with_name (file_name, O_RDONLY, 0);
	if (!data)
		return NULL;
	wrapper = camel_data_wrapper_new ();
	camel_data_wrapper_construct_from_stream (wrapper, data);
	camel_object_unref (CAMEL_OBJECT (data));
	camel_data_wrapper_set_mime_type (wrapper, get_mime_type (file_name));

	part = camel_mime_part_new ();
	camel_medium_set_content_object (CAMEL_MEDIUM (part), wrapper);
	camel_object_unref (CAMEL_OBJECT (wrapper));

	camel_mime_part_set_disposition (part, "attachment");
	if (strchr (file_name, '/'))
		camel_mime_part_set_filename (part, strrchr (file_name, '/') + 1);
	else
		camel_mime_part_set_filename (part, file_name);

	new = e_msg_composer_attachment_new_from_mime_part (part);
	if (stat (file_name, &statbuf) < 0)
		new->size = 0;
	else
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

	g_return_val_if_fail (CAMEL_IS_MIME_PART (part), NULL);

	new = gtk_type_new (e_msg_composer_attachment_get_type ());

	new->editor_gui = NULL;
	new->body = part;
	camel_object_ref (CAMEL_OBJECT (part));
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
	EMsgComposerAttachment *attachment;
};
typedef struct _DialogData DialogData;

static void
destroy_dialog_data (DialogData *data)
{
	g_free (data);
}

static void
update_mime_type (DialogData *data)
{
	const gchar *mime_type;
	const gchar *file_name;

	if (!data->attachment->guessed_type)
		return;

	file_name = gtk_entry_get_text (data->file_name_entry);
	mime_type = get_mime_type (file_name);

	gtk_entry_set_text (data->mime_type_entry, mime_type);
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
	gtk_entry_set_text (entry, value ? value : "");
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
close_cb (GtkWidget *widget,
	  gpointer data)
{
	EMsgComposerAttachment *attachment;
	DialogData *dialog_data;

	dialog_data = (DialogData *) data;
	attachment = dialog_data->attachment;

	gtk_widget_destroy (glade_xml_get_widget (attachment->editor_gui,
						  "dialog"));
	gtk_object_unref (GTK_OBJECT (attachment->editor_gui));
	attachment->editor_gui = NULL;

	destroy_dialog_data (dialog_data);
}

static void
ok_cb (GtkWidget *widget,
       gpointer data)
{
	DialogData *dialog_data;
	EMsgComposerAttachment *attachment;

	dialog_data = (DialogData *) data;
	attachment = dialog_data->attachment;

	camel_mime_part_set_filename (attachment->body, gtk_entry_get_text
				      (dialog_data->file_name_entry));

	camel_mime_part_set_description (attachment->body, gtk_entry_get_text
					 (dialog_data->description_entry));

	camel_mime_part_set_content_type (attachment->body, gtk_entry_get_text
					  (dialog_data->mime_type_entry));
	camel_data_wrapper_set_mime_type (
		camel_medium_get_content_object (CAMEL_MEDIUM (attachment->body)),
		gtk_entry_get_text (dialog_data->mime_type_entry));

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
	DialogData *dialog_data;
	GladeXML *editor_gui;

	g_return_if_fail (attachment != NULL);
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
	dialog_data->dialog = glade_xml_get_widget (editor_gui, "dialog");
	dialog_data->file_name_entry = GTK_ENTRY (glade_xml_get_widget
						  (editor_gui,
						   "file_name_entry"));
	dialog_data->description_entry = GTK_ENTRY (glade_xml_get_widget
						    (editor_gui,
						     "description_entry"));
	dialog_data->mime_type_entry = GTK_ENTRY (glade_xml_get_widget
						  (editor_gui,
						   "mime_type_entry"));

	if (attachment != NULL) {
		GMimeContentField *content_type;
		char *type;

		set_entry (editor_gui, "file_name_entry",
			   camel_mime_part_get_filename (attachment->body));
		set_entry (editor_gui, "description_entry",
			   camel_mime_part_get_description (attachment->body));
		content_type = camel_mime_part_get_content_type (attachment->body);
		type = g_strdup_printf ("%s/%s", content_type->type,
					content_type->subtype);
		set_entry (editor_gui, "mime_type_entry", type);
		g_free (type);
	}

	connect_widget (editor_gui, "ok_button", "clicked", ok_cb, dialog_data);
	connect_widget (editor_gui, "close_button", "clicked", close_cb, dialog_data);

	connect_widget (editor_gui, "file_name_entry", "focus_out_event",
			file_name_focus_out_cb, dialog_data);
}
