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
#include <gnome.h>
#include "e-util/e-util.h"
#include "mail-display.h"
#include "mail-format.h"

/* corba/bonobo stuff */
#include <bonobo.h>
#include <libgnorba/gnorba.h>
#include <bonobo/bonobo-stream-memory.h>

#define PARENT_TYPE (gtk_vbox_get_type ())

static GtkObjectClass *mail_display_parent_class;


/*----------------------------------------------------------------------*
 *                     Helper utility functions
 *----------------------------------------------------------------------*/


/* stuff to display Bonobo Components  inside the html message 
 * body view */
static gboolean
hydrate_persist_stream_from_gstring (Bonobo_PersistStream persist_stream,
				     GString* gstr)
{
	CORBA_Environment ev;
	BonoboStream* mem_stream =
		bonobo_stream_mem_create (gstr->str, gstr->len, TRUE, FALSE);
	CORBA_Object mem_stream_corba =
		bonobo_object_corba_objref (BONOBO_OBJECT (mem_stream));
	
	g_assert (persist_stream != CORBA_OBJECT_NIL);
				
	CORBA_exception_init (&ev);

	/*
	 * Load the file into the component using PersistStream.
	 */
	Bonobo_PersistStream_load (persist_stream, mem_stream_corba, &ev);

	bonobo_object_unref (BONOBO_OBJECT (mem_stream));
				
	if (ev._major != CORBA_NO_EXCEPTION) {
		gnome_warning_dialog (_("An exception occured while trying "
					"to load data into the component with "
					"PersistStream"));
		CORBA_exception_free (&ev);
		return FALSE;
	}

	CORBA_exception_free (&ev);
	return TRUE;
}


static GString*
camel_stream_to_gstring (CamelStream* stream)
{
	gchar tmp_buffer[4097];
	GString *tmp_gstring = g_string_new ("");

	do { /* read next chunk of text */
			
		gint nb_bytes_read;
			
		nb_bytes_read = camel_stream_read (stream,
						   tmp_buffer,
						   4096);
		tmp_buffer [nb_bytes_read] = '\0';

                /* If there's any text, append it to the gstring */
		if (nb_bytes_read > 0) {
			tmp_gstring = g_string_append (tmp_gstring, tmp_buffer);
		}
			
	} while (!camel_stream_eos (stream));

	return tmp_gstring;
}

/*----------------------------------------------------------------------*
 *                        Callbacks
 *----------------------------------------------------------------------*/

static void 
embeddable_destroy_cb (GtkObject *obj, gpointer user_data)
{
	BonoboWidget *be;      /* bonobo embeddable */
	BonoboViewFrame *vf;   /* the embeddable view frame */
	BonoboObjectClient* server;
	CORBA_Environment ev;

	printf ("in the bonobo embeddable destroy callback\n");
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
	bonobo_object_destroy (BONOBO_OBJECT (vf));
	/* gtk_object_unref (obj); */
}

static CamelStream *
cid_stream (const char *cid, CamelMimeMessage *message)
{
	CamelDataWrapper *data;

	data = gtk_object_get_data (GTK_OBJECT (message), cid);
	g_return_val_if_fail (CAMEL_IS_DATA_WRAPPER (data), NULL);

	return camel_data_wrapper_get_output_stream (data);
}	

/*
 * As a page is loaded, when gtkhtml comes across <object> tags, this
 * callback is invoked. The GtkHTMLEmbedded param is a GtkContainer;
 * our job in this function is to simply add a child widget to it.
 */
