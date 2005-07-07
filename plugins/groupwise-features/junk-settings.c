/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Author: Vivek Jain <jvivek@novell.com>
 *
 *  Copyright 2005 Novell, Inc.
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
#include "junk-settings.h"
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
#include <libgnomeui/gnome-ui-init.h>
#include <libgnome/gnome-init.h>
#include <e-util/e-error.h>
#include <e-gw-connection.h>
#define ROOTNODE "vboxSettings"
#define d(x)

struct _JunkEntry {
	EGwJunkEntry *entry;
	int flag;
};

typedef struct _JunkEntry JunkEntry;

static void junk_settings_class_init (JunkSettingsClass *class);
static void junk_settings_init       (JunkSettings *js);
static void junk_settings_destroy    (GtkObject *obj);
static void junk_settings_finalise   (GObject *obj);
static void free_all(JunkSettings *js);
static void get_junk_list (JunkSettings *js);
static void disable_clicked (GtkRadioButton *button, JunkSettings *js);
static void enable_clicked (GtkRadioButton *button, JunkSettings *js);
GType junk_settings_get_type (void);

static GtkVBoxClass *parent_class = NULL;

GType
junk_settings_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (JunkSettingsClass),
			NULL, NULL,
			(GClassInitFunc) junk_settings_class_init,
			NULL, NULL,
			sizeof (JunkSettings),
			0,
			(GInstanceInitFunc) junk_settings_init
		};

		type = g_type_register_static (gtk_vbox_get_type (), "JunkSettings", &info, 0);
	}

	return type;
}

static void
junk_settings_class_init (JunkSettingsClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);

	parent_class = g_type_class_ref (gtk_vbox_get_type ());
	object_class->destroy = junk_settings_destroy;
	gobject_class->finalize = junk_settings_finalise;
}

