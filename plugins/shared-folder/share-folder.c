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

#include <libgnome/gnome-init.h>
#include <e-gw-container.h>
#include <e-gw-connection.h>

#define ROOTNODE "vboxSharing"



#define d(x)

static void share_folder_class_init (ShareFolderClass *class);
static void share_folder_init       (ShareFolder *sf);
static void share_folder_destroy    (GtkObject *obj);
static void share_folder_finalise   (GObject *obj);

static void free_node(EShUsers *user);
static void free_all(ShareFolder *sf);


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
			(GInstanceInitFunc) share_folder_init,
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
	free_all(sf);	
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
			if(!strcmp(user->email, email)){
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
		g_list_foreach (sf->new_list, free_node, NULL);
		g_list_free (sf->new_list);
	}
	if(sf->update_list){
		g_list_foreach (sf->update_list, free_node, NULL);
		g_list_free (sf->update_list);
	}

	sf->new_list = NULL;


	e_gw_container_get_user_list (sf->gcontainer, &(sf->new_list));
	if (sf->new_list) {
		g_list_foreach (sf->new_list, free_node, NULL);
		g_list_free (sf->new_list);
	}
	if (sf->remove_list) {
		g_list_foreach (sf->remove_list, free_node, NULL);
		g_list_free (sf->remove_list);
	}

	sf->new_list = NULL;
	sf->update_list = NULL;
	sf->remove_list = NULL;
	e_gw_connection_free_container_list (sf->container_list);

}


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
	if(add){
		rights = rights|0x1;
	}	
	if(edit){
		rights = rights|0x2;
	}
	if(delete){
		rights = rights|0x4;
	}
	if(sf->update_list){
		tmp = g_list_last(sf->update_list);
		user = g_list_nth_data(tmp, 0);
		if(user){
			if(user->rights != rights){
				user->rights= rights;

			}
			else{
				sf->update_list = g_list_remove(sf->update_list, user);
				free_node(user);
				if(g_list_length(sf->update_list) == 0)
					sf->update_list = NULL;
			}
		}
	}

}


