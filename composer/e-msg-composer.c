/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-msg-composer.c
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

/*

   TODO

   - Somehow users should be able to see if any file(s) are attached even when
     the attachment bar is not shown.

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <errno.h>

#include <bonobo.h>
#include <bonobo/bonobo-stream-memory.h>
#include <glade/glade.h>
#include <gnome.h>
#include <libgnorba/gnorba.h>
#include <gtkhtml/gtkhtml.h>

#include <camel/camel.h>

#include "../mail/mail.h"

#include "e-util/e-html-utils.h"
#include "e-util/e-setup.h"
#include "e-util/e-gui-utils.h"
#include "widgets/misc/e-scroll-frame.h"

#include "e-msg-composer.h"
#include "e-msg-composer-address-dialog.h"
#include "e-msg-composer-attachment-bar.h"
#include "e-msg-composer-hdrs.h"
#include "e-msg-composer-select-file.h"

#define HTML_EDITOR_CONTROL_ID "OAFIID:control:html-editor:63c5499b-8b0c-475a-9948-81ec96a9662c"


#define DEFAULT_WIDTH 600
#define DEFAULT_HEIGHT 500

enum {
	SEND,
	POSTPONE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static GnomeAppClass *parent_class = NULL;

/* local prototypes */
static GList *add_recipients (GList *list, const char *recips, gboolean decode);
static void free_recipients (GList *list);


static GtkWidget *
create_editor (EMsgComposer *composer)
{
	GtkWidget *control;

	control = bonobo_widget_new_control (HTML_EDITOR_CONTROL_ID,
					     bonobo_object_corba_objref (BONOBO_OBJECT (composer->uih)));
	if (control == NULL) {
		g_error ("Cannot activate `%s'. Did you build gtkhtml with Bonobo and OAF support?", HTML_EDITOR_CONTROL_ID);
		return NULL;
	}

	return control;
}

static char *
get_text (Bonobo_PersistStream persist, char *format)
{
	BonoboStream *stream;
	BonoboStreamMem *stream_mem;
	CORBA_Environment ev;
	char *text;

	CORBA_exception_init (&ev);

	stream = bonobo_stream_mem_create (NULL, 0, FALSE, TRUE);
	Bonobo_PersistStream_save (persist, (Bonobo_Stream)bonobo_object_corba_objref (BONOBO_OBJECT (stream)),
				   format, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Exception getting mail '%s'",
			   bonobo_exception_get_txt (&ev));
		return NULL;
	}
	
	CORBA_exception_free (&ev);

	stream_mem = BONOBO_STREAM_MEM (stream);
	text = g_malloc (stream_mem->pos + 1);
	memcpy (text, stream_mem->buffer, stream_mem->pos);
	text[stream_mem->pos] = 0;
	bonobo_object_unref (BONOBO_OBJECT(stream));

	return text;
}

#define LINE_LEN 72

/* This might be a temporary function... the GtkHTML export interfaces are
 * not yet complete, so some or all of this may move into GtkHTML.
 */
static char *
format_text (char *text)
{
	GString *out;
	char *s, *space, *outstr;
	int len, tabbing, i;
	gboolean linestart = TRUE, cited = FALSE;

	tabbing = 0;		/* Shut down compiler.  */
	len = strlen (text);
	out = g_string_sized_new (len + len / LINE_LEN);

	s = text;
	while (*s) {
		if (linestart) {
			tabbing = 0;
			while (*s == '\t') {
				s++;
				tabbing++;
			}
			cited = (tabbing == 0 && *s == '>');
		}

		len = strcspn (s, "\n");
		if (!cited && len > LINE_LEN - tabbing * 8) {
			/* If we can break anywhere between s and
			 * s + LINE_LEN, do that. We can break between
			 * space and anything but &nbsp;
			 */
			space = s + LINE_LEN - tabbing * 8;
			while (space > s && (*space != ' '
					     || (*(space + 1) == '\240')
					     || (*(space - 1) == '\240')))
				space--;

			if (space != s)
				len = space - s;
		}

		/* Do initial tabs */
		for (i = 0; i < tabbing; i++)
			g_string_append_c (out, '\t');

		/* Copy the line... */
		while (len--) {
			g_string_append_c (out, *s == '\240' ? ' ' : *s);
			s++;
		}

		/* Eat whitespace... */
		while (*s == ' ' || *s == '\240')
			s++;
		if (*s == '\n') {
			s++;
			linestart = TRUE;
		} else
			linestart = FALSE;

		/* And end the line. */
		g_string_append_c (out, '\n');
	}

	outstr = out->str;
	g_string_free (out, FALSE);
	return outstr;
}

typedef enum {
	MSG_FORMAT_PLAIN,
	MSG_FORMAT_ALTERNATIVE,
} MsgFormat;

/* This functions builds a CamelMimeMessage for the message that the user has
   composed in `composer'.  */
