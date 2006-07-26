/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *           Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string.h>

#include <gtk/gtkvbox.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkvpaned.h>
#include <gtk/gtkhpaned.h>
#include <gtkhtml/gtkhtml.h>
#include <gdk/gdkkeysyms.h>
#include <gconf/gconf-client.h>

#ifdef G_OS_WIN32
/* Work around 'DATADIR' and 'interface' lossage in <windows.h> */
#define DATADIR crap_DATADIR
#include <windows.h>
#undef DATADIR
#undef interface
#endif

#include <libgnomeprintui/gnome-print-dialog.h>

#include "mail-mt.h"
#include "mail-ops.h"
#include "mail-tools.h"
#include "mail-config.h"

#include <e-util/e-dialog-utils.h>
#include <e-util/e-icon-factory.h>

#include <camel/camel-stream.h>
#include <camel/camel-url.h>
#include <camel/camel-vee-folder.h>
#include <camel/camel-vee-store.h>

#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-ui-component.h>
#include <bonobo/bonobo-ui-util.h>

/* for efilterbar stuff */
#include <libedataserver/e-sexp.h>
#include "mail-vfolder.h"
#include "em-vfolder-rule.h"
#include <misc/e-filter-bar.h>
#include <camel/camel-search-private.h>
#include <camel/camel-store.h>

#include "e-util/e-dialog-utils.h"
#include "e-util/e-error.h"
#include "e-util/e-util-private.h"
#include "em-utils.h"
#include "em-composer-utils.h"
#include "em-format-html-display.h"
#include "em-format-html-print.h"
#include "em-folder-browser.h"
#include "em-folder-properties.h"
#include "em-folder-utils.h"
#include "em-subscribe-editor.h"
#include "em-menu.h"
#include "message-list.h"

#include "mail-component.h"
#include "mail-ops.h"

#include "evolution-shell-component-utils.h" /* Pixmap stuff, sigh */

extern CamelSession *session;
CamelStore *vfolder_store; /* the 1 static vfolder store */

#define d(x)

struct _EMFolderBrowserPrivate {
	GtkWidget *preview;	/* container for message display */

	GtkWidget *subscribe_editor;

	guint search_menu_activated_id;
	guint search_activated_id;
	guint search_query_changed_id;

	double default_scroll_position;
	guint idle_scroll_id;
	guint list_scrolled_id;
	
	guint vpane_resize_id;
	guint list_built_id;	/* hook onto list-built for delayed 'select first unread' stuff */
	
	char *select_uid;
	guint folder_changed_id;

	int show_wide:1;

	EMMenu *menu;		/* toplevel menu manager */
};

static void emfb_activate(EMFolderView *emfv, BonoboUIComponent *uic, int state);
static void emfb_set_folder(EMFolderView *emfv, CamelFolder *folder, const char *uri);
static void emfb_set_search_folder(EMFolderView *emfv, CamelFolder *folder, const char *uri);

/* FilterBar stuff ... */
static void emfb_search_config_search(EFilterBar *efb, FilterRule *rule, int id, const char *query, void *data);
static void emfb_search_menu_activated(ESearchBar *esb, int id, EMFolderBrowser *emfb);
static void emfb_search_search_activated(ESearchBar *esb, EMFolderBrowser *emfb);
static void emfb_search_query_changed(ESearchBar *esb, EMFolderBrowser *emfb);

static int emfb_list_key_press(ETree *tree, int row, ETreePath path, int col, GdkEvent *ev, EMFolderBrowser *emfb);
static void emfb_list_message_selected (MessageList *ml, const char *uid, EMFolderBrowser *emfb);

static const EMFolderViewEnable emfb_enable_map[];

enum {
	ACCOUNT_SEARCH_ACTIVATED,
	ACCOUNT_SEARCH_CLEARED,
	LAST_SIGNAL
};

static guint folder_browser_signals [LAST_SIGNAL] = {0, };

enum {
	ESB_SAVE,
};

static ESearchBarItem emfb_search_items[] = {
	E_FILTERBAR_ADVANCED,
	{ NULL, 0, 0 },
	E_FILTERBAR_SAVE,
	E_FILTERBAR_EDIT,
	{ NULL, 0, 0 },
	{ N_("C_reate Search Folder From Search..."), ESB_SAVE, 0},
	{ NULL, -1, 0 }
};

/* IDs and option items for the ESearchBar */
enum {
	VIEW_ALL_MESSAGES,
	VIEW_UNREAD_MESSAGES,
	VIEW_READ_MESSAGES,
	VIEW_RECENT_MESSAGES,
	VIEW_LAST_FIVE_DAYS,
	VIEW_WITH_ATTACHMENTS,
	VIEW_NOT_JUNK,
	VIEW_NO_LABEL,
	VIEW_LABEL,
	VIEW_ANY_FIELD_CONTAINS,
	VIEW_CUSTOMIZE
};

/* Options for View */
static ESearchBarItem emfb_view_items[] = {
	{ N_("All Messages"), VIEW_ALL_MESSAGES, 0 },
	{ N_("Unread Messages"), VIEW_UNREAD_MESSAGES, 0 },
	{ NULL, 0, 0 },
	{ N_("No Label"),VIEW_NO_LABEL, 0 },
	{ NULL, -1, 0 }
};

/* TODO: Following options should be  customizable */
static ESearchBarItem temp_view_items[] = {
	{ NULL, 0, 0 },
	{ N_("Read Messages"), VIEW_READ_MESSAGES, 0 },
	{ N_("Recent Messages"), VIEW_RECENT_MESSAGES, 0 },
	{ N_("Last 5 Days' Messages"), VIEW_LAST_FIVE_DAYS, 0 },
	{ N_("Messages with Attachments"), VIEW_WITH_ATTACHMENTS, 0 },
	{ N_("Messages Not Junk"), VIEW_NOT_JUNK, 0 },
/* 	{ NULL, 0, NULL }, */
/* 	{ N_("Customize"), NOT_IMPLEMENTED, NULL }, */
	{ NULL, -1, 0 }
};

static ESearchBarItem emfb_search_scope_items[] = {
	E_FILTERBAR_CURRENT_FOLDER,
	E_FILTERBAR_CURRENT_ACCOUNT,
	E_FILTERBAR_ALL_ACCOUNTS,
	E_FILTERBAR_CURRENT_MESSAGE,
	{ NULL, -1, 0 }
};

static EMFolderViewClass *emfb_parent;

/* Needed since the paned wont take the position its given otherwise ... */
static void
emfb_pane_realised(GtkWidget *w, EMFolderBrowser *emfb)
{
	GConfClient *gconf;

	gconf = mail_config_get_gconf_client ();
	
	if (emfb->priv->show_wide)
		gtk_paned_set_position((GtkPaned *)emfb->vpane, gconf_client_get_int(gconf, "/apps/evolution/mail/display/hpaned_size", NULL));
	else
		gtk_paned_set_position((GtkPaned *)emfb->vpane, gconf_client_get_int(gconf, "/apps/evolution/mail/display/paned_size", NULL));
}

static gboolean
emfb_pane_button_release_event(GtkWidget *w, GdkEventButton *e, EMFolderBrowser *emfb)
{
	GConfClient *gconf = mail_config_get_gconf_client ();

	if (GTK_WIDGET_REALIZED (w)) {
		if (emfb->priv->show_wide)
			gconf_client_set_int(gconf, "/apps/evolution/mail/display/hpaned_size",
					     gtk_paned_get_position(GTK_PANED(w)), NULL);
		else
			gconf_client_set_int(gconf, "/apps/evolution/mail/display/paned_size",
					     gtk_paned_get_position(GTK_PANED(w)), NULL);
			
	}
	
	return FALSE;
}

static void
free_one_ui_file (gpointer data,
		  gpointer user_data)
{
	g_free (data);
}

static char *
string_without_underscores (const char *s)
{
	char *new_string;
	const char *sp;
	char *dp;

	new_string = g_malloc (strlen (s) + 1);

	dp = new_string;
	for (sp = s; *sp != '\0'; sp ++) {
		if (*sp != '_') {
			*dp = *sp;
			dp ++;
		} else if (sp[1] == '_') {
			/* Translate "__" in "_".  */
			*dp = '_';
			dp ++;
			sp ++;
		}
	}
	*dp = 0;

	return new_string;
}

