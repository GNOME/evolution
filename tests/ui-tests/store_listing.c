/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Author : Bertrand.Guiheneuf@aful.org                                  */



/* A simple and very dirty hack written to test 
   (and perhaps demonstrate) Camel */


#include <gnome.h>
#include <glade/glade.h>

#include "camel-folder.h"
#include "camel-mh-folder.h"
#include "camel-mh-store.h"
#include "camel.h"

static GladeXML *xml;
static CamelSession *_session;


static void add_mail_store (const gchar *store_url);
static void show_folder_messages (CamelFolder *folder);


void
mailbox_row_selected (GtkCTree *ctree, GList *node, gint column, gpointer user_data)
{
	GtkCTreeNode *mailbox_node = GTK_CTREE_NODE (node);
	CamelFolder *folder;
	const gchar *mailbox_name;

	folder = gtk_ctree_node_get_row_data (ctree, mailbox_node);
	if (folder && IS_CAMEL_FOLDER (folder)) {
		mailbox_name = camel_folder_get_name (folder);
		printf ("Mailbox name : %s\n", mailbox_name);
		show_folder_messages (folder);
	} else {
		printf ("Node is a store\n");
	}
	
}


static void 
message_destroy_notify (gpointer data)
{
	CamelMimeMessage *message = CAMEL_MIME_MESSAGE (data);

	gtk_object_unref (GTK_OBJECT (message));

}





static void
show_folder_messages (CamelFolder *folder)
{
	GtkWidget *message_clist;
	gint folder_message_count;
	CamelMimeMessage *message;
	gint i;
	const gchar *clist_row_text[3];
	const char *sent_date, *subject, *sender;
	gint current_row;

	message_clist = glade_xml_get_widget (xml, "message-clist");

	/* clear old message list */
	gtk_clist_clear (GTK_CLIST (message_clist));

	folder_message_count = camel_folder_get_message_count (folder);
	for (i=0; i<folder_message_count; i++) {
		message = camel_folder_get_message (folder, i);
		gtk_object_ref (GTK_OBJECT (message));
		sent_date = camel_mime_message_get_sent_date (message);
		sender = camel_mime_message_get_from (message);
		subject = camel_mime_message_get_subject (message);

		
		if (sent_date) clist_row_text [0] = sent_date;
		else clist_row_text [0] = NULL;
		if (sender) clist_row_text [1] = sender;
		else clist_row_text [1] = NULL;
		if (subject) clist_row_text [2] = subject;
		else clist_row_text [2] = NULL;

		current_row = gtk_clist_append (GTK_CLIST (message_clist), clist_row_text);
		gtk_clist_set_row_data_full (GTK_CLIST (message_clist), current_row, (gpointer)message, message_destroy_notify);
	}

	
	
}



/* add a mail store given by its URL */
static void
add_mail_store (const gchar *store_url)
{

	CamelStore *store;
	GtkWidget *mailbox_and_store_tree;
	GtkCTreeNode* new_store_node;
	GtkCTreeNode* new_folder_node;
	char *new_tree_text[1];
	GList *subfolder_list;
	CamelFolder *root_folder;
	CamelFolder *new_folder;



	store = camel_session_get_store (_session, store_url);
	if (!store) return;

	//store_list = g_list_append (store_list, (gpointer)store);
	mailbox_and_store_tree = glade_xml_get_widget (xml, "store-and-mailbox-tree");
	new_tree_text[0] = g_strdup (store_url);
	new_store_node = gtk_ctree_insert_node (GTK_CTREE (mailbox_and_store_tree),
					  NULL,
					  NULL,
					  new_tree_text,
					  0,
					  NULL,
					  NULL,
					  NULL,
					  NULL,
					  FALSE,
					  FALSE);

	/* normally, use get_root_folder */
	root_folder = camel_store_get_folder (store, "");
	subfolder_list = camel_folder_list_subfolders (root_folder);
	while (subfolder_list) {
		new_tree_text[0] = subfolder_list->data;
		new_folder = camel_store_get_folder (store, subfolder_list->data);
		new_folder_node = gtk_ctree_insert_node (GTK_CTREE (mailbox_and_store_tree),
							 new_store_node,
							 NULL,
							 new_tree_text,
							 0,
							 NULL,
							 NULL,
							 NULL,
							 NULL,
							 FALSE,
							 FALSE);


		gtk_ctree_node_set_row_data (GTK_CTREE (mailbox_and_store_tree), new_folder_node, (gpointer)new_folder);
		subfolder_list = subfolder_list->next;
	}
	
	
}

