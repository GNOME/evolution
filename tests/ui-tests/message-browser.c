/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*--------------------------------*-C-*---------------------------------*
 *
 *  Copyright 2000, Matt Loper <matt@helixcode.com>.
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
 *----------------------------------------------------------------------*/

#include <gnome.h>
#include <camel/camel.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-stream.h>
#include <camel/camel-stream-fs.h>
#include <camel/gmime-utils.h>
#include "../../mail/html-stream.h"
#include <camel/camel-formatter.h>

/* gtkhtml stuff */
#include <gtkhtml/gtkhtml.h>

/* corba/bonobo stuff */
#include <bonobo.h>
#include <libgnorba/gnorba.h>
#include <bonobo/bonobo-stream-memory.h>

static void
print_usage_and_quit()
{
	g_print ("Usage: message-browser [FILENAME]\n");
	g_print ("Where FILENAME is the filename of a mime message "); 
	g_print ("in \"message/rfc822\" format.\n");
	exit (0);
}


/*----------------------------------------------------------------------*
 *      Filling out the tree control view of a mime-message
 *----------------------------------------------------------------------*/

static void
handle_tree_item (CamelDataWrapper* object, GtkWidget* tree_ctrl)
{
	GtkWidget* tree_item;
	gchar* label = gmime_content_field_get_mime_type (object->mime_type);

	CamelDataWrapper* containee;
	GtkWidget* containee_tree_item;
	gchar* containee_label;
	      
	GtkWidget* subtree = NULL;

	tree_item = gtk_tree_item_new_with_label (label);
	gtk_object_set_data (GTK_OBJECT (tree_item),
			     "camel_data_wrapper", object);
	gtk_tree_append (GTK_TREE (tree_ctrl), tree_item);

	gtk_widget_show(tree_item);

	/* If our object is a leaf, we're done */
	if (!CAMEL_IS_MEDIUM (object))
		return;

	containee = 
		camel_medium_get_content_object (
			CAMEL_MEDIUM (object));
	
	g_assert (containee);

	/* If it is a container, insert its contents into the tree */
	containee_label = camel_data_wrapper_get_mime_type (containee);

	subtree = gtk_tree_new();

	containee_tree_item =
		gtk_tree_item_new_with_label (containee_label);
	gtk_object_set_data (GTK_OBJECT (containee_tree_item),
			     "camel_data_wrapper",
			     containee);		
	gtk_tree_append (GTK_TREE (subtree), containee_tree_item);

	gtk_tree_item_set_subtree (GTK_TREE_ITEM(tree_item),
				   GTK_WIDGET (subtree));
	gtk_widget_show(containee_tree_item);

	if (CAMEL_IS_MULTIPART (containee))
	{
		CamelMultipart* multipart =
			CAMEL_MULTIPART (containee);
		int max_multiparts =
			camel_multipart_get_number (multipart);
		int i;
	      
		if (max_multiparts > 0) {
			subtree = gtk_tree_new();
			gtk_tree_item_set_subtree (
				GTK_TREE_ITEM(containee_tree_item),
				GTK_WIDGET (subtree));
		}
	      
		for (i = 0; i < max_multiparts; i++) {
			CamelMimeBodyPart* body_part =
				camel_multipart_get_part (
					multipart, i);
		
			handle_tree_item (
				CAMEL_DATA_WRAPPER (body_part),
				GTK_WIDGET (subtree));
		}
			
	}
	gtk_tree_item_expand (
		GTK_TREE_ITEM (containee_tree_item));
	gtk_tree_item_expand (GTK_TREE_ITEM (tree_item));		
}

static GtkWidget*
get_gtk_html_contents_window (CamelDataWrapper* data);