static GtkWidget *
generate_viewoption_menu ()
{
	GtkWidget *menu, *menu_item;
	gint i = 0;
	GSList *l;

	menu = gtk_menu_new ();

	for (i = 0; emfb_view_items[i].id != -1; ++i) {
		if (emfb_view_items[i].text) {
			char *str;
			str = string_without_underscores (emfb_view_items[i].text);
			menu_item = gtk_menu_item_new_with_label (str);
			g_free (str);
		} else {
			menu_item = gtk_menu_item_new ();
			gtk_widget_set_sensitive (menu_item, FALSE);
		}

		g_object_set_data (G_OBJECT (menu_item), "EsbItemId",
				   GINT_TO_POINTER (emfb_view_items[i].id));

		gtk_widget_show (menu_item);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
	}

	/* Add the labels */
	for (l = mail_config_get_labels(); l; l = l->next) {
		MailConfigLabel *label = l->data;
		if (label->name && *(label->name)) {
			char *str;
			str = string_without_underscores (label->name);
			menu_item = gtk_menu_item_new_with_label (str);
			g_free (str);

			g_object_set_data (G_OBJECT (menu_item), "EsbItemId",
					   GINT_TO_POINTER (VIEW_LABEL));
		
			g_object_set_data (G_OBJECT (menu_item), "LabelTag",
					   g_strdup(label->tag));
		}

		gtk_widget_show (menu_item);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
	}

	for (i = 0; temp_view_items[i].id != -1; ++i) {
		if (temp_view_items[i].text) {
			char *str;
			str = string_without_underscores (temp_view_items[i].text);
			menu_item = gtk_menu_item_new_with_label (str);
			g_free (str);
		} else {
			menu_item = gtk_menu_item_new ();
			gtk_widget_set_sensitive (menu_item, FALSE);
		}

		g_object_set_data (G_OBJECT (menu_item), "EsbItemId",
				   GINT_TO_POINTER (temp_view_items[i].id));

		gtk_widget_show (menu_item);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
	}

	return menu;
}


static GArray *
viewoption_menu_generator ()
{
	GArray *menu = g_array_new (FALSE, FALSE, sizeof (ESearchBarItem));
	gint i = 0;
	ESearchBarItem dup_item;
	GSList *l;

	for (i = 0; emfb_view_items[i].id != -1; i++) 
		g_array_append_vals (menu, &emfb_view_items[i], 1);

	for (l = mail_config_get_labels(); l; l = l->next) {
		ESearchBarItem item;
		MailConfigLabel *label = l->data;
	
		item.text = label->name;
		item.id = VIEW_LABEL;

		g_array_append_vals (menu, &item, 1);
	}

	dup_item.id = -1;
	dup_item.text = NULL;
	g_array_append_vals (menu, &dup_item, 1);

	return menu;
}

static void
emfb_init(GObject *o)
{
	EMFolderBrowser *emfb = (EMFolderBrowser *)o;
	RuleContext *search_context = mail_component_peek_search_context (mail_component_peek ());
	struct _EMFolderBrowserPrivate *p;
	GtkWidget *menu;

	p = emfb->priv = g_malloc0(sizeof(struct _EMFolderBrowserPrivate));

	emfb->view.preview_active = TRUE;
	emfb->view.list_active = TRUE;

	g_slist_foreach (emfb->view.ui_files, free_one_ui_file, NULL);
	g_slist_free(emfb->view.ui_files);

	emfb->view.ui_files = g_slist_append(NULL,
					     g_build_filename (EVOLUTION_UIDIR,
							       "evolution-mail-global.xml",
							       NULL));
	emfb->view.ui_files = g_slist_append(emfb->view.ui_files,
					     g_build_filename (EVOLUTION_UIDIR,
							       "evolution-mail-list.xml",
							       NULL));
	emfb->view.ui_files = g_slist_append(emfb->view.ui_files,
					     g_build_filename (EVOLUTION_UIDIR,
							       "evolution-mail-message.xml",
							       NULL));

	emfb->view.enable_map = g_slist_prepend(emfb->view.enable_map, (void *)emfb_enable_map);

	if (search_context) {
		const char *systemrules = g_object_get_data (G_OBJECT (search_context), "system");
		const char *userrules = g_object_get_data (G_OBJECT (search_context), "user");

		emfb->search = e_filter_bar_new(search_context, systemrules, userrules, emfb_search_config_search, emfb);
		e_search_bar_set_menu ((ESearchBar *)emfb->search, emfb_search_items);
		e_search_bar_set_scopeoption ((ESearchBar *)emfb->search, emfb_search_scope_items);

		menu = generate_viewoption_menu();
		e_search_bar_set_viewoption_menu ((ESearchBar *)emfb->search, menu);
		gtk_widget_show((GtkWidget *)emfb->search);

		p->search_menu_activated_id = g_signal_connect(emfb->search, "menu_activated", G_CALLBACK(emfb_search_menu_activated), emfb);
		p->search_activated_id = g_signal_connect(emfb->search, "search_activated", G_CALLBACK(emfb_search_search_activated), emfb);
		p->search_query_changed_id = g_signal_connect(emfb->search, "query_changed", G_CALLBACK(emfb_search_query_changed), emfb);

		gtk_box_pack_start((GtkBox *)emfb, (GtkWidget *)emfb->search, FALSE, TRUE, 0);
	}

	emfb->priv->show_wide = gconf_client_get_bool(mail_config_get_gconf_client(), "/apps/evolution/mail/display/show_wide", NULL);
	emfb->vpane = emfb->priv->show_wide?gtk_hpaned_new():gtk_vpaned_new();

	g_signal_connect(emfb->vpane, "realize", G_CALLBACK(emfb_pane_realised), emfb);
	emfb->priv->vpane_resize_id = g_signal_connect(emfb->vpane, "button_release_event", G_CALLBACK(emfb_pane_button_release_event), emfb);

	gtk_widget_show(emfb->vpane);

	gtk_box_pack_start_defaults((GtkBox *)emfb, emfb->vpane);
	
	gtk_paned_add1((GtkPaned *)emfb->vpane, (GtkWidget *)emfb->view.list);
	gtk_widget_show((GtkWidget *)emfb->view.list);

	/* currently: just use a scrolledwindow for preview widget */
	p->preview = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy((GtkScrolledWindow *)p->preview, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type((GtkScrolledWindow *)p->preview, GTK_SHADOW_IN);
	gtk_widget_show(p->preview);

	gtk_container_add((GtkContainer *)p->preview, (GtkWidget *)emfb->view.preview->formathtml.html);
	gtk_widget_show((GtkWidget *)emfb->view.preview->formathtml.html);

	gtk_paned_add2((GtkPaned *)emfb->vpane, p->preview);
	gtk_widget_show(p->preview);

	g_signal_connect (((EMFolderView *) emfb)->list->tree, "key_press", G_CALLBACK(emfb_list_key_press), emfb);
	g_signal_connect (((EMFolderView *) emfb)->list, "message_selected", G_CALLBACK (emfb_list_message_selected), emfb);

}

static void
emfb_finalise(GObject *o)
{
	EMFolderBrowser *emfb = (EMFolderBrowser *)o;
	
	g_free (emfb->priv->select_uid);
	g_free (emfb->priv);

	((GObjectClass *)emfb_parent)->finalize(o);
}

static void
emfb_destroy(GtkObject *o)
{
	EMFolderBrowser *emfb = (EMFolderBrowser *)o;

	if (emfb->priv->list_built_id) {
		g_signal_handler_disconnect(((EMFolderView *)emfb)->list, emfb->priv->list_built_id);
		emfb->priv->list_built_id = 0;
	}
	
	if (emfb->priv->list_scrolled_id) {
		g_signal_handler_disconnect (((EMFolderView *) emfb)->list, emfb->priv->list_scrolled_id);
		emfb->priv->list_scrolled_id = 0;
	}
	
	if (emfb->priv->idle_scroll_id) {
		g_source_remove (emfb->priv->idle_scroll_id);
		emfb->priv->idle_scroll_id = 0;
	}

	if (emfb->view.folder && emfb->priv->folder_changed_id)
		camel_object_remove_event(emfb->view.folder, emfb->priv->folder_changed_id);
	
	((GtkObjectClass *)emfb_parent)->destroy(o);
}

static void
emfb_class_init(GObjectClass *klass)
{
	klass->finalize = emfb_finalise;

	folder_browser_signals[ACCOUNT_SEARCH_ACTIVATED] =
		g_signal_new ("account_search_activated",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EMFolderBrowserClass, account_search_activated),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);

	folder_browser_signals[ACCOUNT_SEARCH_CLEARED] =
		g_signal_new ("account_search_cleared",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EMFolderBrowserClass, account_search_cleared),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);


	((GtkObjectClass *)klass)->destroy = emfb_destroy;
	((EMFolderViewClass *)klass)->set_folder = emfb_set_folder;
	((EMFolderViewClass *)klass)->activate = emfb_activate;
}