static void 
delete_selected_messages ()
{
	GtkWidget *message_clist;
	gint current_row;
	GList *selected;
	gint selected_row;

	CamelMimeMessage *message;
	message_clist = glade_xml_get_widget (xml, "message-clist");
	selected = GTK_CLIST (message_clist)->selection;
	while (selected) {
		selected_row = GPOINTER_TO_INT (selected->data);
		message = CAMEL_MIME_MESSAGE (gtk_clist_get_row_data (GTK_CLIST (message_clist), selected_row));
		camel_mime_message_set_flag (message, "DELETED", TRUE);
		selected = selected->next;
	}

}


static void 
expunge_selected_folders ()
{
	GtkWidget *mailbox_and_store_tree;
	CamelFolder *folder;
	GtkCTreeNode* selected_node;
	GList *selected;
	const gchar *folder_name;

	mailbox_and_store_tree = glade_xml_get_widget (xml, "store-and-mailbox-tree");
	
	selected = GTK_CLIST (mailbox_and_store_tree)->selection;
	while (selected) {
		
		selected_node= GTK_CTREE_NODE (selected->data);
		folder = CAMEL_FOLDER (gtk_ctree_node_get_row_data (GTK_CTREE (mailbox_and_store_tree), 
								    selected_node));
		if (folder && IS_CAMEL_FOLDER (folder)) {
			folder_name = camel_folder_get_name (folder);
			printf ("folder to expunge : %s\n", folder_name);
			camel_folder_expunge (folder, FALSE);
			/* reshowing the folder this way is uggly
			   but allows to check the message are
			   correctly renoved and the cache works correctly */
			show_folder_messages (folder);
			
		} else {
			printf ("A selected node is a store\n");
		}
		
		selected = selected->next;
	}

	
}

/* ----- libglade callbacks */
void
on_exit_activate (GtkWidget *widget, void *data)
{
	gtk_main_quit ();
}


void
on_about_activate (GtkWidget *widget, void *data)
{
	GtkWidget *about_widget;
	
	about_widget = glade_xml_get_widget (xml, "about_widget");
	gtk_widget_show (about_widget);
}

void
on_new_store_activate (GtkWidget *widget, void *data)
{
	GtkWidget *new_store_dialog;
	GtkWidget *new_store_gnome_entry;
	GtkWidget *new_store_entry;
	gchar *url_text;


	gint pressed_button;

	new_store_dialog = glade_xml_get_widget (xml, "new_store_dialog");
	pressed_button = gnome_dialog_run (GNOME_DIALOG (new_store_dialog));

	if ((pressed_button != 0) && (pressed_button != 1))
		return;
	
	new_store_gnome_entry = glade_xml_get_widget (xml, "new-store-entry");
	new_store_entry = gnome_entry_gtk_entry (GNOME_ENTRY (new_store_gnome_entry));
	url_text = gtk_entry_get_text (GTK_ENTRY (new_store_entry));
	
	if (url_text)
		add_mail_store (url_text);
	
}


void
on_expunge_activate (GtkWidget *widget, void *data)
{
	expunge_selected_folders ();
}


void 
on_message_delete_activate (GtkWidget *widget, void *data)
{
	delete_selected_messages();
}



/* ----- init */
int
main(int argc, char *argv[])
{
	GtkWidget *new_store_gnome_entry;

	gnome_init ("store_listing", "1.0", argc, argv);
	
	glade_gnome_init ();
	camel_init ();
	xml = glade_xml_new ("store_listing.glade", NULL);
	if (xml) glade_xml_signal_autoconnect (xml);

	_session = camel_session_new ();
	camel_provider_register_as_module ("../../camel/providers/MH/.libs/libcamelmh.so");
	
	new_store_gnome_entry = glade_xml_get_widget (xml, "new-store-entry");
	gnome_entry_load_history (GNOME_ENTRY (new_store_gnome_entry));
	gtk_main ();
	gnome_entry_save_history (GNOME_ENTRY (new_store_gnome_entry));

	return 0;
}

