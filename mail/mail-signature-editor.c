/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Radek Doulik <rodo@ximian.com>
 *           Jeffrey Stedfast <fejj@ximian.com>
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

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <gtk/gtk.h>

#include <bonobo.h>
#include <bonobo/bonobo-stream-memory.h>

#include <e-util/e-signature-list.h>
#include "widgets/misc/e-error.h"

#include "e-msg-composer.h"
#include "mail-signature-editor.h"
#include "mail-config.h"

#define d(x) 


typedef struct _ESignatureEditor {
	GtkWidget *win;
	GtkWidget *control;
	GtkWidget *name_entry;
	GtkWidget *info_frame;
	
	ESignature *sig;
	gboolean is_new;
	gboolean html;
	
	GNOME_GtkHTML_Editor_Engine engine;
} ESignatureEditor;

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
	char *err;
	
	/* errno is set if the rename() fails in menu_file_save_cb */
	
	err = ev->_major != CORBA_NO_EXCEPTION ? bonobo_exception_get_text (ev) : g_strdup (g_strerror (errno));
	
	e_error_run(NULL, "mail:no-save-signature", err, NULL);
	g_warning ("Exception while saving signature: %s", err);
	
	g_free (err);
}

static GByteArray *
get_text (Bonobo_PersistStream persist, const char *format, CORBA_Environment *ev)
{
	BonoboStream *stream;
	BonoboStreamMem *stream_mem;
	GByteArray *text;
	
	stream = bonobo_stream_mem_create (NULL, 0, FALSE, TRUE);
	Bonobo_PersistStream_save (persist, (Bonobo_Stream)bonobo_object_corba_objref (BONOBO_OBJECT (stream)),
				   format, ev);
	
	if (ev->_major != CORBA_NO_EXCEPTION)
		return NULL;
		
	stream_mem = BONOBO_STREAM_MEM (stream);
	
	text = g_byte_array_new ();
	g_byte_array_append (text, stream_mem->buffer, stream_mem->pos);
	bonobo_object_unref (BONOBO_OBJECT (stream));
	
	return text;
}

static ssize_t
write_all (int fd, const char *buf, size_t n)
{
	ssize_t w, nwritten = 0;
	
	do {
		do {
			w = write (fd, buf + nwritten, n - nwritten);
		} while (w == -1 && (errno == EINTR || errno == EAGAIN));
		
		if (w > 0)
			nwritten += w;
	} while (nwritten < n && w != -1);
	
	if (w == -1)
		return -1;
	
	return nwritten;
}

static void
menu_file_save_cb (BonoboUIComponent *uic, void *user_data, const char *path)
{
	ESignatureEditor *editor = user_data;
	Bonobo_PersistStream pstream_iface;
	char *dirname, *base, *filename;
	CORBA_Environment ev;
	GByteArray *text;
	int fd;
	
	d(printf ("editor->sig->filename = %s\n", editor->sig->filename));
	dirname = g_path_get_dirname (editor->sig->filename);
	d(printf ("dirname = %s\n", dirname));
	base = g_path_get_basename (editor->sig->filename);
	d(printf ("basename = %s\n", base));
	filename = g_strdup_printf ("%s/.#%s", dirname, base);
	d(printf ("filename = %s\n", filename));
	g_free (dirname);
	g_free (base);
	
	CORBA_exception_init (&ev);
	pstream_iface = Bonobo_Unknown_queryInterface
			(bonobo_widget_get_objref (BONOBO_WIDGET (editor->control)),
			 "IDL:Bonobo/PersistStream:1.0", &ev);
	
	if (ev._major != CORBA_NO_EXCEPTION)
		goto exception;
	
	if ((fd = open (filename, O_WRONLY | O_TRUNC | O_CREAT, 0666)) == -1)
		goto exception;
	
	text = get_text (pstream_iface, editor->html ? "text/html" : "text/plain", &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		close (fd);
		goto exception;
	}
	
	if (write_all (fd, text->data, text->len) == -1) {
		g_byte_array_free (text, TRUE);
		close (fd);
		goto exception;
	}
	
	g_byte_array_free (text, TRUE);
	close (fd);
	
	if (rename (filename, editor->sig->filename) == -1)
		goto exception;
	
	g_free (filename);
	
	editor->sig->html = editor->html;
	
	/* if the signature isn't already saved in the config, save it there now... */
	if (editor->is_new) {
		mail_config_add_signature (editor->sig);
		editor->is_new = FALSE;
	} else {
		e_signature_list_change (mail_config_get_signatures (), editor->sig);
	}
	
	return;
	
 exception:
	
	menu_file_save_error (uic, &ev);
	CORBA_exception_free (&ev);
	unlink (filename);
	g_free (filename);
}

