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

#define d(x) x

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
		gtk_object_unref (GTK_OBJECT (folder_browser->search_full));
	
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

gboolean
folder_browser_set_uri (FolderBrowser *folder_browser, const char *uri)
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

enum {
	ESB_SHOW_ALL,
	ESB_ADVANCED,
	ESB_SAVE,
};

static ESearchBarItem folder_browser_search_menu_items[] = {
	{ N_("Show All"),    ESB_SHOW_ALL },
	{ NULL,              0            },
	{ N_("Advanced..."), ESB_ADVANCED },
	{ NULL,              0            },
	{ N_("Save Search"), ESB_SAVE     },
	{ NULL,              -1           }
};

enum {
	ESB_BODY_SUBJECT_CONTAINS,
	ESB_BODY_CONTAINS,
	ESB_SUBJECT_CONTAINS,
	ESB_BODY_DOES_NOT_CONTAIN,
	ESB_SUBJECT_DOES_NOT_CONTAIN,
	ESB_CUSTOM_SEARCH,
};

static ESearchBarItem folder_browser_search_option_items[] = {
	{ N_("Body or subject contains"), ESB_BODY_SUBJECT_CONTAINS    },
	{ N_("Body contains"),            ESB_BODY_CONTAINS            },
        { N_("Subject contains"),         ESB_SUBJECT_CONTAINS         },
	{ N_("Body does not contain"),    ESB_BODY_DOES_NOT_CONTAIN    },
	{ N_("Subject does not contain"), ESB_SUBJECT_DOES_NOT_CONTAIN },
	{ N_("Custom search"),            ESB_CUSTOM_SEARCH            },
	{ NULL,                           -1                           }
};

/* NOTE: If this is changed, then change the search_save() function to match! */
/* %s is replaced by the whole search string in quotes ...
   possibly could split the search string into words as well ? */
static char *search_string[] = {
	"(or (body-contains %s) (match-all (header-contains \"Subject\" %s)))",
	"(body-contains %s)",
	"(match-all (header-contains \"Subject\" %s)",
	"(match-all (not (body-contains %s)))",
	"(match-all (not (header-contains \"Subject\" %s)))",
	"%s",
};

static void
search_full_clicked (MailSearchDialogue *msd, guint button, FolderBrowser *fb)
{
	char *query;
	
	switch (button) {
	case 0:			/* 'ok' */
	case 1:			/* 'search' */
		query = mail_search_dialogue_get_query (msd);
		message_list_set_search (fb->message_list, query);
		g_free (query);
		
		/* save the search as well */
		if (fb->search_full)
			gtk_object_unref (GTK_OBJECT (fb->search_full));
		
		fb->search_full = msd->rule;
		
		gtk_object_ref (GTK_OBJECT (fb->search_full));
		if (button == 0)
			gnome_dialog_close (GNOME_DIALOG (msd));
		break;
	case 2:			/* 'cancel' */
		gnome_dialog_close (GNOME_DIALOG (msd));
	case -1:		/* dialogue closed */
		message_list_set_search (fb->message_list, 0);
		/* reset the search buttons state */
		gtk_menu_set_active (GTK_MENU (GTK_OPTION_MENU (fb->search->option)->menu), 0);
		gtk_widget_set_sensitive (fb->search->entry, TRUE);
		break;
	}
}

/* bring up the 'full search' dialogue and let the user use that to search with */
static void
search_full (GtkWidget *w, FolderBrowser *fb)
{
	MailSearchDialogue *msd;
	
	/* make search dialogue thingy match */
	gtk_menu_set_active (GTK_MENU (GTK_OPTION_MENU (fb->search->option)->menu), ESB_CUSTOM_SEARCH);
	gtk_widget_set_sensitive (fb->search->entry, FALSE);
	
	msd = mail_search_dialogue_new_with_rule (fb->search_full);
	gtk_signal_connect (GTK_OBJECT (msd), "clicked", search_full_clicked, fb);
	gtk_widget_show (GTK_WIDGET (msd));
}

