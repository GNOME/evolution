#include <bonobo.h>
#include <bonobo/bonobo-stream-memory.h>

#include <gal/widgets/e-gui-utils.h>

#include "e-msg-composer.h"
#include "mail-signature-editor.h"

/*
 * Signature editor
 *
 */

struct _ESignatureEditor {
	GtkWidget *win;
	GtkWidget *control;
	
	gchar *filename;
	gboolean html;
	gboolean has_changed;
};
typedef struct _ESignatureEditor ESignatureEditor;

#define E_SIGNATURE_EDITOR(o) ((ESignatureEditor *) o)

#define DEFAULT_WIDTH 600
#define DEFAULT_HEIGHT 500

enum { REPLY_YES = 0, REPLY_NO, REPLY_CANCEL };

static void
destroy_editor (ESignatureEditor *editor)
{
	gtk_widget_destroy (editor->win);
	g_free (editor->filename);
	g_free (editor);
}

static void
menu_file_save_error (BonoboUIComponent *uic, CORBA_Environment *ev) {
	e_notice (GTK_WINDOW (uic), GNOME_MESSAGE_BOX_ERROR,
		  _("Could not save signature file."));
	
	g_warning ("Exception while saving signature (%s)",
		   bonobo_exception_get_text (ev));
}

static void
menu_file_save_cb (BonoboUIComponent *uic,
		   void *data,
		   const char *path)
{
	ESignatureEditor *editor;
	Bonobo_PersistFile pfile_iface;
	CORBA_Environment ev;
	
	editor = E_SIGNATURE_EDITOR (data);
	if (editor->html) {
		CORBA_exception_init (&ev);
		
		pfile_iface = bonobo_object_client_query_interface (bonobo_widget_get_server (BONOBO_WIDGET (editor->control)),
								    "IDL:Bonobo/PersistFile:1.0", NULL);
		Bonobo_PersistFile_save (pfile_iface, editor->filename, &ev);

		if (ev._major != CORBA_NO_EXCEPTION)
			menu_file_save_error (uic, &ev);

		CORBA_exception_free (&ev);
	} else {
		BonoboStream *stream;
		CORBA_Environment ev;
		Bonobo_PersistStream pstream_iface;
		
		CORBA_exception_init (&ev);
	
		stream = bonobo_stream_open (BONOBO_IO_DRIVER_FS, editor->filename,
					     Bonobo_Storage_WRITE | Bonobo_Storage_CREATE, 0);

		pstream_iface = bonobo_object_client_query_interface
			(bonobo_widget_get_server (BONOBO_WIDGET (editor->control)),
			 "IDL:Bonobo/PersistStream:1.0", NULL);

		Bonobo_PersistStream_save (pstream_iface, 
					   (Bonobo_Stream) bonobo_object_corba_objref (BONOBO_OBJECT (stream)),
					   "text/plain", &ev);

		if (ev._major != CORBA_NO_EXCEPTION)
			menu_file_save_error (uic, &ev);
	
		CORBA_exception_free (&ev);
		bonobo_object_unref (BONOBO_OBJECT (stream));
	}
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
	}
}

static void
do_exit (ESignatureEditor *editor)
{
	if (editor->has_changed) {
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
		
		pfile_iface = bonobo_object_client_query_interface (bonobo_widget_get_server (BONOBO_WIDGET (editor->control)),
								    "IDL:Bonobo/PersistFile:1.0", NULL);
		CORBA_exception_init (&ev);
		Bonobo_PersistFile_load (pfile_iface, editor->filename, &ev);
		CORBA_exception_free (&ev);
	} else {
		Bonobo_PersistStream pstream_iface;
		BonoboStream *stream;
		gchar *data, *html;
		
		data = e_msg_composer_get_sig_file_content (editor->filename, FALSE);
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

void
mail_signature_editor (const gchar *filename, gboolean html)
{
	ESignatureEditor *editor;
	BonoboUIComponent *component;
	BonoboUIContainer *container;
	gchar *title;
	
	if (!filename || !*filename)
		return;
	
	editor = g_new0 (ESignatureEditor, 1);
	
	editor->html     = html;
	editor->filename = g_strdup (filename);
	editor->has_changed = TRUE;

	title       = g_strdup_printf ("Edit %ssignature (%s)", html ? "HTML " : "", filename);
	editor->win = bonobo_window_new ("e-sig-editor", title);
	gtk_window_set_default_size (GTK_WINDOW (editor->win), DEFAULT_WIDTH, DEFAULT_HEIGHT);
	gtk_window_set_policy (GTK_WINDOW (editor->win), FALSE, TRUE, FALSE);
	gtk_window_set_modal (GTK_WINDOW (editor->win), TRUE);
	g_free (title);
	
	container = bonobo_ui_container_new ();
	bonobo_ui_container_set_win (container, BONOBO_WINDOW (editor->win));
	
	component = bonobo_ui_component_new_default ();
	bonobo_ui_component_set_container (component, bonobo_object_corba_objref (BONOBO_OBJECT (container)));
	bonobo_ui_component_add_verb_list_with_data (component, verbs, editor);
	bonobo_ui_util_set_ui (component, EVOLUTION_DATADIR, "evolution-signature-editor.xml", "evolution-signature-editor");
	
	editor->control = bonobo_widget_new_control ("OAFIID:GNOME_GtkHTML_Editor",
						     bonobo_ui_component_get_container (component));
	
	if (editor->control == NULL) {
		g_warning ("Cannot get 'OAFIID:GNOME_GtkHTML_Editor'.");
		
		destroy_editor (editor);
		return;
	}
	
	load_signature (editor);

	gtk_signal_connect (GTK_OBJECT (editor->win), "delete_event",
			    GTK_SIGNAL_FUNC (delete_event_cb), editor);

	bonobo_window_set_contents (BONOBO_WINDOW (editor->win), editor->control);
	bonobo_widget_set_property (BONOBO_WIDGET (editor->control), "FormatHTML", html, NULL);
	gtk_widget_show (GTK_WIDGET (editor->win));
	gtk_widget_show (GTK_WIDGET (editor->control));
	gtk_widget_grab_focus (editor->control);
}