static void
exit_dialog_cb (int reply, ESignatureEditor *editor)
{
	switch (reply) {
	case GTK_RESPONSE_YES:
		menu_file_save_cb (NULL, editor, NULL);
		break;
	case GTK_RESPONSE_NO:
		destroy_editor (editor);
		break;
	}
}

static void
do_exit (ESignatureEditor *editor)
{
	CORBA_Environment ev;
	
	CORBA_exception_init (&ev);
	
	if (GNOME_GtkHTML_Editor_Engine_hasUndo (editor->engine, &ev)) {
		int button;

		button = e_error_run((GtkWindow *)editor->win, "mail:ask-signature-changed", NULL);
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
menu_file_close_cb (BonoboUIComponent *uic, gpointer data, const char *path)
{
	ESignatureEditor *editor;
	
	editor = E_SIGNATURE_EDITOR (data);
	do_exit (editor);
}

static void
menu_file_save_close_cb (BonoboUIComponent *uic, gpointer data, const char *path)
{
	ESignatureEditor *editor;
	
	editor = E_SIGNATURE_EDITOR (data);
	
	menu_file_save_cb (uic, editor, path);
	destroy_editor (editor);
}

static BonoboUIVerb verbs [] = {

	BONOBO_UI_VERB ("FileSave",       menu_file_save_cb),
	BONOBO_UI_VERB ("FileClose",      menu_file_close_cb),
	BONOBO_UI_VERB ("FileSaveClose",  menu_file_save_close_cb),
	
	BONOBO_UI_VERB_END
};

static void
load_signature (ESignatureEditor *editor)
{
	CORBA_Environment ev;
	
	if (editor->html) {
		Bonobo_PersistFile pfile_iface;
		
		CORBA_exception_init (&ev);
		pfile_iface = Bonobo_Unknown_queryInterface (bonobo_widget_get_objref (BONOBO_WIDGET (editor->control)),
							     "IDL:Bonobo/PersistFile:1.0", &ev);
		Bonobo_PersistFile_load (pfile_iface, editor->sig->filename, &ev);
		CORBA_exception_free (&ev);
	} else {
		Bonobo_PersistStream pstream_iface;
		BonoboStream *stream;
		char *data, *html;
		
		data = e_msg_composer_get_sig_file_content (editor->sig->filename, FALSE);
		html = g_strdup_printf ("<PRE>\n%s", data);
		g_free (data);
		
		CORBA_exception_init (&ev);
		pstream_iface = Bonobo_Unknown_queryInterface
			(bonobo_widget_get_objref (BONOBO_WIDGET (editor->control)),
			 "IDL:Bonobo/PersistStream:1.0",&ev);
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
	const char *name;
	
	name = gtk_entry_get_text (GTK_ENTRY (editor->name_entry));
	
	g_free (editor->sig->name);
	editor->sig->name = g_strdup (name);
	
	if (!editor->is_new)
		e_signature_list_change (mail_config_get_signatures (), editor->sig);
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
	bonobo_widget_set_property (BONOBO_WIDGET (editor->control), "FormatHTML", TC_CORBA_boolean, editor->html, NULL);
}

void
mail_signature_editor (ESignature *sig, GtkWindow *parent, gboolean is_new)
{
	CORBA_Environment ev;
	ESignatureEditor *editor;
	BonoboUIComponent *component;
	BonoboUIContainer *container;
	GtkWidget *vbox, *hbox, *label, *frame, *vbox1;
	
	if (!sig->filename || !*sig->filename)
		return;
	
	editor = g_new0 (ESignatureEditor, 1);
	
	editor->sig = sig;
	editor->html = sig->html;
	editor->is_new = is_new;
	
	editor->win = bonobo_window_new ("e-sig-editor", _("Edit signature"));
	gtk_window_set_type_hint (GTK_WINDOW (editor->win), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_default_size (GTK_WINDOW (editor->win), DEFAULT_WIDTH, DEFAULT_HEIGHT);
	if (parent != NULL)
		gtk_window_set_transient_for (GTK_WINDOW (editor->win), parent);
	g_object_set (editor->win, "allow_shrink", FALSE, "allow_grow", TRUE, NULL);
	
	container = bonobo_window_get_ui_container (BONOBO_WINDOW(editor->win));
	
	component = bonobo_ui_component_new_default ();
	bonobo_ui_component_set_container (component, bonobo_object_corba_objref (BONOBO_OBJECT (container)), NULL);
	bonobo_ui_component_add_verb_list_with_data (component, verbs, editor);
	bonobo_ui_util_set_ui (component, PREFIX,
			       EVOLUTION_UIDIR "/evolution-signature-editor.xml",
			       "evolution-signature-editor", NULL);
	
	editor->control = bonobo_widget_new_control ("OAFIID:GNOME_GtkHTML_Editor:3.1",
						     bonobo_ui_component_get_container (component));
	
	if (editor->control == NULL) {
		g_warning ("Cannot get 'OAFIID:GNOME_GtkHTML_Editor:3.1'.");
		
		destroy_editor (editor);
		return;
	}
	
	editor->engine = (GNOME_GtkHTML_Editor_Engine) Bonobo_Unknown_queryInterface
		(bonobo_widget_get_objref (BONOBO_WIDGET (editor->control)), "IDL:GNOME/GtkHTML/Editor/Engine:1.0", &ev);
	CORBA_exception_free(&ev);
	load_signature (editor);
	
	bonobo_ui_component_set_prop (component, "/commands/FormatHtml", "state", editor->html ? "1" : "0", NULL);
	bonobo_ui_component_add_listener (component, "FormatHtml", format_html_cb, editor);
	
	g_signal_connect (editor->win, "delete_event", G_CALLBACK (delete_event_cb), editor);
	
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
	gtk_entry_set_text (GTK_ENTRY (editor->name_entry), sig->name);
	g_signal_connect (editor->name_entry, "changed", G_CALLBACK (sig_name_changed), editor);
	gtk_box_pack_start_defaults (GTK_BOX (hbox), editor->name_entry);
	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox, FALSE, TRUE, 0);
	gtk_container_add (GTK_CONTAINER (frame), vbox1);
	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
	gtk_widget_show_all (vbox);
	gtk_box_pack_start_defaults (GTK_BOX (vbox), editor->control);
	
	bonobo_window_set_contents (BONOBO_WINDOW (editor->win), vbox);
	bonobo_widget_set_property (BONOBO_WIDGET (editor->control), "FormatHTML", TC_CORBA_boolean, editor->html, NULL);
	gtk_widget_show (GTK_WIDGET (editor->win));
	gtk_widget_show (GTK_WIDGET (editor->control));
	
	CORBA_exception_init (&ev);
	GNOME_GtkHTML_Editor_Engine_runCommand (editor->engine, "grab-focus", &ev);
	CORBA_exception_free (&ev);
}