static void
tree_selection_changed( GtkWidget *tree )
{
	GList *i;
  
	i = GTK_TREE_SELECTION(tree);
	while (i){
		gchar*     name;
		GtkLabel*  label;
		GtkWidget* item;
		CamelDataWrapper* camel_data_wrapper;
		
		/* Get a GtkWidget pointer from the list node */
		item = GTK_WIDGET (i->data);
		camel_data_wrapper =
			gtk_object_get_data (GTK_OBJECT (item),
					     "camel_data_wrapper");

		g_assert (camel_data_wrapper);
		get_gtk_html_contents_window (camel_data_wrapper);
		
		label = GTK_LABEL (GTK_BIN (item)->child);
		gtk_label_get (label, &name);
		g_print ("\t%s on level %d\n", name, GTK_TREE
			 (item->parent)->level);
		i = i->next;
	}
}

static GtkWidget*
get_message_tree_ctrl (CamelMimeMessage* message)
{
	static GtkWidget* scroll_wnd = NULL;
	static GtkWidget* tree_ctrl = NULL;

	/* create the tree control, if it doesn't exist already */
	if (!tree_ctrl) {

		tree_ctrl = gtk_tree_new ();

		gtk_signal_connect (GTK_OBJECT(tree_ctrl),
				    "selection_changed",
				    GTK_SIGNAL_FUNC(tree_selection_changed),
				    tree_ctrl);
		
		scroll_wnd = gtk_scrolled_window_new (NULL,NULL);
		
		gtk_scrolled_window_add_with_viewport (
			GTK_SCROLLED_WINDOW(scroll_wnd),
			tree_ctrl);

		gtk_widget_set_usize (scroll_wnd, 225, 200);
	}
	else
		gtk_tree_clear_items (GTK_TREE (tree_ctrl), 0, 1);
	

        /* Recursively insert tree items in the tree */
	if (message)
		handle_tree_item (CAMEL_DATA_WRAPPER (message), tree_ctrl);
        gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (scroll_wnd),
		GTK_POLICY_AUTOMATIC,
		GTK_POLICY_AUTOMATIC);	
	return scroll_wnd;
}

static CamelMimeMessage*
filename_to_camel_msg (gchar* filename)
{
	CamelMimeMessage* message;
	CamelStream* input_stream;

	camel_init();
	
	input_stream = camel_stream_fs_new_with_name (
		filename, CAMEL_STREAM_FS_READ);

	if (!input_stream) 
		return NULL;

	message = camel_mime_message_new_with_session (
		(CamelSession *)NULL);
	
	camel_data_wrapper_set_input_stream (
		CAMEL_DATA_WRAPPER (message), input_stream);

	return message;
}

/*----------------------------------------------------------------------*
 *           Filling out the HTML view of a mime message
 *----------------------------------------------------------------------*/

static void
data_wrapper_to_html (CamelDataWrapper *msg, gchar** body_string)
{
	CamelFormatter* cmf = camel_formatter_new();
	CamelStream* body_stream =
		camel_stream_mem_new (CAMEL_STREAM_FS_WRITE);

	g_assert (body_string);

	camel_formatter_wrapper_to_html (
		cmf, msg, body_stream);

	*body_string = g_strndup (
		CAMEL_STREAM_MEM (body_stream)->buffer->data,
		CAMEL_STREAM_MEM (body_stream)->buffer->len);
}

static void
mime_message_header_to_html (CamelMimeMessage *msg, gchar** header_string)
{
	CamelFormatter* cmf = camel_formatter_new();
	CamelStream* header_stream =
		camel_stream_mem_new (CAMEL_STREAM_FS_WRITE);

	g_assert (header_string);

	camel_formatter_mime_message_to_html (
		cmf, CAMEL_MIME_MESSAGE (msg), header_stream, NULL);

	*header_string = g_strndup (
		CAMEL_STREAM_MEM (header_stream)->buffer->data,
		CAMEL_STREAM_MEM (header_stream)->buffer->len);
}