static CamelMimeMessage *
build_message (EMsgComposer *composer)
{
	EMsgComposerAttachmentBar *attachment_bar =
		E_MSG_COMPOSER_ATTACHMENT_BAR (composer->attachment_bar);
	CamelMimeMessage *new;
	CamelMultipart *body = NULL;
	CamelMimePart *part;
	char *html = NULL, *plain = NULL, *fmt = NULL;
	int i;
	MsgFormat type = MSG_FORMAT_ALTERNATIVE;

	if (composer->persist_stream_interface == CORBA_OBJECT_NIL)
		return NULL;

	if (composer->send_html)
		type = MSG_FORMAT_ALTERNATIVE;
	else
		type = MSG_FORMAT_PLAIN;

	new = camel_mime_message_new ();

	e_msg_composer_hdrs_to_message (E_MSG_COMPOSER_HDRS (composer->hdrs), new);
	for (i = 0; i < composer->extra_hdr_names->len; i++) {
		camel_medium_add_header (CAMEL_MEDIUM (new),
					 composer->extra_hdr_names->pdata[i],
					 composer->extra_hdr_values->pdata[i]);
	}
	
	plain = get_text (composer->persist_stream_interface, "text/plain");
	fmt = format_text (plain);
	g_free (plain);
	
	if (type != MSG_FORMAT_PLAIN)
		html = get_text (composer->persist_stream_interface, "text/html");

	if (type == MSG_FORMAT_ALTERNATIVE) {
		body = camel_multipart_new ();
		camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (body),
						  "multipart/alternative");
		camel_multipart_set_boundary (body, NULL);

		part = camel_mime_part_new ();
		camel_mime_part_set_content (part, fmt, strlen (fmt), "text/plain");
		g_free (fmt);
		camel_multipart_add_part (body, part);
		camel_object_unref (CAMEL_OBJECT (part));
		
		part = camel_mime_part_new ();
		camel_mime_part_set_content (part, html, strlen (html), "text/html");
		g_free (html);
		camel_multipart_add_part (body, part);
		camel_object_unref (CAMEL_OBJECT (part));
	}

	if (e_msg_composer_attachment_bar_get_num_attachments (attachment_bar)) {
		CamelMultipart *multipart = camel_multipart_new ();

		/* Generate a random boundary. */
		camel_multipart_set_boundary (multipart, NULL);

		part = camel_mime_part_new ();
		switch (type) {
		case MSG_FORMAT_ALTERNATIVE:
			camel_medium_set_content_object (CAMEL_MEDIUM (part),
							 CAMEL_DATA_WRAPPER (body));
			camel_object_unref (CAMEL_OBJECT (body));
			break;
		case MSG_FORMAT_PLAIN:
			camel_mime_part_set_content (part, fmt, strlen (fmt), "text/plain");
			g_free(fmt);
			break;
		}
		camel_multipart_add_part (multipart, part);
		camel_object_unref (CAMEL_OBJECT (part));

		e_msg_composer_attachment_bar_to_multipart (attachment_bar, multipart);

		camel_medium_set_content_object (CAMEL_MEDIUM (new), CAMEL_DATA_WRAPPER (multipart));
		camel_object_unref (CAMEL_OBJECT (multipart));
	} else {
		CamelDataWrapper *cdw;
		CamelStream *stream;
		switch (type) {
		case MSG_FORMAT_ALTERNATIVE:
			camel_medium_set_content_object (CAMEL_MEDIUM (new), CAMEL_DATA_WRAPPER (body));
			camel_object_unref (CAMEL_OBJECT (body));
			break;
		case MSG_FORMAT_PLAIN:
			stream = camel_stream_mem_new_with_buffer (fmt, strlen (fmt));
			cdw = camel_data_wrapper_new ();
			camel_data_wrapper_construct_from_stream (cdw, stream);
			camel_object_unref (CAMEL_OBJECT (stream));
			
			camel_data_wrapper_set_mime_type (cdw, "text/plain");

			camel_medium_set_content_object (CAMEL_MEDIUM (new), CAMEL_DATA_WRAPPER (cdw));
			camel_object_unref (CAMEL_OBJECT (cdw));
			g_free (fmt);
			break;

		}
	}

	return new;
}

static char *
get_signature (const char *sigfile)
{
	char *rawsig;
	static char *htmlsig = NULL;
	static time_t sigmodtime = -1;
	struct stat st;
	int fd;

	if (!sigfile || !*sigfile) {
		return NULL;
	}

	if (stat (sigfile, &st) == -1) {
		char *msg;

		msg = g_strdup_printf ("Could not open signature file %s:\n%s",
				       sigfile, g_strerror (errno));
		gnome_error_dialog (msg);
		g_free (msg);

		return NULL;
	}

	if (st.st_mtime == sigmodtime)
		return htmlsig;

	rawsig = g_malloc (st.st_size + 1);
	fd = open (sigfile, O_RDONLY);
	if (fd == -1) {
		char *msg;

		msg = g_strdup_printf ("Could not open signature file %s:\n%s",
				       sigfile, g_strerror (errno));
		gnome_error_dialog (msg);
		g_free (msg);

		return NULL;
	}

	read (fd, rawsig, st.st_size);
	rawsig[st.st_size] = '\0';
	close (fd);

	htmlsig = e_text_to_html (rawsig, 0);
	sigmodtime = st.st_mtime;

	return htmlsig;
}

static void
set_editor_text (BonoboWidget *editor, const char *sig_file, const char *text)
{
	Bonobo_PersistStream persist;
	BonoboStream *stream;
	CORBA_Environment ev;
	char *sig, *fulltext;

	sig = get_signature (sig_file);
	if (sig) {
		if (!strncmp ("-- \n", sig, 3))
			fulltext = g_strdup_printf ("%s<br>\n<pre>\n%s</pre>",
						    text, sig);
		else
			fulltext = g_strdup_printf ("%s<br>\n<pre>\n-- \n%s</pre>",
						    text, sig);
	} else {
		if (!*text)
			return;
		fulltext = (char*)text;
	}

	CORBA_exception_init (&ev);
	persist = (Bonobo_PersistStream)
		bonobo_object_client_query_interface (
			bonobo_widget_get_server (editor),
			"IDL:Bonobo/PersistStream:1.0",
			&ev);
	g_assert (persist != CORBA_OBJECT_NIL);

	stream = bonobo_stream_mem_create (fulltext, strlen (fulltext),
					   TRUE, FALSE);
	if (sig)
		g_free (fulltext);
	Bonobo_PersistStream_load (persist, (Bonobo_Stream)bonobo_object_corba_objref (BONOBO_OBJECT (stream)), "text/html", &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		/* FIXME. Some error message. */
		return;
	}
	if (ev._major != CORBA_SYSTEM_EXCEPTION)
		CORBA_Object_release (persist, &ev);

	Bonobo_Unknown_unref (persist, &ev);
	CORBA_exception_free (&ev);
	bonobo_object_unref (BONOBO_OBJECT(stream));
}


/* Commands.  */

static void
show_attachments (EMsgComposer *composer,
		  gboolean show)
{
	if (show) {
		gtk_widget_show (composer->attachment_scroll_frame);
		gtk_widget_show (composer->attachment_bar);
	} else {
		gtk_widget_hide (composer->attachment_scroll_frame);
		gtk_widget_hide (composer->attachment_bar);
	}

	composer->attachment_bar_visible = show;

	/* Update the GUI.  */

#if 0
	gtk_check_menu_item_set_active
		(GTK_CHECK_MENU_ITEM
		 (glade_xml_get_widget (composer->menubar_gui,
					"menu_view_attachments")),
		 show);
#endif

	/* XXX we should update the toggle toolbar item as well.  At
	   this point, it is not a toggle because Glade is broken.  */
}