static void
on_object_requested (GtkHTML *html, GtkHTMLEmbedded *eb, gpointer data)
{
	CamelStream *stream;
	GString *camel_stream_gstr;
	CamelMimeMessage *message = data;
	GtkWidget *bonobo_embeddable;
	BonoboObjectClient* server;
	Bonobo_PersistStream persist;	
	CORBA_Environment ev;
	gchar *uid = gtk_html_embedded_get_parameter (eb, "uid");

	/* Both the classid (which specifies which bonobo object to
         * fire up) and the uid (which tells us where to find data to
         * persist from) must be available; if one of them isn't,
         * print an error and bail.
	 */
	if (!uid || !eb->classid) {
		printf ("on_object_requested: couldn't find %s%s%s\n",
			uid ? "a uid" : "",
			(!uid && !eb->classid) ? " or " : "",
			eb->classid ? "a classid" : "");
		return;
	}
	printf ("object requested : %s\n", eb->classid);
     	printf ("UID = %s\n", uid);

	/* Try to get a server with goadid specified by eb->classid */
	bonobo_embeddable = bonobo_widget_new_subdoc  (eb->classid, NULL);
	gtk_signal_connect (GTK_OBJECT (bonobo_embeddable), 
			    "destroy", embeddable_destroy_cb, NULL);
	
	server = bonobo_widget_get_server (BONOBO_WIDGET (bonobo_embeddable));
	if (!server) {
		printf ("Couldn't get the server for the bonobo embeddable\n");
		return;
	}

	if (!strncmp (uid, "cid:", 4)) {
		stream = cid_stream (uid + 4, message);
		g_return_if_fail (CAMEL_IS_STREAM (stream));
	} else
		return;

	/* Try to get a PersistStream interface from the server;
	   if it doesn't support that interface, bail. */
	persist = (Bonobo_PersistStream) bonobo_object_client_query_interface (
		server,
		"IDL:Bonobo/PersistStream:1.0",
		NULL);

	if (persist == CORBA_OBJECT_NIL) {
		gchar* msg = g_strdup_printf (
			_("The %s component doesn't support PersistStream!\n"),
			uid);
		
		gnome_warning_dialog (msg);
		gtk_object_unref (GTK_OBJECT (bonobo_embeddable));

		g_free (msg);
		return;
	}

	/* Hydrate the PersistStream from the CamelStream */
	camel_stream_gstr = camel_stream_to_gstring (stream);	
	printf ("on_object_requested: The CamelStream has %d elements\n",
		camel_stream_gstr->len);
	hydrate_persist_stream_from_gstring (persist, camel_stream_gstr);
	
	/* Give our new window to the container */
	
	gtk_widget_show (bonobo_embeddable);
	gtk_container_add (GTK_CONTAINER(eb), bonobo_embeddable);
	
	/* Destroy the PersistStream object.*/
	CORBA_exception_init (&ev);
	Bonobo_Unknown_unref (persist, &ev);
	CORBA_Object_release (persist, &ev);
	CORBA_exception_free (&ev);				

	g_string_free (camel_stream_gstr, TRUE);
}

static void
on_url_requested (GtkHTML *html, const char *url, GtkHTMLStreamHandle handle,
		  gpointer user_data)
{
	char buf[1024];
	int nread;
	CamelStream *output;
	CamelMimeMessage *message = CAMEL_MIME_MESSAGE (user_data);

	if (strncmp (url, "camel:", 6) == 0) {
		output = GUINT_TO_POINTER (strtoul (url + 6, NULL, 0));
		g_return_if_fail (CAMEL_IS_STREAM (output));
	} else if (strncmp (url, "cid:", 4) == 0) {
		output = cid_stream (url + 4, message);
		g_return_if_fail (CAMEL_IS_STREAM (output));
	} else
		return;

	camel_stream_reset (output);
	do {
		nread = camel_stream_read (output, buf, sizeof (buf));
		if (nread > 0)
			gtk_html_write (html, handle, buf, nread);
	} while (!camel_stream_eos (output));
}

/* HTML part code */
static void
html_size_req (GtkWidget *widget, GtkRequisition *requisition)
{
	requisition->height = GTK_LAYOUT (widget)->height;
	requisition->width = GTK_LAYOUT (widget)->width;
}

