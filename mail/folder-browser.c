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
#include <ctype.h>
#include <gnome.h>
#include "e-util/e-util.h"
#include "e-util/e-sexp.h"
#include "e-util/e-unicode.h"
#include "folder-browser.h"
#include "mail.h"
#include "mail-tools.h"
#include "message-list.h"
#include "mail-threads.h"
#include "mail-ops.h"
#include <widgets/e-paned/e-vpaned.h>

#include "mail-vfolder.h"
#include "filter/vfolder-rule.h"
#include "filter/vfolder-context.h"
#include "filter/filter-option.h"
#include "filter/filter-input.h"

#include "mail-local.h"
#include "mail-config.h"

#define PARENT_TYPE (gtk_table_get_type ())

static void fb_resize_cb (GtkWidget *w, GtkAllocation *a);

static GtkObjectClass *folder_browser_parent_class;

static void oc_destroy (gpointer obj, gpointer user)
{
	struct fb_ondemand_closure *oc = (struct fb_ondemand_closure *) obj;

	g_free (oc->path);
	g_free (oc);
}

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

	if (folder_browser->folder) {
		mail_do_sync_folder (folder_browser->folder);
		camel_object_unref (CAMEL_OBJECT (folder_browser->folder));
	}

	if (folder_browser->message_list)
		bonobo_object_unref (BONOBO_OBJECT (folder_browser->message_list));

	if (folder_browser->mail_display)
		gtk_widget_destroy (GTK_WIDGET (folder_browser->mail_display));

	if (folder_browser->filter_context)
		gtk_object_unref (GTK_OBJECT (folder_browser->filter_context));
	
	if (folder_browser->filter_menu_paths) {
		g_slist_foreach (folder_browser->filter_menu_paths, oc_destroy, NULL);
		g_slist_free (folder_browser->filter_menu_paths);
	}

	folder_browser_parent_class->destroy (object);
}

static void
folder_browser_class_init (GtkObjectClass *object_class)
{
	object_class->destroy = folder_browser_destroy;

	folder_browser_parent_class = gtk_type_class (PARENT_TYPE);
}

/*
 * static gboolean
 * folder_browser_load_folder (FolderBrowser *fb, const char *name)
 * {
 * 	CamelFolder *new_folder;
 * 
 * 	new_folder = mail_tool_uri_to_folder_noex (name);
 * 
 * 	if (!new_folder)
 * 		return FALSE;
 * 
 * 	if (fb->folder)
 * 		camel_object_unref (CAMEL_OBJECT (fb->folder));
 * 	fb->folder = new_folder;
 * 	message_list_set_folder (fb->message_list, new_folder);
 * 	return TRUE;
 * }
 */

#define EQUAL(a,b) (strcmp (a,b) == 0)

gboolean folder_browser_set_uri (FolderBrowser *folder_browser, const char *uri)
{
	mail_do_load_folder (folder_browser, uri);
	return TRUE;
}

void
folder_browser_set_message_preview (FolderBrowser *folder_browser, gboolean show_message_preview)
{
	if (folder_browser->preview_shown == show_message_preview)
		return;

	g_warning ("FIXME: implement me");
}

static char * search_options[] = {
	"Body or subject contains",
	"Body contains",
	"Subject contains",
	"Body does not contain",
	"Subject does not contain",
	NULL
};

