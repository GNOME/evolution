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
#include "e-util/e-sexp.h"
#include "folder-browser.h"
#include "mail.h"
#include "mail-callbacks.h"
#include "mail-tools.h"
#include "message-list.h"
#include "mail-threads.h"
#include "mail-ops.h"
#include "mail-vfolder.h"
#include "mail-autofilter.h"
#include "mail-mlist-magic.h"

#include <gal/util/e-util.h>
#include <gal/widgets/e-unicode.h>
#include <gal/e-paned/e-vpaned.h>

#include "filter/vfolder-rule.h"
#include "filter/vfolder-context.h"
#include "filter/filter-option.h"
#include "filter/filter-input.h"

#include "mail-search-dialogue.h"

#include "mail-local.h"
#include "mail-config.h"

#include <gal/widgets/e-popup-menu.h>

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
	FolderBrowser *folder_browser;
	CORBA_Environment ev;

	folder_browser = FOLDER_BROWSER (object);

	CORBA_exception_init (&ev);
	
	if (folder_browser->search_full)
		gtk_object_unref((GtkObject *)folder_browser->search_full);
	
	if (folder_browser->shell != CORBA_OBJECT_NIL)
		CORBA_Object_release (folder_browser->shell, &ev);
	
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

	CORBA_exception_free (&ev);

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
	if (*uri)
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
	N_("Body or subject contains"),
	N_("Body contains"),
	N_("Subject contains"),
	N_("Body does not contain"),
	N_("Subject does not contain"),
	N_("Custom search"),
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
	"(match-all (not (header-contains \"Subject\" %s)))",
	"%s",
};
#define CUSTOM_SEARCH_ID (5)

static void
search_full_clicked(MailSearchDialogue *msd, guint button, FolderBrowser *fb)
{
	char *query;

	switch (button) {
	case 0:			/* 'ok' */
	case 1:			/* 'search' */
		query = mail_search_dialogue_get_query(msd);
		message_list_set_search(fb->message_list, query);
		g_free(query);
		/* save the search as well */
		if (fb->search_full)
			gtk_object_unref((GtkObject *)fb->search_full);
		fb->search_full = msd->rule;
		gtk_object_ref((GtkObject *)fb->search_full);
		if (button == 0)
			gnome_dialog_close((GnomeDialog *)msd);
		break;
	case 2:			/* 'cancel' */
		gnome_dialog_close((GnomeDialog *)msd);
	case -1:		/* dialogue closed */
		message_list_set_search(fb->message_list, 0);
		/* reset the search buttons state */
		gtk_menu_set_active(GTK_MENU(GTK_OPTION_MENU(fb->search_menu)->menu), 0);
		gtk_widget_set_sensitive(fb->search_entry, TRUE);
		break;
	}
}

/* bring up the 'full search' dialogue and let the user use that to search with */
static void
search_full(GtkWidget *w, FolderBrowser *fb)
{
	MailSearchDialogue *msd;

	/* make search dialogue thingy match */
	gtk_menu_set_active(GTK_MENU(GTK_OPTION_MENU(fb->search_menu)->menu), CUSTOM_SEARCH_ID);
	gtk_widget_set_sensitive(fb->search_entry, FALSE);

	msd = mail_search_dialogue_new_with_rule(fb->search_full);
	gtk_signal_connect((GtkObject *)msd, "clicked", search_full_clicked, fb);
	gtk_widget_show((GtkWidget*)msd);
}