static void
on_link_clicked (GtkHTML *html, const gchar *url, gpointer data)
{
	GtkWidget* message_box;
	CamelStream *stream;
	CamelStream *save_stream;
	gchar tmp_buffer[4096];
	gint nb_bytes_read;
	
	gchar* message =
		g_strdup_printf ("You have clicked on this link:\n%s", url);

	if (sscanf (url, "camel://%p", &stream))
	{
		save_stream =
			camel_stream_fs_new_with_name ("saved-file",
						       CAMEL_STREAM_FS_WRITE);
	

		do {
	  
			/* read next chunk of text */
			nb_bytes_read = camel_stream_read (stream,
							   tmp_buffer,
							   4096);
			
			/* If there's any text, write it to the stream */
			if (nb_bytes_read > 0) {
				camel_stream_write (save_stream,
						    tmp_buffer,
						    nb_bytes_read);
			}
			
			
		} while (!camel_stream_eos (stream));
		
		camel_stream_close (save_stream);
	}
	    
	message_box = gnome_message_box_new (message,
					     GNOME_MESSAGE_BOX_INFO,
					     "Okay", NULL);
	
	gnome_dialog_set_default (GNOME_DIALOG (message_box), 1);
	gnome_dialog_run (GNOME_DIALOG (message_box));
	g_free (message);
}

/*
 * As a page is being loaded, gtkhtml will come across a few types of
 * tags that it understands (like <img src="foo">). In these cases, it
 * will simply ask us to stream the data to it.
 */
static void
on_url_data_requested (GtkHTML *html, const gchar *url, GtkHTMLStreamHandle handle, gpointer data)
{
	CamelStream *stream;
	
	printf ("url _%s_ (%p) requested\n", url, url);

	if (sscanf (url, "camel://%p", &stream) == 1)
	{		
		gchar tmp_buffer[4096];
		do {
			gint nb_bytes_read;

			/* read next chunk of text */
			nb_bytes_read = camel_stream_read (stream,
							   tmp_buffer,
							   4096);
			
			/* If there's any text, write it to the stream */
			if (nb_bytes_read > 0) {
				gtk_html_write (html, handle, tmp_buffer, nb_bytes_read);
			}
			
			
		} while (!camel_stream_eos (stream));
		
		
	}

}


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




static GtkWidget*
get_gtk_html_contents_window (CamelDataWrapper* data)
{
	static GtkWidget* frame_wnd = NULL;
	static GtkWidget* scroll_wnd = NULL;
	static GtkWidget* html_widget = NULL;
	HTMLStream* html_stream;
	gchar *body_string;

	/* create the html widget and scroll window, if they haven't
           already been created */
	if (!html_widget) {
		html_widget = gtk_html_new();

		gtk_signal_connect (GTK_OBJECT (html_widget),
				    "link_clicked",
				    GTK_SIGNAL_FUNC (on_link_clicked),
				    NULL);		

		gtk_signal_connect (GTK_OBJECT (html_widget),
				    "url_requested",
				    GTK_SIGNAL_FUNC (on_url_data_requested),
				    NULL);		
		gtk_signal_connect (GTK_OBJECT (html_widget), 
				    "object_requested",
				    GTK_SIGNAL_FUNC (on_object_requested), 
				    NULL);

		scroll_wnd = gtk_scrolled_window_new (NULL, NULL);
		gtk_container_add (GTK_CONTAINER (scroll_wnd), html_widget);
	}

	if (data) {
		
		html_stream =
			HTML_STREAM (html_stream_new (GTK_HTML (html_widget)));

		/* turn the mime message into html, and
		   write it to the html stream */
		data_wrapper_to_html (data, &body_string);
		
		camel_stream_write (CAMEL_STREAM (html_stream),
				    body_string,
				    strlen (body_string));
		
		camel_stream_close (CAMEL_STREAM (html_stream));
		
		g_free (body_string);
	}
	

	if (!frame_wnd) {
		
		frame_wnd = gtk_frame_new (NULL);
		gtk_frame_set_shadow_type (
			GTK_FRAME (frame_wnd), GTK_SHADOW_IN);
		
		gtk_widget_set_usize (scroll_wnd, 500, 400);
		gtk_scrolled_window_set_policy (
			GTK_SCROLLED_WINDOW (scroll_wnd),
			GTK_POLICY_AUTOMATIC,
			GTK_POLICY_AUTOMATIC);
		
		gtk_container_add (GTK_CONTAINER (frame_wnd), scroll_wnd);
	}
	
	return frame_wnd;
}