GType
em_folder_browser_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(EMFolderBrowserClass),
			NULL, NULL,
			(GClassInitFunc)emfb_class_init,
			NULL, NULL,
			sizeof(EMFolderBrowser), 0,
			(GInstanceInitFunc)emfb_init
		};
		emfb_parent = g_type_class_ref(em_folder_view_get_type());
		type = g_type_register_static(em_folder_view_get_type(), "EMFolderBrowser", &info, 0);
	}

	return type;
}

GtkWidget *em_folder_browser_new(void)
{
	EMFolderBrowser *emfb = g_object_new(em_folder_browser_get_type(), 0);

	/** @HookPoint-EMMenu: Main Mail Menu
	 * @Id: org.gnome.evolution.mail.browser
	 * @Class: org.gnome.evolution.mail.bonobomenu:1.0
	 * @Target: EMMenuTargetSelect
	 *
	 * The main menu of mail view of the main application window. 
	 * If the folder is NULL (not selected), the target will be empty, not NULL.
	 */
	((EMFolderView *)emfb)->menu = em_menu_new("org.gnome.evolution.mail.browser");

	return (GtkWidget *)emfb;
}

void em_folder_browser_show_preview(EMFolderBrowser *emfb, gboolean state)
{
	if ((emfb->view.preview_active ^ state) == 0
	    || emfb->view.list == NULL)
		return;
	
	emfb->view.preview_active = state;
	
	if (state) {
		GConfClient *gconf = mail_config_get_gconf_client ();
		int paned_size /*, y*/;

		paned_size = gconf_client_get_int(gconf, emfb->priv->show_wide ? "/apps/evolution/mail/display/hpaned_size":"/apps/evolution/mail/display/paned_size", NULL);

		/*y = save_cursor_pos (emfb);*/
		gtk_paned_set_position (GTK_PANED (emfb->vpane), paned_size);
		gtk_widget_show (GTK_WIDGET (emfb->priv->preview));

		if (emfb->view.list->cursor_uid) {
			char *uid = g_alloca(strlen(emfb->view.list->cursor_uid)+1);

			strcpy(uid, emfb->view.list->cursor_uid);
			em_folder_view_set_message(&emfb->view, uid, FALSE);
		}

		/* need to load/show the current message? */
		/*do_message_selected (emfb);*/
		/*set_cursor_pos (emfb, y);*/
	} else {
		em_format_format((EMFormat *)emfb->view.preview, NULL, NULL, NULL);

		g_free(emfb->view.displayed_uid);
		emfb->view.displayed_uid = NULL;

		gtk_widget_hide(emfb->priv->preview);
		/*
		mail_display_set_message (emfb->mail_display, NULL, NULL, NULL);
		emfb_ui_message_loaded (emfb);*/
	}

	/* FIXME: need to update menu's to reflect ui changes */
}

void em_folder_browser_show_wide(EMFolderBrowser *emfb, gboolean state)
{
	GtkWidget *w;
	int paned_size;

	if ((emfb->priv->show_wide &&  state)
	    || emfb->view.list == NULL) {
		emfb->priv->show_wide = state;
		return;
	}

	emfb->priv->show_wide = state;

	w = emfb->priv->show_wide?gtk_hpaned_new():gtk_vpaned_new();

	g_signal_handler_disconnect(emfb->vpane, emfb->priv->vpane_resize_id);
	g_signal_connect(w, "realize", G_CALLBACK(emfb_pane_realised), emfb);
	emfb->priv->vpane_resize_id = g_signal_connect(w, "button_release_event", G_CALLBACK(emfb_pane_button_release_event), emfb);

	gtk_box_pack_start_defaults((GtkBox *)emfb, w);
	gtk_widget_reparent((GtkWidget *)emfb->view.list, w);
	gtk_widget_reparent((GtkWidget *)emfb->priv->preview, w);
	gtk_widget_destroy(emfb->vpane);
	gtk_container_resize_children (w);
	emfb->vpane = w;
	gtk_widget_show(w);

	paned_size = gconf_client_get_int(mail_config_get_gconf_client(), emfb->priv->show_wide ? "/apps/evolution/mail/display/hpaned_size":"/apps/evolution/mail/display/paned_size", NULL);
	gtk_paned_set_position (GTK_PANED (emfb->vpane), paned_size);

	if (((EMFolderView *)emfb)->folder)
		em_folder_view_setup_view_instance ((EMFolderView *) emfb);
}

/* ********************************************************************** */

/* FIXME: Need to separate system rules from user ones */
/* FIXME: Ugh! */

static void
emfb_search_menu_activated(ESearchBar *esb, int id, EMFolderBrowser *emfb)
{
	EFilterBar *efb = (EFilterBar *)esb;
	
	d(printf("menu activated\n"));
	
	switch (id) {
	case ESB_SAVE:
		d(printf("Save vfolder\n"));
		if (efb->current_query) {
			FilterRule *rule = vfolder_clone_rule(efb->current_query);			
			char *name, *text;
			
			text = e_search_bar_get_text(esb);
			name = g_strdup_printf("%s %s", rule->name, (text&&text[0])?text:"''");
			g_free (text);
			filter_rule_set_name(rule, name);
			g_free (name);
			
			filter_rule_set_source(rule, FILTER_SOURCE_INCOMING);
			em_vfolder_rule_add_source((EMVFolderRule *)rule, emfb->view.folder_uri);
			vfolder_gui_add_rule((EMVFolderRule *)rule);
		}
		break;
	}
}

static void
emfb_search_config_search(EFilterBar *efb, FilterRule *rule, int id, const char *query, void *data)
{
	EMFolderBrowser *emfb = data;
	GList *partl;
	struct _camel_search_words *words;
	int i;
	GSList *strings = NULL;

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
			
			words = camel_search_words_split(query);
			for (i=0;i<words->len;i++)
				strings = g_slist_prepend(strings, g_strdup(words->words[i]->word));
			camel_search_words_free (words);
		} else if(!strcmp(part->name, "sender")) {
			FilterInput *input = (FilterInput *)filter_part_find_element(part, "sender");
			if (input)
				filter_input_set_value(input, query);
		} else if(!strcmp(part->name, "to")) {
			FilterInput *input = (FilterInput *)filter_part_find_element(part, "recipient");
			if (input)
				filter_input_set_value(input, query);
		}
		
		partl = partl->next;
	}

	em_format_html_display_set_search(emfb->view.preview,
					  EM_FORMAT_HTML_DISPLAY_SEARCH_SECONDARY|EM_FORMAT_HTML_DISPLAY_SEARCH_ICASE,
					  strings);
	while (strings) {
		GSList *n = strings->next;
		g_free(strings->data);
		g_slist_free_1(strings);
		strings = n;
	}
}

static char *
get_view_query (ESearchBar *esb)
{
	char *view_sexp = NULL;
	gint id;
	GtkWidget *menu_item;
	char *tag;

	/* Get the current selected view */
	id = e_search_bar_get_viewitem_id (esb);
	menu_item = e_search_bar_get_selected_viewitem (esb);

	switch (id) {
	case VIEW_ALL_MESSAGES:
		view_sexp = " ";
		break;

	case VIEW_UNREAD_MESSAGES:
		view_sexp = "(match-all (not (system-flag  \"Seen\")))";
		break;
	case VIEW_READ_MESSAGES:
		view_sexp = "(match-all (system-flag  \"Seen\"))";
		break;
        case VIEW_RECENT_MESSAGES:
		view_sexp = "(match-all (> (get-received-date) (- (get-current-date) 86400)))";
		break;
	case VIEW_LAST_FIVE_DAYS:
		view_sexp = " (match-all (> (get-received-date) (- (get-current-date) 432000)))";
		break;
        case VIEW_WITH_ATTACHMENTS:
		view_sexp = "(match-all (system-flag \"Attachments\"))";
		break;
	case VIEW_NOT_JUNK:
		view_sexp = "(match-all (not (system-flag \"junk\")))";
		break;
        case VIEW_NO_LABEL:
		/* FIXME : cannot hard code this query */
	        view_sexp = "(and (match-all (not (= (user-tag \"label\")  \"important\")))"
                            "(match-all (not (= (user-tag \"label\")  \"work\"))) (match-all (not (= (user-tag \"label\")  \"personal\")))" 
			    "(match-all (not (= (user-tag \"label\")  \"todo\"))) (match-all (not (= (user-tag \"label\")  \"later\"))) ))";
		break;
        case VIEW_LABEL:
		tag = (char *)g_object_get_data (G_OBJECT (menu_item), "LabelTag");
		view_sexp = g_strdup_printf ("(match-all (= (user-tag \"label\")  \"%s\"))",tag);
		break;
	case VIEW_ANY_FIELD_CONTAINS:
		break;

	case VIEW_CUSTOMIZE:
		view_sexp = " ";
		break;
	}
	return view_sexp;
}


