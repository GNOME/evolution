/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Radek Doulik <rodo@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
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

#include <errno.h>
#include <bonobo.h>
#include <bonobo/bonobo-stream-memory.h>

#include <gal/widgets/e-gui-utils.h>
#include <gal/widgets/e-unicode.h>

#include "e-msg-composer.h"
#include "mail-signature-editor.h"
#include "mail-config.h"

/*
 * Signature editor
 *
 */

struct _ESignatureEditor {
	GtkWidget *win;
	GtkWidget *control;
	GtkWidget *name_entry;
	GtkWidget *info_frame;
	
	MailConfigSignature *sig;
	gboolean html;

	GNOME_GtkHTML_Editor_Engine engine;
};
typedef struct _ESignatureEditor ESignatureEditor;

#define E_SIGNATURE_EDITOR(o) ((ESignatureEditor *) o)

#define DEFAULT_WIDTH 600
#define DEFAULT_HEIGHT 350

enum { REPLY_YES = 0, REPLY_NO, REPLY_CANCEL };

static void
destroy_editor (ESignatureEditor *editor)
{
	gtk_widget_destroy (editor->win);
	g_free (editor);
}

static void
menu_file_save_error (BonoboUIComponent *uic, CORBA_Environment *ev)
{
	const char *err;
	
	/* errno is set if the rename() fails in menu_file_save_cb */
	
	err = ev->_major != CORBA_NO_EXCEPTION ? bonobo_exception_get_text (ev) : g_strerror (errno);
	
	e_notice (GTK_WINDOW (uic), GNOME_MESSAGE_BOX_ERROR,
		  _("Could not save signature file: %s"), err);
	
	g_warning ("Exception while saving signature: %s", err);
}

static void
menu_file_save_cb (BonoboUIComponent *uic,
		   void *data,
		   const char *path)
{
	ESignatureEditor *editor;
	CORBA_Environment ev;
	char *filename;
	char *dirname;
	
	editor = E_SIGNATURE_EDITOR (data);
	
	printf ("editor->sig->filename = %s\n", editor->sig->filename);
	dirname = g_dirname (editor->sig->filename);
	printf ("dirname = %s\n", dirname);
	filename = g_basename (editor->sig->filename);
	printf ("basename = %s\n", filename);
	filename = g_strdup_printf ("%s/.#%s", dirname, filename);
	printf ("filename = %s\n", filename);
	g_free (dirname);
	
	CORBA_exception_init (&ev);
	
	if (editor->html) {
		Bonobo_PersistFile pfile_iface;
		
		pfile_iface = bonobo_object_client_query_interface (bonobo_widget_get_server (BONOBO_WIDGET (editor->control)),
								    "IDL:Bonobo/PersistFile:1.0", NULL);
		Bonobo_PersistFile_save (pfile_iface, filename, &ev);
	} else {
		Bonobo_PersistStream pstream_iface;
		BonoboStream *stream;
		
		stream = bonobo_stream_open (BONOBO_IO_DRIVER_FS, filename,
					     Bonobo_Storage_WRITE | Bonobo_Storage_CREATE, 0644);
		BONOBO_STREAM_CLASS (GTK_OBJECT (stream)->klass)->truncate (stream, 0, &ev);
		
		pstream_iface = bonobo_object_client_query_interface
			(bonobo_widget_get_server (BONOBO_WIDGET (editor->control)),
			 "IDL:Bonobo/PersistStream:1.0", NULL);
		
		Bonobo_PersistStream_save (pstream_iface, 
					   (Bonobo_Stream) bonobo_object_corba_objref (BONOBO_OBJECT (stream)),
					   "text/plain", &ev);
		
		bonobo_object_unref (BONOBO_OBJECT (stream));
	}
	
	if (ev._major != CORBA_NO_EXCEPTION || rename (filename, editor->sig->filename) == -1) {
		menu_file_save_error (uic, &ev);
		unlink (filename);
	}
	
	g_free (filename);
	
	CORBA_exception_free (&ev);
	
	mail_config_signature_set_html (editor->sig, editor->html);
	mail_config_signature_emit_event (MAIL_CONFIG_SIG_EVENT_CONTENT_CHANGED, editor->sig);
}

static void
exit_dialog_cb (int reply, ESignatureEditor *editor)
{
	switch (reply) {
	case REPLY_YES:
		menu_file_save_cb (NULL, editor, NULL);
		destroy_editor (editor);
		break;
	case REPLY_NO:
		destroy_editor (editor);
		break;
	case REPLY_CANCEL:
	default:
		break;
	}
}