static void
save (EMsgComposer *composer,
      const char *file_name)
{
	CORBA_Environment ev;
	char *my_file_name;

	if (file_name != NULL)
		my_file_name = g_strdup (file_name);
	else
		my_file_name = e_msg_composer_select_file (composer, _("Save as..."));

	if (my_file_name == NULL)
		return;

	CORBA_exception_init (&ev);

	Bonobo_PersistFile_save (composer->persist_file_interface, my_file_name, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		e_notice (GTK_WINDOW (composer), GNOME_MESSAGE_BOX_ERROR,
			  _("Error saving file: %s"), g_basename (file_name));
	}

	CORBA_exception_free (&ev);

	g_free (my_file_name);
}

static void
load (EMsgComposer *composer,
      const char *file_name)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	Bonobo_PersistFile_load (composer->persist_file_interface, file_name, &ev);

	if (ev._major != CORBA_NO_EXCEPTION)
		e_notice (GTK_WINDOW (composer), GNOME_MESSAGE_BOX_ERROR,
			  _("Error loading file: %s"), g_basename (file_name));

	CORBA_exception_free (&ev);
}


/* Exit dialog.  (Displays a "Save composition to 'Drafts' before exiting?" warning before actually exiting.)  */

enum { REPLY_YES = 0, REPLY_NO, REPLY_CANCEL };

static void
exit_dialog_cb (int reply, EMsgComposer *composer)
{
	extern CamelFolder *drafts_folder;
	CamelMimeMessage *msg;
	CamelException *ex;
	char *reason;
	
	switch (reply) {
	case REPLY_YES:
		msg = e_msg_composer_get_message (composer);

		ex = camel_exception_new ();
		camel_folder_append_message (drafts_folder, msg, CAMEL_MESSAGE_DRAFT, ex);
		if (camel_exception_is_set (ex))
			goto error;
		
		camel_exception_free (ex);
	case REPLY_NO:
		gtk_widget_destroy (GTK_WIDGET (composer));
		break;
	case REPLY_CANCEL:
	default:
	}
	
	return;
	
 error:
	reason = g_strdup_printf ("Error saving composition to 'Drafts': %s",
				  camel_exception_get_description (ex));
	
	camel_exception_free (ex);
	gnome_warning_dialog_parented (reason, GTK_WINDOW (composer));
	g_free (reason);
}

static void
do_exit (EMsgComposer *composer)
{
	GtkWidget *dialog;
	GtkWidget *label;
	gint button;
	
	dialog = gnome_dialog_new (_("Evolution"),
				   GNOME_STOCK_BUTTON_YES,      /* Save */
				   GNOME_STOCK_BUTTON_NO,       /* Don't save */
				   GNOME_STOCK_BUTTON_CANCEL,   /* Cancel */
				   NULL);
	
	label = gtk_label_new (_("This message has not been sent.\n\nDo you wish to save your changes?"));
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), label, TRUE, TRUE, 0);
	gtk_widget_show (label);
	gnome_dialog_set_parent (GNOME_DIALOG (dialog), GTK_WINDOW (composer));
	gnome_dialog_set_default (GNOME_DIALOG (dialog), 0);
	button = gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
	
	exit_dialog_cb (button, composer);
}


/* Menu callbacks.  */

static void
menu_file_open_cb (BonoboUIHandler *uih,
		   void *data,
		   const char *path)
{
	EMsgComposer *composer;
	char *file_name;

	composer = E_MSG_COMPOSER (data);

	file_name = e_msg_composer_select_file (composer, _("Open file"));
	if (file_name == NULL)
		return;

	load (composer, file_name);

	g_free (file_name);
}

static void
menu_file_save_cb (BonoboUIHandler *uih,
		   void *data,
		   const char *path)
{
	EMsgComposer *composer;
	CORBA_char *file_name;
	CORBA_Environment ev;

	composer = E_MSG_COMPOSER (data);

	CORBA_exception_init (&ev);

	file_name = Bonobo_PersistFile_get_current_file (composer->persist_file_interface, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		save (composer, NULL);
	} else {
		save (composer, file_name);
		CORBA_free (file_name);
	}

	CORBA_exception_free (&ev);
}

static void
menu_file_save_as_cb (BonoboUIHandler *uih,
		      void *data,
		      const char *path)
{
	EMsgComposer *composer;

	composer = E_MSG_COMPOSER (data);

	save (composer, NULL);
}

static void
menu_file_send_cb (BonoboUIHandler *uih,
		   void *data,
		   const char *path)
{
	/* FIXME: We should really write this to Outbox in the future? */
	gtk_signal_emit (GTK_OBJECT (data), signals[SEND]);
}

static void
menu_file_close_cb (BonoboUIHandler *uih,
		    void *data,
		    const char *path)
{
	EMsgComposer *composer;

	composer = E_MSG_COMPOSER (data);
	do_exit (composer);
}
	
static void
menu_file_add_attachment_cb (BonoboUIHandler *uih,
			     void *data,
			     const char *path)
{
	EMsgComposer *composer;

	composer = E_MSG_COMPOSER (data);

	e_msg_composer_attachment_bar_attach
		(E_MSG_COMPOSER_ATTACHMENT_BAR (composer->attachment_bar),
		 NULL);
}

static void
menu_view_attachments_activate_cb (BonoboUIHandler *uih,
				   void *data,
				   const char *path)
{
	gboolean state;

	state = bonobo_ui_handler_menu_get_toggle_state (uih, path);
	e_msg_composer_show_attachments (E_MSG_COMPOSER (data), state);
}

