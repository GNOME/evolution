/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * mail-display.c: Mail display widget
 *
 * Author:
 *   Miguel de Icaza
 *
 * (C) 2000 Helix Code, Inc.
 */
#include <config.h>
#include <gnome.h>
#include "e-util/e-util.h"
#include "mail-display.h"
#include "html-stream.h"
#include "camel/camel-formatter.h"

/* corba/bonobo stuff */
#include <bonobo.h>
#include <libgnorba/gnorba.h>
#include <bonobo/bonobo-stream-memory.h>

#define PARENT_TYPE (gtk_table_get_type ())

static GtkObjectClass *mail_display_parent_class;

            




static gchar default_header_html_string[] = "
<!doctype html public \"-//w3c//dtd html 4.0 transitional//en\">
<html>
<head>
   <meta name=\"GENERATOR\" content=\"Evolution Mail Component (Rhon Rhon release)\">
</head>
<body text=\"#000000\" bgcolor=\"#999999\">
<table CELLSPACING=0 WIDTH=\"100\%\">
<tr>
<td><b>From:&nbsp;</b></td>
<td><b>To:&nbsp;</b></td>
</tr>

<tr>
<td><b>Subject:&nbsp;</b></td>
<td><b>Cc:&nbsp;</b></td>
</tr>
</table>
</body>
</html>
";



static gchar default_body_html_string[] = "
<!doctype html public \"-//w3c//dtd html 4.0 transitional//en\">
<html>
<head>
   <meta name=\"GENERATOR\" content=\"Evolution Mail Component (Rhon Rhon release)\">
</head>
<body text=\"#000000\" bgcolor=\"#FFFFFF\">
<center>
Nothing to display in this view
</center>
</body>
</html>
";



/* stuff to display Bonobo Components  inside the html message 
 * body view */