struct _setup_msg {
	struct _mail_msg msg;

	CamelFolder *folder;
	char *query;
	GList *sources_uri;
	GList *sources_folder;
};

static char *
vfolder_setup_desc(struct _mail_msg *mm, int done)
{
	struct _setup_msg *m = (struct _setup_msg *)mm;

	return g_strdup(_("Searching"));
}

static void
vfolder_setup_do(struct _mail_msg *mm)
{
	struct _setup_msg *m = (struct _setup_msg *)mm;
	GList *l, *list = NULL;
	CamelFolder *folder;

	d(printf("Setting up Search Folder: %s\n", m->folder->full_name));

	camel_vee_folder_set_expression((CamelVeeFolder *)m->folder, m->query);

	l = m->sources_uri;
	while (l) {
		d(printf(" Adding uri: %s\n", (char *)l->data));
		folder = mail_tool_uri_to_folder (l->data, 0, &mm->ex);
		if (folder) {
			list = g_list_append(list, folder);
		} else {
			g_warning("Could not open vfolder source: %s", (char *)l->data);
			camel_exception_clear(&mm->ex);
		}
		l = l->next;
	}

	l = m->sources_folder;
	while (l) {
		d(printf(" Adding folder: %s\n", ((CamelFolder *)l->data)->full_name));
		camel_object_ref(l->data);
		list = g_list_append(list, l->data);
		l = l->next;
	}

	camel_vee_folder_set_folders((CamelVeeFolder *)m->folder, list);

	l = list;
	while (l) {
		camel_object_unref(l->data);
		l = l->next;
	}
	g_list_free(list);
}

static void
vfolder_setup_done(struct _mail_msg *mm)
{
	struct _setup_msg *m = (struct _setup_msg *)mm;

	m = m;
}

static void
vfolder_setup_free (struct _mail_msg *mm)
{
	struct _setup_msg *m = (struct _setup_msg *)mm;
	GList *l;

	camel_object_unref(m->folder);
	g_free(m->query);

	l = m->sources_uri;
	while (l) {
		g_free(l->data);
		l = l->next;
	}
	g_list_free(m->sources_uri);

	l = m->sources_folder;
	while (l) {
		camel_object_unref(l->data);
		l = l->next;
	}
	g_list_free(m->sources_folder);
}

static struct _mail_msg_op vfolder_setup_op = {
	vfolder_setup_desc,
	vfolder_setup_do,
	vfolder_setup_done,
	vfolder_setup_free,
};

/* sources_uri should be camel uri's */
static int
vfolder_setup(CamelFolder *folder, const char *query, GList *sources_uri, GList *sources_folder)
{
	struct _setup_msg *m;
	int id;
	
	m = mail_msg_new(&vfolder_setup_op, NULL, sizeof (*m));
	m->folder = folder;
	camel_object_ref(folder);
	m->query = g_strdup(query);
	m->sources_uri = sources_uri;
	m->sources_folder = sources_folder;
	
	id = m->msg.seq;
	e_thread_put(mail_thread_queued_slow, (EMsg *)m);

	return id;
}

static void
emfb_search_search_activated(ESearchBar *esb, EMFolderBrowser *emfb)
{
	EMFolderView *emfv = (EMFolderView *) emfb;
	EFilterBar *efb = (EFilterBar *)esb;
	char *search_state, *view_sexp, *folder_uri;
	char *word = NULL, *storeuri = NULL, *search_word = NULL;;
	gint id, i;
	CamelFolder *folder;
	CamelStore *store;
	GPtrArray *folders;
	GList *folder_list_account = NULL ;
	GList *l, *folder_list = NULL ;
	CamelException *ex;

	ex = camel_exception_new ();

	if (emfv->list == NULL || emfv->folder == NULL)
		return;
	
	id = e_search_bar_get_search_scope (esb);

	switch (id) {
	    case E_FILTERBAR_CURRENT_MESSAGE_ID:
		    word = e_search_bar_get_text (esb);
		    if ( word && *word ) {
			    gtk_widget_set_sensitive (esb->option_button, FALSE);
			    em_format_html_display_search_with (emfb->view.preview, word);
		    } else {
			    em_format_html_display_search_close (emfb->view.preview);
		    }
		    return;
		    break;

	    case E_FILTERBAR_CURRENT_FOLDER_ID:
		    g_object_get (esb, "query", &search_word, NULL);
		    break;

	    case E_FILTERBAR_CURRENT_ACCOUNT_ID:
		    word = e_search_bar_get_text (esb);
		    if (!(word && *word)) {
			    g_signal_emit (emfb, folder_browser_signals [ACCOUNT_SEARCH_CLEARED], 0);
			    gtk_widget_set_sensitive (esb->scopeoption, TRUE);
			    break;
		    }

		    gtk_widget_set_sensitive (esb->scopeoption, FALSE);		    

		    /* Disable the folder tree */
		    g_signal_emit (emfb, folder_browser_signals [ACCOUNT_SEARCH_ACTIVATED], 0);

		    store = emfv->folder->parent_store;
		    if (store->folders) {
			    folders = camel_object_bag_list(store->folders);
			    for (i=0;i<folders->len;i++) {
				    folder = folders->pdata[i];
				    folder_list_account = g_list_append(folder_list_account, folder);
			    }
		    }

		    /* Create a camel vee folder */
		    storeuri = g_strdup_printf("vfolder:%s/mail/vfolder", mail_component_peek_base_directory (mail_component_peek ()));
		    vfolder_store = camel_session_get_store (session, storeuri, NULL);
		    efb->account_search_vf = camel_vee_folder_new (vfolder_store,"Account Search",CAMEL_STORE_VEE_FOLDER_AUTO);

		    /* Set the search expression  */
		    g_object_get (esb, "query", &search_word, NULL);

		    vfolder_setup (efb->account_search_vf, search_word, NULL, folder_list_account);

		    folder_uri = mail_tools_folder_to_url ((CamelFolder *)efb->account_search_vf);
		    emfb_set_search_folder (emfv, (CamelFolder *)efb->account_search_vf, folder_uri);

/* 		    g_list_free (folder_list_account); */
/* 		    g_free (folder_uri); */
/* 		    g_free (storeuri); */
		    break;

	    case E_FILTERBAR_ALL_ACCOUNTS_ID:
		    word = e_search_bar_get_text (esb);
		    if (!(word && *word)) {
			    g_signal_emit (emfb, folder_browser_signals [ACCOUNT_SEARCH_CLEARED], 0);
			    gtk_widget_set_sensitive (esb->scopeoption, TRUE);		    
			    break;
		    }
		    
		    gtk_widget_set_sensitive (esb->scopeoption, FALSE);		    
		    g_signal_emit (emfb, folder_browser_signals [ACCOUNT_SEARCH_ACTIVATED], 0);

		    /* Create a camel vee folder */
		    storeuri = g_strdup_printf("vfolder:%s/mail/vfolder", mail_component_peek_base_directory (mail_component_peek ()));
		    vfolder_store = camel_session_get_store (session, storeuri, NULL);
		    efb->all_account_search_vf = camel_vee_folder_new (vfolder_store,"All Account Search",CAMEL_STORE_VEE_FOLDER_AUTO);

		    /* Set sexp  */
		    g_object_get (esb, "query", &search_word, NULL);

		    /* FIXME: there got to be a better way :) */

		    /* Add the local folders */
		    l = mail_vfolder_get_sources_local ();
		    while (l) {
			    folder = mail_tool_uri_to_folder ((const char *)l->data, 0,ex);
			    if (folder)
				    folder_list = g_list_append(folder_list, folder);
			    else {
				    g_warning("Could not open vfolder source: %s", (char *)l->data);
				    camel_exception_clear(ex);
			    }
			    l = l->next;
		    }

		    /* Add the remote source folder */
		    l = mail_vfolder_get_sources_remote ();
		    while (l) {
			    folder = mail_tool_uri_to_folder ((const char *)l->data, 0,ex);
			    if (folder)
				    folder_list = g_list_append(folder_list, folder);
			    else {
				    g_warning("Could not open vfolder source: %s", (char *)l->data);
				    camel_exception_clear(ex);
			    }
			    l = l->next;
		    }

		    vfolder_setup (efb->all_account_search_vf, search_word, NULL, folder_list);

		    folder_uri = mail_tools_folder_to_url ((CamelFolder *)efb->all_account_search_vf);
		    emfb_set_search_folder (emfv, (CamelFolder *)efb->all_account_search_vf, folder_uri);

		    g_list_free (l);
		    break;
	}

	/* Merge the view and search expresion*/
	view_sexp = get_view_query (esb);
	g_object_get (esb, "query", &search_word, NULL);

	if (search_word && *search_word)
		search_word = g_strconcat ("(and ", view_sexp, search_word, " )", NULL);
	else
		search_word = g_strdup (view_sexp);

	message_list_set_search(emfb->view.list, search_word);

	/* Fixme */
	g_object_get (esb, "state", &search_state, NULL);
	camel_object_meta_set (emfv->folder, "evolution:search_state", search_state);
	camel_object_state_write (emfv->folder);

	camel_exception_free (ex);
}

