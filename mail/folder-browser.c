/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * folder-browser.c: Folder browser top level component
 *
 * Author:
 *   Miguel de Icaza (miguel@kernel.org)
 *
 * (C) 2000 Helix Code, Inc.
 */
#include <config.h>
#include <gnome.h>
#include "e-util/e-util.h"
#include "e-util/e-sexp.h"
#include "folder-browser.h"
#include "mail.h"
#include "message-list.h"
#include <widgets/e-paned/e-vpaned.h>

#define PARENT_TYPE (gtk_table_get_type ())

static GtkObjectClass *folder_browser_parent_class;


#define PROPERTY_FOLDER_URI          "folder_uri"
#define PROPERTY_MESSAGE_PREVIEW     "message_preview"

#define PROPERTY_FOLDER_URI_IDX      1
#define PROPERTY_MESSAGE_PREVIEW_IDX 2



static void
folder_browser_destroy (GtkObject *object)
{
	FolderBrowser *folder_browser = FOLDER_BROWSER (object);
	
	if (folder_browser->shell) {
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		Bonobo_Unknown_unref (folder_browser->shell, &ev);
		CORBA_exception_free (&ev);
	}
	
	if (folder_browser->uri)
		g_free (folder_browser->uri);

	if (folder_browser->folder)
		gtk_object_unref (GTK_OBJECT (folder_browser->folder));
	
	if (folder_browser->message_list)
		bonobo_object_unref (BONOBO_OBJECT (folder_browser->message_list));

	folder_browser_parent_class->destroy (object);
}

static void
folder_browser_class_init (GtkObjectClass *object_class)
{
	object_class->destroy = folder_browser_destroy;

	folder_browser_parent_class = gtk_type_class (PARENT_TYPE);
}

static gboolean
folder_browser_load_folder (FolderBrowser *fb, const char *name)
{
	char *store_name, *msg;
	CamelStore *store;
	CamelFolder *new_folder = NULL;
	CamelException *ex;
	gboolean new_folder_exists = FALSE;

	ex = camel_exception_new ();

	if (!strncmp(name, "vfolder:", 8)) {
		char *query, *newquery;
		store_name = g_strdup(name);
		query = strchr(store_name, '?');
		if (query) {
			*query++ = 0;
		} else {
			query = "";
		}
		newquery = g_strdup_printf("mbox?%s", query);
		store = camel_session_get_store (session, store_name, ex);

		if (store) {
			new_folder = camel_store_get_folder (store, newquery, ex);
			/* FIXME: do this properly rather than hardcoding */
#warning "Find a way not to hardcode vfolder source"
			{
				CamelStore *st;
				char *stname;
				CamelFolder *source_folder;
				extern char *evolution_dir;
				
				stname = g_strdup_printf("mbox://%s/local/Inbox", evolution_dir);
				st = camel_session_get_store (session, stname, ex);
				g_free (stname);
				if (st) {
					source_folder = camel_store_get_folder (st, "mbox", ex);
					if (source_folder) {
						camel_vee_folder_add_folder(new_folder, source_folder);
					}
				}
			}
		}
		g_free(newquery);
		g_free(store_name);

	} else if (!strncmp(name, "file:", 5)) {
		/* Change "file:" to "mbox:". */
		store_name = g_strdup_printf ("mbox:%s", name + 5);
		store = camel_session_get_store (session, store_name, ex);
		g_free (store_name);
		if (store) {
			new_folder = camel_store_get_folder (store, "mbox", ex);
		}
	} else {
		char *msg;

		msg = g_strdup_printf ("Can't open URI %s", name);
		gnome_error_dialog (msg);
		g_free (msg);
		camel_exception_free (ex);
		return FALSE;
	}
	
	if (store)
		gtk_object_unref (GTK_OBJECT (store));

	if (camel_exception_get_id (ex)) {
		msg = g_strdup_printf ("Unable to get folder %s: %s\n", name,
				       camel_exception_get_description (ex));
		gnome_error_dialog (msg);
		camel_exception_free (ex);
		if (new_folder)
			gtk_object_unref((GtkObject *)new_folder);
		return FALSE;
	}
	
	/* If the folder does not exist, we don't want to show it */
	new_folder_exists = camel_folder_exists (new_folder, ex);
	if (camel_exception_get_id (ex)) {
		msg = g_strdup_printf ("Unable to test if folder %s "
				       "exists: %s\n", name,
				       camel_exception_get_description (ex));
		gnome_error_dialog (msg);
		camel_exception_free (ex);
		return FALSE;
	}
	camel_exception_free (ex);
		
	if (!new_folder_exists) {
		gtk_object_unref (GTK_OBJECT (new_folder));
		return FALSE;
	}
	
	if (fb->folder)
		gtk_object_unref (GTK_OBJECT (fb->folder));
	
	fb->folder = new_folder;
	
	message_list_set_folder (fb->message_list, new_folder);

	return TRUE;
}

