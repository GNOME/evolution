/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * folder-browser.c: Folder browser top level component
 *
 * Author:
 *   Miguel de Icaza (miguel@kernel.org)
 *
 * (C) 2000, 2001 Ximian, Inc.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>

#include <gdk/gdkkeysyms.h>
#include <gal/e-paned/e-vpaned.h>
#include <gal/e-table/e-table.h>
#include <gal/util/e-util.h>
#include <gal/widgets/e-gui-utils.h>
#include <gal/widgets/e-popup-menu.h>
#include <gal/widgets/e-unicode.h>

#include <gtkhtml/htmlengine.h>

#include "filter/vfolder-rule.h"
#include "filter/vfolder-context.h"
#include "filter/filter-option.h"
#include "filter/filter-input.h"

#include "mail-search-dialogue.h"
#include "e-util/e-sexp.h"
#include "folder-browser.h"
#include "e-searching-tokenizer.h"
#include "mail.h"
#include "mail-callbacks.h"
#include "mail-tools.h"
#include "message-list.h"
#include "mail-ops.h"
#include "mail-vfolder.h"
#include "mail-autofilter.h"
#include "mail-mt.h"

#include "mail-local.h"
#include "mail-config.h"

#include <camel/camel-vtrash-folder.h>

#define d(x) x

#define PARENT_TYPE (gtk_table_get_type ())

static void fb_resize_cb (GtkWidget *w, GtkAllocation *a);
static void update_unread_count (CamelObject *, gpointer, gpointer);

static GtkObjectClass *folder_browser_parent_class;

enum {
	FOLDER_LOADED,
	MESSAGE_LOADED,
	LAST_SIGNAL
};

static guint folder_browser_signals [LAST_SIGNAL] = {0, };

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
		CamelObject *folder = CAMEL_OBJECT (folder_browser->folder);
		EvolutionStorage *storage;

		if ((storage = mail_lookup_storage (folder_browser->folder->parent_store))) {
			gtk_object_unref (GTK_OBJECT (storage));
			camel_object_unhook_event (folder, "message_changed",
						   update_unread_count,
						   folder_browser);
			camel_object_unhook_event (folder, "folder_changed",
						   update_unread_count,
						   folder_browser);
		}

		mail_sync_folder (folder_browser->folder, NULL, NULL);
		camel_object_unref (folder);
	}
	
	if (folder_browser->message_list)
		gtk_widget_destroy (GTK_WIDGET (folder_browser->message_list));
	
	if (folder_browser->mail_display)
		gtk_widget_destroy (GTK_WIDGET (folder_browser->mail_display));
	
	CORBA_exception_free (&ev);
	
	folder_browser_parent_class->destroy (object);
}

