/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * mail-display.c: Mail display widget
 *
 * Author:
 *   Miguel de Icaza
 *   Bertrand Guiheneuf (bg@aful.org)
 *
 * (C) 2000 Helix Code, Inc.
 */
#include <config.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <gnome.h>
#include "e-util/e-setup.h"
#include "e-util/e-util.h"
#include "mail-display.h"
#include "mail.h"

#include <bonobo.h>
#include <libgnorba/gnorba.h>
#include <bonobo/bonobo-stream-memory.h>

#define PARENT_TYPE (gtk_vbox_get_type ())

static GtkObjectClass *mail_display_parent_class;


/*----------------------------------------------------------------------*
 *                        Callbacks
 *----------------------------------------------------------------------*/

static void
save_data_eexist_cb (int reply, gpointer user_data)
{
	gboolean *ok = user_data;

	*ok = reply == 0;
	gtk_main_quit ();
}

static void
save_data_cb (GtkWidget *widget, gpointer user_data)
{
	CamelDataWrapper *data = user_data;
	CamelStream *stream_fs;
	GtkFileSelection *file_select = (GtkFileSelection *)
		gtk_widget_get_ancestor (widget, GTK_TYPE_FILE_SELECTION);
	char *name;
	int fd;

	name = gtk_file_selection_get_filename (file_select);

	fd = open (name, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
	if (fd == -1 && errno == EEXIST) {
		gboolean ok = FALSE;

		gnome_ok_cancel_dialog_modal_parented (
			"A file by that name already exists.\nOverwrite it?",
			save_data_eexist_cb, &ok, GTK_WINDOW (file_select));
		gtk_main ();
		if (!ok)
			return;
		fd = open (name, O_WRONLY | O_TRUNC);
	}

	if (fd == -1) {
		char *msg;

		msg = g_strdup_printf ("Could not open file %s:\n%s",
				       name, g_strerror (errno));
		gnome_error_dialog_parented (msg, GTK_WINDOW (file_select));
		return;
	}

	stream_fs = camel_stream_fs_new_with_fd (fd);
	if (camel_data_wrapper_write_to_stream (data, stream_fs) == -1
	    || camel_stream_flush (stream_fs) == -1) {
		char *msg;

		msg = g_strdup_printf ("Could not write data: %s", strerror(errno));
		gnome_error_dialog_parented (msg, GTK_WINDOW (file_select));
	}
	gtk_object_unref (GTK_OBJECT (stream_fs));

	gtk_widget_destroy (GTK_WIDGET (file_select));
}

static void
save_data (const char *cid, CamelMimeMessage *message)
{
	GHashTable *urls;
	CamelMimePart *part;
	CamelDataWrapper *data;
	GtkFileSelection *file_select;
	char *filename;

	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));
	urls = gtk_object_get_data (GTK_OBJECT (message), "urls");
	part = g_hash_table_lookup (urls, cid);
	g_return_if_fail (CAMEL_IS_MIME_PART (part));
	data = camel_medium_get_content_object (CAMEL_MEDIUM (part));

	filename = (char *)camel_mime_part_get_filename (part);
	if (filename) {
		char *p;

		p = strrchr (filename, '/');
		if (p)
			filename = g_strdup_printf ("%s%s", evolution_dir, p);
		else {
			filename = g_strdup_printf ("%s/%s", evolution_dir,
						    filename);
		}

		for (p = strrchr (filename, '/') + 1; *p; p++) {
			if (!isascii ((unsigned char)*p) ||
			    strchr (" /'\"`&();|<>${}!", *p))
				*p = '_';
		}
	} else
		filename = g_strdup_printf ("%s/attachment", evolution_dir);

	file_select = GTK_FILE_SELECTION (gtk_file_selection_new ("Save Attachment"));
	gtk_file_selection_set_filename (file_select, filename);
	g_free (filename);

	gtk_signal_connect (GTK_OBJECT (file_select->ok_button), "clicked", 
			    GTK_SIGNAL_FUNC (save_data_cb), data);
	gtk_signal_connect_object (GTK_OBJECT (file_select->cancel_button),
				   "clicked",
				   GTK_SIGNAL_FUNC (gtk_widget_destroy),
				   GTK_OBJECT (file_select));

	gtk_widget_show (GTK_WIDGET (file_select));
}