#define EQUAL(a,b) (strcmp (a,b) == 0)

void
folder_browser_set_uri (FolderBrowser *folder_browser, const char *uri)
{
	if (folder_browser->uri)
		g_free (folder_browser->uri);

	folder_browser->uri = g_strdup (uri);
	folder_browser_load_folder (folder_browser, folder_browser->uri);
}

void
folder_browser_set_message_preview (FolderBrowser *folder_browser, gboolean show_message_preview)
{
	if (folder_browser->preview_shown == show_message_preview)
		return;

	g_warning ("FIXME: implement me");
}

static void
get_prop (BonoboPropertyBag *bag,
	  BonoboArg         *arg,
	  guint              arg_id,
	  gpointer           user_data)
{
	FolderBrowser *fb = user_data;

	switch (arg_id) {

	case PROPERTY_FOLDER_URI_IDX:
		if (fb && fb->uri)
			BONOBO_ARG_SET_STRING (arg, fb->uri);
		else
			BONOBO_ARG_SET_STRING (arg, "");
		break;

	case PROPERTY_MESSAGE_PREVIEW_IDX:
		g_warning ("Implement me; no return value");
		BONOBO_ARG_SET_BOOLEAN (arg, FALSE);
		break;

	default:
		g_warning ("Unhandled arg %d\n", arg_id);
	}
}

static void
set_prop (BonoboPropertyBag *bag,
	  const BonoboArg   *arg,
	  guint              arg_id,
	  gpointer           user_data)
{
	FolderBrowser *fb = user_data;

	switch (arg_id) {

	case PROPERTY_FOLDER_URI_IDX:
		folder_browser_set_uri (fb, BONOBO_ARG_GET_STRING (arg));
		break;

	case PROPERTY_MESSAGE_PREVIEW_IDX:
		folder_browser_set_message_preview (fb, BONOBO_ARG_GET_BOOLEAN (arg));
		break;

	default:
		g_warning ("Unhandled arg %d\n", arg_id);
		break;
	}
}

static void
folder_browser_properties_init (FolderBrowser *fb)
{
	fb->properties = bonobo_property_bag_new (get_prop, set_prop, fb);

	bonobo_property_bag_add (
		fb->properties, PROPERTY_FOLDER_URI, PROPERTY_FOLDER_URI_IDX,
		BONOBO_ARG_STRING, NULL, _("The URI that the Folder Browser will display"), 0);
	bonobo_property_bag_add (
		fb->properties, PROPERTY_MESSAGE_PREVIEW, PROPERTY_MESSAGE_PREVIEW_IDX,
		BONOBO_ARG_BOOLEAN, NULL, _("Whether a message preview should be shown"), 0);
}

static char * search_options[] = {
	"Body or subject contains",
	"Body contains",
	"Subject contains",
	"Body does not contain",
	"Subject does not contain",
	NULL
};

/* %s is replaced by the whole search string in quotes ...
   possibly could split the search string into words as well ? */
static char * search_string[] = {
	"(or (body-contains %s) (match-all (header-contains \"Subject\" %s)))",
	"(body-contains %s)",
	"(match-all (header-contains \"Subject\" %s)",
	"(match-all (not (body-contains %s)))",
	"(match-all (not (header-contains \"Subject\" %s)))"
};

static void
search_set(FolderBrowser *fb)
{
	GtkWidget *widget;
	GString *out;
	char *str;
	int index;
	char *text;

	text = gtk_entry_get_text((GtkEntry *)fb->search_entry);

	if (text == NULL || text[0] == 0) {
		message_list_set_search (fb->message_list, NULL);
		return;
	}

	widget = gtk_menu_get_active (GTK_MENU(GTK_OPTION_MENU(fb->search_menu)->menu));
	index = (int)gtk_object_get_data((GtkObject *)widget, "search_option");
	if (index > sizeof(search_string)/sizeof(search_string[0]))
		index = 0;
	str = search_string[index];

	out = g_string_new("");
	while (*str) {
		if (str[0] == '%' && str[1]=='s') {
			str+=2;
			e_sexp_encode_string(out, text);
		} else {
			g_string_append_c(out, *str);
			str++;
		}
	}
	message_list_set_search (fb->message_list, out->str);
	g_string_free(out, TRUE);
}

static void
search_menu_deactivate(GtkWidget *menu, FolderBrowser *fb)
{
	search_set(fb);
}