static GtkWidget*
get_gtk_html_header_window (CamelMimeMessage* mime_message)
{
	static GtkWidget* frame_wnd = NULL;
	static GtkWidget* scroll_wnd = NULL;	
	static GtkWidget* html_widget = NULL;
	HTMLStream*       html_stream;
	gchar*            header_string;

        /* create the html widget and scroll window, if they haven't
           already been created */
	if (!html_widget) {
		html_widget = gtk_html_new();
		scroll_wnd = gtk_scrolled_window_new (NULL, NULL);
		gtk_container_add (GTK_CONTAINER (scroll_wnd), html_widget);
	}

	if (mime_message) {
		
		html_stream =
			HTML_STREAM (html_stream_new (GTK_HTML (html_widget)));
		
		/* turn the mime message into html, and
		   write it to the html stream */
		mime_message_header_to_html (mime_message, &header_string);
		
		camel_stream_write (CAMEL_STREAM (html_stream),
				    header_string,
				    strlen (header_string));
		
		camel_stream_close (CAMEL_STREAM (html_stream));
		
		g_free (header_string);
	}
	
	if (!frame_wnd) {
		
		frame_wnd = gtk_frame_new (NULL);
		gtk_frame_set_shadow_type (
			GTK_FRAME (frame_wnd), GTK_SHADOW_OUT);
		
		gtk_widget_set_usize (scroll_wnd, 500, 75);
		gtk_scrolled_window_set_policy (
			GTK_SCROLLED_WINDOW (scroll_wnd),
			GTK_POLICY_AUTOMATIC,
			GTK_POLICY_AUTOMATIC);
		
		gtk_container_add (GTK_CONTAINER (frame_wnd), scroll_wnd);
	}
	
	
	return frame_wnd;
}

static GtkWidget*
get_gtk_html_window (CamelMimeMessage* mime_message)
{
	static GtkWidget* vbox = NULL;
	GtkWidget* html_header_window = NULL;
	GtkWidget* html_content_window = NULL;	

	html_content_window =
		get_gtk_html_contents_window (
			CAMEL_DATA_WRAPPER (mime_message));
	
	html_header_window =
		get_gtk_html_header_window (mime_message);	
	
	if (!vbox) {
		vbox = gtk_vbox_new (FALSE, 0);
		
		gtk_box_pack_start (
			GTK_BOX (vbox),
			html_header_window,
			TRUE, TRUE, 5);
		
		gtk_box_pack_start (
			GTK_BOX (vbox),
			html_content_window,
			TRUE, FALSE, 5);
	}

	return vbox;
}
		


/*----------------------------------------------------------------------*
 *                  Menu callbacks and information
 *----------------------------------------------------------------------*/

static gchar* fileselection_prev_file = NULL;

static void
open_ok (GtkWidget *widget, GtkFileSelection *fs)
{
	int ret;
	GtkWidget *error_dialog;

	if (fileselection_prev_file)
		g_free (fileselection_prev_file);
	
	if(!g_file_exists (gtk_file_selection_get_filename (fs))) {
		error_dialog = gnome_message_box_new (
			_("File not found"),
			GNOME_MESSAGE_BOX_ERROR,
			GNOME_STOCK_BUTTON_OK,
			NULL);

		gnome_dialog_set_parent (GNOME_DIALOG (error_dialog),
					 GTK_WINDOW (fs));
		
		ret = gnome_dialog_run (GNOME_DIALOG (error_dialog));
	}
	else {
		gchar *filename = gtk_file_selection_get_filename (fs);
		CamelMimeMessage* message = filename_to_camel_msg (filename);

		if (message) {
			fileselection_prev_file = g_strdup (filename);
			get_message_tree_ctrl (message);
			get_gtk_html_window (message);
		}
		else
			gnome_message_box_new ("Couldn't load message.",
					       GNOME_MESSAGE_BOX_WARNING);

		gtk_widget_destroy (GTK_WIDGET (fs));
	}
}


