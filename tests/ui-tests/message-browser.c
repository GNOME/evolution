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

static void
handle_tree_item (CamelDataWrapper* object, GtkWidget* tree_ctrl)
{
	GtkWidget* tree_item;
	gchar* label = gmime_content_field_get_mime_type (object->mime_type);
	
	tree_item = gtk_tree_item_new_with_label (label);
	gtk_tree_append (GTK_TREE (tree_ctrl), tree_item);
	printf ("Appending %s\n", label);

	if (CAMEL_IS_MULTIPART (object))
	{
		CamelMultipart* multipart = CAMEL_MULTIPART (object);
		GtkWidget* subtree = NULL;
		int max_multiparts = camel_multipart_get_number (multipart);
		int i;

		g_print ("found a multipart w/ %d parts\n", max_multiparts);
		
		if (max_multiparts > 0) {
			subtree = gtk_tree_new();
			gtk_tree_item_set_subtree (GTK_TREE_ITEM(tree_item),
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


static GtkWidget*
get_message_tree_ctrl (CamelMimeMessage* message)
{
	GtkWidget* tree_ctrl = gtk_tree_new ();
	CamelDataWrapper* message_contents =
		camel_medium_get_content_object (CAMEL_MEDIUM (message));

/*
 * Set up the scroll window
 */
	GtkWidget* scroll_wnd = gtk_scrolled_window_new (NULL,NULL);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW(scroll_wnd),
					       tree_ctrl);
	gtk_widget_set_usize (scroll_wnd, 150, 200);

/*
 * Recursively insert tree items in the tree
 */
	handle_tree_item (message_contents, tree_ctrl);
	
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
	camel_data_wrapper_construct_from_stream (
		CAMEL_DATA_WRAPPER (message), input_stream);

	camel_stream_close (input_stream); 

	return message;
}


static void
print_usage_and_quit()
{
	g_print ("Usage: message-browser [FILENAME]\n");
	g_print ("Where FILENAME is the filename of a mime message "); 
	g_print ("in \"message/rfc822\" format.\n");
	exit (0);
}


int
main (int argc, char *argv[])
{
	GtkWidget* app;
	CamelMimeMessage* message;

	if (argc != 2)
		print_usage_and_quit();
	
	gnome_init ("MessageBrowser", "1.0", argc, argv);
	
	app = gnome_app_new ("Message Browser Test", NULL);

	message = filename_to_camel_msg (argv[1]);

#if 0
	if (!message) {
		g_print ("Couldn't open message \"%s\", bailing...\n",
			 argv[1]);
		exit (0);
	}
#endif	
	gnome_app_set_contents (GNOME_APP (app),
				get_message_tree_ctrl (message));

	gtk_widget_show_all (app);
	gtk_signal_connect (GTK_OBJECT (app), "destroy",
			    GTK_SIGNAL_FUNC(gtk_main_quit),
			    &app);
	gtk_main();

	return 1;
}
