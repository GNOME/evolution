/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Author: Vivek Jain <jvivek@novell.com>
 *
 *  Copyright 2004 Novell, Inc.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public
 *  License as published by the Free Software Foundation.
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
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include <glade/glade.h>
#include "share-folder.h"
#include <glib/gmain.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtk.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkcellrenderertoggle.h>
#include <gtk/gtkcellrenderertext.h>
#include <libedataserverui/e-contact-store.h>
#include <libgnomeui/gnome-ui-init.h>
#include <libgnome/gnome-init.h>
#include <widgets/misc/e-error.h>
#include <e-gw-container.h>
#include <e-gw-connection.h>
#define ROOTNODE "vboxSharing"
#define NROOTNODE "vbox191"
#define d(x)

static void share_folder_class_init (ShareFolderClass *class);
static void share_folder_init       (ShareFolder *sf);
static void share_folder_destroy    (GtkObject *obj);
static void share_folder_finalise   (GObject *obj);
static void free_node(EShUsers *user);
static void free_all(ShareFolder *sf);
static void update_list_update (ShareFolder *sf);
static int find_node(GList *list, gchar *email);
static void free_all(ShareFolder *sf);
static void get_container_list (ShareFolder *sf);
static void user_selected(GtkTreeSelection *selection, ShareFolder *sf);
static void not_shared_clicked (GtkRadioButton *button, ShareFolder *sf);
static void shared_clicked (GtkRadioButton *button, ShareFolder *sf);
static void add_clicked(GtkButton *button, ShareFolder *sf);
static void remove_clicked(GtkButton *button, ShareFolder *sf);
static void not_ok_clicked(GtkButton *button, ShareFolder *sf);
static void not_cancel_clicked(GtkButton *button, GtkWidget *window);
static void not_cancel_clicked(GtkButton *button, GtkWidget *window);
static void share_folder_construct (ShareFolder *sf);
GType share_folder_get_type (void);

static GtkVBoxClass *parent_class = NULL;


GType
share_folder_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (ShareFolderClass),
			NULL, NULL,
			(GClassInitFunc) share_folder_class_init,
			NULL, NULL,
			sizeof (ShareFolder),
			0,
			(GInstanceInitFunc) share_folder_init
		};

		type = g_type_register_static (gtk_vbox_get_type (), "ShareFolder", &info, 0);
	}

	return type;
}

static void
share_folder_class_init (ShareFolderClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);

	parent_class = g_type_class_ref (gtk_vbox_get_type ());
	object_class->destroy = share_folder_destroy;
	gobject_class->finalize = share_folder_finalise;
}