static void
file_menu_open_cb (GtkWidget *widget, void* data)
{
	GtkFileSelection *fs;

	fs = GTK_FILE_SELECTION (
		gtk_file_selection_new (_("Open Mime Message")));

	if (fileselection_prev_file)
		gtk_file_selection_set_filename (fs, fileselection_prev_file);
	
	gtk_signal_connect (GTK_OBJECT (fs->ok_button), "clicked",
			    (GtkSignalFunc) open_ok,
			    fs);

	gtk_signal_connect_object (GTK_OBJECT (fs->cancel_button), "clicked",
				   (GtkSignalFunc) gtk_widget_destroy,
				   GTK_OBJECT (fs));

	gtk_widget_show (GTK_WIDGET (fs));
	gtk_grab_add (GTK_WIDGET (fs)); /* Make it modal */
}

static void 
file_menu_exit_cb (GtkWidget *widget, void *data)
{
	gtk_main_quit ();
}


static GnomeUIInfo file_menu [] = {
	GNOMEUIINFO_MENU_OPEN_ITEM (file_menu_open_cb, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_EXIT_ITEM (file_menu_exit_cb, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo main_menu[] = {
	GNOMEUIINFO_MENU_FILE_TREE (file_menu),
	GNOMEUIINFO_END
};


/*----------------------------------------------------------------------*
 *                               Main()
 *----------------------------------------------------------------------*/

int
main (int argc, char *argv[])
{
	/* app contains hbox, hbox contains other 2 windows */
	GtkWidget* app;
	GtkWidget* hpane;
	GtkWidget* tree_ctrl_window;
	GtkWidget* html_window;
	CORBA_Environment ev;

	CamelMimeMessage* message = NULL;

	/* initialization */

	/* Corba and Bonobo stuff */
	CORBA_exception_init (&ev);

	gnome_CORBA_init ("Message Browser", "1.0", &argc, argv, 0, &ev);

	if (bonobo_init (gnome_CORBA_ORB (), NULL, NULL) == FALSE)
		g_error ("Cannot bonobo_init");

	
	
	gdk_rgb_init ();

	gtk_widget_set_default_colormap (gdk_rgb_get_cmap ());
	gtk_widget_set_default_visual (gdk_rgb_get_visual ());

	app = gnome_app_new ("Message Browser Test", NULL);
	gnome_app_create_menus (GNOME_APP (app), main_menu);

	/* parse command line */
	if (argc > 2 ||
	    (argc==2 && strstr (argv[1], "--help") != 0))
		print_usage_and_quit();
	if (argc == 2) {
		if (strstr (argv[1], "--help") != 0)
			print_usage_and_quit();
		message = filename_to_camel_msg (argv[1]);
		if (!message) {
			g_print ("Couldn't load message.");
		}
	}

        hpane = gtk_hpaned_new();

	/* add the tree control view of the message*/
	tree_ctrl_window = get_message_tree_ctrl (message);
        gtk_paned_add1 (GTK_PANED (hpane), tree_ctrl_window);	

	/* add the HTML view of the message */
	html_window = get_gtk_html_window (message);
        gtk_paned_add2 (GTK_PANED (hpane), html_window);		
	
	/* rock n roll */
	gnome_app_set_contents (GNOME_APP (app),
				hpane);
	gtk_widget_show_all (app);
	gtk_signal_connect (GTK_OBJECT (app), "destroy",
			    GTK_SIGNAL_FUNC(gtk_main_quit),
			    &app);
	if (!message)
		file_menu_open_cb (NULL, NULL);

	bonobo_main();

	return 1;
}