static void
emfb_search_query_changed(ESearchBar *esb, EMFolderBrowser *emfb)
{
	int search_scope;
	int item_id;

	search_scope = e_search_bar_get_search_scope (esb);
	item_id = e_search_bar_get_item_id (esb);

	/* Close the current message search bar */
	if ( search_scope != E_FILTERBAR_CURRENT_MESSAGE_ID ) {
		em_format_html_display_search_close (emfb->view.preview);
		gtk_widget_set_sensitive (esb->option_button, TRUE);
	} else
		gtk_widget_set_sensitive (esb->option_button, FALSE);
	
	switch (item_id) {
	    case E_FILTERBAR_ADVANCED_ID:
		    emfb_search_search_activated(esb, emfb);
		    break;
	}
}

/* ********************************************************************** */

static int
emfb_list_key_press(ETree *tree, int row, ETreePath path, int col, GdkEvent *ev, EMFolderBrowser *emfb)
{
	if ((ev->key.state & GDK_CONTROL_MASK) != 0)
		return FALSE;
	
	switch (ev->key.keyval) {
	case GDK_space:
		em_utils_adjustment_page(gtk_scrolled_window_get_vadjustment((GtkScrolledWindow *)emfb->priv->preview), TRUE);
		break;
	case GDK_BackSpace:
		em_utils_adjustment_page(gtk_scrolled_window_get_vadjustment((GtkScrolledWindow *)emfb->priv->preview), FALSE);
		break;
	default:
		return FALSE;
	}
	
	return TRUE;
}

static void
emfb_list_message_selected (MessageList *ml, const char *uid, EMFolderBrowser *emfb)
{
	EMFolderView *emfv = (EMFolderView *) emfb;
	
	if (emfv->folder == NULL)
		return;
	
	camel_object_meta_set (emfv->folder, "evolution:selected_uid", uid);
	camel_object_state_write (emfv->folder);
}

/* ********************************************************************** */

static void
emfb_edit_cut(BonoboUIComponent *uid, void *data, const char *path)
{
	EMFolderBrowser *emfb = data;

	/* TODO: pity we can't sucblass this method, ugh, virtualise it? */

/* 	if (GTK_HAS_FOCUS(((ESearchBar *)emfb->search)->entry)) */
/* 		gtk_editable_cut_clipboard((GtkEditable *)((ESearchBar *)emfb->search)->entry); */
/* 	else if (GTK_WIDGET_HAS_FOCUS(emfb->view.preview->formathtml.html)) */
/* 		em_format_html_display_cut(emfb->view.preview); */
/* 	else */
/* 		message_list_copy(emfb->view.list, TRUE); */
}

static void
emfb_edit_copy(BonoboUIComponent *uid, void *data, const char *path)
{
	EMFolderBrowser *emfb = data;

	if (GTK_WIDGET_HAS_FOCUS(((ESearchBar *)emfb->search)->entry))
		gtk_editable_copy_clipboard((GtkEditable *)((ESearchBar *)emfb->search)->entry);
	else if (GTK_WIDGET_HAS_FOCUS(emfb->view.preview->formathtml.html))
		em_format_html_display_copy(emfb->view.preview);
	else
		message_list_copy(emfb->view.list, FALSE);
}

static void
emfb_edit_paste(BonoboUIComponent *uid, void *data, const char *path)
{
	EMFolderBrowser *emfb = data;

	if (GTK_WIDGET_HAS_FOCUS(((ESearchBar *)emfb->search)->entry))
		gtk_editable_paste_clipboard((GtkEditable *)((ESearchBar *)emfb->search)->entry);
	else
		message_list_paste(emfb->view.list);
}

static void
emfb_edit_invert_selection(BonoboUIComponent *uid, void *data, const char *path)
{
	EMFolderView *emfv = data;
	
	message_list_invert_selection(emfv->list);
}

static void
emfb_edit_select_all(BonoboUIComponent *uid, void *data, const char *path)
{
	EMFolderView *emfv = data;
	
	message_list_select_all(emfv->list);
	gtk_widget_grab_focus (emfv->list);
}

static void
emfb_edit_select_thread(BonoboUIComponent *uid, void *data, const char *path)
{
	EMFolderView *emfv = data;
	
	message_list_select_thread(emfv->list);
}

static void
emfb_folder_properties(BonoboUIComponent *uid, void *data, const char *path)
{
	EMFolderBrowser *emfb = data;

	if (emfb->view.folder_uri)
		em_folder_properties_show(NULL, emfb->view.folder, emfb->view.folder_uri);
}

/* VIEWTHREADED*/
static void
emfb_expand_all_threads(BonoboUIComponent *uid, void *data, const char *path)
{
	EMFolderView *emfv = data;

	message_list_set_threaded_expand_all(emfv->list);

}

static void
emfb_collapse_all_threads(BonoboUIComponent *uid, void *data, const char *path)
{
	EMFolderView *emfv = data;

	message_list_set_threaded_collapse_all(emfv->list);
}

static void
emfb_folder_copy(BonoboUIComponent *uid, void *data, const char *path)
{
	EMFolderBrowser *emfb = data;
	CamelFolderInfo *fi = NULL;
	CamelException ex;

	/* FIXME: This function MUST become multi-threaded.
	   FIXME: This interface should NOT use a folderinfo */

	camel_exception_init (&ex);

	if ((fi = camel_store_get_folder_info (emfb->view.folder->parent_store,
					       emfb->view.folder->full_name,
					       CAMEL_STORE_FOLDER_INFO_FAST,
					       &ex)) != NULL)
		em_folder_utils_copy_folder(fi, FALSE);

	camel_exception_clear (&ex);

	return;
}

static void
emfb_folder_move(BonoboUIComponent *uid, void *data, const char *path)
{
	EMFolderBrowser *emfb = data;
	CamelFolderInfo *fi = NULL;
	CamelException ex;

	camel_exception_init (&ex);

	/* FIXME: This function MUST become multi-threaded.
	   FIXME: This interface should NOT use a folderinfo */

	if ((fi = camel_store_get_folder_info (emfb->view.folder->parent_store,
					       emfb->view.folder->full_name,
					       CAMEL_STORE_FOLDER_INFO_FAST,
					       &ex)) != NULL)
		em_folder_utils_copy_folder(fi, TRUE);

	camel_exception_clear (&ex);

	return;
}

static void
emfb_folder_delete(BonoboUIComponent *uid, void *data, const char *path)
{
	EMFolderBrowser *emfb = data;

	em_folder_utils_delete_folder (emfb->view.folder);

	return;
}

static void
emfb_folder_rename(BonoboUIComponent *uid, void *data, const char *path)
{
	EMFolderBrowser *emfb = data;

	em_folder_utils_rename_folder (emfb->view.folder);

	return;
}

static void
emfb_folder_create(BonoboUIComponent *uid, void *data, const char *path)
{
	EMFolderBrowser *emfb = data;
	CamelFolderInfo *fi = NULL;
	CamelException ex;

	camel_exception_init (&ex);

	/* FIXME: This function MUST be multithreaded
	   FIXME: This interface should NOT use a folderinfo */
	if (emfb->view.folder) {
		if ((fi = camel_store_get_folder_info (emfb->view.folder->parent_store,
						       emfb->view.folder->full_name,
						       CAMEL_STORE_FOLDER_INFO_FAST,
						       &ex)) != NULL)
			em_folder_utils_create_folder(fi);
	} else {
		em_folder_utils_create_folder(NULL);
	}
			

	camel_exception_clear (&ex);

	return;
}

static void
emfb_folder_expunge(BonoboUIComponent *uid, void *data, const char *path)
{
	EMFolderBrowser *emfb = data;

	if (emfb->view.folder)
		em_utils_expunge_folder(gtk_widget_get_toplevel((GtkWidget *)emfb), emfb->view.folder);
}