static void
search_save (GtkWidget *w, FolderBrowser *fb)
{
	char *text;
	FilterElement *element;
	VfolderRule *rule;
	FilterPart *part;
	int index;
	
	text = e_utf8_gtk_entry_get_text (GTK_ENTRY (fb->search->entry));
	
	index = fb->search->option_choice;
	
	/* some special case code for the custom search position */
	if (index == ESB_CUSTOM_SEARCH) {
		g_free (text);
		text = g_strdup (_("Custom"));
	} else {
		if (text == NULL || text[0] == 0) {
			g_free (text);
			return;
		}
	}
	
	rule = vfolder_rule_new ();
	((FilterRule *)rule)->grouping = FILTER_GROUP_ANY;
	vfolder_rule_add_source (rule, fb->uri);
	filter_rule_set_name ((FilterRule *)rule, text);
	
	switch (index) {
	case ESB_CUSTOM_SEARCH:
		if (fb->search_full) {
			GList *partl;
			
			/* copy the parts from the search rule to the vfolder rule */
			partl = fb->search_full->parts;
			while (partl) {
				FilterPart *old = partl->data;
				part = filter_part_clone (old);
				filter_rule_add_part ((FilterRule *)rule, part);
				partl = g_list_next (partl);
			}
			break;
		}
	default:
		/* header or body contains */
		index = ESB_BODY_SUBJECT_CONTAINS;
	case ESB_BODY_CONTAINS:
	case ESB_SUBJECT_CONTAINS:
		if (index == ESB_BODY_SUBJECT_CONTAINS || index == ESB_BODY_CONTAINS) {
			part = vfolder_create_part ("body");
			filter_rule_add_part ((FilterRule *)rule, part);
			element = filter_part_find_element (part, "body-type");
			filter_option_set_current ((FilterOption *)element, "contains");
			element = filter_part_find_element (part, "word");
			filter_input_set_value ((FilterInput *)element, text);
		}
		
		if (index == ESB_BODY_SUBJECT_CONTAINS || index == ESB_SUBJECT_CONTAINS) {
			part = vfolder_create_part ("subject");
			filter_rule_add_part ((FilterRule *)rule, part);
			element = filter_part_find_element (part, "subject-type");
			filter_option_set_current ((FilterOption *)element, "contains");
			element = filter_part_find_element (part, "subject");
			filter_input_set_value ((FilterInput *)element, text);
		}
		break;
	case ESB_BODY_DOES_NOT_CONTAIN:
		part = vfolder_create_part ("body");
		filter_rule_add_part ((FilterRule *)rule, part);
		element = filter_part_find_element (part, "body-type");
		filter_option_set_current ((FilterOption *)element, "not contains");
		element = filter_part_find_element (part, "word");
		filter_input_set_value ((FilterInput *)element, text);
		break;
	case ESB_SUBJECT_DOES_NOT_CONTAIN:
		part = vfolder_create_part ("subject");
		filter_rule_add_part ((FilterRule *)rule, part);
		element = filter_part_find_element (part, "subject-type");
		filter_option_set_current ((FilterOption *)element, "not contains");
		element = filter_part_find_element (part, "subject");
		filter_input_set_value ((FilterInput *)element, text);
		break;
	}
	
	vfolder_gui_add_rule (rule);
	
	g_free (text);
}

static void
folder_browser_search_menu_activated (ESearchBar *esb, int id, FolderBrowser *fb)
{
	switch (id) {
	case ESB_SHOW_ALL:
		gtk_entry_set_text (GTK_ENTRY (esb->entry), "");
		message_list_set_search (fb->message_list, NULL);
		break;
	case ESB_ADVANCED:
		search_full (NULL, fb);
		break;
	case ESB_SAVE:
		search_save (NULL, fb);
		break;
	}
}

static void
folder_browser_search_query_changed (ESearchBar *esb, FolderBrowser *fb)
{
	GString *search_query;
	char *search_word, *str;
	int search_type;
	
	gtk_object_get (GTK_OBJECT (esb),
			"text", &search_word,
			"option_choice", &search_type,
			NULL);
	
	if (search_word && strlen (search_word)) {
		str = search_string[search_type];
		
		search_query = g_string_new ("");
		while (*str) {
			if (str[0] == '%' && str[1]=='s') {
				str += 2;
				e_sexp_encode_string (search_query, search_word);
			} else {
				g_string_append_c (search_query, *str);
				str++;
			}
		}
		
		message_list_set_search (fb->message_list, search_query->str);
		g_string_free (search_query, TRUE);
	} else {
		message_list_set_search (fb->message_list, NULL);
	}
	
	g_free (search_word);
}

void
folder_browser_clear_search (FolderBrowser *fb)
{
	gtk_entry_set_text (GTK_ENTRY (fb->search->entry), "");
	gtk_option_menu_set_history (GTK_OPTION_MENU (fb->search->option), 0);
	
	message_list_set_search (fb->message_list, NULL);
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
	
	mail_config_set_thread_list (atoi (state));
	message_list_set_threaded (fb->message_list, atoi (state));
}