void
mail_html_new (GtkHTML **html, GtkHTMLStreamHandle **stream,
	       CamelMimeMessage *root, gboolean init)
{
	*html = GTK_HTML (gtk_html_new ());
	gtk_html_set_editable (*html, FALSE);
	gtk_signal_connect (GTK_OBJECT (*html), "size_request",
			    GTK_SIGNAL_FUNC (html_size_req), NULL);
	gtk_signal_connect (GTK_OBJECT (*html), "object_requested",
			    GTK_SIGNAL_FUNC (on_object_requested), root);
	gtk_signal_connect (GTK_OBJECT (*html), "url_requested",
			    GTK_SIGNAL_FUNC (on_url_requested), root);

	*stream = gtk_html_begin (*html, "");
	if (init) {
		mail_html_write (*html, *stream, HTML_HEADER
				 "<BODY TEXT=\"#000000\" "
				 "BGCOLOR=\"#FFFFFF\">\n");
	}
}

void
mail_html_write (GtkHTML *html, GtkHTMLStreamHandle *stream,
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

void
mail_html_end (GtkHTML *html, GtkHTMLStreamHandle *stream,
	       gboolean finish, GtkBox *box)
{
	GtkWidget *scroll;

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_NEVER,
					GTK_POLICY_NEVER);
	gtk_container_add (GTK_CONTAINER (scroll), GTK_WIDGET (html));

	if (finish)
		mail_html_write (html, stream, "</BODY></HTML>\n");
	gtk_html_end (html, stream, GTK_HTML_STREAM_OK);

	gtk_box_pack_start (box, scroll, FALSE, TRUE, 0);
	gtk_widget_show (GTK_WIDGET (html));
	gtk_widget_show (scroll);
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
	if (mail_display->current_message) {
		GtkContainer *container =
			GTK_CONTAINER (mail_display->inner_box);
		GList *htmls;

		htmls = gtk_container_children (container);
		while (htmls) {
			gtk_container_remove (container, htmls->data);
			htmls = htmls->next;
		}

		gtk_object_unref (GTK_OBJECT (mail_display->current_message));
	}

	mail_display->current_message = CAMEL_MIME_MESSAGE (medium);
	gtk_object_ref (GTK_OBJECT (medium));

	mail_format_mime_message (CAMEL_MIME_MESSAGE (medium),
				  mail_display->inner_box);

	adj = gtk_scrolled_window_get_vadjustment (mail_display->scroll);
	gtk_adjustment_set_value (adj, 0);
	gtk_scrolled_window_set_vadjustment (mail_display->scroll, adj);

	adj = gtk_scrolled_window_get_hadjustment (mail_display->scroll);
	gtk_adjustment_set_value (adj, 0);
	gtk_scrolled_window_set_hadjustment (mail_display->scroll, adj);
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
	GtkWidget *scroll, *vbox;

	g_assert (parent_folder_browser);

	mail_display->parent_folder_browser = parent_folder_browser;

	gtk_box_set_homogeneous (GTK_BOX (mail_display), FALSE);
	gtk_widget_show (GTK_WIDGET (mail_display));

	/* For now, the box only contains a single scrolled window,
	 * which in turn contains a vbox itself.
	 */
	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_ALWAYS);
	gtk_box_pack_start_defaults (GTK_BOX (mail_display),
				     GTK_WIDGET (scroll));
	gtk_widget_show (GTK_WIDGET (scroll));

	vbox = gtk_vbox_new (FALSE, 2);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scroll),
					       vbox);
	gtk_widget_show (GTK_WIDGET (vbox));

	mail_display->scroll = GTK_SCROLLED_WINDOW (scroll);
	mail_display->inner_box = GTK_BOX (vbox);

	return GTK_WIDGET (mail_display);
}



E_MAKE_TYPE (mail_display, "MailDisplay", MailDisplay, mail_display_class_init, mail_display_init, PARENT_TYPE);