static void
emfb_mark_all_read(BonoboUIComponent *uid, void *data, const char *path)
{
	/* FIXME: make a 'mark messages' function? */
	EMFolderView *emfv = data;
	GPtrArray *uids;
	int i;

	if (emfv->folder == NULL)
		return;
	if( em_utils_prompt_user((GtkWindow *)emfv, "/apps/evolution/mail/prompts/mark_all_read","mail:ask-mark-all-read", NULL)){
		uids = message_list_get_uids(emfv->list);
		camel_folder_freeze(emfv->folder);
		for (i=0;i<uids->len;i++)
			camel_folder_set_message_flags(emfv->folder, uids->pdata[i], CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);
		camel_folder_thaw(emfv->folder);
		message_list_free_uids(emfv->list, uids);
	}
}

static void
emfb_view_hide_read(BonoboUIComponent *uid, void *data, const char *path)
{
	EMFolderView *emfv = data;
	
	message_list_hide_add(emfv->list, "(match-all (system-flag \"seen\"))", ML_HIDE_SAME, ML_HIDE_SAME);
}

static void
emfb_view_hide_selected(BonoboUIComponent *uid, void *data, const char *path)
{
	EMFolderView *emfv = data;
	GPtrArray *uids;

	/* TODO: perhaps this should sit directly on message_list? */
	/* is it worth it, it's so trivial */
	uids = message_list_get_selected(emfv->list);
	message_list_hide_uids(emfv->list, uids);
	message_list_free_uids(emfv->list, uids);
}

static void
emfb_view_show_all(BonoboUIComponent *uid, void *data, const char *path)
{
	EMFolderView *emfv = data;

	message_list_hide_clear(emfv->list);
}

/* ********************************************************************** */

static void
emfb_mail_stop(BonoboUIComponent *uid, void *data, const char *path)
{
	mail_cancel_all();
}

static void
emfb_mail_post(BonoboUIComponent *uid, void *data, const char *path)
{
	EMFolderView *emfv = data;
	em_utils_post_to_folder (emfv->folder);
}

static void
emfb_tools_filters(BonoboUIComponent *uid, void *data, const char *path)
{
	EMFolderBrowser *emfb = data;
	
	em_utils_edit_filters ((GtkWidget *) emfb);
}

static void
emfb_subscribe_editor_destroy(GtkWidget *w, EMFolderBrowser *emfb)
{
	emfb->priv->subscribe_editor = NULL;
}

static void
emfb_tools_subscriptions(BonoboUIComponent *uid, void *data, const char *path)
{
	EMFolderBrowser *emfb = data;

	if (emfb->priv->subscribe_editor) {
		gdk_window_show(emfb->priv->subscribe_editor->window);
	} else {
		emfb->priv->subscribe_editor = (GtkWidget *)em_subscribe_editor_new();
		e_dialog_set_transient_for((GtkWindow *)emfb->priv->subscribe_editor, (GtkWidget *)emfb);
		g_signal_connect(emfb->priv->subscribe_editor, "destroy", G_CALLBACK(emfb_subscribe_editor_destroy), emfb);
		gtk_widget_show(emfb->priv->subscribe_editor);
	}
}

static void
emfb_focus_search(BonoboUIComponent *uid, void *data, const char *path)
{
	EMFolderBrowser *emfb = data;

	gtk_widget_grab_focus (((ESearchBar *)emfb->search)->entry);
}

static void
emfb_tools_vfolders(BonoboUIComponent *uid, void *data, const char *path)
{
	/* FIXME: rename/refactor this */
	vfolder_edit();
}

static BonoboUIVerb emfb_verbs[] = {
	BONOBO_UI_UNSAFE_VERB ("EditCut", emfb_edit_cut),
	BONOBO_UI_UNSAFE_VERB ("EditCopy", emfb_edit_copy),
	BONOBO_UI_UNSAFE_VERB ("EditPaste", emfb_edit_paste),

	BONOBO_UI_UNSAFE_VERB ("EditInvertSelection", emfb_edit_invert_selection),
	BONOBO_UI_UNSAFE_VERB ("EditSelectAll", emfb_edit_select_all),
        BONOBO_UI_UNSAFE_VERB ("EditSelectThread", emfb_edit_select_thread),
	BONOBO_UI_UNSAFE_VERB ("ChangeFolderProperties", emfb_folder_properties),
	BONOBO_UI_UNSAFE_VERB ("FolderExpunge", emfb_folder_expunge),
	/* HideDeleted is a toggle */
	BONOBO_UI_UNSAFE_VERB ("MessageMarkAllAsRead", emfb_mark_all_read),
	BONOBO_UI_UNSAFE_VERB ("ViewHideRead", emfb_view_hide_read),
	BONOBO_UI_UNSAFE_VERB ("ViewHideSelected", emfb_view_hide_selected),
	BONOBO_UI_UNSAFE_VERB ("ViewShowAll", emfb_view_show_all),
	/* ViewThreaded is a toggle */

	BONOBO_UI_UNSAFE_VERB ("ViewThreadsExpandAll", emfb_expand_all_threads),
	BONOBO_UI_UNSAFE_VERB ("ViewThreadsCollapseAll", emfb_collapse_all_threads),	

	BONOBO_UI_UNSAFE_VERB ("FolderCopy", emfb_folder_copy),
	BONOBO_UI_UNSAFE_VERB ("FolderMove", emfb_folder_move),
	BONOBO_UI_UNSAFE_VERB ("FolderDelete", emfb_folder_delete),
	BONOBO_UI_UNSAFE_VERB ("FolderRename", emfb_folder_rename),
	BONOBO_UI_UNSAFE_VERB ("FolderCreate", emfb_folder_create),

	BONOBO_UI_UNSAFE_VERB ("MailPost", emfb_mail_post),
	BONOBO_UI_UNSAFE_VERB ("MailStop", emfb_mail_stop),
	BONOBO_UI_UNSAFE_VERB ("ToolsFilters", emfb_tools_filters),
	BONOBO_UI_UNSAFE_VERB ("ToolsSubscriptions", emfb_tools_subscriptions),
	BONOBO_UI_UNSAFE_VERB ("ToolsVFolders", emfb_tools_vfolders),
	BONOBO_UI_UNSAFE_VERB ("FocusSearch", emfb_focus_search),
	
	/* ViewPreview is a toggle */

	BONOBO_UI_VERB_END
};

static EPixmap emfb_pixmaps[] = {
	E_PIXMAP ("/commands/ChangeFolderProperties", "stock_folder-properties", E_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/FolderCopy", "stock_folder-copy", E_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/FolderMove", "stock_folder-move", E_ICON_SIZE_MENU),

	E_PIXMAP_END
};

static const EMFolderViewEnable emfb_enable_map[] = {
	{ "EditInvertSelection", EM_POPUP_SELECT_FOLDER },
	{ "EditSelectAll", EM_POPUP_SELECT_FOLDER },
	{ "EditSelectThread", EM_FOLDER_VIEW_SELECT_THREADED },
	{ "FolderExpunge", EM_POPUP_SELECT_FOLDER },
	{ "FolderCopy", EM_POPUP_SELECT_FOLDER },
	{ "FolderMove", EM_POPUP_SELECT_FOLDER },
	{ "FolderDelete", EM_POPUP_SELECT_FOLDER },
	{ "FolderRename", EM_POPUP_SELECT_FOLDER },
	{ "MailPost", EM_POPUP_SELECT_FOLDER },
	{ "MessageMarkAllAsRead", EM_POPUP_SELECT_FOLDER },
	{ "ViewHideSelected", EM_POPUP_SELECT_MANY },
	{ "ViewThreadsCollapseAll", EM_FOLDER_VIEW_SELECT_THREADED},
	{ "ViewThreadsExpandAll", EM_FOLDER_VIEW_SELECT_THREADED},
	{ NULL },
};

static void
emfb_hide_deleted(BonoboUIComponent *uic, const char *path, Bonobo_UIComponent_EventType type, const char *state, void *data)
{
	GConfClient *gconf;
	EMFolderView *emfv = data;

	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	gconf = mail_config_get_gconf_client ();
	gconf_client_set_bool(gconf, "/apps/evolution/mail/display/show_deleted", state[0] == '0', NULL);
	em_folder_view_set_hide_deleted(emfv, state[0] != '0');
}

static void
emfb_view_threaded(BonoboUIComponent *uic, const char *path, Bonobo_UIComponent_EventType type, const char *state, void *data)
{
	GConfClient *gconf;
	EMFolderView *emfv = data;

	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	gconf = mail_config_get_gconf_client ();
	gconf_client_set_bool(gconf, "/apps/evolution/mail/display/thread_list", state[0] != '0', NULL);

	if (camel_object_meta_set(emfv->folder, "evolution:thread_list", state))
		camel_object_state_write(emfv->folder);

	/* FIXME: do set_threaded via meta-data listener on folder? */
	message_list_set_threaded(emfv->list, state[0] != '0');
	
	/* FIXME: update selection state? */
}