static void
share_folder_finalise (GObject *obj)
{
	ShareFolder *sf = (ShareFolder *) obj;
	g_object_unref (sf->xml);
	free_all(sf);	
	G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
share_folder_destroy (GtkObject *obj)
{

	ShareFolder *sf = (ShareFolder *) obj;
	free_all (sf);	
	GTK_OBJECT_CLASS (parent_class)->destroy (obj);
}

static void
share_folder_init (ShareFolder *sf)
{
	sf->gcontainer = NULL;
	sf->users = 0;
	sf->duplicate = -1;
	sf->flag_for_ok = 0;
	sf->shared = FALSE;
	sf->container_list = NULL;
	sf->new_list = NULL;
	sf->update_list = NULL;
	sf->remove_list = NULL;
	sf->sub = "Shared Folder notification";
	sf->message = NULL;
}

static void
free_node(EShUsers *user)
{
	if(user){
		g_free(user->email);
		user->email = NULL;
	}
	return ;
}

static int
find_node(GList *list, gchar *email)
{

	EShUsers *user = NULL;
	GList *tmp;
	gint i ;
	gint duplicate = -1;
	if(list){
		tmp = g_list_first(list);
		for(i=0; tmp  ; i++)
		{
			user= g_list_nth_data(tmp, 0);
			if(!g_ascii_strcasecmp(user->email, email)){
				duplicate = i;
				break;
			}
			tmp= g_list_next(tmp);
		}

	}
	return duplicate;
}

static void 
free_all(ShareFolder *sf)
{
	if(sf->new_list){
		g_list_foreach (sf->new_list,(GFunc) free_node, NULL);
		g_list_free (sf->new_list);
	}
	if(sf->update_list){
		g_list_foreach (sf->update_list, (GFunc) free_node, NULL);
		g_list_free (sf->update_list);
	}

	sf->new_list = NULL;
	e_gw_container_get_user_list (sf->gcontainer, &(sf->new_list));
	if (sf->new_list) {
		g_list_foreach (sf->new_list, (GFunc) free_node, NULL);
		g_list_free (sf->new_list);
	}
	if (sf->remove_list) {
		g_list_foreach (sf->remove_list, (GFunc) free_node, NULL);
		g_list_free (sf->remove_list);
	}

	sf->new_list = NULL;
	sf->update_list = NULL;
	sf->remove_list = NULL;
	e_gw_connection_free_container_list (sf->container_list);
}

/* updates the rights in case changed in the prev selection*/
static void 
update_list_update (ShareFolder *sf)
{
	gboolean add = FALSE;
	gboolean edit = FALSE;
	gboolean delete = FALSE;
	GList *tmp = NULL;
	EShUsers *user = NULL;
	int rights = 0;
	add = gtk_toggle_button_get_active(sf->add);
	edit = gtk_toggle_button_get_active(sf->edit);
	delete = gtk_toggle_button_get_active(sf->del);
	if (add) {
		rights = rights | 0x1;
	}	
	if (edit) {
		rights = rights | 0x2;
	}
	if (delete) {
		rights = rights | 0x4;
	}

	if(sf->update_list){
		tmp = g_list_last(sf->update_list);
		user = g_list_nth_data(tmp, 0);
		/* if the user is still in the new list then remove from update list*/
		if (sf->new_list && user->email){
			sf->duplicate = find_node (sf->new_list, user->email);
			if (sf->duplicate != -1) {
				sf->update_list = g_list_remove(sf->update_list, user);
				free_node (user);
				if (g_list_length (sf->update_list) == 0)
					sf->update_list = NULL;
				user = g_list_nth_data (sf->new_list, sf->duplicate);	
				sf->duplicate = -1;
				if(user->rights != rights)
					user->rights= rights;

				return ;
			}
		}

		if (user) {
			if(user->rights != rights){
				user->rights= rights;

			} else {
				sf->update_list = g_list_remove (sf->update_list, user);
				free_node (user);
				if (g_list_length (sf->update_list) == 0)
					sf->update_list = NULL;
			}
		}
	}

}

static void	
display_container (EGwContainer *container , ShareFolder *sf)
{
	gchar **tail;
	gchar *id_shared;
	gchar *id_unshared;
	gboolean byme = FALSE;
	gboolean tome = FALSE;
	gchar *email = NULL;
	gchar *msg;
	GList *user_list = NULL;
	EShUsers *user = NULL;

	id_shared = g_strdup(e_gw_container_get_id(container));
	g_print ("folder id: %s ours is %s\n",id_shared, sf->container_id);
	/* this has to be done since id changes after the folder is shared*/
	if( g_str_has_suffix (id_shared, "35")){
		tail = g_strsplit(id_shared, "@", 2);
		id_unshared = g_strconcat(tail[0], "@", "13", NULL);
		g_strfreev(tail);
	}

	if((!g_ascii_strcasecmp(id_unshared, sf->container_id)) || (!g_ascii_strcasecmp(id_shared, sf->container_id)) ){
		sf->gcontainer = container;
		byme = e_gw_container_get_is_shared_by_me(container);
		tome = e_gw_container_get_is_shared_to_me(container);
		if(byme || tome) {
			e_gw_container_get_user_list (sf->gcontainer, &user_list);	
			sf->users = g_list_length (user_list);
			if(sf->users != 0) {
				sf->is_shared = TRUE;
				gtk_toggle_button_set_active((GtkToggleButton *) sf->shared, TRUE);
				shared_clicked(sf->shared , sf);
				if (tome) {
					gtk_widget_set_sensitive (GTK_WIDGET (sf->table), FALSE);
					gtk_widget_set_sensitive (GTK_WIDGET (sf->shared), FALSE);
					gtk_widget_set_sensitive (GTK_WIDGET (sf->not_shared), FALSE);
					gtk_widget_set_sensitive (GTK_WIDGET (sf->scrolled_window), TRUE);
					gtk_widget_set_sensitive (GTK_WIDGET (sf->user_list), TRUE);

					email = g_strdup (e_gw_container_get_owner (sf->gcontainer));
					msg = g_strconcat (email, "  (Owner)", NULL);
					gtk_list_store_append (GTK_LIST_STORE (sf->model), &(sf->iter));
					gtk_list_store_set (GTK_LIST_STORE (sf->model), &(sf->iter), 0, msg, -1);			 
					g_free (msg);
					g_free (email);

				} else
					gtk_widget_set_sensitive (GTK_WIDGET (sf->table), TRUE);

				while (user_list) {
					user = user_list->data;	
					email = g_strdup (user->email);
					msg = g_strdup_printf ("%s", email);
					gtk_list_store_append (GTK_LIST_STORE (sf->model), &(sf->iter));
					gtk_list_store_set (GTK_LIST_STORE (sf->model), &(sf->iter), 0, msg, -1);			 
					g_free (msg);
					g_free (email);
					msg = NULL;
					email = NULL;
					user_list = user_list->next;
				}
				/* i also need to display status*/
			} else {

				gtk_toggle_button_set_active ((GtkToggleButton *) sf->not_shared,  TRUE);
				not_shared_clicked (sf->not_shared , sf);
			}
		}
	}
}

static void
get_container_list (ShareFolder *sf)
{
	sf->container_list = NULL;
	if (E_IS_GW_CONNECTION (sf->cnc)) {
		/* get list of containers */
		if (e_gw_connection_get_container_list (sf->cnc, "folders", &(sf->container_list)) == E_GW_CONNECTION_STATUS_OK) {
			GList *container = NULL;

			for (container = sf->container_list; container != NULL; container = container->next)
				display_container (E_GW_CONTAINER (container->data), sf);

		}
		else
			g_warning("Could not get the Container List");
	}
}

static void 
user_selected(GtkTreeSelection *selection, ShareFolder *sf)
{
	EShUsers *user = NULL;
	gint index = -1;
	int rights = 0;
	gchar *email = NULL;
	int length=0;

	/* This function should be called in the beginning of any probable subsequent event*/
	update_list_update (sf);

	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	if (gtk_tree_selection_get_selected (selection, &(sf->model), &(sf->iter))){
		gtk_widget_set_sensitive (GTK_WIDGET (sf->frame), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (sf->remove), TRUE);

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sf->add), FALSE);	
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sf->del), FALSE);	
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sf->edit), FALSE);
		gtk_tree_model_get ((GtkTreeModel *) sf->model, &(sf->iter), 0, &email, -1);
		index = (gint)g_ascii_strtod(gtk_tree_model_get_string_from_iter(( GtkTreeModel *)sf->model, &(sf->iter)), NULL);

		gtk_label_set_text (GTK_LABEL (sf->user_rights), email); 

		sf->duplicate = find_node(sf->update_list, email);
		if( sf->duplicate == -1) {
			if (sf->shared && index < sf->users) {
				rights = e_gw_container_get_rights (sf->gcontainer, email);
			} else {
				user = g_list_nth_data (sf->new_list, index - sf->users);
				rights = user->rights;
			}
		} else { 
			user = g_list_nth_data (sf->update_list, sf->duplicate);
			rights = user->rights;
			sf->duplicate = -1;
		}
		if (rights & 0x1)
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sf->add), TRUE);		
		if (rights & 0x2)
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sf->edit), TRUE);		
		if (rights & 0x4)
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sf->del), TRUE);


		user = g_new0(EShUsers, 1);	
		user->email = g_strdup (email);
		user->rights = rights;

		if (sf->duplicate != -1) {
			EShUsers *usr = NULL;
			usr = g_list_nth_data (sf->update_list, sf->duplicate);
			if (usr) {
				sf->update_list = g_list_remove (sf->update_list, usr);
				free_node (usr);
			}
			sf->duplicate = -1;
		}
		sf->update_list = g_list_append (sf->update_list, user);
		length = g_list_length (sf->update_list);
	}
	else {
		gtk_widget_set_sensitive (GTK_WIDGET (sf->frame), FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (sf->remove), FALSE);
	}
}