static void
do_exit (ESignatureEditor *editor)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	if (GNOME_GtkHTML_Editor_Engine_hasUndo (editor->engine, &ev)) {
		GtkWidget *dialog;
		GtkWidget *label;
		gint button;
		
		dialog = gnome_dialog_new (_("Save signature"),
					   GNOME_STOCK_BUTTON_YES,      /* Save */
					   GNOME_STOCK_BUTTON_NO,       /* Don't save */
					   GNOME_STOCK_BUTTON_CANCEL,   /* Cancel */
					   NULL);
		
		label = gtk_label_new (_("This signature has been changed, but hasn't been saved.\n"
					 "\nDo you wish to save your changes?"));
		gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), label, TRUE, TRUE, 0);
		gtk_widget_show (label);
		gnome_dialog_set_parent (GNOME_DIALOG (dialog), GTK_WINDOW (editor->win));
		gnome_dialog_set_default (GNOME_DIALOG (dialog), 0);
		button = gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
		
		exit_dialog_cb (button, editor);
	} else
		destroy_editor (editor);
	CORBA_exception_free (&ev);
}

static int
delete_event_cb (GtkWidget *w, GdkEvent *event, ESignatureEditor *editor)
{
	do_exit (editor);
	
	return FALSE;
}

static void
menu_file_close_cb (BonoboUIComponent *uic, gpointer data, const gchar *path)
{
	ESignatureEditor *editor;
	
	editor = E_SIGNATURE_EDITOR (data);
	do_exit (editor);
}

static void
menu_file_save_close_cb (BonoboUIComponent *uic, gpointer data, const gchar *path)
{
	ESignatureEditor *editor;
	
	editor = E_SIGNATURE_EDITOR (data);

	menu_file_save_cb (uic, editor, path);
	destroy_editor (editor);
}

static void
menu_help (BonoboUIComponent *uih, void *data, const char *path)
{
	GnomeHelpMenuEntry he;

	he.name = PACKAGE;
	he.path = "usage-mail-getnsend-send.html#HTML-SIGNATURE-HOWTO";
	gnome_help_display (NULL, &he);
}

static BonoboUIVerb verbs [] = {

	BONOBO_UI_VERB ("FileSave",       menu_file_save_cb),
	BONOBO_UI_VERB ("FileClose",      menu_file_close_cb),
	BONOBO_UI_VERB ("FileSaveClose",  menu_file_save_close_cb),
	BONOBO_UI_VERB ("HelpSigEditor",  menu_help),

	BONOBO_UI_VERB_END
};

static void
load_signature (ESignatureEditor *editor)
{
	CORBA_Environment ev;
	
	if (editor->html) {
		Bonobo_PersistFile pfile_iface;
		
		pfile_iface = bonobo_object_client_query_interface (bonobo_widget_get_server (BONOBO_WIDGET (editor->control)),
								    "IDL:Bonobo/PersistFile:1.0", NULL);
		CORBA_exception_init (&ev);
		Bonobo_PersistFile_load (pfile_iface, editor->sig->filename, &ev);
		CORBA_exception_free (&ev);
	} else {
		Bonobo_PersistStream pstream_iface;
		BonoboStream *stream;
		gchar *data, *html;
		
		data = e_msg_composer_get_sig_file_content (editor->sig->filename, FALSE);
		html = g_strdup_printf ("<PRE>\n%s", data);
		g_free (data);
		
		pstream_iface = bonobo_object_client_query_interface
			(bonobo_widget_get_server (BONOBO_WIDGET (editor->control)),
			 "IDL:Bonobo/PersistStream:1.0", NULL);
		CORBA_exception_init (&ev);
		stream = bonobo_stream_mem_create (html, strlen (html), TRUE, FALSE);
		
		if (stream == NULL) {
			g_warning ("Couldn't create memory stream\n");
		} else {
			BonoboObject *stream_object;
			Bonobo_Stream corba_stream;
			
			stream_object = BONOBO_OBJECT (stream);
			corba_stream = bonobo_object_corba_objref (stream_object);
			Bonobo_PersistStream_load (pstream_iface, corba_stream,
						   "text/html", &ev);
		}
		
		Bonobo_Unknown_unref (pstream_iface, &ev);
		CORBA_Object_release (pstream_iface, &ev);
		CORBA_exception_free (&ev);
		bonobo_object_unref (BONOBO_OBJECT (stream));
		
		g_free (html);
	}
}

static void
sig_name_changed (GtkWidget *w, ESignatureEditor *editor)
{
	mail_config_signature_set_name (editor->sig, e_utf8_gtk_entry_get_text (GTK_ENTRY (editor->name_entry)));
}