static GtkWidget *
create_option_menu (char **menu_list, int item, void *data)
{
	GtkWidget *omenu;
	GtkWidget *menu;
	int i = 0;
       
	omenu = gtk_option_menu_new ();
	menu = gtk_menu_new ();
	while (*menu_list){
		GtkWidget *entry;

		entry = gtk_menu_item_new_with_label (*menu_list);
		gtk_widget_show (entry);
		gtk_object_set_data((GtkObject *)entry, "search_option", (void *)i);
		gtk_menu_append (GTK_MENU (menu), entry);
		menu_list++;
		i++;
	}
	gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu), menu);
	gtk_option_menu_set_history (GTK_OPTION_MENU (omenu), item);
	gtk_widget_show (omenu);

	gtk_signal_connect (GTK_OBJECT (menu), 
			    "deactivate",
			    GTK_SIGNAL_FUNC (search_menu_deactivate), data);

	return omenu;
}

static void
search_activate(GtkEntry *entry, FolderBrowser *fb)
{
	search_set(fb);
}

static int
etable_key (ETable *table, int row, int col, GdkEvent *ev, FolderBrowser *fb)
{
	if (ev->key.state != 0)
		return FALSE;

	if (ev->key.keyval == GDK_space || ev->key.keyval == GDK_BackSpace) {
		GtkAdjustment *vadj;

		vadj = e_scroll_frame_get_vadjustment (fb->mail_display->scroll);
		if (ev->key.keyval == GDK_BackSpace) {
			if (vadj->value > vadj->lower + vadj->page_size)
				vadj->value -= vadj->page_size;
			else
				vadj->value = vadj->lower;
		} else {
			if (vadj->value < vadj->upper - 2 * vadj->page_size)
				vadj->value += vadj->page_size;
			else
				vadj->value = vadj->upper - vadj->page_size;
		}

		gtk_adjustment_value_changed (vadj);
		return TRUE;
	}

	return FALSE;
}

static void
folder_browser_gui_init (FolderBrowser *fb)
{
	GtkWidget *hbox, *label;

	/*
	 * The panned container
	 */
	fb->vpaned = e_vpaned_new ();
	gtk_widget_show (fb->vpaned);

	gtk_table_attach (
		GTK_TABLE (fb), fb->vpaned,
		0, 1, 1, 3,
		GTK_FILL | GTK_EXPAND,
		GTK_FILL | GTK_EXPAND,
		0, 0);

	/* quick-search entry */
	hbox = gtk_hbox_new(FALSE, 3);
	gtk_widget_show(hbox);
	fb->search_entry = gtk_entry_new();
	gtk_widget_show(fb->search_entry);
	gtk_signal_connect(GTK_OBJECT (fb->search_entry), "activate", search_activate, fb);
	/* gtk_signal_connect(fb->search_entry, "changed", search_activate, fb); */
	label = gtk_label_new("Search");
	gtk_widget_show(label);
	fb->search_menu = create_option_menu(search_options, 0, fb);
	gtk_box_pack_end((GtkBox *)hbox, fb->search_entry, FALSE, FALSE, 3);
	gtk_box_pack_end((GtkBox *)hbox, fb->search_menu, FALSE, FALSE, 3);
	gtk_box_pack_end((GtkBox *)hbox, label, FALSE, FALSE, 3);
	gtk_table_attach (
		GTK_TABLE (fb), hbox,
		0, 1, 0, 1,
		GTK_FILL | GTK_EXPAND,
		0,
		0, 0);

	fb->message_list_w = message_list_get_widget (fb->message_list);
	e_paned_add1 (E_PANED (fb->vpaned), fb->message_list_w);
	gtk_widget_show (fb->message_list_w);

	e_paned_add2 (E_PANED (fb->vpaned), GTK_WIDGET (fb->mail_display));
	e_paned_set_position (E_PANED (fb->vpaned), 200);

	gtk_widget_show (GTK_WIDGET (fb->mail_display));
	gtk_widget_show (GTK_WIDGET (fb));

}

static void
folder_browser_init (GtkObject *object)
{
}

static void
my_folder_browser_init (GtkObject *object)
{
	FolderBrowser *fb = FOLDER_BROWSER (object);

	/*
	 * Setup parent class fields.
	 */ 
	GTK_TABLE (fb)->homogeneous = FALSE;
	gtk_table_resize (GTK_TABLE (fb), 1, 2);

	/*
	 * Our instance data
	 */
	fb->message_list = MESSAGE_LIST (message_list_new (fb));
	fb->mail_display = MAIL_DISPLAY (mail_display_new (fb));

	gtk_signal_connect (GTK_OBJECT (fb->message_list->etable),
			    "key_press", GTK_SIGNAL_FUNC (etable_key), fb);

	folder_browser_properties_init (fb);
	folder_browser_gui_init (fb);
}

GtkWidget *
folder_browser_new (void)
{
	FolderBrowser *folder_browser = gtk_type_new (folder_browser_get_type ());

	my_folder_browser_init (GTK_OBJECT (folder_browser));
	folder_browser->uri = NULL;

	return GTK_WIDGET (folder_browser);
}


E_MAKE_TYPE (folder_browser, "FolderBrowser", FolderBrowser, folder_browser_class_init, folder_browser_init, PARENT_TYPE);


