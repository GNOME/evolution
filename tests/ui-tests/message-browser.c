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
	gtk_tree_append (GTK_TREE (tree_ctrl), tree_item);

	gtk_widget_show(tree_item);

	containee = 
		camel_medium_get_content_object (CAMEL_MEDIUM (object));

	if (containee) {

		containee_label = camel_data_wrapper_get_mime_type (containee);

		subtree = gtk_tree_new();

		containee_tree_item =
			gtk_tree_item_new_with_label (containee_label);

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
	      
			g_print ("found a multipart w/ %d parts\n",
				 max_multiparts);
	      
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
		
				g_print ("handling part %d\n", i);
				handle_tree_item (
					CAMEL_DATA_WRAPPER (body_part),
					GTK_WIDGET (subtree));
			}
			
		}
		gtk_tree_item_expand (
			GTK_TREE_ITEM (containee_tree_item));
		gtk_tree_item_expand (GTK_TREE_ITEM (tree_item));		
	}
}

static void
tree_selection_changed( GtkWidget *tree )
{
	GList *i;
  
	i = GTK_TREE_SELECTION(tree);
	while (i){
		gchar *name;
		GtkLabel *label;
		GtkWidget *item;

		/* Get a GtkWidget pointer from the list node */
		item = GTK_WIDGET (i->data);
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
		GTK_POLICY_ALWAYS);	
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
	
	//camel_data_wrapper_construct_from_stream (
	//	CAMEL_DATA_WRAPPER (message), input_stream);

	camel_data_wrapper_set_input_stream (
					    CAMEL_DATA_WRAPPER (message), input_stream);

	return message;
}

/*----------------------------------------------------------------------*
 *           Filling out the HTML view of a mime message
 *----------------------------------------------------------------------*/

static void
mime_message_to_html (CamelDataWrapper *msg, gchar** body_string)
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


static GtkWidget*
get_gtk_html_contents_window (CamelDataWrapper* data)
{
	static GtkWidget* scroll_wnd = NULL;
	static GtkWidget* html_widget = NULL;
	HTMLStream* html_stream;
	gchar *body_string;

	/* create the html widget and scroll window, if they haven't
           already been created */
	if (!html_widget) {
		html_widget = gtk_html_new();
		scroll_wnd = gtk_scrolled_window_new (NULL, NULL);
		gtk_container_add (GTK_CONTAINER (scroll_wnd), html_widget);
	}

	if (data) {
		
		html_stream =
			HTML_STREAM (html_stream_new (GTK_HTML (html_widget)));
		
		/* turn the mime message into html, and
		   write it to the html stream */
		mime_message_to_html (data, &body_string);
		
		camel_stream_write (CAMEL_STREAM (html_stream),
				    body_string,
				    strlen (body_string));
		
		camel_stream_close (CAMEL_STREAM (html_stream));
		
		g_free (body_string);
	}
	
	gtk_widget_set_usize (scroll_wnd, 500, 400);
        gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (scroll_wnd),
		GTK_POLICY_AUTOMATIC,
		GTK_POLICY_ALWAYS);
	return scroll_wnd;
}


static GtkWidget*
get_gtk_html_header_window (CamelMimeMessage* mime_message)
{
	static GtkWidget* hbox = NULL;
	gchar* subj_string = NULL;
	gchar* to_string = NULL;
	gchar* from_string = NULL;	

	if (!hbox) {
		hbox = gtk_hbox_new (FALSE, 0);
		gtk_box_pack_start (GTK_BOX (hbox),
				    gtk_button_new_with_label("hello"),
				    FALSE, TRUE, 5);
	}
	
	return hbox;
}

static GtkWidget*
get_gtk_html_window (CamelDataWrapper* data)
{
	static GtkWidget* vbox = NULL;
	CamelMimeMessage* msg = NULL;

	if (CAMEL_IS_MIME_MESSAGE (data))
		msg = CAMEL_MIME_MESSAGE (data);

	if (!vbox) vbox = gtk_vbox_new (FALSE, 0);
	if (msg)
		gtk_box_pack_start (
			GTK_BOX (vbox),
			get_gtk_html_header_window(msg),
			FALSE, TRUE, 5);

	gtk_box_pack_start (GTK_BOX (vbox),
			    get_gtk_html_contents_window(data),
			    FALSE, TRUE, 5);

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
			get_gtk_html_window (CAMEL_DATA_WRAPPER (message));
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
	
	CamelMimeMessage* message = NULL;

	/* initialization */
	gnome_init ("MessageBrowser", "1.0", argc, argv);
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
	g_message ("hi world");
	gtk_main();

	return 1;
}