static void
format_html_cb (BonoboUIComponent           *component,
		const char                  *path,
		Bonobo_UIComponent_EventType type,
		const char                  *state,
		gpointer                     data)

{
	ESignatureEditor *editor = (ESignatureEditor *) data;

	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;
	
	editor->html = atoi (state);
	bonobo_widget_set_property (BONOBO_WIDGET (editor->control), "FormatHTML", editor->html, NULL);
}

void
mail_signature_editor (MailConfigSignature *sig)
{
	CORBA_Environment ev;
	ESignatureEditor *editor;
	BonoboUIComponent *component;
	BonoboUIContainer *container;
	GtkWidget *vbox, *hbox, *label, *frame, *vbox1;
	gchar *title;
	
	if (!sig->filename || !*sig->filename)
		return;
	
	editor = g_new0 (ESignatureEditor, 1);
	
	editor->sig = sig;
	editor->html = sig->html;
	
	title       = g_strdup_printf (_("Edit signature"));
	editor->win = bonobo_window_new ("e-sig-editor", title);
	gtk_window_set_default_size (GTK_WINDOW (editor->win), DEFAULT_WIDTH, DEFAULT_HEIGHT);
	gtk_window_set_policy (GTK_WINDOW (editor->win), FALSE, TRUE, FALSE);
	g_free (title);
	
	container = bonobo_ui_container_new ();
	bonobo_ui_container_set_win (container, BONOBO_WINDOW (editor->win));
	
	component = bonobo_ui_component_new_default ();
	bonobo_ui_component_set_container (component, bonobo_object_corba_objref (BONOBO_OBJECT (container)));
	bonobo_ui_component_add_verb_list_with_data (component, verbs, editor);
	bonobo_ui_util_set_ui (component, EVOLUTION_DATADIR, "evolution-signature-editor.xml", "evolution-signature-editor");
	
	editor->control = bonobo_widget_new_control ("OAFIID:GNOME_GtkHTML_Editor:1.1",
						     bonobo_ui_component_get_container (component));
	
	if (editor->control == NULL) {
		g_warning ("Cannot get 'OAFIID:GNOME_GtkHTML_Editor:1.1'.");
		
		destroy_editor (editor);
		return;
	}

	editor->engine = (GNOME_GtkHTML_Editor_Engine) bonobo_object_client_query_interface
		(bonobo_widget_get_server (BONOBO_WIDGET (editor->control)), "IDL:GNOME/GtkHTML/Editor/Engine:1.0", NULL);
	
	load_signature (editor);

	bonobo_ui_component_set_prop (component, "/commands/FormatHtml", "state", editor->html ? "1" : "0", NULL);
	bonobo_ui_component_add_listener (component, "FormatHtml", format_html_cb, editor);

	gtk_signal_connect (GTK_OBJECT (editor->win), "delete_event",
			    GTK_SIGNAL_FUNC (delete_event_cb), editor);

	vbox = gtk_vbox_new (FALSE, 0);
	hbox = gtk_hbox_new (FALSE, 4);
	vbox1 = gtk_vbox_new (FALSE, 3);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), 3);
	label = gtk_label_new (_("Enter a name for this signature."));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
	gtk_box_pack_start (GTK_BOX (vbox1), label, FALSE, TRUE, 0);
	label = gtk_label_new (_("Name:"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
	editor->name_entry = gtk_entry_new ();
	e_utf8_gtk_entry_set_text (GTK_ENTRY (editor->name_entry), sig->name);
	gtk_signal_connect (GTK_OBJECT (editor->name_entry), "changed", GTK_SIGNAL_FUNC (sig_name_changed), editor);
	gtk_box_pack_start_defaults (GTK_BOX (hbox), editor->name_entry);
	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox, FALSE, TRUE, 0);
	gtk_container_add (GTK_CONTAINER (frame), vbox1);
	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
	gtk_widget_show_all (vbox);
	gtk_box_pack_start_defaults (GTK_BOX (vbox), editor->control);

	bonobo_window_set_contents (BONOBO_WINDOW (editor->win), vbox);
	bonobo_widget_set_property (BONOBO_WIDGET (editor->control), "FormatHTML", editor->html, NULL);
	gtk_widget_show (GTK_WIDGET (editor->win));
	gtk_widget_show (GTK_WIDGET (editor->control));

	CORBA_exception_init (&ev);
	GNOME_GtkHTML_Editor_Engine_runCommand (editor->engine, "grab-focus", &ev);
	CORBA_exception_free (&ev);
}