static void
emfb_view_preview(BonoboUIComponent *uic, const char *path, Bonobo_UIComponent_EventType type, const char *state, void *data)
{
	GConfClient *gconf;
	EMFolderView *emfv = data;

	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	gconf = mail_config_get_gconf_client ();
	gconf_client_set_bool(gconf, "/apps/evolution/mail/display/show_preview", state[0] != '0', NULL);

	if (camel_object_meta_set(emfv->folder, "evolution:show_preview", state))
		camel_object_state_write(emfv->folder);

	/* FIXME: do this via folder listener */
	em_folder_browser_show_preview((EMFolderBrowser *)emfv, state[0] != '0');
}

static void
emfb_show_next(BonoboUIComponent *uic, const char *path, Bonobo_UIComponent_EventType type, const char *state, void *data)
{
	GConfClient *gconf;
	EMFolderBrowser *emfb = data;

	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	gconf = mail_config_get_gconf_client ();
	gconf_client_set_bool(gconf, "/apps/evolution/mail/display/show_wide", state[0] != '0', NULL);

	em_folder_browser_show_wide(emfb, state[0] != '0');
}

static void
emfb_show_below(BonoboUIComponent *uic, const char *path, Bonobo_UIComponent_EventType type, const char *state, void *data)
{
	GConfClient *gconf;
	EMFolderBrowser *emfb = data;

	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	gconf = mail_config_get_gconf_client ();
	gconf_client_set_bool(gconf, "/apps/evolution/mail/display/show_wide", state[0] == '0', NULL);

	em_folder_browser_show_wide(emfb, state[0] == '0');
}

static void
emfb_list_scrolled (MessageList *ml, EMFolderBrowser *emfb)
{
	EMFolderView *emfv = (EMFolderView *) emfb;
	double position;
	char *state;
	
	position = message_list_get_scrollbar_position (ml);
	state = g_strdup_printf ("%f", position);
	
	if (camel_object_meta_set (emfv->folder, "evolution:list_scroll_position", state))
		camel_object_state_write (emfv->folder);
	
	g_free (state);
}

static gboolean
scroll_idle_cb (EMFolderBrowser *emfb)
{
	EMFolderView *emfv = (EMFolderView *) emfb;
	double position;
	char *state;
	
	if ((state = camel_object_meta_get (emfv->folder, "evolution:list_scroll_position"))) {
		position = strtod (state, NULL);
		g_free (state);
	} else {
		position = emfb->priv->default_scroll_position;
	}
	
	message_list_set_scrollbar_position (emfv->list, position);
	
	emfb->priv->list_scrolled_id = g_signal_connect (emfv->list, "message_list_scrolled", G_CALLBACK (emfb_list_scrolled), emfb);
	
	emfb->priv->idle_scroll_id = 0;
	
	return FALSE;
}

static void
emfb_gui_folder_changed(CamelFolder *folder, void *dummy, EMFolderBrowser *emfb)
{
	if (emfb->priv->select_uid) {
		CamelMessageInfo *mi;

		mi = camel_folder_get_message_info(emfb->view.folder, emfb->priv->select_uid);
		if (mi) {
			camel_folder_free_message_info(emfb->view.folder, mi);
			em_folder_view_set_message(&emfb->view, emfb->priv->select_uid, TRUE);
			g_free (emfb->priv->select_uid);
			emfb->priv->select_uid = NULL;
		}
	}

	g_object_unref(emfb);
}

static void
emfb_folder_changed(CamelFolder *folder, CamelFolderChangeInfo *changes, EMFolderBrowser *emfb)
{
	g_object_ref(emfb);
	mail_async_event_emit(emfb->view.async, MAIL_ASYNC_GUI, (MailAsyncFunc)emfb_gui_folder_changed, folder, NULL, emfb);
}

static void
emfb_etree_unfreeze (GtkWidget *widget, GdkEvent *event, EMFolderView *emfv)
{

	ETableItem *item = e_tree_get_item (emfv->list->tree);

	g_object_set_data (G_OBJECT (((GnomeCanvasItem *) item)->canvas), "freeze-cursor", 0);
}


/* TODO: This should probably be handled by message-list, by storing/queueing
   up the select operation if its busy rebuilding the message-list */
static void
emfb_list_built (MessageList *ml, EMFolderBrowser *emfb)
{
	EMFolderView *emfv = (EMFolderView *) emfb;
	double position = 0.0f;
	
	g_signal_handler_disconnect (ml, emfb->priv->list_built_id);
	emfb->priv->list_built_id = 0;
	
	if (emfv->list->cursor_uid == NULL) {
		if (emfb->priv->select_uid) {
			CamelMessageInfo *mi;

			/* If the message isn't in the folder yet, keep select_uid around, it could be caught by
			   folder_changed, at some later date */
			mi = camel_folder_get_message_info(emfv->folder, emfb->priv->select_uid);
			if (mi) {
				camel_folder_free_message_info(emfv->folder, mi);
				em_folder_view_set_message(emfv, emfb->priv->select_uid, TRUE);
				g_free (emfb->priv->select_uid);
				emfb->priv->select_uid = NULL;
			}
			
			/* change the default to the current position */
			position = message_list_get_scrollbar_position (ml);
		} else {
			/* NOTE: not all users want this, so we need a preference for it perhaps? see bug #52887 */
			/* FIXME: if the 1st message in the list is unread, this will actually select the second unread msg */
			/*message_list_select (ml, MESSAGE_LIST_SELECT_NEXT, 0, CAMEL_MESSAGE_SEEN, TRUE);*/
		}
	}
	
	emfb->priv->default_scroll_position = position;
	
	/* FIXME: this is a gross workaround for an etable bug that I can't fix - bug #55303 */
	/* this needs to be a lower priority than anything in e-table-item/e-canvas, since
	 * e_canvas_item_region_show_relay() uses a timeout, we have to use a timeout of the
	 * same interval but a lower priority. */
	emfb->priv->idle_scroll_id = g_timeout_add_full (G_PRIORITY_LOW, 250, (GSourceFunc) scroll_idle_cb, emfb, NULL);
	/* FIXME: This is another ugly hack done to hide a bug that above hack leaves. */
	g_signal_connect (((GtkScrolledWindow *) ml)->vscrollbar, "button-press-event", G_CALLBACK (emfb_etree_unfreeze), emfb);
}

static void
emfb_set_search_folder(EMFolderView *emfv, CamelFolder *folder, const char *uri)
{
	EMFolderBrowser *emfb = (EMFolderBrowser *) emfv;
	
	message_list_freeze(emfv->list);
	
	if (emfb->priv->list_scrolled_id) {
		g_signal_handler_disconnect (emfv->list, emfb->priv->list_scrolled_id);
		emfb->priv->list_scrolled_id = 0;
	}
	
	if (emfb->priv->idle_scroll_id) {
		g_source_remove (emfb->priv->idle_scroll_id);
		emfb->priv->idle_scroll_id = 0;
	}

	if (emfb->view.folder) {
		camel_object_remove_event(emfb->view.folder, emfb->priv->folder_changed_id);
		emfb->priv->folder_changed_id = 0;
	}

	emfb_parent->set_folder(emfv, folder, uri);

	/* etspec for search results */
	char *state = "<ETableState>"
			"<column source=\"0\"/> <column source=\"3\"/> <column source=\"1\"/>"
			"<column source=\"14\"/> <column source=\"5\"/>"
			"<column source=\"7\"/> <column source=\"13\"/> "
                        "<grouping> </grouping> </ETableState>";
	e_tree_set_state (((MessageList *)emfv->list)->tree, state);
	
	message_list_thaw(emfv->list);
}