static void 
shared_clicked (GtkRadioButton *button, ShareFolder *sf)
{

	gtk_widget_set_sensitive (GTK_WIDGET (sf->table) ,TRUE);
	sf->flag_for_ok = 0;        
}

static void 
not_shared_clicked (GtkRadioButton *button, ShareFolder *sf)
{
	if (!sf->is_shared) {
		sf->flag_for_ok = 0;
	} else {
		sf->flag_for_ok = 2;
	}
	gtk_widget_set_sensitive (GTK_WIDGET (sf->table), FALSE);

}

static void
add_clicked(GtkButton *button, ShareFolder *sf)
{
	const char *email = NULL;
	EShUsers *user = NULL;
	GList *list = NULL;
	gint rights = 0;
	gint length;
	gchar *msg = NULL;
	EDestinationStore *destination_store;
	GList *destinations, *tmp;
	ENameSelectorEntry *name_selector_entry;

	name_selector_entry = e_name_selector_peek_section_entry (sf->name_selector, "Add User");
	destination_store = e_name_selector_entry_peek_destination_store (E_NAME_SELECTOR_ENTRY (
				name_selector_entry));
	destinations = e_destination_store_list_destinations (destination_store);
	tmp = destinations;
	for (; tmp != NULL; tmp = g_list_next (tmp)) {
		email = e_destination_get_email (tmp->data);
		if (g_strrstr (email, "@") == NULL) 
			e_error_run ((GtkWindow *)gtk_widget_get_toplevel((GtkWidget *) sf->table), "mail:shared-folder-invalid-user-error", email, NULL); 
		else {	
			if (g_ascii_strcasecmp (email, "" )) {
				update_list_update (sf);	
				user = g_new0 (EShUsers, 1);
				user->email = g_strdup(email);
			} else { 

				return;	
			}
			/*check whether already exists*/
			if (sf->gcontainer)
				e_gw_container_get_user_list (sf->gcontainer, &list); 

			if (list && user->email){

				sf->duplicate = find_node (list, user->email);
				if (sf->duplicate != -1) {	
					sf->duplicate = -1;
					return ;
				}
			}
			if (sf->new_list && user->email){

				sf->duplicate = find_node (sf->new_list, user->email);
				if (sf->duplicate != -1) {	
					sf->duplicate = -1;
					return ;
				}
			}

			user->rights = rights;
			msg = g_strdup (user->email);
			gtk_list_store_append (GTK_LIST_STORE (sf->model), &(sf->iter));
			gtk_list_store_set (GTK_LIST_STORE (sf->model), &(sf->iter), 0, msg, -1);

			g_free(msg);
			sf->new_list = g_list_append (sf->new_list, user);
			length = g_list_length (sf->new_list);
			sf->flag_for_ok = 0;
		}
	}
	gtk_entry_set_text (GTK_ENTRY(name_selector_entry), "");

}