#if 0
static void
insert_file_ok_cb (GtkWidget *widget, void *user_data)
{
	GtkFileSelection *fs;
	GdkAtom selection_atom = GDK_NONE;
	char *name;
	EMsgComposer *composer;
	struct stat sb;
	int fd;
	guint8 *buffer;
	size_t bufsz, actual;

	fs = GTK_FILE_SELECTION (gtk_widget_get_ancestor (widget,
							  GTK_TYPE_FILE_SELECTION));
	composer = E_MSG_COMPOSER (user_data);
	name = gtk_file_selection_get_filename (fs);
									   
	if (stat (name, &sb) < 0) {
		GtkWidget *dlg;

		dlg = gnome_error_dialog_parented( _("That file does not exist."),
						   GTK_WINDOW (fs));
		gnome_dialog_run_and_close (GNOME_DIALOG (dlg));
		gtk_widget_destroy (GTK_WIDGET (dlg));
		return;
	}

	if( !(S_ISREG (sb.st_mode)) ) {
		GtkWidget *dlg;

		dlg = gnome_error_dialog_parented( _("That is not a regular file."),
						   GTK_WINDOW (fs));
		gnome_dialog_run_and_close (GNOME_DIALOG (dlg));
		gtk_widget_destroy (GTK_WIDGET (dlg));
		return;
	}

	if (access (name, R_OK) != 0) {
		GtkWidget *dlg;

		dlg = gnome_error_dialog_parented( _("That file exists but is not readable."),
						   GTK_WINDOW (fs));
		gnome_dialog_run_and_close (GNOME_DIALOG (dlg));
		gtk_widget_destroy (GTK_WIDGET (dlg));
		return;
	}

	if ((fd = open (name, O_RDONLY)) < 0) {
		GtkWidget *dlg;

		dlg = gnome_error_dialog_parented( _("That file appeared accesible but open(2) failed."),
						   GTK_WINDOW (fs));
		gnome_dialog_run_and_close (GNOME_DIALOG (dlg));
		gtk_widget_destroy (GTK_WIDGET (dlg));
		return;
	}

	buffer = NULL;
	bufsz = 0;
	actual = 0;
	#define CHUNK 5120

	while( 1 ) {
		ssize_t chunk;

		if( bufsz - actual < CHUNK ) {
			bufsz += CHUNK;

			if( bufsz >= 102400 ) {
				GtkWidget *dlg;
				gint result;

				dlg = gnome_dialog_new( _("The file is very large (more than 100K).\n"
							  "Are you sure you wish to insert it?"),
							GNOME_STOCK_BUTTON_YES,
							GNOME_STOCK_BUTTON_NO,
							NULL);
				gnome_dialog_set_parent (GNOME_DIALOG (dlg), GTK_WINDOW (fs));
				result = gnome_dialog_run_and_close (GNOME_DIALOG (dlg));
				gtk_widget_destroy (GTK_WIDGET (dlg));
				
				if (result == 1)
					goto cleanup;
			}

			buffer = g_realloc (buffer, bufsz * sizeof (guint8));
		}

		chunk = read (fd, &(buffer[actual]), CHUNK);

		if (chunk < 0) {
			GtkWidget *dlg;

			dlg = gnome_error_dialog_parented( _("An error occurred while reading the file."),
							   GTK_WINDOW (fs));
			gnome_dialog_run_and_close (GNOME_DIALOG (dlg));
			gtk_widget_destroy (GTK_WIDGET (dlg));
			goto cleanup;
		}

		if( chunk == 0 )
			break;

		actual += chunk;
	}

	buffer[actual] = '\0';

	if (selection_atom == GDK_NONE)
		selection_atom = gdk_atom_intern ("TEMP_PASTE", FALSE);
	gtk_object_set_data (GTK_OBJECT (fs), "ev_file_buffer", buffer);
	gtk_selection_owner_set (GTK_WIDGET (fs), selection_atom, GDK_CURRENT_TIME);
	/*gtk_html_paste (composer->send_html);*/

 cleanup:
	close( fd );
	g_free( buffer );
	gtk_widget_destroy (GTK_WIDGET(fs));
}

static void fs_selection_get (GtkWidget *widget, GtkSelectionData *sdata,
			      guint info, guint time)
{
	gchar *buffer;
	GdkAtom encoding;
	gint format;
	guchar *ctext;
	gint length;

	buffer = gtk_object_get_data (GTK_OBJECT (widget), "ev_file_buffer");
	if (gdk_string_to_compound_text (buffer, &encoding, &format, &ctext,
					 &length) == Success)
		gtk_selection_data_set (sdata, encoding, format, ctext, length);
	g_free (buffer);
	gtk_object_remove_data (GTK_OBJECT (widget), "ev_file_buffer");
}

#endif
static void
menu_file_insert_file_cb (BonoboUIHandler *uih,
		void *data,
		const char *path)
{
#if 0
	EMsgComposer *composer;
	GtkFileSelection *fs;

	composer = E_MSG_COMPOSER (data);

	fs = GTK_FILE_SELECTION (gtk_file_selection_new ("Choose File"));
	/* FIXME: remember the location or something */
	/*gtk_file_selection_set_filename( fs, g_get_home_dir() );*/
	gtk_signal_connect (GTK_OBJECT (fs->ok_button), "clicked",
			    GTK_SIGNAL_FUNC (insert_file_ok_cb), data);
	gtk_signal_connect_object (GTK_OBJECT (fs->cancel_button), 
				   "clicked",
				   GTK_SIGNAL_FUNC (gtk_widget_destroy), 
				   GTK_OBJECT (fs));
	gtk_widget_show (GTK_WIDGET(fs));
#else
	g_message ("Insert file is unimplemented! oh no!");
#endif
}

static void
menu_format_html_cb (BonoboUIHandler *uih,
		     void *data,
		     const char *path)
{
	EMsgComposer *composer;
	gboolean new_state;

	composer = E_MSG_COMPOSER (data);

	new_state = bonobo_ui_handler_menu_get_toggle_state (uih, path);
	if ((new_state && composer->send_html) || (! new_state && ! composer->send_html))
		return;

	e_msg_composer_set_send_html (composer, new_state);
}


/* Menu bar creation.  */