static void
junk_settings_finalise (GObject *obj)
{
	JunkSettings *js = (JunkSettings *) obj;
	g_object_unref (js->xml);
	free_all(js);

	G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
junk_settings_destroy (GtkObject *obj)
{
	JunkSettings *js = (JunkSettings *) obj;
	free_all (js);	
	GTK_OBJECT_CLASS (parent_class)->destroy (obj);
}

static void
junk_settings_init (JunkSettings *js)
{
	js->users = 0;
	js->flag_for_ok = 0;
	js->enabled = FALSE;
	js->junk_list = NULL;
	js->entry_list = NULL;
}

static void
free_entry_node(EGwJunkEntry *entry)
{
	if(entry){
		g_free(entry->match);
		entry->match = NULL;
	}

	return;
}

static void
free_node(JunkEntry *nentry)
{
	EGwJunkEntry *junk_entry = nentry->entry;

	if(junk_entry){
		g_free(junk_entry->match);
		junk_entry->match = NULL;
	}

	return;
}

static JunkEntry * 
find_node(GList *list, gchar *match)
{
	JunkEntry *one_entry = NULL;
	EGwJunkEntry *ent = NULL; 
	GList *tmp;
	gint i ;
	
	if(list){
		tmp = g_list_first(list); 
		for(i=0; tmp  ; i++)
		{
			one_entry = tmp->data;
			ent = one_entry->entry;
			if(!g_ascii_strcasecmp(ent->match, match)){
				return one_entry;
	/*if found, it returns that user*/
			}
			tmp= g_list_next(tmp);
		}
	}
	
	return NULL;
}

static void 
free_all (JunkSettings *js)
{
	if (js->junk_list){
		g_list_foreach (js->junk_list,(GFunc) free_node, NULL);
		g_list_free (js->junk_list);
		js->junk_list = NULL;
	}
}

static void
get_junk_list (JunkSettings *js)
{
	GList *list = NULL;
	char *entry;
	char *msg;
	int use_junk, use_block, use_pab, persistence;
	
	if (E_IS_GW_CONNECTION (js->cnc)) {
		if (e_gw_connection_get_junk_settings (js->cnc, &use_junk, &use_block, &use_pab, &persistence) == E_GW_CONNECTION_STATUS_OK) {
			if (use_junk) {
				js->enabled = TRUE;
				gtk_toggle_button_set_active((GtkToggleButton *) js->enable, TRUE);
				enable_clicked(js->enable , js);
				gtk_widget_set_sensitive (GTK_WIDGET (js->table), TRUE);
			} else {
				gtk_toggle_button_set_active ((GtkToggleButton *) js->disable,  TRUE);
				disable_clicked (js->disable , js);
			}
		}
		/* get list of containers */
		if (e_gw_connection_get_junk_entries (js->cnc, &(list)) == E_GW_CONNECTION_STATUS_OK) {
			js->users = g_list_length (list);
			if (js->users) {
/* I populate the list and set flags to 0 for the existing users*/ 
				while (list) {
					JunkEntry *junk_entry = g_new0 (JunkEntry , 1);
					junk_entry->entry = list->data;
					junk_entry->flag = 0;	
					entry = g_strdup ((junk_entry->entry)->match);
					msg = g_strdup_printf ("%s", entry);
					gtk_list_store_append (GTK_LIST_STORE (js->model), &(js->iter));
					gtk_list_store_set (GTK_LIST_STORE (js->model), &(js->iter), 0, msg, -1);			 
					js->junk_list = g_list_append (js->junk_list, junk_entry);

					g_free (msg);
					g_free (entry);
					msg = NULL;
					entry = NULL;
					list = list->next;
				}
			} 
		}
		else
			g_warning("Could not get the JUNK List");
	}
}

void
commit_changes (JunkSettings *js)
{
	GList *new_list = NULL;
	GList *remove_list = NULL;
	GList *node = NULL;
	JunkEntry *junk_entry = NULL;
	EGwJunkEntry *entry;
	int use_junk, use_pab, use_block, persistence;

	for (node = js->junk_list; node; node = node->next)
	{
		junk_entry = node->data;	
		if (junk_entry->flag & 0x1)
			new_list = g_list_append (new_list, junk_entry->entry);	
		else if (junk_entry->flag & 0x4) {
			remove_list = g_list_append (remove_list, junk_entry->entry);	
		}
	}

	if (E_IS_GW_CONNECTION (js->cnc)) {
		if(js->flag_for_ok == 2 && js->enabled){  /* just turn off the bits*/
			use_junk = use_pab = use_block = persistence = 0;
			if (e_gw_connection_modify_junk_settings (js->cnc, use_junk, use_pab, use_block, persistence) == E_GW_CONNECTION_STATUS_OK);

		}
		if (js->flag_for_ok == 0 && !js->enabled) {
			use_block = use_pab =0;
			use_junk = 1;
			persistence = 14; /* We are setting the default persistence*/
			if (e_gw_connection_modify_junk_settings (js->cnc, use_junk, use_pab, use_block, persistence) == E_GW_CONNECTION_STATUS_OK);
		}

		while (new_list) {
			entry = new_list->data;
			if (e_gw_connection_create_junk_entry (js->cnc, entry->match, "email", "junk") == E_GW_CONNECTION_STATUS_OK);
			new_list = new_list->next;
		}
		while (remove_list) {
			entry = remove_list->data;
			if (e_gw_connection_remove_junk_entry (js->cnc, entry->id) == E_GW_CONNECTION_STATUS_OK);
			remove_list = remove_list->next;
		}

	}
	if(new_list){
		g_list_foreach (new_list, (GFunc) free_entry_node, NULL);
		g_list_free (new_list);
	}
	new_list = NULL;
	if(remove_list){
		g_list_foreach (remove_list,(GFunc) free_entry_node, NULL);
		g_list_free (remove_list);
	}
	remove_list = NULL;
}

static void 
enable_clicked (GtkRadioButton *button, JunkSettings *js)
{
	js->flag_for_ok = 0;        
	gtk_widget_set_sensitive (GTK_WIDGET (js->table) ,TRUE);
}

static void 
disable_clicked (GtkRadioButton *button, JunkSettings *js)
{
	js->flag_for_ok = 2;
	gtk_widget_set_sensitive (GTK_WIDGET (js->table), FALSE);
}

static void
add_clicked(GtkButton *button, JunkSettings *js)
{
	const char *email = NULL;
	const char *self_email = NULL;
	JunkEntry *new_entry = NULL;
	EGwJunkEntry *junk_entry = NULL;
	gchar *msg = NULL;
	
	self_email = g_strdup (e_gw_connection_get_user_email (js->cnc));
		email = gtk_entry_get_text (js->entry);
		/* You can't mark junk sender yourself*/
		if (g_strrstr (email, "@") == NULL || (!g_ascii_strcasecmp (email , self_email)) || !g_ascii_strcasecmp (email, "" ))
		       return; 	
		else {	
			/*check whether already exists*/
			if (js->junk_list && email){
				new_entry = find_node (js->junk_list, (gchar *)email);
				if (new_entry) 
					return;

			}
			junk_entry = g_new0 (EGwJunkEntry, 1);
			new_entry = g_new0 (JunkEntry, 1);
			junk_entry->match = g_strdup(email);
/*XXX:populate more fields*/
			new_entry->entry = junk_entry;
			new_entry->flag = 1;
			msg = g_strdup (email);
			gtk_list_store_append (GTK_LIST_STORE (js->model), &(js->iter)); 
			gtk_list_store_set (GTK_LIST_STORE (js->model), &(js->iter), 0, msg, -1);
			g_free(msg);
			js->junk_list = g_list_append (js->junk_list, new_entry);
			js->flag_for_ok = 0;
		}
	gtk_entry_set_text (GTK_ENTRY(js->entry), ""); 
}

static void
remove_clicked(GtkButton *button, JunkSettings *js)
{
	JunkEntry *entry = NULL;
	gchar *email;

	gtk_tree_model_get ((GtkTreeModel *) js->model, &(js->iter), 0, &email, -1);
	entry = find_node (js->junk_list, email);
	if (entry->flag & 0x1) {	
		js->junk_list = g_list_remove (js->junk_list, entry);
		free_node(entry);		
	} else {
		entry->flag = 0;
		entry->flag |= 0x4;
	}
	g_free (email);
	gtk_list_store_remove (GTK_LIST_STORE (js->model), &(js->iter));
}


static void 
user_selected(GtkTreeSelection *selection, JunkSettings *js)
{
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	if (gtk_tree_selection_get_selected (selection, &(js->model), &(js->iter))){
		gtk_widget_set_sensitive (GTK_WIDGET (js->remove), TRUE);

	} 
}
	
static void 
junk_settings_construct (JunkSettings *js)
{
	GladeXML *xml;
	GtkWidget *box;

	xml = glade_xml_new (EVOLUTION_GLADEDIR "/junk-settings.glade", ROOTNODE, NULL);
	js->xml =xml; 

	if (!js->xml) {
		g_warning ("could not get xml");
	}
	js->vbox = GTK_VBOX (glade_xml_get_widget(js->xml, "vboxSettings"));
	js->table = GTK_WIDGET (glade_xml_get_widget (js->xml, "vbox194"));
	gtk_widget_set_sensitive (GTK_WIDGET (js->table), FALSE);

	js->enable = GTK_RADIO_BUTTON (glade_xml_get_widget (js->xml, "radEnable"));
	g_signal_connect ((gpointer) js->enable, "clicked", G_CALLBACK (enable_clicked), js);

	js->disable = GTK_RADIO_BUTTON (glade_xml_get_widget (js->xml, "radDisable"));
	g_signal_connect ((gpointer) js->disable, "clicked", G_CALLBACK (disable_clicked), js);

	js->add_button = GTK_BUTTON (glade_xml_get_widget(js->xml, "Add"));
	g_signal_connect((GtkWidget *) js->add_button, "clicked", G_CALLBACK (add_clicked), js);

	js->remove = GTK_BUTTON(glade_xml_get_widget(js->xml, "Remove"));
	g_signal_connect ((GtkWidget *) js->remove, "clicked", G_CALLBACK (remove_clicked), js);
	gtk_widget_set_sensitive(GTK_WIDGET (js->remove), FALSE);

	js->entry = GTK_ENTRY (glade_xml_get_widget (js->xml, "entry4"));
		/*TODO:connect entry and label*/
	box = GTK_WIDGET (glade_xml_get_widget (js->xml, "hbox227"));
	gtk_widget_show ((GtkWidget *) js->entry);

	js->scrolled_window = GTK_WIDGET (glade_xml_get_widget (js->xml,"scrolledwindow4"));

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (js->scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	js->model = gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);
	js->entry_list = gtk_tree_view_new ();
	/*gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (js->scrolled_window), (GtkWidget *)js->entry_list);*/
	gtk_container_add (GTK_CONTAINER (js->scrolled_window), (GtkWidget *)js->entry_list);
	gtk_tree_view_set_model (GTK_TREE_VIEW (js->entry_list), GTK_TREE_MODEL (js->model));
	gtk_widget_show (GTK_WIDGET (js->entry_list));

	js->cell = gtk_cell_renderer_text_new ();
	js->column = gtk_tree_view_column_new_with_attributes ("Email", js->cell, "text", 0, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (js->entry_list),
			GTK_TREE_VIEW_COLUMN (js->column));

	g_signal_connect(gtk_tree_view_get_selection(GTK_TREE_VIEW (js->entry_list)), "changed", G_CALLBACK (user_selected), js);
}

JunkSettings *
junk_settings_new (EGwConnection *ccnc)
{
	JunkSettings *new;
	new = (JunkSettings *) g_object_new (junk_settings_get_type (), NULL);
	junk_settings_construct (new);
	new->cnc = ccnc;
	if (new->cnc)
	get_junk_list(new);

	return (JunkSettings *) new;
}