static void
remove_clicked(GtkButton *button, ShareFolder *sf)
{

	GList *list = NULL;
	EShUsers *usr = NULL;
	gchar *email;

	/*check whether this is required*/
	gtk_tree_model_get ((GtkTreeModel *) sf->model, &(sf->iter), 0, &email, -1);
	list = g_list_last (sf->update_list);
	usr = g_list_nth_data (list, 0);
	sf->duplicate = find_node (sf->new_list, usr->email);
	sf->update_list = g_list_remove (sf->update_list, usr);
	if (sf->duplicate != -1) {	
		free_node (usr);
		usr = g_list_nth_data (sf->new_list, sf->duplicate);
		sf->new_list = g_list_remove (sf->new_list, usr);
		free_node(usr);		
		sf->duplicate = -1;
	} else {
		sf->remove_list = g_list_append (sf->remove_list, usr);
	}
	g_free (email);
	gtk_list_store_remove (GTK_LIST_STORE (sf->model), &(sf->iter));
	sf->flag_for_ok = 1;
}

void 
share_folder (ShareFolder *sf)
{
	update_list_update (sf);	
	if (E_IS_GW_CONNECTION (sf->cnc)) {
		if(sf->flag_for_ok == 2){  /* you have to remove all the users*/
			GList *list = NULL;

			if(sf->new_list){
				g_list_foreach (sf->new_list, (GFunc) free_node, NULL);
				g_list_free (sf->new_list);
			}
			if(sf->update_list){
				g_list_foreach (sf->update_list, (GFunc) free_node, NULL);
				g_list_free (sf->update_list);
			}

			sf->new_list = NULL;
			if(sf->remove_list){
				g_list_foreach (sf->remove_list,(GFunc) free_node, NULL);
				g_list_free (sf->remove_list);
			}
			sf->remove_list = NULL;
			if (sf->gcontainer) {
				e_gw_container_get_user_list (sf->gcontainer, &list);
				sf->remove_list = g_list_copy (list);

			} else {
				g_warning("Container is Null");
			}	


		} else {
			if (sf->new_list) {
				if (e_gw_connection_share_folder (sf->cnc, sf->container_id, sf->new_list, sf->sub, sf->mesg, 0) == E_GW_CONNECTION_STATUS_OK);
			}

			if (sf->update_list) {
				sf->sub = "Shared Folder rights updated";

			printf ("\n\nthe message is :%s\n\n", sf->mesg);
				if (e_gw_connection_share_folder (sf->cnc, sf->container_id, sf->update_list, sf->sub, sf->mesg, 2) == E_GW_CONNECTION_STATUS_OK);
			}
		}  
		if (sf->remove_list) {
			sf->sub = "Shared Folder removed";
			if (e_gw_connection_share_folder (sf->cnc, sf->container_id, sf->remove_list, sf->sub, sf->mesg, 1) == E_GW_CONNECTION_STATUS_OK);
		}

	}
}