static gboolean
hydrate_persist_stream_from_gstring (Bonobo_PersistStream persist_stream,
				     GString* gstr)
{
	CORBA_Environment ev;
	BonoboStream* mem_stream =
		bonobo_stream_mem_create (gstr->str, gstr->len, TRUE);
        /*
	 * If the component doesn't support
	 * PersistStream, then we destroy the
	 * stream we created and bail.
	 */
	if (persist_stream == CORBA_OBJECT_NIL) {
		gnome_warning_dialog (_("The component now claims that it "
					"doesn't support PersistStream!"));
		bonobo_object_unref (BONOBO_OBJECT (mem_stream));
		return FALSE;
	}
				
	CORBA_exception_init (&ev);

	/*
	 * Load the file into the component using PersistStream.
	 */
	Bonobo_PersistStream_load (persist_stream,
				   (Bonobo_Stream) bonobo_object_corba_objref (BONOBO_OBJECT (mem_stream)),
				   &ev);

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

/*
 * As a page is loaded, when gtkhtml comes across <object> tags, this
 * callback is invoked. The GtkHTMLEmbedded param is a GtkContainer;
 * our job in this function is to simply add a child widget to it.
 */
static void
on_object_requested (GtkHTML *html, GtkHTMLEmbedded *eb, void *unused)
{
	CamelStream *stream;
	GString *camel_stream_gstr;

	GtkWidget *bonobo_embeddable;
	BonoboObjectClient* server;
	Bonobo_PersistStream persist;	
	CORBA_Environment ev;
	gchar *uid = gtk_html_embedded_get_parameter (eb, "uid");

	printf ("Here\n");
	/* Both the classid (which specifies which bonobo object to
         * fire up) and the uid (which tells us where to find data to
         * persist from) must be available; if one of them isn't,
         * print an error and bail. */
	if (!uid || !eb->classid) {
		printf ("on_object_requested: couldn't find %s%s%s\n",
			uid?"a uid":"",
			(!uid && !eb->classid)?" or ":"",
			eb->classid?"a classid":"");
		return;
	}
	printf ("object requested : %s\n", eb->classid);
     	printf ("UID = %s\n", uid);

	/* Try to get a server with goadid specified by eb->classid */
	bonobo_embeddable = bonobo_widget_new_subdoc  (eb->classid, NULL);
	server = bonobo_widget_get_server (BONOBO_WIDGET (bonobo_embeddable));
	if (!server) {
		printf ("Couldn't get the server for the bonobo embeddable\n");
		return;
	}

	/* The UID should be a pointer to a CamelStream */
	if (sscanf (uid, "camel://%p", &stream) != 1) {
		printf ("Couldn't get a pointer from url \"%s\"\n", uid);
		gtk_object_unref (GTK_OBJECT (bonobo_embeddable));
		
		return;
	}

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

	g_string_free (camel_stream_gstr, FALSE);
}



void 
mail_display_set_message (MailDisplay *mail_display, 
			  CamelMedium *medium)
{
	CamelFormatter *camel_formatter;

	/* okay, we should not create a formatter
	 * each time we need to display a message
	 * but I don't know how the formatter reacts
	 * to consecutive call to *_to_html - ber */
	camel_formatter = camel_formatter_new ();


	/*
	 * for the moment, camel-formatter deals only with 
	 * mime messages, but in the future, it should be 
	 * able to deal with any medium.
	 * It can work on pretty much data wrapper, but in 
	 * fact, only the medium class has the distinction 
	 * header / body 
	 */
	if (CAMEL_IS_MIME_MESSAGE (medium)) {
		
		/* 
		 * reset the html stream to clean 
		 * the gtkhtml widget 
		 */
		camel_stream_reset (mail_display->body_stream);
		camel_stream_reset (mail_display->headers_stream);
		
		/* 
		 * convert the message into html
		 * and stream the result to the gtkhtml
		 * widgets 
		 */
		camel_stream_write_string (mail_display->headers_stream, "
<!doctype html public \"-//w3c//dtd html 4.0 transitional//en\">
<html>
<head>
   <meta name=\"GENERATOR\" content=\"Evolution Mail Component (Rhon Rhon release)\">
</head>
<body text=\"#000000\" bgcolor=\"#999999\">
<font>
");;

		camel_stream_write_string (mail_display->body_stream, "
<!doctype html public \"-//w3c//dtd html 4.0 transitional//en\">
<html>
<head>
   <meta name=\"GENERATOR\" content=\"Evolution Mail Component (Rhon Rhon release)\">
</head>
<body text=\"#000000\" bgcolor=\"#FFFFFF\">
");;


		 camel_formatter_mime_message_to_html 
			(camel_formatter,
			 CAMEL_MIME_MESSAGE (medium),
			 mail_display->headers_stream,
			 mail_display->body_stream);
		
		
		 /*camel_formatter_wrapper_to_html (camel_formatter,
						 CAMEL_DATA_WRAPPER (medium),
						 mail_display->body_stream);*/

		camel_stream_write_string (mail_display->headers_stream, "
</font>
</body>
</html>
");;

		camel_stream_write_string (mail_display->body_stream, "
</body>
</html>
");;

		camel_stream_close (mail_display->body_stream);
		camel_stream_close (mail_display->headers_stream);
		
	}	
}


/* generic class stuff */


static void
mail_display_init (GtkObject *object)
{
	MailDisplay *mail_display = MAIL_DISPLAY (object);

	/* create the headers html widget */
	mail_display->headers_html_widget =  (GtkHTML *) gtk_html_new ();	
	mail_display->headers_stream = html_stream_new (mail_display->headers_html_widget);
	gtk_widget_show (GTK_WIDGET (mail_display->headers_html_widget));
	
	/* create the body html widget */
	mail_display->body_html_widget =  (GtkHTML *) gtk_html_new ();	
	gtk_signal_connect (GTK_OBJECT (mail_display->body_html_widget), 
			    "object_requested",
			    GTK_SIGNAL_FUNC (on_object_requested), 
			    NULL);

	mail_display->body_stream = html_stream_new (mail_display->body_html_widget);
	gtk_widget_show (GTK_WIDGET (mail_display->body_html_widget));
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
	GtkTable *table = GTK_TABLE (mail_display);
	GtkWidget *scroll_wnd;
	GtkWidget* frame_wnd = NULL;

	g_assert (parent_folder_browser);

	mail_display->parent_folder_browser = parent_folder_browser;

	/* the table has table with 1 column and 2 lines */
  	table->homogeneous = FALSE; 

  	gtk_table_resize (table, 1, 2); 
	gtk_widget_show (GTK_WIDGET (mail_display));


	/* create a scrolled window and put the headers 
	 * html widget inside */
	scroll_wnd = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll_wnd),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_NEVER);
	frame_wnd = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (
				   GTK_FRAME (frame_wnd), GTK_SHADOW_OUT);
	gtk_widget_show (scroll_wnd);
	gtk_container_add (GTK_CONTAINER (frame_wnd), scroll_wnd);
	gtk_widget_show (frame_wnd);
	gtk_container_add (GTK_CONTAINER (scroll_wnd), 
			   GTK_WIDGET (mail_display->headers_html_widget));
	gtk_widget_set_usize (GTK_WIDGET (scroll_wnd), -1, 50);
	/* add it on the top part of the table */
  	gtk_table_attach (table, GTK_WIDGET (frame_wnd), 
  			  0, 1, 0, 1,
  			  GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0); 


	/* create a scrolled window and put the body 
	 * html widget inside */
	scroll_wnd = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll_wnd),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_ALWAYS);

	gtk_widget_show (scroll_wnd);
	gtk_container_add (GTK_CONTAINER (scroll_wnd), 
			   GTK_WIDGET (mail_display->body_html_widget));
	
	/* add it at the bottom part of the table */
  	gtk_table_attach (table, GTK_WIDGET (scroll_wnd), 
  			  0, 1, 1, 2,
  			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0); 

	
	/* write the default text to the  html widgets */
	camel_stream_write_string (mail_display->headers_stream, default_header_html_string);
	camel_stream_write_string (mail_display->body_stream,    default_body_html_string);
	 

	return GTK_WIDGET (mail_display);
}



E_MAKE_TYPE (mail_display, "MailDisplay", MailDisplay, mail_display_class_init, mail_display_init, PARENT_TYPE);