static void
emfb_set_folder(EMFolderView *emfv, CamelFolder *folder, const char *uri)
{
	EMFolderBrowser *emfb = (EMFolderBrowser *) emfv;
	struct _EMFolderBrowserPrivate *p = emfb->priv;
	
	message_list_freeze(emfv->list);
	
	if (emfb->priv->list_scrolled_id) {
		g_signal_handler_disconnect (emfv->list, emfb->priv->list_scrolled_id);
		emfb->priv->list_scrolled_id = 0;
	}
	
	if (emfb->priv->idle_scroll_id) {
		g_source_remove (emfb->priv->idle_scroll_id);
		emfb->priv->idle_scroll_id = 0;
	}

	if (emfb->view.folder) {
		camel_object_remove_event(emfb->view.folder, emfb->priv->folder_changed_id);
		emfb->priv->folder_changed_id = 0;
	}

	emfb_parent->set_folder(emfv, folder, uri);
	
	/* This is required since we get activated the first time
	   before the folder is open and need to override the
	   defaults */
	if (folder) {
		char *sstate;
		int state;
		GConfClient *gconf = mail_config_get_gconf_client();
		
		mail_refresh_folder(folder, NULL, NULL);

		emfb->priv->folder_changed_id = camel_object_hook_event(folder, "folder_changed",
									(CamelObjectEventHookFunc)emfb_folder_changed, emfb);

		/* FIXME: this mostly copied from activate() */
		if ((sstate = camel_object_meta_get(folder, "evolution:show_preview"))) {
			state = sstate[0] != '0';
			g_free(sstate);
		} else
			state = gconf_client_get_bool(gconf, "/apps/evolution/mail/display/show_preview", NULL);
		em_folder_browser_show_preview(emfb, state);
		if (emfv->uic)
			bonobo_ui_component_set_prop(emfv->uic, "/commands/ViewPreview", "state", state?"1":"0", NULL);
		
		if ((sstate = camel_object_meta_get(folder, "evolution:thread_list"))) {
			state = sstate[0] != '0';
			g_free(sstate);
		} else
			state = gconf_client_get_bool(gconf, "/apps/evolution/mail/display/thread_list", NULL);
		message_list_set_threaded(emfv->list, state);
		if (emfv->uic) {
			bonobo_ui_component_set_prop(emfv->uic, "/commands/ViewThreaded", "state", state?"1":"0", NULL);
			bonobo_ui_component_set_prop(emfv->uic, "/commands/ViewThreadsCollapseAll", "sensitive", state?"1":"0", NULL);		
			bonobo_ui_component_set_prop(emfv->uic, "/commands/ViewThreadsExpandAll", "sensitive", state?"1":"0", NULL);					
		}
		
		if (emfv->uic) {
			state = (folder->folder_flags & CAMEL_FOLDER_IS_TRASH) == 0;
			bonobo_ui_component_set_prop(emfv->uic, "/commands/HideDeleted", "sensitive", state?"1":"0", NULL);
		}

		/* Fixme */
/* 		sstate = camel_object_meta_get(folder, "evolution:search_state"); */
/* 		g_object_set(emfb->search, "state", sstate, NULL); */
/* 		g_free(sstate); */
		
		/* set the query manually, so we dont pop up advanced or saved search stuff */
		g_object_get(emfb->search, "query", &sstate, NULL);
		message_list_set_search(emfb->view.list, sstate);
		g_free(sstate);

		if ((sstate = camel_object_meta_get (folder, "evolution:selected_uid"))) {
			emfb->priv->select_uid = sstate;
		} else {
			g_free(p->select_uid);
			p->select_uid = NULL;
		}
		
		if (emfv->list->cursor_uid == NULL && emfb->priv->list_built_id == 0)
			p->list_built_id = g_signal_connect(emfv->list, "message_list_built", G_CALLBACK (emfb_list_built), emfv);
	}

	message_list_thaw(emfv->list);
}

static void
emfb_activate(EMFolderView *emfv, BonoboUIComponent *uic, int act)
{
	if (act) {
		GConfClient *gconf;
		gboolean state;
		char *sstate;
		EMFolderBrowser *emfb = (EMFolderBrowser *) emfv;

		gconf = mail_config_get_gconf_client ();

		/* parent loads all ui files via ui_files */
		emfb_parent->activate(emfv, uic, act);

		bonobo_ui_component_add_verb_list_with_data(uic, emfb_verbs, emfv);
		e_pixmaps_update(uic, emfb_pixmaps);

		/* FIXME: finish */
		/* (Pre)view pane size (do this first because it affects the
	           preview settings - see folder_browser_set_message_preview()
	           internals for details) */
		g_signal_handler_block(emfb->vpane, emfb->priv->vpane_resize_id);
		gtk_paned_set_position((GtkPaned *)emfb->vpane, gconf_client_get_int (gconf, emfb->priv->show_wide ? "/apps/evolution/mail/display/hpaned_size": "/apps/evolution/mail/display/paned_size", NULL));
		g_signal_handler_unblock(emfb->vpane, emfb->priv->vpane_resize_id);
		
		/* (Pre)view toggle */
		if (emfv->folder
		    && (sstate = camel_object_meta_get(emfv->folder, "evolution:show_preview"))) {
			state = sstate[0] == '1';
			g_free(sstate);
		} else {
			state = gconf_client_get_bool(gconf, "/apps/evolution/mail/display/show_preview", NULL);
		}

		bonobo_ui_component_set_prop(uic, "/commands/ViewPreview", "state", state?"1":"0", NULL);
		em_folder_browser_show_preview((EMFolderBrowser *)emfv, state);
		bonobo_ui_component_add_listener(uic, "ViewPreview", emfb_view_preview, emfv);
	
		/* Stop button */
		state = mail_msg_active((unsigned int)-1);
		bonobo_ui_component_set_prop(uic, "/commands/MailStop", "sensitive", state?"1":"0", NULL);

		/* HideDeleted */
		state = !gconf_client_get_bool(gconf, "/apps/evolution/mail/display/show_deleted", NULL);
		if (emfv->folder && (emfv->folder->folder_flags & CAMEL_FOLDER_IS_TRASH)) {
			state = FALSE;
			bonobo_ui_component_set_prop(uic, "/commands/HideDeleted", "sensitive", "0", NULL);
		} else
			bonobo_ui_component_set_prop(uic, "/commands/HideDeleted", "sensitive", "1", NULL);
		bonobo_ui_component_set_prop(uic, "/commands/HideDeleted", "state", state ? "1" : "0", NULL);
		bonobo_ui_component_add_listener(uic, "HideDeleted", emfb_hide_deleted, emfv);
		em_folder_view_set_hide_deleted(emfv, state); /* <- not sure if this optimal, but it'll do */

		/* FIXME: If we have no folder, we can't do a few of the lookups we need,
		   perhaps we should postpone till we can */

		/* ViewThreaded */
		if (emfv->folder
		    && (sstate = camel_object_meta_get(emfv->folder, "evolution:thread_list"))) {
			state = sstate[0] != '0';
			g_free(sstate);
		} else {
			state = gconf_client_get_bool(gconf, "/apps/evolution/mail/display/thread_list", NULL);
		}

		bonobo_ui_component_set_prop(uic, "/commands/ViewThreaded", "state", state?"1":"0", NULL);
		bonobo_ui_component_set_prop(uic, "/commands/ViewThreadsCollapseAll", "sensitive", state?"1":"0", NULL);		
		bonobo_ui_component_set_prop(uic, "/commands/ViewThreadsExpandAll", "sensitive", state?"1":"0", NULL);				
		bonobo_ui_component_add_listener(uic, "ViewThreaded", emfb_view_threaded, emfv);
		message_list_set_threaded(emfv->list, state);

		/* Show wide display */
		state = gconf_client_get_bool(gconf, "/apps/evolution/mail/display/show_wide", NULL);
		if (state) {
			bonobo_ui_component_set_prop(uic, "/commands/ViewAfter", "state", "1", NULL);
			bonobo_ui_component_set_prop(uic, "/commands/ViewBelow", "state", "0", NULL);
		} else {
			bonobo_ui_component_set_prop(uic, "/commands/ViewAfter", "state", "0", NULL);
			bonobo_ui_component_set_prop(uic, "/commands/ViewBelow", "state", "1", NULL);			
		}
		
		bonobo_ui_component_add_listener(uic, "ViewAfter", emfb_show_next, emfv);
		bonobo_ui_component_add_listener(uic, "ViewBelow", emfb_show_below, emfv);
		/* em_folder_browser_show_wide((EMFolderBrowser *)emfv, state); */

		/* FIXME: Selection state */

		/* FIXME: property menu customisation */
		/*folder_browser_setup_property_menu (fb, fb->uicomp);*/

		if (((EMFolderBrowser *)emfv)->search)
			e_search_bar_set_ui_component((ESearchBar *)((EMFolderBrowser *)emfv)->search, uic);
	} else {
		const BonoboUIVerb *v;
		
		for (v = &emfb_verbs[0]; v->cname; v++)
			bonobo_ui_component_remove_verb(uic, v->cname);

		if (((EMFolderBrowser *)emfv)->search)
			e_search_bar_set_ui_component((ESearchBar *)((EMFolderBrowser *)emfv)->search, NULL);

		emfb_parent->activate(emfv, uic, act);
	}
}