static void
search_set(FolderBrowser *fb)
{
	GtkWidget *widget;
	GString *out;
	char *str;
	int index;
	char *text;

	widget = gtk_menu_get_active (GTK_MENU(GTK_OPTION_MENU(fb->search_menu)->menu));
	index = (int)gtk_object_get_data((GtkObject *)widget, "search_option");
	if (index == CUSTOM_SEARCH_ID) {
		search_full(NULL, fb);
		return;
	}
	gtk_widget_set_sensitive(fb->search_entry, TRUE);

	text = e_utf8_gtk_entry_get_text((GtkEntry *)fb->search_entry);

	if (text == NULL || text[0] == 0) {
		if (text) 
			g_free(text);
		message_list_set_search(fb->message_list, NULL);
		return;
	}

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
	message_list_set_search(fb->message_list, out->str);
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

		/* We don't use e_utf8_gtk_menu_item_new_with_label here
		 * because the string comes from gettext and so is localized,
		 * not UTF-8.
		 */
		entry = gtk_menu_item_new_with_label (_(*menu_list));
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

	widget = gtk_menu_get_active (GTK_MENU(GTK_OPTION_MENU(fb->search_menu)->menu));
	index = (int)gtk_object_get_data((GtkObject *)widget, "search_option");

	/* some special case code for the custom search position */
	if (index == CUSTOM_SEARCH_ID) {
		g_free(text);
		text = g_strdup(_("Custom"));
	} else {
		if (text == NULL || text[0] == 0) {
			g_free (text);
			return;
		}
	}

	rule = vfolder_rule_new();
	((FilterRule *)rule)->grouping = FILTER_GROUP_ANY;
	vfolder_rule_add_source(rule, fb->uri);
	filter_rule_set_name((FilterRule *)rule, text);
	switch(index) {
	case 5:			/* custom search */
		if (fb->search_full) {
			GList *partl;

			/* copy the parts from the search rule to the vfolder rule */
			partl = fb->search_full->parts;
			while (partl) {
				FilterPart *old = partl->data;
				part = filter_part_clone(old);
				filter_rule_add_part((FilterRule *)rule, part);
				partl = g_list_next(partl);
			}
			break;
		}
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
	message_list_set_search(fb->message_list, NULL);
}

void
folder_browser_toggle_threads (BonoboUIComponent           *component,
			       const char                  *path,
			       Bonobo_UIComponent_EventType type,
			       const char                  *state,
			       gpointer                     user_data)
{
	FolderBrowser *fb = user_data;

	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;
	
	mail_config_set_thread_list(atoi(state));
	message_list_set_threaded(fb->message_list, atoi(state));
}

void
vfolder_subject(GtkWidget *w, FolderBrowser *fb)
{
	vfolder_gui_add_from_message(fb->mail_display->current_message, AUTO_SUBJECT,
				     fb->uri);
}

void
vfolder_sender(GtkWidget *w, FolderBrowser *fb)
{
	vfolder_gui_add_from_message(fb->mail_display->current_message, AUTO_FROM,
				     fb->uri);
}

void
vfolder_recipient(GtkWidget *w, FolderBrowser *fb)
{
	vfolder_gui_add_from_message(fb->mail_display->current_message, AUTO_TO,
				     fb->uri);
}

void
filter_subject(GtkWidget *w, FolderBrowser *fb)
{
	filter_gui_add_from_message(fb->mail_display->current_message, AUTO_SUBJECT);
}

void
filter_sender(GtkWidget *w, FolderBrowser *fb)
{
	filter_gui_add_from_message(fb->mail_display->current_message, AUTO_FROM);
}

void
filter_recipient(GtkWidget *w, FolderBrowser *fb)
{
	filter_gui_add_from_message(fb->mail_display->current_message, AUTO_TO);
}

void
filter_mlist (GtkWidget *w, FolderBrowser *fb)
{
	char *name;
	char *header_value;
	const char *header_name;

	name = mail_mlist_magic_detect_list (fb->mail_display->current_message, &header_name, &header_value);
	if (name == NULL)
		return;

	filter_gui_add_for_mailing_list (fb->mail_display->current_message, name, header_name, header_value);

	g_free (name);
	g_free (header_value);
}

/* handle context menu over message-list */
static gint
on_right_click (ETableScrolled *table, gint row, gint col, GdkEvent *event, FolderBrowser *fb)
{
	extern CamelFolder *drafts_folder;
	const CamelMessageInfo *info;
	int enable_mask = 0;
	int last_item;
	char *mailing_list_name;
	EPopupMenu menu[] = {
		{ _("Open"),                       NULL, GTK_SIGNAL_FUNC (view_msg),          0 },
		{ _("Edit"),                       NULL, GTK_SIGNAL_FUNC (edit_msg),          1 },
		{ _("Print"),                      NULL, GTK_SIGNAL_FUNC (print_msg),         0 },
		{ "",                              NULL, GTK_SIGNAL_FUNC (NULL),              0 },
		{ _("Reply to Sender"),            NULL, GTK_SIGNAL_FUNC (reply_to_sender),   0 },
		{ _("Reply to All"),               NULL, GTK_SIGNAL_FUNC (reply_to_all),      0 },
		{ _("Forward"),                    NULL, GTK_SIGNAL_FUNC (forward_msg),       0 },
		/*{ _("Forward as Attachment"),      NULL, GTK_SIGNAL_FUNC (forward_msg),       0 },*/
		{ "",                              NULL, GTK_SIGNAL_FUNC (NULL),              0 },
		{ _("Mark as Read"),               NULL, GTK_SIGNAL_FUNC (mark_as_seen),      4 },
		{ _("Mark as Unread"),             NULL, GTK_SIGNAL_FUNC (mark_as_unseen),    8 },
		{ "",                              NULL, GTK_SIGNAL_FUNC (NULL),              0 },
		{ _("Move to Folder..."),          NULL, GTK_SIGNAL_FUNC (move_msg),          0 },
		{ _("Copy to Folder..."),          NULL, GTK_SIGNAL_FUNC (copy_msg),          0 },
		{ _("Delete"),                     NULL, GTK_SIGNAL_FUNC (delete_msg),       16 },
		{ _("Undelete"),                   NULL, GTK_SIGNAL_FUNC (undelete_msg),     32 },
		{ "",                              NULL, GTK_SIGNAL_FUNC (NULL),              0 },
		/*{ _("Add Sender to Address Book"), NULL, GTK_SIGNAL_FUNC (addrbook_sender),   0 },
		  { "",                              NULL, GTK_SIGNAL_FUNC (NULL),              0 },*/
		{ _("Apply Filters"),              NULL, GTK_SIGNAL_FUNC (apply_filters),     0 },
		{ "",                              NULL, GTK_SIGNAL_FUNC (NULL),              0 },
		{ _("VFolder on Subject"),         NULL, GTK_SIGNAL_FUNC (vfolder_subject),   2 },
		{ _("VFolder on Sender"),          NULL, GTK_SIGNAL_FUNC (vfolder_sender),    2 },
		{ _("VFolder on Recipients"),      NULL, GTK_SIGNAL_FUNC (vfolder_recipient), 2 },
		{ "",                              NULL, GTK_SIGNAL_FUNC (NULL),              0 },
		{ _("Filter on Subject"),          NULL, GTK_SIGNAL_FUNC (filter_subject),    2 },
		{ _("Filter on Sender"),           NULL, GTK_SIGNAL_FUNC (filter_sender),     2 },
		{ _("Filter on Recipients"),       NULL, GTK_SIGNAL_FUNC (filter_recipient),  2 },
		{ _("Filter on Mailing List"),     NULL, GTK_SIGNAL_FUNC (filter_mlist),     66 },
		{ NULL,                            NULL, NULL,                                0 }
	};
	
	/* Evil Hack.  */
	
	last_item = (sizeof (menu) / sizeof (*menu)) - 2;
	
	if (fb->folder != drafts_folder)
		enable_mask |= 1;
	
	if (fb->mail_display->current_message == NULL) {
		enable_mask |= 2;
		mailing_list_name = NULL;
	} else {
		mailing_list_name = mail_mlist_magic_detect_list (fb->mail_display->current_message,
								  NULL, NULL);
	}
	
	/* FIXME: the following 2 should really account for when multiple messages are selected. */
	info = camel_folder_get_message_info (fb->folder, fb->message_list->cursor_uid);
	if (info->flags & CAMEL_MESSAGE_SEEN)
		enable_mask |= 4;
	else
		enable_mask |= 8;
	
	if (info->flags & CAMEL_MESSAGE_DELETED)
		enable_mask |= 16;
	else
		enable_mask |= 32;
	
	
	if (mailing_list_name == NULL) {
		enable_mask |= 64;
		menu[last_item].name = g_strdup (_("Filter on Mailing List"));
	} else {
		menu[last_item].name = g_strdup_printf (_("Filter on Mailing List (%s)"),
							mailing_list_name);
	}
	
	e_popup_menu_run (menu, (GdkEventButton *)event, enable_mask, 0, fb);
	
	g_free (menu[last_item].name);
	
	return TRUE;
}

static int
etable_key (ETable *table, int row, int col, GdkEvent *ev, FolderBrowser *fb)
{
	if ((ev->key.state & !(GDK_SHIFT_MASK | GDK_LOCK_MASK)) != 0)
		return FALSE;

	switch (ev->key.keyval) {
	case GDK_space:
	case GDK_BackSpace:
	{
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
	}

	case GDK_Delete:
	case GDK_KP_Delete:
		delete_msg (NULL, fb);
		message_list_select (fb->message_list, row,
				     MESSAGE_LIST_SELECT_NEXT,
				     0, CAMEL_MESSAGE_DELETED);
		return TRUE;

	case GDK_Home:
	case GDK_KP_Home:
		message_list_select(fb->message_list, 0, MESSAGE_LIST_SELECT_NEXT, 0, 0);
		return TRUE;

	case GDK_End:
	case GDK_KP_End:
		message_list_select(fb->message_list, -1, MESSAGE_LIST_SELECT_PREVIOUS, 0, 0);
		return TRUE;

	case 'n':
	case 'N':
		message_list_select (fb->message_list, row,
				     MESSAGE_LIST_SELECT_NEXT,
				     0, CAMEL_MESSAGE_SEEN);
		return TRUE;

	case 'p':
	case 'P':
		message_list_select (fb->message_list, row,
				     MESSAGE_LIST_SELECT_PREVIOUS,
				     0, CAMEL_MESSAGE_SEEN);
		return TRUE;

	default:
		return FALSE;
	}

	return FALSE;
}

static void
on_double_click (ETableScrolled *table, gint row, FolderBrowser *fb)
{
	view_msg (NULL, fb);
}

static void
folder_browser_gui_init (FolderBrowser *fb)
{
	GtkWidget *hbox, *label;
	GtkButton *button, *searchbutton;
	GtkWidget *search_alignment, *save_alignment;

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
	search_alignment = gtk_alignment_new(.5, .5, 0, 0);
	gtk_widget_show(search_alignment);
	searchbutton = (GtkButton *)gtk_button_new_with_label(_("Full Search"));
	gtk_widget_show((GtkWidget *)searchbutton);
	label = gtk_label_new(_("Search"));
	gtk_widget_show(label);
	fb->search_menu = create_option_menu(search_options, 0, fb);
	button = (GtkButton *)gtk_button_new_with_label(_("Save"));
	gtk_widget_show((GtkWidget *)button);
	save_alignment = gtk_alignment_new(.5, .5, 0, 0);
	gtk_widget_show(save_alignment);
	gtk_signal_connect((GtkObject *)button, "clicked", search_save, fb);
	gtk_signal_connect((GtkObject *)searchbutton, "clicked", search_full, fb);
	gtk_container_add(GTK_CONTAINER(save_alignment), GTK_WIDGET(button));
	gtk_box_pack_end((GtkBox *)hbox, save_alignment, FALSE, FALSE, 3);
	gtk_box_pack_end((GtkBox *)hbox, fb->search_entry, FALSE, FALSE, 3);
	gtk_box_pack_end((GtkBox *)hbox, fb->search_menu, FALSE, FALSE, 3);
	gtk_box_pack_end((GtkBox *)hbox, label, FALSE, FALSE, 3);
	gtk_container_add(GTK_CONTAINER(search_alignment), GTK_WIDGET(searchbutton));
	gtk_box_pack_end((GtkBox *)hbox, search_alignment, FALSE, FALSE, 3);
	gtk_table_attach (
		GTK_TABLE (fb), hbox,
		0, 1, 0, 1,
		GTK_FILL | GTK_EXPAND,
		0,
		0, 0);

	fb->message_list_w = message_list_get_widget (fb->message_list);
	e_paned_add1 (E_PANED (fb->vpaned), fb->message_list_w);
	gtk_widget_show (fb->message_list_w);

	gtk_signal_connect (GTK_OBJECT (fb->message_list_w), "size_allocate",
	                    GTK_SIGNAL_FUNC (fb_resize_cb), NULL);

	e_paned_add2 (E_PANED (fb->vpaned), GTK_WIDGET (fb->mail_display));
	e_paned_set_position (E_PANED (fb->vpaned), mail_config_paned_size());
	gtk_widget_show (GTK_WIDGET (fb->mail_display));
	gtk_widget_show (GTK_WIDGET (fb));
}

static gint 
mark_msg_seen (gpointer data)
{
	MessageList *ml = data;
	GPtrArray *uids;

	if (!ml->cursor_uid) 
		return FALSE;

	uids = g_ptr_array_new ();
	g_ptr_array_add (uids, g_strdup (ml->cursor_uid));
	mail_do_flag_messages (ml->folder, uids, FALSE,
			       CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);
	return FALSE;
}

static void
on_message_selected(MessageList *ml, const char *uid, FolderBrowser *fb)
{
	printf("selecting uid %s\n", uid);
	mail_do_display_message(ml, fb->mail_display, uid, mark_msg_seen);
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
	fb->message_list = (MessageList *)message_list_new ();
	fb->mail_display = (MailDisplay *)mail_display_new ();

	gtk_signal_connect (GTK_OBJECT (fb->message_list->etable),
			    "key_press", GTK_SIGNAL_FUNC (etable_key), fb);

	gtk_signal_connect (GTK_OBJECT (fb->message_list->etable), "right_click",
			    GTK_SIGNAL_FUNC (on_right_click), fb);
	
	gtk_signal_connect (GTK_OBJECT (fb->message_list->etable), "double_click",
			    GTK_SIGNAL_FUNC (on_double_click), fb);

	gtk_signal_connect (GTK_OBJECT(fb->message_list), "message_selected",
			    on_message_selected, fb);

	fb->filter_menu_paths = NULL;
	fb->filter_context = NULL;

	folder_browser_gui_init (fb);
}

GtkWidget *
folder_browser_new (const Evolution_Shell shell)
{
	static int serial = 0;
	CORBA_Environment ev;
	FolderBrowser *folder_browser;

	CORBA_exception_init (&ev);

	folder_browser = gtk_type_new (folder_browser_get_type ());

	my_folder_browser_init (GTK_OBJECT (folder_browser));
	folder_browser->uri = NULL;
	folder_browser->serial = serial++;

	folder_browser->shell = CORBA_Object_duplicate (shell, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		folder_browser->shell = CORBA_OBJECT_NIL;
		gtk_widget_destroy (GTK_WIDGET (folder_browser));
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	return GTK_WIDGET (folder_browser);
}


E_MAKE_TYPE (folder_browser, "FolderBrowser", FolderBrowser, folder_browser_class_init, folder_browser_init, PARENT_TYPE);

static void fb_resize_cb (GtkWidget *w, GtkAllocation *a)
{
	mail_config_set_paned_size (a->height);
}