static void
on_link_clicked (GtkHTML *html, const char *url, gpointer user_data)
{
	CamelMimeMessage *message;

	message = gtk_object_get_data (GTK_OBJECT (html), "message");

	if (!strncasecmp (url, "news:", 5) ||
	    !strncasecmp (url, "nntp:", 5))
		g_warning ("Can't handle news URLs yet.");
	else if (!strncasecmp (url, "mailto:", 7))
		send_to_url (url);
	else if (!strncasecmp (url, "cid:", 4))
		save_data (url, message);
	else
		gnome_url_show (url);
}

static void 
embeddable_destroy_cb (GtkObject *obj, gpointer user_data)
{
	BonoboWidget *be;      /* bonobo embeddable */
	BonoboViewFrame *vf;   /* the embeddable view frame */
	BonoboObjectClient* server;
	CORBA_Environment ev;

	be = BONOBO_WIDGET (obj);
	server = bonobo_widget_get_server (be);

	vf = bonobo_widget_get_view_frame (be);
	bonobo_control_frame_control_deactivate (
		BONOBO_CONTROL_FRAME (vf));
	/* w = bonobo_control_frame_get_widget (BONOBO_CONTROL_FRAME (vf)); */
	
	/* gtk_widget_destroy (w); */
	
	CORBA_exception_init (&ev);
	Bonobo_Unknown_unref (
		bonobo_object_corba_objref (BONOBO_OBJECT(server)), &ev);
	CORBA_Object_release (
		bonobo_object_corba_objref (BONOBO_OBJECT(server)), &ev);

	CORBA_exception_free (&ev);
	bonobo_object_unref (BONOBO_OBJECT (vf));
}

static gboolean
on_object_requested (GtkHTML *html, GtkHTMLEmbedded *eb, gpointer data)
{
	CamelMimeMessage *message;
	GHashTable *urls;
	CamelMedium *medium;
	CamelDataWrapper *wrapper;
	const char *goad_id;
	GtkWidget *embedded;
	BonoboObjectClient *server;
	Bonobo_PersistStream persist;	
	CORBA_Environment ev;
	GByteArray *ba;
	CamelStream *cstream;
	BonoboStream *bstream;

	if (strncmp (eb->classid, "cid:", 4) != 0)
		return FALSE;
	message = gtk_object_get_data (GTK_OBJECT (html), "message");
	urls = gtk_object_get_data (GTK_OBJECT (message), "urls");
	medium = g_hash_table_lookup (urls, eb->classid);
	g_return_val_if_fail (CAMEL_IS_MEDIUM (medium), FALSE);
	wrapper = camel_medium_get_content_object (medium);

	goad_id = gnome_mime_get_value (eb->type, "bonobo-goad-id");
	if (!goad_id) {
		char *main_type =
			g_strndup (eb->type, strcspn (eb->type, "/"));

		goad_id = gnome_mime_get_value (main_type, "bonobo-goad-id");
		g_free (main_type);
	}
	if (!goad_id)
		return FALSE;

	embedded = bonobo_widget_new_subdoc (goad_id, NULL);
	if (!embedded)
		return FALSE;
	gtk_signal_connect (GTK_OBJECT (embedded), "destroy",
			    embeddable_destroy_cb, NULL);
	server = bonobo_widget_get_server (BONOBO_WIDGET (embedded));

	persist = (Bonobo_PersistStream) bonobo_object_client_query_interface (
		server, "IDL:Bonobo/PersistStream:1.0", NULL);
	if (persist == CORBA_OBJECT_NIL) {
		bonobo_object_unref (BONOBO_OBJECT (embedded));
		return FALSE;
	}

	/* Write the data to a CamelStreamMem... */
	ba = g_byte_array_new ();
	cstream = camel_stream_mem_new_with_byte_array (ba);
	camel_data_wrapper_write_to_stream (wrapper, cstream);

	/* ...convert the CamelStreamMem to a BonoboStreamMem... */
	bstream = bonobo_stream_mem_create (ba->data, ba->len, TRUE, FALSE);
	gtk_object_unref (GTK_OBJECT (cstream));

	/* ...and hydrate the PersistStream from the BonoboStream. */
	CORBA_exception_init (&ev);
	Bonobo_PersistStream_load (persist,
				   bonobo_object_corba_objref (
					   BONOBO_OBJECT (bstream)),
				   eb->type, &ev);
	bonobo_object_unref (BONOBO_OBJECT (bstream));
	Bonobo_Unknown_unref (persist, &ev);
	CORBA_Object_release (persist, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		bonobo_object_unref (BONOBO_OBJECT (embedded));
		CORBA_exception_free (&ev);				
		return FALSE;
	}
	CORBA_exception_free (&ev);

	gtk_widget_show (embedded);
	gtk_container_add (GTK_CONTAINER (eb), embedded);

	return TRUE;
}