static void
folder_browser_class_init (GtkObjectClass *object_class)
{
	object_class->destroy = folder_browser_destroy;
	
	folder_browser_parent_class = gtk_type_class (PARENT_TYPE);
	
	folder_browser_signals[FOLDER_LOADED] =
		gtk_signal_new ("folder_loaded",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (FolderBrowserClass, folder_loaded),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1, GTK_TYPE_STRING);
	
	folder_browser_signals[MESSAGE_LOADED] =
		gtk_signal_new ("message_loaded",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (FolderBrowserClass, message_loaded),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1, GTK_TYPE_STRING);
	
	gtk_object_class_add_signals (object_class, folder_browser_signals, LAST_SIGNAL);
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

static void
update_unread_count_main(CamelObject *object, gpointer event_data, gpointer user_data)
{
	CamelFolder *folder = (CamelFolder *)object;
	FolderBrowser *fb = user_data;
	EvolutionStorage *storage;
	char *name;

	storage = mail_lookup_storage (folder->parent_store);

	if (fb->unread_count == 0)
		name = g_strdup (camel_folder_get_name (folder));
	else
		name = g_strdup_printf ("%s (%d)", camel_folder_get_name (folder), fb->unread_count);

	evolution_storage_update_folder_by_uri (storage, fb->uri, name, fb->unread_count != 0);
	g_free (name);
	gtk_object_unref (GTK_OBJECT (storage));
}

static void
update_unread_count(CamelObject *object, gpointer event_data, gpointer user_data)
{
	CamelFolder *folder = (CamelFolder *)object;
	FolderBrowser *fb = user_data;
	int unread;

	unread = camel_folder_get_unread_message_count (folder);
	if (unread == fb->unread_count)
		return;
	fb->unread_count = unread;
	mail_proxy_event (update_unread_count_main, object, event_data, user_data);
}

static void
got_folder(char *uri, CamelFolder *folder, void *data)
{
	FolderBrowser *fb = data;
	EvolutionStorage *storage;

	printf("got folder '%s' = %p\n", uri, folder);

	if (fb->folder == folder)
		goto done;

	if (fb->folder)
		camel_object_unref((CamelObject *)fb->folder);
	g_free(fb->uri);
	fb->uri = g_strdup(uri);
	fb->folder = folder;

	if (folder == NULL)
		goto done;

	camel_object_ref((CamelObject *)folder);

	if ((storage = mail_lookup_storage (folder->parent_store))) {
		gtk_object_unref (GTK_OBJECT (storage));
		fb->unread_count = camel_folder_get_unread_message_count (folder);
		update_unread_count_main ((CamelObject *)folder, NULL, fb);
		camel_object_hook_event ((CamelObject *)folder, "message_changed",
					 update_unread_count, fb);
		camel_object_hook_event ((CamelObject *)folder, "folder_changed",
					 update_unread_count, fb);
	}

	gtk_widget_set_sensitive(GTK_WIDGET(fb->search), camel_folder_has_search_capability(folder));
	message_list_set_threaded(fb->message_list, mail_config_get_thread_list());
	message_list_set_folder(fb->message_list, folder);
	vfolder_register_source(folder);
done:
	gtk_object_unref((GtkObject *)fb);

	/* Sigh, i dont like this (it can be set in reconfigure folder),
	   but its just easier right now to do it this way */
	fb->reconfigure = FALSE;
	
	gtk_signal_emit (GTK_OBJECT (fb), folder_browser_signals [FOLDER_LOADED], fb->uri);
}

gboolean
folder_browser_set_uri (FolderBrowser *folder_browser, const char *uri)
{
	if (uri && *uri) {
		gtk_object_ref((GtkObject *)folder_browser);
		mail_get_folder(uri, got_folder, folder_browser);
	} else {
		/* Sigh, i dont like this (it can be set in reconfigure folder),
		   but its just easier right now to do it this way */
		folder_browser->reconfigure = FALSE;
	}

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
	ESB_SAVE,
};

static ESearchBarItem folder_browser_search_menu_items[] = {
	E_FILTERBAR_RESET,
	E_FILTERBAR_SAVE,
	{ N_("Store search as vFolder"),  ESB_SAVE     },
	E_FILTERBAR_EDIT,
	{ NULL,                           -1           }
};

static void
folder_browser_search_menu_activated (ESearchBar *esb, int id, FolderBrowser *fb)
{
	EFilterBar *efb = (EFilterBar *)esb;

	printf("menyu activated\n");

	switch (id) {
	case ESB_SAVE:
		printf("Save vfolder\n");
		if (efb->current_query) {
			FilterRule *rule = vfolder_clone_rule(efb->current_query);			

			filter_rule_set_source(rule, FILTER_SOURCE_INCOMING);
			vfolder_rule_add_source((VfolderRule *)rule, fb->uri);
			vfolder_gui_add_rule((VfolderRule *)rule);
		}
		break;
	}
}

static void folder_browser_config_search(EFilterBar *efb, FilterRule *rule, int id, const char *query, void *data)
{
	FolderBrowser *fb = FOLDER_BROWSER (data);
	ESearchingTokenizer *st;
	GList *partl;

	st = E_SEARCHING_TOKENIZER (fb->mail_display->html->engine->ht); 

	e_searching_tokenizer_set_secondary_search_string (st, NULL);
	
	/* we scan the parts of a rule, and set all the types we know about to the query string */
	partl = rule->parts;
	while (partl) {
		FilterPart *part = partl->data;

		if (!strcmp(part->name, "subject")) {
			FilterInput *input = (FilterInput *)filter_part_find_element(part, "subject");
			if (input)
				filter_input_set_value(input, query);
		} else if (!strcmp(part->name, "body")) {
			FilterInput *input = (FilterInput *)filter_part_find_element(part, "word");
			if (input)
				filter_input_set_value(input, query);
			e_searching_tokenizer_set_secondary_search_string (st, query);
		} else if(!strcmp(part->name, "sender")) {
			FilterInput *input = (FilterInput *)filter_part_find_element(part, "sender");
			if (input)
				filter_input_set_value(input, query);
		}
		
		partl = partl->next;
	}
	printf("configuring search for search string '%s', rule is '%s'\n", query, rule->name);

	mail_display_redisplay (fb->mail_display, FALSE);
}

static void
folder_browser_search_query_changed (ESearchBar *esb, FolderBrowser *fb)
{
	char *search_word;

	printf("query changed\n");

	gtk_object_get (GTK_OBJECT (esb),
			"query", &search_word,
			NULL);

	message_list_set_search (fb->message_list, search_word);

	printf("query is %s\n", search_word);
	g_free(search_word);
	return;
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
folder_browser_toggle_hide_deleted (BonoboUIComponent           *component,
				    const char                  *path,
				    Bonobo_UIComponent_EventType type,
				    const char                  *state,
				    gpointer                     user_data)
{
	FolderBrowser *fb = user_data;

	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	if (!(fb->folder && CAMEL_IS_VTRASH_FOLDER(fb->folder)))
		mail_config_set_hide_deleted (atoi (state));
	message_list_set_hidedeleted (fb->message_list, atoi (state));
}

void
folder_browser_toggle_view_source (BonoboUIComponent           *component,
				   const char                  *path,
				   Bonobo_UIComponent_EventType type,
				   const char                  *state,
				   gpointer                     user_data)
{
	FolderBrowser *fb = user_data;
	
	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;
	
	mail_config_set_view_source (atoi (state));
	mail_display_redisplay (fb->mail_display, TRUE);
}

void
vfolder_subject (GtkWidget *w, FolderBrowser *fb)
{
	vfolder_gui_add_from_message (fb->mail_display->current_message, AUTO_SUBJECT, fb->uri);
}

void
vfolder_sender (GtkWidget *w, FolderBrowser *fb)
{
	vfolder_gui_add_from_message (fb->mail_display->current_message, AUTO_FROM, fb->uri);
}

void
vfolder_recipient (GtkWidget *w, FolderBrowser *fb)
{
	vfolder_gui_add_from_message (fb->mail_display->current_message, AUTO_TO, fb->uri);
}

void
vfolder_mlist (GtkWidget *w, FolderBrowser *fb)
{
	char *name;

	g_return_if_fail (fb->mail_display->current_message != NULL);

	name = header_raw_check_mailing_list(&((CamelMimePart *)fb->mail_display->current_message)->headers);
	if (name) {
		vfolder_gui_add_from_mlist(fb->mail_display->current_message, name, fb->uri);
		g_free(name);
	}
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

	g_return_if_fail (fb->mail_display->current_message != NULL);

	name = header_raw_check_mailing_list(&((CamelMimePart *)fb->mail_display->current_message)->headers);
	if (name) {
		filter_gui_add_from_mlist(name);
		g_free(name);
	}
}

void
hide_none(GtkWidget *w, FolderBrowser *fb)
{
	message_list_hide_clear(fb->message_list);
}

void
hide_selected(GtkWidget *w, FolderBrowser *fb)
{
	GPtrArray *uids;
	int i;

	uids = g_ptr_array_new();
	message_list_foreach(fb->message_list, enumerate_msg, uids);
	message_list_hide_uids(fb->message_list, uids);
	for (i=0; i<uids->len; i++)
		g_free(uids->pdata[i]);
	g_ptr_array_free(uids, TRUE);
}

void
hide_deleted(GtkWidget *w, FolderBrowser *fb)
{
	MessageList *ml = fb->message_list;

	message_list_hide_add(ml, "(match-all (system-flag \"deleted\"))", ML_HIDE_SAME, ML_HIDE_SAME);
}

void
hide_read(GtkWidget *w, FolderBrowser *fb)
{
	MessageList *ml = fb->message_list;

	message_list_hide_add(ml, "(match-all (system-flag \"seen\"))", ML_HIDE_SAME, ML_HIDE_SAME);
}

/* dum de dum, about the 3rd copy of this function throughout the mailer/camel */
static const char *
strip_re(const char *subject)
{
	const unsigned char *s, *p;
	
	s = (unsigned char *) subject;
	
	while (*s) {
		while(isspace (*s))
			s++;
		if (s[0] == 0)
			break;
		if ((s[0] == 'r' || s[0] == 'R')
		    && (s[1] == 'e' || s[1] == 'E')) {
			p = s+2;
			while (isdigit(*p) || (ispunct(*p) && (*p != ':')))
				p++;
			if (*p == ':') {
				s = p + 1;
			} else
				break;
		} else
			break;
	}
	return (char *) s;
}

void
hide_subject(GtkWidget *w, FolderBrowser *fb)
{
	const char *subject;
	GString *expr;

	if (fb->mail_display->current_message) {
		subject = camel_mime_message_get_subject(fb->mail_display->current_message);
		if (subject) {
			subject = strip_re(subject);
			if (subject && subject[0]) {
				expr = g_string_new("(match-all (header-contains \"subject\" ");
				e_sexp_encode_string(expr, subject);
				g_string_append(expr, "))");
				message_list_hide_add(fb->message_list, expr->str, ML_HIDE_SAME, ML_HIDE_SAME);
				g_string_free(expr, TRUE);
				return;
			}
		}
	}
}

void
hide_sender(GtkWidget *w, FolderBrowser *fb)
{
	const CamelInternetAddress *from;
	const char *real, *addr;
	GString *expr;

	if (fb->mail_display->current_message) {
		from = camel_mime_message_get_from(fb->mail_display->current_message);
		if (camel_internet_address_get(from, 0, &real, &addr)) {
			expr = g_string_new("(match-all (header-contains \"from\" ");
			e_sexp_encode_string(expr, addr);
			g_string_append(expr, "))");
			message_list_hide_add(fb->message_list, expr->str, ML_HIDE_SAME, ML_HIDE_SAME);
			g_string_free(expr, TRUE);
			return;
		}
	}
}

enum {
	SELECTION_SET   = 2,
	CAN_MARK_READ   = 4,
	CAN_MARK_UNREAD = 8,
	CAN_DELETE      = 16,
	CAN_UNDELETE    = 32,
	IS_MAILING_LIST = 64,
	CAN_RESEND      = 128,
};

#define SEPARATOR  { "", NULL, (NULL), NULL,  0 }
#define TERMINATOR { NULL, NULL, (NULL), NULL,  0 }

#define MLIST_VFOLDER (3)
#define MLIST_FILTER (8)

static EPopupMenu filter_menu[] = {
	{ N_("VFolder on _Subject"),           NULL,
	  GTK_SIGNAL_FUNC (vfolder_subject),   NULL,
	  SELECTION_SET },
	{ N_("VFolder on Se_nder"),            NULL,
	  GTK_SIGNAL_FUNC (vfolder_sender),    NULL,
	  SELECTION_SET },
	{ N_("VFolder on _Recipients"),        NULL,
	  GTK_SIGNAL_FUNC (vfolder_recipient), NULL,
	  SELECTION_SET },
	{ N_("VFolder on Mailing _List"),      NULL,
	  GTK_SIGNAL_FUNC (vfolder_mlist),     NULL,
	  SELECTION_SET | IS_MAILING_LIST },
	
	SEPARATOR,
	
	{ N_("Filter on Sub_ject"),            NULL,
	  GTK_SIGNAL_FUNC (filter_subject),    NULL,
	  SELECTION_SET },
	{ N_("Filter on Sen_der"),             NULL,
	  GTK_SIGNAL_FUNC (filter_sender),     NULL,
	  SELECTION_SET },
	{ N_("Filter on Re_cipients"),         NULL,
	  GTK_SIGNAL_FUNC (filter_recipient),  NULL,
	  SELECTION_SET },
	{ N_("Filter on _Mailing List"),       NULL,
	  GTK_SIGNAL_FUNC (filter_mlist),      NULL,
	  SELECTION_SET | IS_MAILING_LIST },
	
	TERMINATOR
};


static EPopupMenu context_menu[] = {
	{ N_("_Open"),                        NULL,
	  GTK_SIGNAL_FUNC (open_msg),         NULL,  0 },
	{ N_("Resend"),                       NULL,
	  GTK_SIGNAL_FUNC (resend_msg),       NULL,  CAN_RESEND },
	{ N_("_Save As..."),                  NULL,
	  GTK_SIGNAL_FUNC (save_msg),         NULL,  0 },
	{ N_("_Print"),                       NULL,
	  GTK_SIGNAL_FUNC (print_msg),        NULL,  0 },
	
	SEPARATOR,
	
	{ N_("_Reply to Sender"),             NULL,
	  GTK_SIGNAL_FUNC (reply_to_sender),  NULL,  0 },
	{ N_("Reply to _All"),                NULL,
	  GTK_SIGNAL_FUNC (reply_to_all),     NULL,  0 },
	{ N_("_Forward"),                     NULL,
	  GTK_SIGNAL_FUNC (forward),          NULL,  0 },
	{ "", NULL, (NULL), NULL,  0 },
	{ N_("Mar_k as Read"),                NULL,
	  GTK_SIGNAL_FUNC (mark_as_seen),     NULL,  CAN_MARK_READ },
	{ N_("Mark as U_nread"),              NULL,
	  GTK_SIGNAL_FUNC (mark_as_unseen),   NULL,  CAN_MARK_UNREAD },
	
	SEPARATOR,
	
	{ N_("_Move to Folder..."),           NULL,
	  GTK_SIGNAL_FUNC (move_msg),         NULL,  0 },
	{ N_("_Copy to Folder..."),           NULL,
	  GTK_SIGNAL_FUNC (copy_msg),         NULL,  0 },
	{ N_("_Delete"),                      NULL,
	  GTK_SIGNAL_FUNC (delete_msg),       NULL, CAN_DELETE },
	{ N_("_Undelete"),                    NULL,
	  GTK_SIGNAL_FUNC (undelete_msg),     NULL, CAN_UNDELETE },
	
	SEPARATOR,
	
	/*{ _("Add Sender to Address Book"),  NULL,
	  GTK_SIGNAL_FUNC (addrbook_sender),  NULL,  0 },
	  { "",                               NULL,
	  GTK_SIGNAL_FUNC (NULL),             NULL,  0 },*/
	
	{ N_("Apply Filters"),                NULL,
	  GTK_SIGNAL_FUNC (apply_filters),    NULL,  0 },
	{ "",                                 NULL,
	  GTK_SIGNAL_FUNC (NULL),             NULL,  0 },
	{ N_("Create Ru_le From Message"),    NULL,
	  GTK_SIGNAL_FUNC (NULL), filter_menu,  SELECTION_SET },
	
	TERMINATOR
};


struct cmpf_data {
	ETree *tree;
	int row, col;
};

static void
context_menu_position_func (GtkMenu *menu, gint *x, gint *y,
			    gpointer user_data)
{
	int tx, ty, tw, th;
	struct cmpf_data *closure = user_data;

	gdk_window_get_origin (GTK_WIDGET (closure->tree)->window, x, y);
	e_tree_get_cell_geometry (closure->tree, closure->row, closure->col,
				  &tx, &ty, &tw, &th);
	*x += tx + tw / 2;
	*y += ty + th / 2;
}

/* handle context menu over message-list */
static gint
on_right_click (ETree *tree, gint row, ETreePath path, gint col, GdkEvent *event, FolderBrowser *fb)
{
	extern CamelFolder *sent_folder;
	CamelMessageInfo *info;
	GPtrArray *uids;
	int enable_mask = 0;
	int hide_mask = 0;
	int i;
	char *mailing_list_name = NULL;
	char *subject_match = NULL, *from_match = NULL;
	GtkMenu *menu;
	
	if (fb->reconfigure) {
		enable_mask = 0;
		goto display_menu;
	}
	
	if (fb->folder != sent_folder) {
		enable_mask |= CAN_RESEND;
		hide_mask |= CAN_RESEND;
	}
	
	if (fb->mail_display->current_message == NULL) {
		enable_mask |= SELECTION_SET;
		mailing_list_name = NULL;
	} else {
		const char *subject, *real, *addr;
		const CamelInternetAddress *from;

		mailing_list_name = header_raw_check_mailing_list(
			&((CamelMimePart *)fb->mail_display->current_message)->headers);

		if ((subject = camel_mime_message_get_subject(fb->mail_display->current_message))
		    && (subject = strip_re(subject))
		    && subject[0])
			subject_match = g_strdup(subject);

		if ((from = camel_mime_message_get_from(fb->mail_display->current_message))
		    && camel_internet_address_get(from, 0, &real, &addr)
		    && addr && addr[0])
			from_match = g_strdup(addr);
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
			if (info == NULL)
				continue;

			if (info->flags & CAMEL_MESSAGE_SEEN)
				have_seen = TRUE;
			else
				have_unseen = TRUE;
			
			if (info->flags & CAMEL_MESSAGE_DELETED)
				have_deleted = TRUE;
			else
				have_undeleted = TRUE;
			
			camel_folder_free_message_info(fb->folder, info);

			if (have_seen && have_unseen && have_deleted && have_undeleted)
				break;
		}

		if (!have_unseen)
			enable_mask |= CAN_MARK_READ;
		if (!have_seen)
			enable_mask |= CAN_MARK_UNREAD;
		
		if (!have_undeleted)
			enable_mask |= CAN_DELETE;
		if (!have_deleted)
			enable_mask |= CAN_UNDELETE;

		/*
		 * Hide items that wont get used.
		 */
		if (!(have_unseen && have_seen)){
			if (have_seen)
				hide_mask |= CAN_MARK_READ;
			else
				hide_mask |= CAN_MARK_UNREAD;
		}
		if (!(have_undeleted && have_deleted)){
			if (have_deleted)
				hide_mask |= CAN_DELETE;
			else
				hide_mask |= CAN_UNDELETE;
		}
	}

	/* free uids */
	for (i = 0; i < uids->len; i++)
		g_free (uids->pdata[i]);
	g_ptr_array_free (uids, TRUE);

display_menu:
	
	/* generate the "Filter on Mailing List menu item name */
	if (mailing_list_name == NULL) {
		enable_mask |= IS_MAILING_LIST;
		filter_menu[MLIST_FILTER].name = g_strdup (_("Filter on Mailing List"));
		filter_menu[MLIST_VFOLDER].name = g_strdup (_("VFolder on Mailing List"));
	} else {
		filter_menu[MLIST_FILTER].name = g_strdup_printf (_("Filter on Mailing List (%s)"), mailing_list_name);
		filter_menu[MLIST_VFOLDER].name = g_strdup_printf (_("VFolder on Mailing List (%s)"), mailing_list_name);
		g_free(mailing_list_name);
	}

	menu = e_popup_menu_create (context_menu, enable_mask, hide_mask, fb);
	e_auto_kill_popup_menu_on_hide (menu);

	if (event->type == GDK_KEY_PRESS) {
		struct cmpf_data closure;

		closure.tree = tree;
		closure.row = row;
		closure.col = col;
		gtk_menu_popup (menu, NULL, NULL, context_menu_position_func,
				&closure, 0, event->key.time);
	} else {
		gtk_menu_popup (menu, NULL, NULL, NULL, NULL,
				event->button.button, event->button.time);
	}

	g_free(filter_menu[MLIST_FILTER].name);
	g_free(filter_menu[MLIST_VFOLDER].name);

	return TRUE;
}

static gint
on_key_press (GtkWidget *widget, GdkEventKey *key, gpointer data)
{
	FolderBrowser *fb = data;
	ETreePath *path;
	int row;

	if (key->state & GDK_CONTROL_MASK)
		return FALSE;

	path = e_tree_get_cursor (fb->message_list->tree);
	row = e_tree_row_of_node (fb->message_list->tree, path);

	switch (key->keyval) {
	case GDK_Delete:
	case GDK_KP_Delete:
		delete_msg (NULL, fb);
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

	case GDK_Menu:
		on_right_click (fb->message_list->tree, row, path, 2,
				(GdkEvent *)key, fb);
		return TRUE;
	}

	return FALSE;
}

static int
etree_key (ETree *tree, int row, ETreePath path, int col, GdkEvent *ev, FolderBrowser *fb)
{
	GtkAdjustment *vadj;
	gfloat page_size;

	if ((ev->key.state & GDK_CONTROL_MASK) != 0)
		return FALSE;
	if (ev->key.keyval != GDK_space && ev->key.keyval != GDK_BackSpace)
		return on_key_press ((GtkWidget *)tree, (GdkEventKey *)ev, fb);

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

static void
on_double_click (ETree *tree, gint row, ETreePath path, gint col, GdkEvent *event, FolderBrowser *fb)
{
	/* Ignore double-clicks on columns where single-click doesn't
	 * just select.
	 */
	if (MESSAGE_LIST_COLUMN_IS_ACTIVE (col))
		return;
	
	open_msg (NULL, fb);
}

static void
folder_browser_gui_init (FolderBrowser *fb)
{
	/* The panned container */
	fb->vpaned = e_vpaned_new ();
	gtk_widget_show (fb->vpaned);
	
	gtk_table_attach (
		GTK_TABLE (fb), fb->vpaned,
			  0, 1, 1, 3,
			  GTK_FILL | GTK_EXPAND,
			  GTK_FILL | GTK_EXPAND,
			  0, 0);
	
	/* quick-search bar */
	{
		RuleContext *rc = (RuleContext *)rule_context_new ();
		char *user = g_strdup_printf("%s/searches.xml", evolution_dir);
		/* we reuse the vfolder types here, they should match */
		char *system = EVOLUTION_DATADIR "/evolution/vfoldertypes.xml";

		rule_context_add_part_set((RuleContext *)rc, "partset", filter_part_get_type(),
					  rule_context_add_part, rule_context_next_part);
		
		rule_context_add_rule_set((RuleContext *)rc, "ruleset", filter_rule_get_type(),
					  rule_context_add_rule, rule_context_next_rule);
	
		fb->search = e_filter_bar_new(rc, system, user, folder_browser_config_search, fb);
		e_search_bar_set_menu((ESearchBar *)fb->search, folder_browser_search_menu_items);
		/*e_search_bar_set_option((ESearchBar *)fb->search, folder_browser_search_option_items);*/
		g_free(user);
		gtk_object_unref((GtkObject *)rc);
	}

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
	
	e_paned_add1 (E_PANED (fb->vpaned), GTK_WIDGET (fb->message_list));
	gtk_widget_show (GTK_WIDGET (fb->message_list));
	
	gtk_signal_connect (GTK_OBJECT (fb->message_list), "size_allocate",
	                    GTK_SIGNAL_FUNC (fb_resize_cb), NULL);
	
	e_paned_add2 (E_PANED (fb->vpaned), GTK_WIDGET (fb->mail_display));
	e_paned_set_position (E_PANED (fb->vpaned), mail_config_get_paned_size ());
	gtk_widget_show (GTK_WIDGET (fb->mail_display));
	gtk_widget_show (GTK_WIDGET (fb));
}

/* mark the message seen if the current message still matches */
static gint 
do_mark_seen (gpointer data)
{
	FolderBrowser *fb = data;

	if (fb->new_uid && fb->loaded_uid
	    && strcmp(fb->new_uid, fb->loaded_uid) == 0) {
		camel_folder_set_message_flags(fb->folder, fb->new_uid, CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);
	}

	return FALSE;
}

/* callback when we have the message to display, after async loading it (see below) */
/* if we have pending uid's, it means another was selected before we finished displaying
   the last one - so we cycle through and start loading the pending one immediately now */
static void done_message_selected(CamelFolder *folder, char *uid, CamelMimeMessage *msg, void *data)
{
	FolderBrowser *fb = data;
	int timeout = mail_config_get_mark_as_seen_timeout ();

	if (folder != fb->folder)
		return;
	
	mail_display_set_message (fb->mail_display, (CamelMedium *)msg);
	/* FIXME: should this signal be emitted here?? */
	gtk_signal_emit (GTK_OBJECT (fb), folder_browser_signals [MESSAGE_LOADED], uid);
	
	/* pain, if we have pending stuff, re-run */
	if (fb->pending_uid) {	
		g_free(fb->loading_uid);
		fb->loading_uid = fb->pending_uid;
		fb->pending_uid = NULL;

		mail_get_message(fb->folder, fb->loading_uid, done_message_selected, fb, mail_thread_new);
		return;
	}

	g_free(fb->loaded_uid);
	fb->loaded_uid = fb->loading_uid;
	fb->loading_uid = NULL;

	/* if we are still on the same message, do the 'idle read' thing */
	if (fb->seen_id)
		gtk_timeout_remove(fb->seen_id);

	if (msg) {
		if (timeout > 0)
			fb->seen_id = gtk_timeout_add(timeout, do_mark_seen, fb);
		else
			do_mark_seen(fb);
	}
}

/* ok we waited enough, display it anyway (see below) */
static gboolean
do_message_selected(FolderBrowser *fb)
{
	d(printf ("selecting uid %s (delayed)\n", fb->new_uid ? fb->new_uid : "NONE"));

	/* keep polling if we are busy */
	if (fb->reconfigure) {
		if (fb->new_uid == NULL) {
			mail_display_set_message(fb->mail_display, NULL);
			return FALSE;
		}
		return TRUE;
	}

	fb->loading_id = 0;

	/* if we are loading, then set a pending, but leave the loading, coudl cancel here (?) */
	if (fb->loading_uid) {
		g_free(fb->pending_uid);
		fb->pending_uid = g_strdup(fb->new_uid);
	} else {
		if (fb->new_uid) {
			fb->loading_uid = g_strdup(fb->new_uid);
			mail_get_message(fb->folder, fb->loading_uid, done_message_selected, fb, mail_thread_new);
		} else {
			mail_display_set_message(fb->mail_display, NULL);
		}
	}

	return FALSE;
}

/* when a message is selected, wait a while before trying to display it */
static void
on_message_selected (MessageList *ml, const char *uid, FolderBrowser *fb)
{
	d(printf ("selecting uid %s (direct)\n", uid ? uid : "NONE"));

	if (fb->loading_id != 0)
		gtk_timeout_remove(fb->loading_id);

	g_free(fb->new_uid);
	fb->new_uid = g_strdup(uid);
	fb->loading_id = gtk_timeout_add(100, (GtkFunction)do_message_selected, fb);
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

	e_scroll_frame_set_policy(E_SCROLL_FRAME(fb->message_list),
				  GTK_POLICY_NEVER,
				  GTK_POLICY_ALWAYS);
	
	gtk_signal_connect (GTK_OBJECT (fb->mail_display->html),
			    "key_press_event", GTK_SIGNAL_FUNC (on_key_press), fb);

	gtk_signal_connect (GTK_OBJECT (fb->message_list->tree),
			    "key_press", GTK_SIGNAL_FUNC (etree_key), fb);

	gtk_signal_connect (GTK_OBJECT (fb->message_list->tree),
			    "right_click", GTK_SIGNAL_FUNC (on_right_click), fb);

	gtk_signal_connect (GTK_OBJECT (fb->message_list->tree),
			    "double_click", GTK_SIGNAL_FUNC (on_double_click), fb);

	gtk_signal_connect (GTK_OBJECT(fb->message_list), "message_selected",
			    on_message_selected, fb);

	folder_browser_gui_init (fb);
}

GtkWidget *
folder_browser_new (const GNOME_Evolution_Shell shell)
{
	CORBA_Environment ev;
	FolderBrowser *folder_browser;

	CORBA_exception_init (&ev);

	folder_browser = gtk_type_new (folder_browser_get_type ());

	my_folder_browser_init (GTK_OBJECT (folder_browser));
	folder_browser->uri = NULL;

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
