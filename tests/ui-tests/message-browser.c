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
 *      Filling out the tree control with a mime-message structure
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
					camel_multipart_get_part (multipart, i);
		
				g_print ("handling part %d\n", i);
				handle_tree_item (CAMEL_DATA_WRAPPER (body_part),
						  GTK_WIDGET (subtree));
			}	
		}
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
		scroll_wnd = gtk_scrolled_window_new (NULL,NULL);
		
		gtk_scrolled_window_add_with_viewport (
			GTK_SCROLLED_WINDOW(scroll_wnd),
			tree_ctrl);

		gtk_widget_set_usize (scroll_wnd, 150, 200);
	}
	else
		gtk_tree_clear_items (GTK_TREE (tree_ctrl), 0, 1);
	

        /* Recursively insert tree items in the tree */
	handle_tree_item (CAMEL_DATA_WRAPPER (message), tree_ctrl);
	
	return scroll_wnd;
}

static CamelMimeMessage*
filename_to_camel_msg (gchar* filename)
{
	CamelMimeMessage* message;
	CamelStream* input_stream;

	camel_init();
	
	message = camel_mime_message_new_with_session (
		(CamelSession *)NULL);
	input_stream = camel_stream_fs_new_with_name (
		filename, CAMEL_STREAM_FS_READ);
	g_assert (input_stream);
	
	camel_data_wrapper_construct_from_stream (
		CAMEL_DATA_WRAPPER (message), input_stream);

	camel_stream_close (input_stream); 

	return message;
}

/*----------------------------------------------------------------------*
 *                  Menu callbacks and information
 *----------------------------------------------------------------------*/

static void
open_ok (GtkWidget *widget, GtkFileSelection *fs)
{
	int ret;
	GtkWidget *error_dialog;
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
		if (message)
			get_message_tree_ctrl (message);
		
		gtk_widget_destroy (GTK_WIDGET (fs));
	}
}


static void
file_menu_open_cb (GtkWidget *widget, void* data)
{
	GtkFileSelection *fs;
	
	fs = GTK_FILE_SELECTION (
		gtk_file_selection_new (_("Open Mime Message")));
	
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
 *           Filling out the HTML view of a mime message
 *----------------------------------------------------------------------*/

static GtkWidget*
get_gtk_html_window (gchar* filename)
{
	static GtkWidget* scroll_wnd = NULL;
	static GtkWidget* html_widget = NULL;
	HTMLStream* html_stream;

	g_assert (filename);

	if (!html_widget) {
		html_widget = gtk_html_new();
		scroll_wnd = gtk_scrolled_window_new (NULL, NULL);
		gtk_container_add (GTK_CONTAINER (scroll_wnd), html_widget);
	}

	html_stream = html_stream_new (GTK_HTML (html_widget));

	camel_stream_write (CAMEL_STREAM (html_stream),
			    "<html><body>hello</body></html>",
			    sizeof("<html><body>hello</body></html>"));

	return scroll_wnd;
}


int
main (int argc, char *argv[])
{
	/* app contains vbox, vbox contains other 2 windows */
	GtkWidget* app;
	GtkWidget* vbox;
	GtkWidget* tree_ctrl_window;
	GtkWidget* html_window;
	
	CamelMimeMessage* message = NULL;

	/* initialization */
	gnome_init ("MessageBrowser", "1.0", argc, argv);
	app = gnome_app_new ("Message Browser Test", NULL);
	gnome_app_create_menus (GNOME_APP (app), main_menu);

	/* parse command line */
	if (argc != 2)
		print_usage_and_quit();
	else
		message = filename_to_camel_msg (argv[1]);
#if 0
	if (!message) {
		g_print ("Couldn't open message \"%s\", bailing...\n",
			 argv[1]);
		exit (0);
	}
#endif
	vbox = gtk_vbox_new (FALSE, 0);

	/* add the tree control view of the message*/
	tree_ctrl_window = get_message_tree_ctrl (message);
	gtk_box_pack_start (GTK_BOX (vbox), tree_ctrl_window,
			    TRUE, TRUE, 0);

	/* add the HTML view of the message */
	html_window = get_gtk_html_window (argv[1]);
	gtk_box_pack_start (GTK_BOX (vbox), html_window,
			    TRUE, TRUE, 0);	
	
	/* rock n roll */
	gnome_app_set_contents (GNOME_APP (app),
				vbox);
	gtk_widget_show_all (app);
	gtk_signal_connect (GTK_OBJECT (app), "destroy",
			    GTK_SIGNAL_FUNC(gtk_main_quit),
			    &app);
	gtk_main();

	return 1;
}