static void
on_url_requested (GtkHTML *html, const char *url, GtkHTMLStream *handle,
		  gpointer user_data)
{
	CamelMimeMessage *message;
	GHashTable *urls;

	message = gtk_object_get_data (GTK_OBJECT (html), "message");
	urls = gtk_object_get_data (GTK_OBJECT (message), "urls");

	user_data = g_hash_table_lookup (urls, url);
	if (user_data == NULL)
		return;

	if (strncmp (url, "cid:", 4) == 0) {
		CamelMedium *medium = user_data;
		CamelDataWrapper *data;
		CamelStream *stream_mem;
		GByteArray *ba;

		g_return_if_fail (CAMEL_IS_MEDIUM (medium));
		data = camel_medium_get_content_object (medium);

		ba = g_byte_array_new ();
		stream_mem = camel_stream_mem_new_with_byte_array (ba);
		camel_data_wrapper_write_to_stream (data, stream_mem);
		gtk_html_write (html, handle, ba->data, ba->len);
		gtk_object_unref (GTK_OBJECT (stream_mem));
	} else if (strncmp (url, "x-evolution-data:", 17) == 0) {
		GByteArray *ba = user_data;

		g_return_if_fail (ba != NULL);
		gtk_html_write (html, handle, ba->data, ba->len);
	}
}

void
mail_html_write (GtkHTML *html, GtkHTMLStream *stream,
		 const char *format, ...)
{
	char *buf;
	va_list ap;

	va_start (ap, format);
	buf = g_strdup_vprintf (format, ap);
	va_end (ap);
	gtk_html_write (html, stream, buf, strlen (buf));
	g_free (buf);
}


/**
 * mail_display_set_message:
 * @mail_display: the mail display object
 * @mime_message: the input camel medium
 *
 * Makes the mail_display object show the contents of the medium
 * param. This means feeding mail_display->body_stream and
 * mail_display->headers_stream with html.
 *
 **/