static void
not_ok_clicked(GtkButton *button, ShareFolder *sf)
{

	gchar *subj = NULL;
	gchar *msg = NULL;
	GtkTextIter *start, *end;
	GtkTextBuffer *buffer;

	buffer = gtk_text_buffer_new (NULL);
	start = g_new0 (GtkTextIter, 1);
	end = g_new0 (GtkTextIter, 1);
	subj = g_strdup (gtk_entry_get_text (sf->subject));
	if(subj)
		sf->sub = subj;
	buffer = gtk_text_view_get_buffer (sf->message);
	gtk_text_buffer_get_start_iter (buffer, start);
	gtk_text_buffer_get_end_iter (buffer, end);
	msg = gtk_text_buffer_get_text (buffer, start, end, FALSE);
	if(msg)
		sf->mesg = msg;
	gtk_widget_destroy (GTK_WIDGET (sf->window));

}

static void 
not_cancel_clicked(GtkButton *button, GtkWidget *window)
{
	gtk_widget_destroy(window);
}


static void
notification_clicked(GtkButton *button, ShareFolder *sf)
{

	static  GladeXML *xmln;
	GtkButton *not_ok;
	GtkButton *not_cancel;
	GtkWidget *vbox;

	sf->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	xmln = glade_xml_new (EVOLUTION_GLADEDIR "/properties.glade", NROOTNODE , NULL);
	vbox = GTK_WIDGET (glade_xml_get_widget (xmln, "vbox191"));
	gtk_container_add (GTK_CONTAINER (sf->window), vbox);
	sf->subject = GTK_ENTRY (glade_xml_get_widget (xmln, "entry3"));
	gtk_entry_set_text(GTK_ENTRY (sf->subject) , sf->sub);
	sf->message = GTK_TEXT_VIEW (glade_xml_get_widget (xmln, "textview1"));
	not_ok = GTK_BUTTON (glade_xml_get_widget (xmln, "nOK"));
	g_signal_connect ((gpointer) not_ok, "clicked", G_CALLBACK (not_ok_clicked), sf);
	not_cancel = GTK_BUTTON (glade_xml_get_widget (xmln, "nCancel"));
	g_signal_connect ((gpointer) not_cancel, "clicked", G_CALLBACK (not_cancel_clicked), sf->window);
	gtk_window_set_position (GTK_WINDOW (sf->window) , GTK_WIN_POS_CENTER_ALWAYS);
	gtk_widget_show_all (sf->window);
}

static void
addressbook_dialog_response (ENameSelectorDialog *name_selector_dialog, gint response, gpointer user_data)
{
	gtk_widget_hide (GTK_WIDGET (name_selector_dialog));
}

static void
addressbook_entry_changed (GtkWidget *entry, gpointer   user_data)
{

}