static void
create_menubar_file (EMsgComposer *composer,
		     BonoboUIHandler *uih)
{
	bonobo_ui_handler_menu_new_subtree (uih, "/File",
					    _("_File"),
					    NULL, -1,
					    BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					    0, 0);

	bonobo_ui_handler_menu_new_item (uih, "/File/Open",
					 _("_Open..."),
					 _("Load a previously saved message"),
					 -1,
					 BONOBO_UI_HANDLER_PIXMAP_STOCK,
					 GNOME_STOCK_MENU_OPEN,
					 0, 0,
					 menu_file_open_cb, composer);

	bonobo_ui_handler_menu_new_item (uih, "/File/Save",
					 _("_Save..."),
					 _("Save message"),
					 -1,
					 BONOBO_UI_HANDLER_PIXMAP_STOCK,
					 GNOME_STOCK_MENU_SAVE,
					 0, 0,
					 menu_file_save_cb, composer);

	bonobo_ui_handler_menu_new_item (uih, "/File/Save as",
					 _("_Save as..."),
					 _("Save message with a different name"),
					 -1,
					 BONOBO_UI_HANDLER_PIXMAP_STOCK,
					 GNOME_STOCK_MENU_SAVE_AS,
					 0, 0,
					 menu_file_save_as_cb, composer);

	bonobo_ui_handler_menu_new_item (uih, "/File/Save in folder",
					 _("Save in _folder..."),
					 _("Save the message in a specified folder"),
					 -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					 0, 0,
					 NULL, composer);

	bonobo_ui_handler_menu_new_separator (uih, "/File/Separator1", -1);

	bonobo_ui_handler_menu_new_item (uih, "/File/Insert text file",
					 _("_Insert text file... (FIXME)"),
					 _("Insert a file as text into the message"),
					 -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					 0, 0,
					 menu_file_insert_file_cb, composer);

	bonobo_ui_handler_menu_new_separator (uih, "/File/Separator2", -1);

	bonobo_ui_handler_menu_new_item (uih, "/File/Send",
					 _("_Send"),
					 _("Send the message"),
					 -1,
					 BONOBO_UI_HANDLER_PIXMAP_STOCK,
					 GNOME_STOCK_MENU_MAIL_SND,
					 0, 0,
					 menu_file_send_cb, composer);

	bonobo_ui_handler_menu_new_separator (uih, "/File/Separator3", -1);

	bonobo_ui_handler_menu_new_item (uih, "/File/Close",
					 _("_Close..."),
					 _("Quit the message composer"),
					 -1,
					 BONOBO_UI_HANDLER_PIXMAP_STOCK,
					 GNOME_STOCK_MENU_CLOSE,
					 0, 0,
					 menu_file_close_cb, composer);
}

static void
create_menubar_edit (EMsgComposer *composer,
		     BonoboUIHandler *uih)
{
	bonobo_ui_handler_menu_new_subtree (uih, "/Edit",
					    _("_Edit"),
					    NULL, -1,
					    BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					    0, 0);
}

static void
create_menubar_format (EMsgComposer *composer,
		       BonoboUIHandler *uih)
{
	bonobo_ui_handler_menu_new_subtree (uih, "/Format",
					    _("_Format"),
					    NULL, -1,
					    BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					    0, 0);

	bonobo_ui_handler_menu_new_toggleitem (uih, "/Format/HTML",
					       _("HTML"),
					       _("Send the mail in HTML format"),
					       -1,
					       0, 0,
					       menu_format_html_cb, composer);

	bonobo_ui_handler_menu_set_toggle_state (uih, "/Format/HTML", composer->send_html);
}

static void
create_menubar_view (EMsgComposer *composer,
		     BonoboUIHandler *uih)
{
	bonobo_ui_handler_menu_new_subtree (uih, "/View",
					    _("_View"),
					    NULL, -1,
					    BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					    0, 0);

	bonobo_ui_handler_menu_new_toggleitem (uih, "/View/Show attachments",
					       _("Show _attachments"),
					       _("Show/hide attachments"),
					       -1,
					       0, 0,
					       menu_view_attachments_activate_cb, composer);
}

static void
create_menubar (EMsgComposer *composer)
{
	BonoboUIHandler *uih;

	uih = composer->uih;
	bonobo_ui_handler_create_menubar (uih);

	create_menubar_file   (composer, uih);
	create_menubar_edit   (composer, uih);
	create_menubar_format (composer, uih);
	create_menubar_view   (composer, uih);
}


/* Toolbar implementation.  */

static void
create_toolbar (EMsgComposer *composer)
{
	BonoboUIHandler *uih;

	uih = composer->uih;
	bonobo_ui_handler_create_toolbar (uih, "Toolbar");

	bonobo_ui_handler_toolbar_new_item (uih,
					    "/Toolbar/Send",
					    _("Send"),
					    _("Send this message"),
					    -1,
					    BONOBO_UI_HANDLER_PIXMAP_STOCK,
					    GNOME_STOCK_PIXMAP_MAIL_SND,
					    0, 0,
					    menu_file_send_cb, composer);

	bonobo_ui_handler_toolbar_new_item (uih,
					    "/Toolbar/Attach",
					    _("Attach"),
					    _("Attach a file"),
					    -1,
					    BONOBO_UI_HANDLER_PIXMAP_STOCK,
					    GNOME_STOCK_PIXMAP_ATTACH,
					    0, 0,
					    menu_file_add_attachment_cb, composer);
}


/* Miscellaneous callbacks.  */