/* NOTE: If this is changed, then change the search_save() function to match! */
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

	text = e_utf8_gtk_entry_get_text((GtkEntry *)fb->search_entry);

	if (text == NULL || text[0] == 0) {
		if (text) g_free (text);
		mail_do_regenerate_messagelist (fb->message_list, NULL);
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
	mail_do_regenerate_messagelist (fb->message_list, out->str);
	g_string_free(out, TRUE);

	g_free (text);
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

		entry = e_utf8_gtk_menu_item_new_with_label (*menu_list);
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

static void
search_save(GtkWidget *w, FolderBrowser *fb)
{
	GtkWidget *widget;
	int index;
	char *text;
	FilterElement *element;
	VfolderRule *rule;
	FilterPart *part;

	text = e_utf8_gtk_entry_get_text((GtkEntry *)fb->search_entry);

	if (text == NULL || text[0] == 0) {
		if (text) g_free (text);
		return;
	}

	widget = gtk_menu_get_active (GTK_MENU(GTK_OPTION_MENU(fb->search_menu)->menu));
	index = (int)gtk_object_get_data((GtkObject *)widget, "search_option");
	rule = vfolder_rule_new();
	((FilterRule *)rule)->grouping = FILTER_GROUP_ANY;
	vfolder_rule_add_source(rule, fb->uri);
	filter_rule_set_name((FilterRule *)rule, text);
	switch(index) {
	default:		/* header or body contains */
		index = 0;
	case 1: case 2:
		if (index == 0 || index == 1) {	/* body-contains */
			part = vfolder_create_part("body");
			filter_rule_add_part((FilterRule *)rule, part);
			element = filter_part_find_element(part, "body-type");
			filter_option_set_current((FilterOption *)element, "contains");
			element = filter_part_find_element(part, "word");
			filter_input_set_value((FilterInput *)element, text);
		}
		if (index == 0 || index == 2) {	/* subject contains */
			part = vfolder_create_part("subject");
			filter_rule_add_part((FilterRule *)rule, part);
			element = filter_part_find_element(part, "subject-type");
			filter_option_set_current((FilterOption *)element, "contains");
			element = filter_part_find_element(part, "subject");
			filter_input_set_value((FilterInput *)element, text);
		}
		break;
	case 3:			/* not body contains */
		part = vfolder_create_part("body");
		filter_rule_add_part((FilterRule *)rule, part);
		element = filter_part_find_element(part, "body-type");
		filter_option_set_current((FilterOption *)element, "not contains");
		element = filter_part_find_element(part, "word");
		filter_input_set_value((FilterInput *)element, text);
		break;
	case 4:			/* not header contains */
		part = vfolder_create_part("subject");
		filter_rule_add_part((FilterRule *)rule, part);
		element = filter_part_find_element(part, "subject-type");
		filter_option_set_current((FilterOption *)element, "not contains");
		element = filter_part_find_element(part, "subject");
		filter_input_set_value((FilterInput *)element, text);
		break;
	}

	vfolder_gui_add_rule(rule);

	g_free (text);
}

void
folder_browser_clear_search (FolderBrowser *fb)
{
	gtk_entry_set_text (GTK_ENTRY (fb->search_entry), "");
	gtk_option_menu_set_history (GTK_OPTION_MENU (fb->search_menu), 0);
	mail_do_regenerate_messagelist (fb->message_list, NULL);
}

static int
etable_key (ETable *table, int row, int col, GdkEvent *ev, FolderBrowser *fb)
{
	if ((ev->key.state & !(GDK_SHIFT_MASK | GDK_LOCK_MASK)) != 0)
		return FALSE;

	if (ev->key.keyval == GDK_space || ev->key.keyval == GDK_BackSpace) {
		GtkAdjustment *vadj;
		gfloat page_size;

		vadj = e_scroll_frame_get_vadjustment (fb->mail_display->scroll);
		page_size = vadj->page_size - vadj->step_increment;

		if (ev->key.keyval == GDK_BackSpace) {
			if (vadj->value > vadj->lower + page_size)
				vadj->value -= page_size;
			else
				vadj->value = vadj->lower;
		} else {
			if (vadj->value < vadj->upper - vadj->page_size - page_size)
				vadj->value += page_size;
			else
				vadj->value = vadj->upper - vadj->page_size;
		}

		gtk_adjustment_value_changed (vadj);
		return TRUE;
	} else if (ev->key.keyval == GDK_Delete ||
		   ev->key.keyval == GDK_KP_Delete) {
		delete_msg (NULL, fb);
		message_list_select (fb->message_list, row,
				     MESSAGE_LIST_SELECT_NEXT,
				     0, CAMEL_MESSAGE_DELETED);
		return TRUE;
	} else if (ev->key.keyval == 'n' || ev->key.keyval == 'N' ||
		   ev->key.keyval == 'p' || ev->key.keyval == 'P') {
		message_list_select (fb->message_list, row,
				     tolower (ev->key.keyval) == 'p' ?
				     MESSAGE_LIST_SELECT_PREVIOUS :
				     MESSAGE_LIST_SELECT_NEXT,
				     0, CAMEL_MESSAGE_SEEN);
	}

	return FALSE;
}

static void
folder_browser_gui_init (FolderBrowser *fb)
{
	GtkWidget *hbox, *label;
	GtkButton *button;

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
	label = gtk_label_new(_("Search"));
	gtk_widget_show(label);
	fb->search_menu = create_option_menu(search_options, 0, fb);
	button = (GtkButton *)gtk_button_new_with_label(_("Save"));
	gtk_widget_show((GtkWidget *)button);
	gtk_signal_connect((GtkObject *)button, "clicked", search_save, fb);
	gtk_box_pack_end((GtkBox *)hbox, (GtkWidget *)button, FALSE, FALSE, 3);
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

	/* (jwise) <-- for searching purposes :) */
	gtk_signal_connect (GTK_OBJECT (fb->message_list_w), "size_allocate",
	                    GTK_SIGNAL_FUNC (fb_resize_cb), NULL);

	e_paned_add2 (E_PANED (fb->vpaned), GTK_WIDGET (fb->mail_display));
	e_paned_set_position (E_PANED (fb->vpaned), mail_config_paned_size());
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
	fb->mail_display = MAIL_DISPLAY (mail_display_new ());

	gtk_signal_connect (GTK_OBJECT (fb->message_list->etable),
			    "key_press", GTK_SIGNAL_FUNC (etable_key), fb);

	fb->filter_menu_paths = NULL;
	fb->filter_context = NULL;

	folder_browser_gui_init (fb);
}

GtkWidget *
folder_browser_new (Evolution_Shell shell)
{
	static int serial;
	FolderBrowser *folder_browser = gtk_type_new (folder_browser_get_type ());

	my_folder_browser_init (GTK_OBJECT (folder_browser));
	folder_browser->uri = NULL;
	folder_browser->serial = serial++;
	folder_browser->shell = shell;

	return GTK_WIDGET (folder_browser);
}


E_MAKE_TYPE (folder_browser, "FolderBrowser", FolderBrowser, folder_browser_class_init, folder_browser_init, PARENT_TYPE);

static void fb_resize_cb (GtkWidget *w, GtkAllocation *a)
{
	mail_config_set_paned_size (a->height + 90);
}