void
vfolder_subject (GtkWidget *w, FolderBrowser *fb)
{
	vfolder_gui_add_from_message (fb->mail_display->current_message, AUTO_SUBJECT,
				      fb->uri);
}

void
vfolder_sender (GtkWidget *w, FolderBrowser *fb)
{
	vfolder_gui_add_from_message (fb->mail_display->current_message, AUTO_FROM,
				      fb->uri);
}

void
vfolder_recipient (GtkWidget *w, FolderBrowser *fb)
{
	vfolder_gui_add_from_message (fb->mail_display->current_message, AUTO_TO,
				      fb->uri);
}

void
filter_subject (GtkWidget *w, FolderBrowser *fb)
{
	filter_gui_add_from_message (fb->mail_display->current_message, AUTO_SUBJECT);
}

void
filter_sender (GtkWidget *w, FolderBrowser *fb)
{
	filter_gui_add_from_message (fb->mail_display->current_message, AUTO_FROM);
}

void
filter_recipient (GtkWidget *w, FolderBrowser *fb)
{
	filter_gui_add_from_message (fb->mail_display->current_message, AUTO_TO);
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
	GPtrArray *uids;
	int enable_mask = 0;
	int last_item, i;
	char *mailing_list_name;
	EPopupMenu filter_menu[] = {
		{ _("VFolder on Subject"),         NULL, GTK_SIGNAL_FUNC (vfolder_subject),   NULL,  2 },
		{ _("VFolder on Sender"),          NULL, GTK_SIGNAL_FUNC (vfolder_sender),    NULL,  2 },
		{ _("VFolder on Recipients"),      NULL, GTK_SIGNAL_FUNC (vfolder_recipient), NULL,  2 },
		{ "",                              NULL, GTK_SIGNAL_FUNC (NULL),              NULL,  0 },
		{ _("Filter on Subject"),          NULL, GTK_SIGNAL_FUNC (filter_subject),    NULL,  2 },
		{ _("Filter on Sender"),           NULL, GTK_SIGNAL_FUNC (filter_sender),     NULL,  2 },
		{ _("Filter on Recipients"),       NULL, GTK_SIGNAL_FUNC (filter_recipient),  NULL,  2 },
		{ _("Filter on Mailing List"),     NULL, GTK_SIGNAL_FUNC (filter_mlist),      NULL, 66 },
		{ NULL,                            NULL, NULL,                                NULL,  0 }
	};
	EPopupMenu menu[] = {
		{ _("Open"),                       NULL, GTK_SIGNAL_FUNC (view_msg),          NULL,  0 },
		{ _("Edit"),                       NULL, GTK_SIGNAL_FUNC (edit_msg),          NULL,  1 },
		{ _("Print"),                      NULL, GTK_SIGNAL_FUNC (print_msg),         NULL,  0 },
		{ "",                              NULL, GTK_SIGNAL_FUNC (NULL),              NULL,  0 },
		{ _("Reply to Sender"),            NULL, GTK_SIGNAL_FUNC (reply_to_sender),   NULL,  0 },
		{ _("Reply to All"),               NULL, GTK_SIGNAL_FUNC (reply_to_all),      NULL,  0 },
		{ _("Forward"),                    NULL, GTK_SIGNAL_FUNC (forward_msg),       NULL,  0 },
		/*{ _("Forward as Attachment"),      NULL, GTK_SIGNAL_FUNC (forward_msg),       NULL,  0 },*/
		{ "",                              NULL, GTK_SIGNAL_FUNC (NULL),              NULL,  0 },
		{ _("Mark as Read"),               NULL, GTK_SIGNAL_FUNC (mark_as_seen),      NULL,  4 },
		{ _("Mark as Unread"),             NULL, GTK_SIGNAL_FUNC (mark_as_unseen),    NULL,  8 },
		{ "",                              NULL, GTK_SIGNAL_FUNC (NULL),              NULL,  0 },
		{ _("Move to Folder..."),          NULL, GTK_SIGNAL_FUNC (move_msg),          NULL,  0 },
		{ _("Copy to Folder..."),          NULL, GTK_SIGNAL_FUNC (copy_msg),          NULL,  0 },
		{ _("Delete"),                     NULL, GTK_SIGNAL_FUNC (delete_msg),        NULL, 16 },
		{ _("Undelete"),                   NULL, GTK_SIGNAL_FUNC (undelete_msg),      NULL, 32 },
		{ "",                              NULL, GTK_SIGNAL_FUNC (NULL),              NULL,  0 },
		/*{ _("Add Sender to Address Book"), NULL, GTK_SIGNAL_FUNC (addrbook_sender),   NULL,  0 },
		  { "",                              NULL, GTK_SIGNAL_FUNC (NULL),              NULL,  0 },*/
		{ _("Apply Filters"),              NULL, GTK_SIGNAL_FUNC (apply_filters),     NULL,  0 },
		{ "",                              NULL, GTK_SIGNAL_FUNC (NULL),              NULL,  0 },
		{ _("Create Rule From Message"),   NULL, GTK_SIGNAL_FUNC (NULL),       filter_menu,  2 },
		{ NULL,                            NULL, NULL,                                NULL,  0 }
	};
	
	/* Evil Hack.  */
	last_item = (sizeof (filter_menu) / sizeof (*filter_menu)) - 2;
	
	if (fb->folder != drafts_folder)
		enable_mask |= 1;
	
	if (fb->mail_display->current_message == NULL) {
		enable_mask |= 2;
		mailing_list_name = NULL;
	} else {
		mailing_list_name = mail_mlist_magic_detect_list (fb->mail_display->current_message,
								  NULL, NULL);
	}
	
	/* get a list of uids */
	uids = g_ptr_array_new ();
	message_list_foreach (fb->message_list, enumerate_msg, uids);
	if (uids->len >= 1) {
		/* gray-out any items we don't need */
		gboolean have_deleted = FALSE;
		gboolean have_undeleted = FALSE;
		gboolean have_seen = FALSE;
		gboolean have_unseen = FALSE;
		
		for (i = 0; i < uids->len; i++) {
			info = camel_folder_get_message_info (fb->folder, uids->pdata[i]);
			if (info->flags & CAMEL_MESSAGE_SEEN)
				have_seen = TRUE;
			else
				have_unseen = TRUE;
			
			if (info->flags & CAMEL_MESSAGE_DELETED)
				have_deleted = TRUE;
			else
				have_undeleted = TRUE;
			
			if (have_seen && have_unseen && have_deleted && have_undeleted)
				break;
		}
		
		if (!have_unseen)
			enable_mask |= 4;
		if (!have_seen)
			enable_mask |= 8;
		
		if (!have_undeleted)
			enable_mask |= 16;
		if (!have_deleted)
			enable_mask |= 32;
	}
	
	/* generate the "Filter on Mailing List menu item name */
	if (mailing_list_name == NULL) {
		enable_mask |= 64;
		filter_menu[last_item].name = g_strdup (_("Filter on Mailing List"));
	} else {
		filter_menu[last_item].name = g_strdup_printf (_("Filter on Mailing List (%s)"),
							       mailing_list_name);
	}
	
	/* free uids */
	for (i = 0; i < uids->len; i++)
		g_free (uids->pdata[i]);
	g_ptr_array_free (uids, TRUE);
	
	e_popup_menu_run (menu, (GdkEventButton *)event, enable_mask, 0, fb);
	
	g_free (filter_menu[last_item].name);
	
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
	/* The panned container */
	fb->vpaned = e_vpaned_new ();
	gtk_widget_show (fb->vpaned);
	
	gtk_table_attach (GTK_TABLE (fb), fb->vpaned,
			  0, 1, 1, 3,
			  GTK_FILL | GTK_EXPAND,
			  GTK_FILL | GTK_EXPAND,
			  0, 0);
	
	/* quick-search entry */
	fb->search = E_SEARCH_BAR (e_search_bar_new (folder_browser_search_menu_items,
						     folder_browser_search_option_items));
	gtk_widget_show (GTK_WIDGET (fb->search));
	
	gtk_signal_connect (GTK_OBJECT (fb->search), "query_changed",
			    GTK_SIGNAL_FUNC (folder_browser_search_query_changed), fb);
	gtk_signal_connect (GTK_OBJECT (fb->search), "menu_activated",
			    GTK_SIGNAL_FUNC (folder_browser_search_menu_activated), fb);
	
	gtk_table_attach (GTK_TABLE (fb), GTK_WIDGET (fb->search),
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
	e_paned_set_position (E_PANED (fb->vpaned), mail_config_paned_size ());
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
on_message_selected (MessageList *ml, const char *uid, FolderBrowser *fb)
{
	d(printf ("selecting uid %s\n", uid));
	mail_do_display_message (ml, fb->mail_display, uid, mark_msg_seen);
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