static void	
display_container (EGwContainer *container , ShareFolder *sf)
{
	gint i;
	gchar **tail;
	gchar *id_shared;
	gchar *id_unshared;
	gboolean byme = FALSE;
	gboolean tome = FALSE;

	id_shared = e_gw_container_get_id(container);
	if( g_str_has_suffix (id_shared, "35")){
		tail = g_strsplit(id_shared, "@", 2);
		id_unshared = g_strconcat(tail[0], "@", "13", NULL);
		g_strfreev(tail);
	}

	if((!strcmp(id_unshared, sf->container_id)) ||(!strcmp(id_shared, sf->container_id)) ){
		sf->gcontainer = container;
		byme = e_gw_container_get_is_shared_by_me(container);
		tome = e_gw_container_get_is_shared_to_me(container);
		if(byme | tome) {

			sf->users = e_gw_container_get_length (sf->gcontainer);

			if(sf->users != 0){
				sf->is_shared = TRUE;
				gtk_toggle_button_set_active((GtkToggleButton *) sf->shared, TRUE);
				shared_clicked(sf->shared , sf);
				if(tome){
					gtk_widget_set_sensitive (sf->table, FALSE);
					gtk_widget_set_sensitive (sf->shared, FALSE);
					gtk_widget_set_sensitive (sf->not_shared, FALSE);

				}

				gchar *email= NULL;
				gchar *msg;
				for(i = 0; i < sf->users; i++){	

					email = g_strdup (e_gw_container_get_email(container, i));
					msg = g_strdup_printf ("%s", email);
					gtk_list_store_append (GTK_LIST_STORE (sf->model), &(sf->iter));
					gtk_list_store_set (GTK_LIST_STORE (sf->model), &(sf->iter), 0, msg, -1);			 
					g_free (msg);
					g_free (email);
					msg = NULL;
					email = NULL;
					g_print("\n");
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
		gtk_widget_set_sensitive (sf->frame, TRUE);
		gtk_widget_set_sensitive (sf->remove, TRUE);

		gtk_toggle_button_set_active (sf->add, FALSE);	
		gtk_toggle_button_set_active (sf->del, FALSE);	
		gtk_toggle_button_set_active (sf->edit, FALSE);
		gtk_tree_model_get (sf->model, &(sf->iter), 0, &email, -1);
		index = (gint)g_ascii_strtod(gtk_tree_model_get_string_from_iter(sf->model, &(sf->iter)), NULL);

		gtk_label_set_text (sf->user_rights, email); 

		sf->duplicate = find_node(sf->update_list, email);
		if( sf->duplicate == -1){
			if (sf->shared && index < sf->users){
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
		if(rights & 0x1)
			gtk_toggle_button_set_active (sf->add, TRUE);		
		if(rights & 0x2)
			gtk_toggle_button_set_active (sf->edit, TRUE);		
		if(rights & 0x4)
			gtk_toggle_button_set_active (sf->del, TRUE);


		user = g_new0(EShUsers, 1);	
		user->email = g_strdup (email);
		user->rights = rights;

		if(sf->duplicate != -1) {
			EShUsers *usr = NULL;
			usr = g_list_nth_data (sf->update_list, sf->duplicate);
			if(usr){
				sf->update_list = g_list_remove (sf->update_list, usr);
				free_node (usr);
			}
			sf->duplicate = -1;
		}



		sf->update_list = g_list_append (sf->update_list, user);
		length = g_list_length (sf->update_list);
	}
	else {
		gtk_widget_set_sensitive (sf->frame, FALSE);
		gtk_widget_set_sensitive (sf->remove, FALSE);

	}

}







static void 
shared_clicked (GtkRadioButton *button, ShareFolder *sf)
{

	gtk_widget_set_sensitive (sf->table ,TRUE);
	sf->flag_for_ok = 0;        

}

static void 
not_shared_clicked (GtkRadioButton *button, ShareFolder *sf)
{
	if (!sf->is_shared) {
		sf->flag_for_ok = 0;
	}else{
		sf->flag_for_ok = 2;
	}

	gtk_widget_set_sensitive (sf->table, FALSE);

}


void
add_clicked(GtkButton       *button, ShareFolder *sf)
{
	static gchar *email = NULL;
	EShUsers *user = NULL;
	GList *list = NULL;
	gint rights = 0;
	gint length;
	gchar *msg = NULL;
	gboolean add,edit, delete;

	email = gtk_entry_get_text (sf->name);
	if (strcmp (email, "" )) {
		update_list_update (sf);	
		user = g_new0 (EShUsers, 1);
		user->email = g_strdup(email);
	}else{ 
		return;	
	}
	/*check whether already exists*/
	e_gw_container_get_user_list (sf->gcontainer, &list); 

	if(list && user->email){

		sf->duplicate = find_node (list, user->email);
		if (sf->duplicate != -1) {	
			sf->duplicate = -1;
			return ;
		}
	}
	if(sf->new_list && user->email){

		sf->duplicate = find_node (sf->new_list, user->email);
		if (sf->duplicate != -1) {	
			sf->duplicate = -1;
			return ;
		}
	}

	add = gtk_toggle_button_get_active (sf->add);
	edit = gtk_toggle_button_get_active (sf->edit);
	delete = gtk_toggle_button_get_active (sf->del);
	if(add) {
		rights = rights|0x1;
	}	
	if(edit) {
		rights = rights|0x2;
	}
	if(delete) {
		rights = rights|0x4;
	}


	e_gw_container_set_rights (user, rights);
	msg = g_strdup (user->email);
	gtk_list_store_append (GTK_LIST_STORE (sf->model), &(sf->iter));
	gtk_list_store_set (GTK_LIST_STORE (sf->model), &(sf->iter), 0, msg, -1);

	g_free(msg);
	sf->new_list = g_list_append (sf->new_list, user);
	length = g_list_length (sf->new_list);
	g_print("\nlist length: %d\n\n\n",length);

	sf->flag_for_ok = 0;
	gtk_entry_set_text (sf->name, "");	

}


static void
remove_clicked(GtkButton *button, ShareFolder *sf)
{

	GList *list = NULL;
	EShUsers *usr = NULL;
	gchar *email;
	gchar *removed_addr;

	/*check whether this is required*/
	gtk_tree_model_get (sf->model, &(sf->iter), 0, &email, -1);

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

		if(sf->gcontainer){

			if(sf->flag_for_ok == 2){  /* you have to remove all the users*/
				GList *list = NULL;


				if(sf->new_list){
					g_list_foreach (sf->new_list, free_node, NULL);
					g_list_free (sf->new_list);
				}
				if(sf->update_list){
					g_list_foreach (sf->update_list, free_node, NULL);
					g_list_free (sf->update_list);
				}

				sf->new_list = NULL;
				if(sf->remove_list){
					g_list_foreach (sf->remove_list, free_node, NULL);
					g_list_free (sf->remove_list);
				}
				sf->remove_list = NULL;

				e_gw_container_get_user_list (sf->gcontainer, &list);
				sf->remove_list = g_list_copy (list);


			} else {


				if (sf->new_list) {
					if (e_gw_connection_share_folder (sf->cnc, sf->gcontainer, sf->new_list, sf->sub, sf->mesg, 0) == E_GW_CONNECTION_STATUS_OK);
				}

				if (sf->update_list) {
					if (e_gw_connection_share_folder (sf->cnc, sf->gcontainer, sf->update_list, sf->sub, sf->mesg, 2) == E_GW_CONNECTION_STATUS_OK);
				}
			}  


			if (sf->remove_list) {
				if (e_gw_connection_share_folder (sf->cnc, sf->gcontainer, sf->remove_list, sf->sub, sf->mesg, 1) == E_GW_CONNECTION_STATUS_OK);
			}
		}
		else
			g_warning("Container is Null");

	}

}



static void
not_ok_clicked(GtkButton *button, ShareFolder *sf)
{

	gchar *subj = NULL;
	gchar *msg = NULL;
	GtkTextIter *start,*end;
	GtkTextBuffer *buffer;

	buffer=g_new0(GtkTextBuffer,1);
	start = g_new0 (GtkTextIter, 1);
	end = g_new0 (GtkTextIter, 1);
	subj = g_strdup (gtk_entry_get_text (sf->subject));
	if(subj)
		sf->sub = subj;


	buffer = gtk_text_view_get_buffer (sf->message);
	gtk_text_buffer_get_start_iter (buffer, start);
	gtk_text_buffer_get_end_iter (buffer, end);

	msg = g_strdup(gtk_text_buffer_get_text (buffer, start, end, FALSE));
	if(msg)
		sf->mesg = msg;

	gtk_widget_destroy(sf->window);


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
	GtkWidget *not_ok;
	GtkWidget *not_cancel;
	int length = 0;

	xmln = glade_xml_new (EVOLUTION_GLADEDIR "/properties.glade" ,"window1", NULL);
	sf->window = GTK_WINDOW (glade_xml_get_widget (xmln, "window1"));
	sf->subject = GTK_ENTRY (glade_xml_get_widget (xmln, "entry3"));
	gtk_entry_set_text(sf->subject , sf->sub);

	sf->message = GTK_TEXT_VIEW (glade_xml_get_widget (xmln, "textview1"));
	not_ok = GTK_BUTTON (glade_xml_get_widget (xmln, "nOK"));
	g_signal_connect ((gpointer) not_ok, "clicked", G_CALLBACK (not_ok_clicked), sf);
	not_cancel = GTK_BUTTON (glade_xml_get_widget (xmln, "nCancel"));
	g_signal_connect ((gpointer) not_cancel, "clicked", G_CALLBACK (not_cancel_clicked), sf->window);
}

static void 
share_folder_construct (ShareFolder *sf)
{

	GladeXML *xml;
	xml = glade_xml_new (EVOLUTION_GLADEDIR "/properties.glade", ROOTNODE, NULL);
	sf->xml =xml; 

	if(!sf->xml) {
		g_warning("could not get xml");
	}
	/*checck abt this Parent?*/
	sf->vbox = GTK_VBOX(glade_xml_get_widget(sf->xml, "vboxSharing"));


	sf->table = GTK_TABLE (glade_xml_get_widget (sf->xml, "table26"));
	if(!sf->table)
	gtk_widget_set_sensitive (sf->table, FALSE);

	sf->shared = GTK_RADIO_BUTTON (glade_xml_get_widget (sf->xml, "radShared"));
	g_signal_connect ((gpointer) sf->shared, "clicked", G_CALLBACK (shared_clicked),  sf);

	sf->not_shared = GTK_RADIO_BUTTON (glade_xml_get_widget (sf->xml, "radNotShared"));
	g_signal_connect ((gpointer) sf->not_shared, "clicked", G_CALLBACK (not_shared_clicked), sf);


	sf->add_book = GTK_BUTTON (glade_xml_get_widget (sf->xml, "Address"));
	gtk_widget_set_sensitive (sf->add_book, FALSE);

	sf->add_button = GTK_BUTTON (glade_xml_get_widget(sf->xml, "Add"));
	g_signal_connect((GtkWidget *) sf->add_button, "clicked", G_CALLBACK (add_clicked), sf);

	sf->remove = GTK_BUTTON(glade_xml_get_widget(sf->xml, "Remove"));
	g_signal_connect ((GtkWidget *) sf->remove, "clicked", G_CALLBACK (remove_clicked), sf);
	gtk_widget_set_sensitive(sf->remove, FALSE);

	sf->notification = GTK_BUTTON (glade_xml_get_widget (sf->xml, "Notification"));
	g_signal_connect((GtkWidget *) sf->notification, "clicked", G_CALLBACK (notification_clicked), sf);


	sf->name = GTK_ENTRY (glade_xml_get_widget (sf->xml, "entry2"));
	/*TODO:connect name and label*/
	gtk_widget_show(sf->name);

	sf->frame = GTK_FRAME (glade_xml_get_widget(sf->xml, "frame1"));
	gtk_widget_set_sensitive(sf->frame, FALSE);


	sf->add = GTK_TOGGLE_BUTTON (glade_xml_get_widget (sf->xml, "checkbutton1"));

	sf->del = GTK_TOGGLE_BUTTON (glade_xml_get_widget (sf->xml, "checkbutton2"));

	sf->edit = GTK_TOGGLE_BUTTON (glade_xml_get_widget (sf->xml, "checkbutton3"));

	sf->user_rights = GTK_LABEL (glade_xml_get_widget (sf->xml,"label550"));

	sf->scrolledwindow = GTK_SCROLLED_WINDOW (glade_xml_get_widget (sf->xml,"scrolledwindow1"));

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sf->scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	sf->model = gtk_list_store_new (1, G_TYPE_STRING);
	sf->user_list = gtk_tree_view_new ();
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sf->scrolledwindow),  sf->user_list);
	gtk_tree_view_set_model (GTK_TREE_VIEW (sf->user_list), GTK_TREE_MODEL (sf->model));
	gtk_widget_show (sf->user_list);


	sf->cell = gtk_cell_renderer_text_new ();
	sf->column = gtk_tree_view_column_new_with_attributes ("Users", sf->cell, "text", 0, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (sf->user_list),
			GTK_TREE_VIEW_COLUMN (sf->column));

	g_signal_connect(gtk_tree_view_get_selection(sf->user_list), "changed", G_CALLBACK (user_selected), sf);


}



ShareFolder *
share_folder_new (gchar *ccnc, gchar *id)
{
	ShareFolder *new;
	new = (ShareFolder *) g_object_new (share_folder_get_type (), NULL);
	share_folder_construct (new);
	new->cnc = ccnc;
	new->container_id = id;
	get_container_list(new);
	return (GtkWidget *) new;

}


