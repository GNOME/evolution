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
init_mime_type (EMsgComposerAttachment *attachment)
{
	attachment->mime_type = g_strdup (get_mime_type (attachment->file_name));
}

static void
set_mime_type (EMsgComposerAttachment *attachment)
{
	g_free (attachment->mime_type);
	init_mime_type (attachment);
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

	g_free (attachment->file_name);
	g_free (attachment->description);
	g_free (attachment->mime_type);
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
	msg_composer_attachment->file_name = NULL;
	msg_composer_attachment->description = NULL;
	msg_composer_attachment->mime_type = NULL;
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
	struct stat statbuf;

	g_return_val_if_fail (file_name != NULL, NULL);

	new = gtk_type_new (e_msg_composer_attachment_get_type ());

	new->editor_gui = NULL;

	new->file_name = g_strdup (file_name);
	new->description = g_strdup (g_basename (new->file_name));

	if (stat (file_name, &statbuf) < 0)
		new->size = 0;
	else
		new->size = statbuf.st_size;

	init_mime_type (new);

	return new;
}


/* The attachment property dialog.  */

struct _DialogData {
	GtkWidget *dialog;
	GtkEntry *file_name_entry;
	GtkEntry *description_entry;
	GtkEntry *mime_type_entry;
	GtkWidget *browse_widget;
	EMsgComposerAttachment *attachment;
};
typedef struct _DialogData DialogData;

static void
destroy_dialog_data (DialogData *data)
{
	if (data->browse_widget != NULL)
		gtk_widget_destroy (data->browse_widget);
	g_free (data);
}

static void
update_mime_type (DialogData *data)
{
	const gchar *mime_type;
	const gchar *file_name;

	file_name = gtk_entry_get_text (data->file_name_entry);
	mime_type = get_mime_type (file_name);

	gtk_entry_set_text (data->mime_type_entry, mime_type);
}

static void
browse_ok_cb (GtkWidget *widget,
	      gpointer data)
{
	GtkWidget *file_selection;
	DialogData *dialog_data;
	const gchar *file_name;

	dialog_data = (DialogData *) data;
	file_selection = gtk_widget_get_toplevel (widget);

	file_name = gtk_file_selection_get_filename
					(GTK_FILE_SELECTION (file_selection));

	gtk_entry_set_text (dialog_data->file_name_entry, file_name);

	update_mime_type (dialog_data);

	gtk_widget_hide (file_selection);
}

static void
browse (DialogData *data)
{
	if (data->browse_widget == NULL) {
		GtkWidget *file_selection;
		GtkWidget *cancel_button;
		GtkWidget *ok_button;

		file_selection
			= gtk_file_selection_new (_("Select attachment"));
		gtk_window_set_position (GTK_WINDOW (file_selection),
					 GTK_WIN_POS_MOUSE);
		gtk_window_set_transient_for (GTK_WINDOW (file_selection),
					      GTK_WINDOW (data->dialog));

		ok_button = GTK_FILE_SELECTION (file_selection)->ok_button;
		gtk_signal_connect (GTK_OBJECT (ok_button),
				    "clicked", GTK_SIGNAL_FUNC (browse_ok_cb),
				    data);

		cancel_button
			= GTK_FILE_SELECTION (file_selection)->cancel_button;
		gtk_signal_connect_object (GTK_OBJECT (cancel_button),
					   "clicked",
					   GTK_SIGNAL_FUNC (gtk_widget_hide),
					   GTK_OBJECT (file_selection));

		data->browse_widget = file_selection;
	}

	gtk_widget_show (GTK_WIDGET (data->browse_widget));
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
	gtk_entry_set_text (entry, value);
}

static void
connect_entry_changed (GladeXML *gui,
		       const gchar *name,
		       GtkSignalFunc func,
		       gpointer data)
{
	GtkWidget *widget;

	widget = glade_xml_get_widget (gui, name);
	gtk_signal_connect (GTK_OBJECT (widget), "changed", func, data);
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
apply (DialogData *data)
{
	EMsgComposerAttachment *attachment;

	attachment = data->attachment;

	g_free (attachment->file_name);
	attachment->file_name = g_strdup (gtk_entry_get_text
					  (data->file_name_entry));

	g_free (attachment->description);
	attachment->description = g_strdup (gtk_entry_get_text
					    (data->description_entry));

	g_free (attachment->mime_type);
	attachment->mime_type = g_strdup (gtk_entry_get_text
					  (data->mime_type_entry));

	changed (attachment);
}

static void
entry_changed_cb (GtkWidget *widget, gpointer data)
{
	DialogData *dialog_data;
	GladeXML *gui;
	GtkWidget *apply_button;

	dialog_data = (DialogData *) data;
	gui = dialog_data->attachment->editor_gui;

	apply_button = glade_xml_get_widget (gui, "apply_button");
	gtk_widget_set_sensitive (apply_button, TRUE);
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
apply_cb (GtkWidget *widget,
	  gpointer data)
{
	DialogData *dialog_data;

	dialog_data = (DialogData *) data;
	apply (dialog_data);
}

static void
ok_cb (GtkWidget *widget,
       gpointer data)
{
	apply_cb (widget, data);
	close_cb (widget, data);
}

static void
browse_cb (GtkWidget *widget,
	   gpointer data)
{
	DialogData *dialog_data;

	dialog_data = (DialogData *) data;
	browse (dialog_data);
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

	editor_gui = glade_xml_new (E_GUIDIR "/e-msg-composer-attachment.glade",
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
	dialog_data->browse_widget = NULL;
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
		set_entry (editor_gui, "file_name_entry", attachment->file_name);
		set_entry (editor_gui, "description_entry", attachment->description);
		set_entry (editor_gui, "mime_type_entry", attachment->mime_type);
	}

	connect_entry_changed (editor_gui, "file_name_entry",
			       entry_changed_cb, dialog_data);
	connect_entry_changed (editor_gui, "description_entry",
			       entry_changed_cb, dialog_data);

	connect_widget (editor_gui, "ok_button", "clicked", ok_cb, dialog_data);
	connect_widget (editor_gui, "apply_button", "clicked", apply_cb, dialog_data);
	connect_widget (editor_gui, "close_button", "clicked", close_cb, dialog_data);

	connect_widget (editor_gui, "browse_button", "clicked", browse_cb, dialog_data);

	connect_widget (editor_gui, "file_name_entry", "focus_out_event",
			file_name_focus_out_cb, dialog_data);
}