void 
mail_display_set_message (MailDisplay *mail_display, 
			  CamelMedium *medium)
{
	GtkHTMLStream *stream;
	GtkAdjustment *adj;

	/*
	 * for the moment, camel-formatter deals only with 
	 * mime messages, but in the future, it should be 
	 * able to deal with any medium.
	 * It can work on pretty much data wrapper, but in 
	 * fact, only the medium class has the distinction 
	 * header / body 
	 */
	if (!CAMEL_IS_MIME_MESSAGE (medium))
		return;

	/* Clean up from previous message. */
	if (mail_display->current_message)
		gtk_object_unref (GTK_OBJECT (mail_display->current_message));

	mail_display->current_message = CAMEL_MIME_MESSAGE (medium);
	gtk_object_ref (GTK_OBJECT (medium));

	gtk_object_set_data (GTK_OBJECT (mail_display->html), "message", medium);
	stream = gtk_html_begin (mail_display->html);
	mail_html_write (mail_display->html, stream, "%s%s", HTML_HEADER,
			 "<BODY TEXT=\"#000000\" BGCOLOR=\"#FFFFFF\">\n");
	mail_format_mime_message (CAMEL_MIME_MESSAGE (medium),
				  mail_display->html, stream,
				  CAMEL_MIME_MESSAGE (medium));

	mail_html_write (mail_display->html, stream, "</BODY></HTML>\n");
	gtk_html_end (mail_display->html, stream, GTK_HTML_STREAM_OK);

	adj = e_scroll_frame_get_vadjustment (mail_display->scroll);
	gtk_adjustment_set_value (adj, 0);
	e_scroll_frame_set_vadjustment (mail_display->scroll, adj);

	adj = e_scroll_frame_get_hadjustment (mail_display->scroll);
	gtk_adjustment_set_value (adj, 0);
	e_scroll_frame_set_hadjustment (mail_display->scroll, adj);
}


/*----------------------------------------------------------------------*
 *                     Standard Gtk+ Class functions
 *----------------------------------------------------------------------*/

static void
mail_display_init (GtkObject *object)
{
	MailDisplay *mail_display = MAIL_DISPLAY (object);

	/* various other initializations */
	mail_display->current_message = NULL;
}

static void
mail_display_destroy (GtkObject *object)
{
	/* MailDisplay *mail_display = MAIL_DISPLAY (object); */

	mail_display_parent_class->destroy (object);
}

static void
mail_display_class_init (GtkObjectClass *object_class)
{
	object_class->destroy = mail_display_destroy;
	mail_display_parent_class = gtk_type_class (PARENT_TYPE);
}

GtkWidget *
mail_display_new (FolderBrowser *parent_folder_browser)
{
	MailDisplay *mail_display = gtk_type_new (mail_display_get_type ());
	GtkWidget *scroll, *html;

	g_assert (parent_folder_browser);

	mail_display->parent_folder_browser = parent_folder_browser;

	gtk_box_set_homogeneous (GTK_BOX (mail_display), FALSE);
	gtk_widget_show (GTK_WIDGET (mail_display));

	scroll = e_scroll_frame_new (NULL, NULL);
	e_scroll_frame_set_policy (E_SCROLL_FRAME (scroll),
				   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	e_scroll_frame_set_shadow_type (E_SCROLL_FRAME (scroll), GTK_SHADOW_IN);
	gtk_box_pack_start_defaults (GTK_BOX (mail_display), GTK_WIDGET (scroll));
	gtk_widget_show (GTK_WIDGET (scroll));

	html = gtk_html_new ();
	gtk_html_set_editable (GTK_HTML (html), FALSE);
	gtk_signal_connect (GTK_OBJECT (html), "url_requested",
			    GTK_SIGNAL_FUNC (on_url_requested), NULL);
	gtk_signal_connect (GTK_OBJECT (html), "object_requested",
			    GTK_SIGNAL_FUNC (on_object_requested), NULL);
	gtk_signal_connect (GTK_OBJECT (html), "link_clicked",
			    GTK_SIGNAL_FUNC (on_link_clicked), NULL);
	gtk_container_add (GTK_CONTAINER (scroll), html);
	gtk_widget_show (GTK_WIDGET (html));

	mail_display->scroll = E_SCROLL_FRAME (scroll);
	mail_display->html = GTK_HTML (html);

	return GTK_WIDGET (mail_display);
}



E_MAKE_TYPE (mail_display, "MailDisplay", MailDisplay, mail_display_class_init, mail_display_init, PARENT_TYPE);