static void
attachment_bar_changed_cb (EMsgComposerAttachmentBar *bar,
			   void *data)
{
	EMsgComposer *composer;
	
	composer = E_MSG_COMPOSER (data);

	if (e_msg_composer_attachment_bar_get_num_attachments (bar) > 0)
		e_msg_composer_show_attachments (composer, TRUE);
	else
		e_msg_composer_show_attachments (composer, FALSE);
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	EMsgComposer *composer;
	CORBA_Environment ev;

	composer = E_MSG_COMPOSER (object);

	bonobo_object_unref (BONOBO_OBJECT (composer->uih));

	/* FIXME?  I assume the Bonobo widget will get destroyed
           normally?  */

	if (composer->address_dialog != NULL)
		gtk_widget_destroy (composer->address_dialog);
	if (composer->hdrs != NULL)
		gtk_widget_destroy (composer->hdrs);

	if (composer->extra_hdr_names) {
		int i;

		for (i = 0; i < composer->extra_hdr_names->len; i++) {
			g_free (composer->extra_hdr_names->pdata[i]);
			g_free (composer->extra_hdr_values->pdata[i]);
		}
		g_ptr_array_free (composer->extra_hdr_names, TRUE);
		g_ptr_array_free (composer->extra_hdr_values, TRUE);
	}

	CORBA_exception_init (&ev);

	if (composer->persist_stream_interface != CORBA_OBJECT_NIL) {
		Bonobo_Unknown_unref (composer->persist_stream_interface, &ev);
		CORBA_Object_release (composer->persist_stream_interface, &ev);
	}

	if (composer->persist_file_interface != CORBA_OBJECT_NIL) {
		Bonobo_Unknown_unref (composer->persist_file_interface, &ev);
		CORBA_Object_release (composer->persist_file_interface, &ev);
	}

	CORBA_exception_free (&ev);

	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


/* GtkWidget methods.  */

static int
delete_event (GtkWidget *widget,
	      GdkEventAny *event)
{
	do_exit (E_MSG_COMPOSER (widget));

	return TRUE;
}


static void
class_init (EMsgComposerClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = GTK_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);

	object_class->destroy = destroy;

	widget_class->delete_event = delete_event;

	parent_class = gtk_type_class (gnome_app_get_type ());

	signals[SEND] =
		gtk_signal_new ("send",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EMsgComposerClass, send),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	signals[POSTPONE] =
		gtk_signal_new ("postpone",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EMsgComposerClass, postpone),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
init (EMsgComposer *composer)
{
	composer->uih                      = NULL;

	composer->hdrs                     = NULL;
	composer->extra_hdr_names          = g_ptr_array_new ();
	composer->extra_hdr_values         = g_ptr_array_new ();

	composer->editor                   = NULL;

	composer->address_dialog           = NULL;

	composer->attachment_bar           = NULL;
	composer->attachment_scroll_frame  = NULL;

	composer->persist_file_interface   = CORBA_OBJECT_NIL;
	composer->persist_stream_interface = CORBA_OBJECT_NIL;

	composer->attachment_bar_visible   = FALSE;
	composer->send_html                = FALSE;
}


GtkType
e_msg_composer_get_type (void)
{
	static GtkType type = 0;

	if (type == 0) {
		static const GtkTypeInfo info = {
			"EMsgComposer",
			sizeof (EMsgComposer),
			sizeof (EMsgComposerClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		type = gtk_type_unique (gnome_app_get_type (), &info);
	}

	return type;
}

/**
 * e_msg_composer_construct:
 * @composer: A message composer widget
 * 
 * Construct @composer.
 **/
void
e_msg_composer_construct (EMsgComposer *composer)
{
	GtkWidget *vbox;
	BonoboObject *editor_server;
	
	g_return_if_fail (gtk_main_level () > 0);
	
	gtk_window_set_default_size (GTK_WINDOW (composer),
				     DEFAULT_WIDTH, DEFAULT_HEIGHT);
	
	gnome_app_construct (GNOME_APP (composer), "e-msg-composer",
			     _("Compose a message"));
	
	composer->uih = bonobo_ui_handler_new ();
	bonobo_ui_handler_set_app (composer->uih, GNOME_APP (composer));
	
	vbox = gtk_vbox_new (FALSE, 0);
	
	composer->hdrs = e_msg_composer_hdrs_new ();
	gtk_box_pack_start (GTK_BOX (vbox), composer->hdrs, FALSE, FALSE, 0);
	gtk_widget_show (composer->hdrs);
	
	/* Editor component.  */
	
	create_menubar (composer);
	create_toolbar (composer);
	composer->editor = create_editor (composer);

	editor_server = BONOBO_OBJECT (bonobo_widget_get_server (BONOBO_WIDGET (composer->editor)));
	
	composer->persist_file_interface
		= bonobo_object_query_interface (editor_server, "IDL:Bonobo/PersistFile:1.0");
	composer->persist_stream_interface
		= bonobo_object_query_interface (editor_server, "IDL:Bonobo/PersistStream:1.0");
	
	gtk_widget_show (composer->editor);
	gtk_box_pack_start (GTK_BOX (vbox), composer->editor, TRUE, TRUE, 0);
	gtk_widget_show (composer->editor);

	/* Attachment editor, wrapped into an EScrollFrame.  We don't
           show it for now.  */

	composer->attachment_scroll_frame = e_scroll_frame_new (NULL, NULL);
	e_scroll_frame_set_shadow_type (E_SCROLL_FRAME (composer->attachment_scroll_frame),
					GTK_SHADOW_IN);
	e_scroll_frame_set_policy (E_SCROLL_FRAME (composer->attachment_scroll_frame),
				   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	composer->attachment_bar = e_msg_composer_attachment_bar_new (NULL);
	GTK_WIDGET_SET_FLAGS (composer->attachment_bar, GTK_CAN_FOCUS);
	gtk_container_add (GTK_CONTAINER (composer->attachment_scroll_frame),
			   composer->attachment_bar);
	gtk_box_pack_start (GTK_BOX (vbox),
			    composer->attachment_scroll_frame,
			    FALSE, FALSE, GNOME_PAD_SMALL);

	gtk_signal_connect (GTK_OBJECT (composer->attachment_bar), "changed",
			    GTK_SIGNAL_FUNC (attachment_bar_changed_cb), composer);

	gnome_app_set_contents (GNOME_APP (composer), vbox);
	gtk_widget_show (vbox);

	e_msg_composer_show_attachments (composer, FALSE);

	/* Set focus on the `To:' field.  */

	gtk_widget_grab_focus (e_msg_composer_hdrs_get_to_entry (E_MSG_COMPOSER_HDRS (composer->hdrs)));
}

/**
 * e_msg_composer_new:
 *
 * Create a new message composer widget.  This function must be called
 * within the GTK+ main loop, or it will fail.
 * 
 * Return value: A pointer to the newly created widget
 **/
GtkWidget *
e_msg_composer_new (void)
{
	GtkWidget *new;

	g_return_val_if_fail (gtk_main_level () > 0, NULL);
 
	new = gtk_type_new (e_msg_composer_get_type ());
	e_msg_composer_construct (E_MSG_COMPOSER (new));

	/* Load the signature, if any. */
	set_editor_text (BONOBO_WIDGET (E_MSG_COMPOSER (new)->editor), 
			 NULL, "");

	return new;
}

/**
 * e_msg_composer_new_with_sig_file:
 *
 * Create a new message composer widget.  This function must be called
 * within the GTK+ main loop, or it will fail.  Sets the signature
 * file.
 * 
 * Return value: A pointer to the newly created widget
 **/
GtkWidget *
e_msg_composer_new_with_sig_file (const char *sig_file)
{
	GtkWidget *new;
	
	g_return_val_if_fail (gtk_main_level () > 0, NULL);
	
	new = gtk_type_new (e_msg_composer_get_type ());
	e_msg_composer_construct (E_MSG_COMPOSER (new));
	
	/* Load the signature, if any. */
	set_editor_text (BONOBO_WIDGET (E_MSG_COMPOSER (new)->editor), 
			 sig_file, "");
	
	return new;
}

/**
 * e_msg_composer_new_with_message:
 *
 * Create a new message composer widget.  This function must be called
 * within the GTK+ main loop, or it will fail.
 * 
 * Return value: A pointer to the newly created widget
 **/
GtkWidget *
e_msg_composer_new_with_message (CamelMimeMessage *msg)
{
	const CamelInternetAddress *to, *cc, *bcc;
	GList *To = NULL, *Cc = NULL, *Bcc = NULL;
	gboolean want_plain, is_html;
	CamelDataWrapper *contents;
	const MailConfig *config;
	const gchar *subject;
	GtkWidget *new;
	char *text, *final_text;
	guint len, i;
	
	g_return_val_if_fail (gtk_main_level () > 0, NULL);
	
	new = gtk_type_new (e_msg_composer_get_type ());
	e_msg_composer_construct (E_MSG_COMPOSER (new));
	
	subject = camel_mime_message_get_subject (msg);
	
	to = camel_mime_message_get_recipients (msg, CAMEL_RECIPIENT_TYPE_TO);
	cc = camel_mime_message_get_recipients (msg, CAMEL_RECIPIENT_TYPE_CC);
	bcc = camel_mime_message_get_recipients (msg, CAMEL_RECIPIENT_TYPE_BCC);
	
	len = CAMEL_ADDRESS (to)->addresses->len;
	for (i = 0; i < len; i++) {
		const char *addr;
		
		if (camel_internet_address_get (to, i, NULL, &addr))
			To = g_list_append (To, g_strdup (addr));
	}
	
	len = CAMEL_ADDRESS (cc)->addresses->len;
	for (i = 0; i < len; i++) {
		const char *addr;
		
		if (camel_internet_address_get (cc, i, NULL, &addr))
			Cc = g_list_append (Cc, g_strdup (addr));
	}
	
	len = CAMEL_ADDRESS (bcc)->addresses->len;
	for (i = 0; i < len; i++) {
		const char *addr;
		
		if (camel_internet_address_get (bcc, i, NULL, &addr))
			Bcc = g_list_append (Bcc, g_strdup (addr));
	}
	
	e_msg_composer_set_headers (E_MSG_COMPOSER (new), To, Cc, Bcc, subject);
	
	free_recipients (To);
	free_recipients (Cc);
	free_recipients (Bcc);
	
	config = mail_config_fetch ();
	want_plain = !config->send_html;

	contents = camel_medium_get_content_object (CAMEL_MEDIUM (msg));
	text = mail_get_message_body (contents, want_plain, &is_html);
	if (is_html)
		final_text = g_strdup (text);
	else
		final_text = e_text_to_html (text, E_TEXT_TO_HTML_CONVERT_NL |
					     E_TEXT_TO_HTML_CONVERT_SPACES);
	g_free (text);
	
	e_msg_composer_set_body_text (E_MSG_COMPOSER (new), final_text);
	
	/*set_editor_text (BONOBO_WIDGET (E_MSG_COMPOSER (new)->editor), 
	  NULL, "FIXME: like, uh... put the message here and stuff\n");*/
	
	return new;
}

static GList *
add_recipients (GList *list, const char *recips, gboolean decode)
{
	int len;
	char *addr;

	while (*recips) {
		len = strcspn (recips, ",");
		if (len) {
			addr = g_strndup (recips, len);
			if (decode)
				camel_url_decode (addr);
			list = g_list_append (list, addr);
		}
		recips += len;
		if (*recips == ',')
			recips++;
	}

	return list;
}

static void
free_recipients (GList *list)
{
	GList *l;

	for (l = list; l; l = l->next)
		g_free (l->data);
	g_list_free (list);
}

/**
 * e_msg_composer_new_from_url:
 * @url: a mailto URL
 *
 * Create a new message composer widget, and fill in fields as
 * defined by the provided URL.
 **/
GtkWidget *
e_msg_composer_new_from_url (const char *url)
{
	EMsgComposer *composer;
	EMsgComposerHdrs *hdrs;
	GList *to = NULL, *cc = NULL, *bcc = NULL;
	char *subject = NULL, *body = NULL;
	const char *p, *header;
	int len, clen;
	char *content;

	g_return_val_if_fail (g_strncasecmp (url, "mailto:", 7) == 0, NULL);

	/* Parse recipients (everything after ':' until '?' or eos. */
	p = url + 7;
	len = strcspn (p, "?,");
	if (len) {
		content = g_strndup (p, len);
		to = add_recipients (to, content, TRUE);
		g_free (content);
	}

	p += len;
	if (*p == '?') {
		p++;

		while (*p) {
			len = strcspn (p, "=&");

			/* If it's malformed, give up. */
			if (p[len] != '=')
				break;

			header = p;
			p += len + 1;

			clen = strcspn (p, "&");
			content = g_strndup (p, clen);
			camel_url_decode (content);

			if (!g_strncasecmp (header, "to", len))
				to = add_recipients (to, content, FALSE);
			else if (!g_strncasecmp (header, "cc", len))
				cc = add_recipients (cc, content, FALSE);
			else if (!g_strncasecmp (header, "bcc", len))
				bcc = add_recipients (bcc, content, FALSE);
			else if (!g_strncasecmp (header, "subject", len))
				subject = g_strdup (content);
			else if (!g_strncasecmp (header, "body", len))
				body = g_strdup (content);

			g_free (content);
			p += clen;
			if (*p == '&') {
				p++;
				if (!strcmp (p, "amp;"))
					p += 4;
			}
		}
	}
		
	composer = E_MSG_COMPOSER (e_msg_composer_new ());
	hdrs = E_MSG_COMPOSER_HDRS (composer->hdrs);
	e_msg_composer_hdrs_set_to (hdrs, to);
	free_recipients (to);
	e_msg_composer_hdrs_set_cc (hdrs, cc);
	free_recipients (cc);
	e_msg_composer_hdrs_set_bcc (hdrs, bcc);
	free_recipients (bcc);
	if (subject) {
		e_msg_composer_hdrs_set_subject (hdrs, subject);
		g_free (subject);
	}

	if (body) {
		char *htmlbody = e_text_to_html (body, E_TEXT_TO_HTML_PRE);
		set_editor_text (BONOBO_WIDGET (composer->editor), 
				 NULL, htmlbody);
		g_free (htmlbody);
	}

	return GTK_WIDGET (composer);
}

/**
 * e_msg_composer_show_attachments:
 * @composer: A message composer widget
 * @show: A boolean specifying whether the attachment bar should be shown or
 * not
 * 
 * If @show is %FALSE, hide the attachment bar.  Otherwise, show it.
 **/
void
e_msg_composer_show_attachments (EMsgComposer *composer,
				 gboolean show)
{
	g_return_if_fail (composer != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	show_attachments (composer, show);
}

/**
 * e_msg_composer_set_headers:
 * @composer: a composer object
 * @to: the values for the "To" header
 * @cc: the values for the "Cc" header
 * @bcc: the values for the "Bcc" header
 * @subject: the value for the "Subject" header
 *
 * Sets the headers in the composer to the given values.
 **/
void 
e_msg_composer_set_headers (EMsgComposer *composer, const GList *to,
			    const GList *cc, const GList *bcc,
			    const char *subject)
{
	EMsgComposerHdrs *hdrs;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	hdrs = E_MSG_COMPOSER_HDRS (composer->hdrs);

	e_msg_composer_hdrs_set_to (hdrs, to);
	e_msg_composer_hdrs_set_cc (hdrs, cc);
	e_msg_composer_hdrs_set_bcc (hdrs, bcc);
	e_msg_composer_hdrs_set_subject (hdrs, subject);
}


/**
 * e_msg_composer_set_body_text:
 * @composer: a composer object
 * @text: the HTML text to initialize the editor with
 *
 * Loads the given HTML text into the editor.
 **/
void
e_msg_composer_set_body_text (EMsgComposer *composer, const char *text)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	set_editor_text (BONOBO_WIDGET (composer->editor), 
			 composer->sig_file, text);
}


/**
 * e_msg_composer_add_header:
 * @composer: a composer object
 * @name: the header name
 * @value: the header value
 *
 * Adds a header with @name and @value to the message. This header
 * may not be displayed by the composer, but will be included in
 * the message it outputs.
 **/
void
e_msg_composer_add_header (EMsgComposer *composer, const char *name,
			   const char *value)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (name != NULL);
	g_return_if_fail (value != NULL);

	g_ptr_array_add (composer->extra_hdr_names, g_strdup (name));
	g_ptr_array_add (composer->extra_hdr_values, g_strdup (value));
}


/**
 * e_msg_composer_attach:
 * @composer: a composer object
 * @attachment: the CamelMimePart to attach
 *
 * Attaches @attachment to the message being composed in the composer.
 **/
void
e_msg_composer_attach (EMsgComposer *composer, CamelMimePart *attachment)
{
	EMsgComposerAttachmentBar *bar;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (CAMEL_IS_MIME_PART (attachment));

	bar = E_MSG_COMPOSER_ATTACHMENT_BAR (composer->attachment_bar);
	e_msg_composer_attachment_bar_attach_mime_part (bar, attachment);
}


/**
 * e_msg_composer_get_message:
 * @composer: A message composer widget
 * 
 * Retrieve the message edited by the user as a CamelMimeMessage.  The
 * CamelMimeMessage object is created on the fly; subsequent calls to this
 * function will always create new objects from scratch.
 * 
 * Return value: A pointer to the new CamelMimeMessage object
 **/
CamelMimeMessage *
e_msg_composer_get_message (EMsgComposer *composer)
{
	g_return_val_if_fail (composer != NULL, NULL);
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	return build_message (composer);
}



/**
 * e_msg_composer_set_sig:
 * @composer: A message composer widget
 * @path: Signature file
 * 
 * Set a signature
 **/
void
e_msg_composer_set_sig_file (EMsgComposer *composer, const char *sig_file)
{
	g_return_if_fail (composer != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	composer->sig_file = g_strdup (sig_file);
}

/**
 * e_msg_composer_get_sig_file:
 * @composer: A message composer widget
 * 
 * Get the signature file
 * 
 * Return value: The signature file.
 **/
char *
e_msg_composer_get_sig_file (EMsgComposer *composer)
{
	g_return_val_if_fail (composer != NULL, NULL);
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	return composer->sig_file;
}


/**
 * e_msg_composer_set_send_html:
 * @composer: A message composer widget
 * @send_html: Whether the composer should have the "Send HTML" flag set
 * 
 * Set the status of the "Send HTML" toggle item.  The user can override it.
 **/
void
e_msg_composer_set_send_html (EMsgComposer *composer,
			      gboolean send_html)
{
	g_return_if_fail (composer != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	if (composer->send_html && send_html)
		return;
	if (! composer->send_html && ! send_html)
		return;

	composer->send_html = send_html;
	bonobo_ui_handler_menu_set_toggle_state (composer->uih, "/Format/HTML", send_html);
}

/**
 * e_msg_composer_get_send_html:
 * @composer: A message composer widget
 * 
 * Get the status of the "Send HTML mail" flag.
 * 
 * Return value: The status of the "Send HTML mail" flag.
 **/
gboolean
e_msg_composer_get_send_html (EMsgComposer *composer)
{
	g_return_val_if_fail (composer != NULL, FALSE);
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);

	return composer->send_html;
}