static void
address_button_clicked_cb (GtkButton *button, gpointer data)
{
	ShareFolder *sf = data;
	ENameSelectorDialog *name_selector_dialog;

	name_selector_dialog = e_name_selector_peek_dialog (sf->name_selector);
	gtk_widget_show (GTK_WIDGET (name_selector_dialog));
}
static void 
share_folder_construct (ShareFolder *sf)
{
	GladeXML *xml;
	ENameSelectorDialog *name_selector_dialog;
	ENameSelectorModel *name_selector_model;
	ENameSelectorEntry *name_selector_entry;

	xml = glade_xml_new (EVOLUTION_GLADEDIR "/properties.glade", ROOTNODE, NULL);
	sf->xml =xml; 

	if (!sf->xml) {
		g_warning ("could not get xml");
	}
	sf->vbox = GTK_VBOX (glade_xml_get_widget(sf->xml, "vboxSharing"));
	sf->table = GTK_WIDGET (glade_xml_get_widget (sf->xml, "table26"));
	gtk_widget_set_sensitive (GTK_WIDGET (sf->table), FALSE);

	sf->shared = GTK_RADIO_BUTTON (glade_xml_get_widget (sf->xml, "radShared"));
	g_signal_connect ((gpointer) sf->shared, "clicked", G_CALLBACK (shared_clicked), sf);

	sf->not_shared = GTK_RADIO_BUTTON (glade_xml_get_widget (sf->xml, "radNotShared"));
	g_signal_connect ((gpointer) sf->not_shared, "clicked", G_CALLBACK (not_shared_clicked), sf);

	sf->add_book = GTK_BUTTON (glade_xml_get_widget (sf->xml, "Address"));
	gtk_widget_set_sensitive (GTK_WIDGET (sf->add_book), TRUE);
	g_signal_connect((GtkWidget *) sf->add_book, "clicked", G_CALLBACK (address_button_clicked_cb), sf);

	sf->name_selector = e_name_selector_new ();
	name_selector_dialog = e_name_selector_peek_dialog (sf->name_selector);
	g_signal_connect (name_selector_dialog, "response",
			G_CALLBACK (addressbook_dialog_response), sf);

	name_selector_model = e_name_selector_peek_model (sf->name_selector);
	e_name_selector_model_add_section (name_selector_model, "Add User", "Add User", NULL);

	name_selector_entry = e_name_selector_peek_section_entry (sf->name_selector, "Add User");
	g_signal_connect (name_selector_entry, "changed",
			G_CALLBACK (addressbook_entry_changed), sf);

	sf->add_button = GTK_BUTTON (glade_xml_get_widget(sf->xml, "Add"));
	g_signal_connect((GtkWidget *) sf->add_button, "clicked", G_CALLBACK (add_clicked), sf);

	sf->remove = GTK_BUTTON(glade_xml_get_widget(sf->xml, "Remove"));
	g_signal_connect ((GtkWidget *) sf->remove, "clicked", G_CALLBACK (remove_clicked), sf);
	gtk_widget_set_sensitive(GTK_WIDGET (sf->remove), FALSE);

	sf->notification = GTK_BUTTON (glade_xml_get_widget (sf->xml, "Notification"));
	g_signal_connect((GtkWidget *) sf->notification, "clicked", G_CALLBACK (notification_clicked), sf);

	sf->name = GTK_ENTRY (glade_xml_get_widget (sf->xml, "entry2"));
	/*TODO:connect name and label*/
	gtk_widget_hide (GTK_WIDGET(sf->name));
	gtk_table_attach ((GtkTable *) sf->table, (GtkWidget *) name_selector_entry, 1, 2, 0, 1, GTK_FILL, GTK_EXPAND, 8, 0);
	gtk_widget_show ((GtkWidget *) name_selector_entry);
	sf->frame = GTK_FRAME (glade_xml_get_widget(sf->xml, "frame1"));
	gtk_widget_set_sensitive(GTK_WIDGET (sf->frame), FALSE);


	sf->add = GTK_TOGGLE_BUTTON (glade_xml_get_widget (sf->xml, "checkbutton1"));

	sf->del = GTK_TOGGLE_BUTTON (glade_xml_get_widget (sf->xml, "checkbutton2"));

	sf->edit = GTK_TOGGLE_BUTTON (glade_xml_get_widget (sf->xml, "checkbutton3"));

	sf->user_rights = GTK_LABEL (glade_xml_get_widget (sf->xml,"label550"));

	sf->scrolled_window = GTK_WIDGET (glade_xml_get_widget (sf->xml,"scrolledwindow1"));

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sf->scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	sf->model = gtk_list_store_new (1, G_TYPE_STRING);
	sf->user_list = gtk_tree_view_new ();
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sf->scrolled_window), (GtkWidget *)sf->user_list);
	gtk_tree_view_set_model (GTK_TREE_VIEW (sf->user_list), GTK_TREE_MODEL (sf->model));
	gtk_widget_show (GTK_WIDGET (sf->user_list));

	sf->cell = gtk_cell_renderer_text_new ();
	sf->column = gtk_tree_view_column_new_with_attributes ("Users", sf->cell, "text", 0, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (sf->user_list),
			GTK_TREE_VIEW_COLUMN (sf->column));
	g_signal_connect(gtk_tree_view_get_selection(GTK_TREE_VIEW (sf->user_list)), "changed", G_CALLBACK (user_selected), sf);
}

ShareFolder *
share_folder_new (EGwConnection *ccnc, gchar *id)
{
	ShareFolder *new;
	new = (ShareFolder *) g_object_new (share_folder_get_type (), NULL);
	share_folder_construct (new);
	new->cnc = ccnc;
	new->container_id = id;
	get_container_list(new);

	return (ShareFolder *) new;
}


