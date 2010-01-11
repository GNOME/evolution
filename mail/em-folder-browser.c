/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Michael Zucchi <notzed@ximian.com>
 *      Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gconf/gconf-client.h>

#ifdef G_OS_WIN32
/* Work around 'DATADIR' and 'interface' lossage in <windows.h> */
#define DATADIR crap_DATADIR
#include <windows.h>
#undef DATADIR
#undef interface
#endif

#include "mail-mt.h"
#include "mail-ops.h"
#include "mail-tools.h"
#include "mail-config.h"

#include <e-util/e-dialog-utils.h>
#include <e-util/e-icon-factory.h>

#include <camel/camel-stream.h>
#include <camel/camel-url.h>
#include <camel/camel-folder.h>
#include <camel/camel-vee-folder.h>
#include <camel/camel-vee-store.h>
#include <camel/camel-operation.h>

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
#include "em-folder-tree.h"
#include <misc/e-filter-bar.h>
#include <camel/camel-search-private.h>
#include <camel/camel-store.h>

#include "e-util/e-dialog-utils.h"
#include "e-util/e-util.h"
#include "e-util/e-error.h"
#include "e-util/e-util-private.h"
#include "e-util/e-util-labels.h"
#include "e-mail-search-bar.h"
#include "em-utils.h"
#include "em-composer-utils.h"
#include "em-format-html-display.h"
#include "em-format-html-print.h"
#include "em-folder-browser.h"
#include "em-folder-properties.h"
#include "em-folder-utils.h"
#include "em-subscribe-editor.h"
#include "em-menu.h"
#include "em-event.h"
#include "message-list.h"

#include "mail-component.h"
#include "mail-ops.h"

#include "evolution-shell-component-utils.h" /* Pixmap stuff, sigh */

#include <gtkhtml/gtkhtml.h>

extern CamelSession *session;
CamelStore *vfolder_store; /* the 1 static vfolder store */

#define d(x)

struct _EMFolderBrowserPrivate {
	GtkWidget *preview;	/* container for message display */
	GtkWidget *scroll;
	GtkWidget *search_bar;

	GtkWidget *subscribe_editor;

	guint search_menu_activated_id;
	guint search_activated_id;

	double default_scroll_position;
	guint idle_scroll_id;
	guint list_scrolled_id;

	guint vpane_resize_id;
	guint list_built_id;	/* hook onto list-built for delayed 'select first unread' stuff */

	gchar *select_uid;
	guint folder_changed_id;

	guint show_wide:1;
	guint suppress_message_selection:1;
	gboolean scope_restricted;

	EMMenu *menu;		/* toplevel menu manager */

	guint labels_change_notify_id; /* mail_config's notify id */
	guint labels_change_idle_id; /* rebuild menu on idle, when all know about a change */
};

typedef struct EMFBSearchBarItem {
	ESearchBarItem search;
	const gchar *image;
} EMFBSearchBarItem;

static void emfb_activate(EMFolderView *emfv, BonoboUIComponent *uic, gint state);
static void emfb_set_folder(EMFolderView *emfv, CamelFolder *folder, const gchar *uri);
static void emfb_set_search_folder(EMFolderView *emfv, CamelFolder *folder, const gchar *uri);

/* FilterBar stuff ... */
static void emfb_search_config_search(EFilterBar *efb, FilterRule *rule, gint id, const gchar *query, gpointer data);
static void emfb_search_menu_activated(ESearchBar *esb, gint id, EMFolderBrowser *emfb);
static void emfb_search_search_activated(ESearchBar *esb, EMFolderBrowser *emfb);
static void emfb_search_search_cleared(ESearchBar *esb);

static gint emfb_list_key_press(ETree *tree, gint row, ETreePath path, gint col, GdkEvent *ev, EMFolderBrowser *emfb);
static void emfb_list_message_selected (MessageList *ml, const gchar *uid, EMFolderBrowser *emfb);

static void emfb_expand_all_threads(BonoboUIComponent *uid, gpointer data, const gchar *path);

static const EMFolderViewEnable emfb_enable_map[] = {
	{ "EditInvertSelection", EM_POPUP_SELECT_FOLDER },
	{ "EditSelectAll", EM_POPUP_SELECT_FOLDER },
	{ "EditSelectThread", EM_FOLDER_VIEW_SELECT_THREADED },
	{ "EditSelectSubthread", EM_FOLDER_VIEW_SELECT_THREADED },
	{ "FolderExpunge", EM_POPUP_SELECT_FOLDER },
	{ "FolderCopy", EM_POPUP_SELECT_FOLDER },
	{ "FolderMove", EM_POPUP_SELECT_FOLDER },
	{ "FolderDelete", EM_POPUP_SELECT_FOLDER },
	{ "FolderRename", EM_POPUP_SELECT_FOLDER },
	{ "FolderRefresh", EM_POPUP_SELECT_FOLDER },
	{ "ChangeFolderProperties", EM_POPUP_SELECT_FOLDER },
	{ "MessageMarkAllAsRead", EM_POPUP_SELECT_FOLDER },
	{ "ViewHideSelected", EM_POPUP_SELECT_MANY },
	{ "ViewThreadsCollapseAll", EM_FOLDER_VIEW_SELECT_THREADED},
	{ "ViewThreadsExpandAll", EM_FOLDER_VIEW_SELECT_THREADED},
	{ NULL },
};

enum {
	ACCOUNT_SEARCH_ACTIVATED,
	ACCOUNT_SEARCH_CLEARED,
	LAST_SIGNAL
};

static guint folder_browser_signals [LAST_SIGNAL] = {0, };

enum {
	ESB_SAVE
};

static ESearchBarItem emfb_search_items[] = {
	E_FILTERBAR_ADVANCED,
	{ NULL, 0, 0 },
	E_FILTERBAR_SAVE,
	E_FILTERBAR_EDIT,
	{ NULL, 0, 0 },
	{ (gchar *) N_("C_reate Search Folder From Search..."), ESB_SAVE, 0},
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
	VIEW_MESSAGES_MARKED_AS_IMPORTANT,
	VIEW_CUSTOMIZE
};

/* label IDs are set above this number */
#define VIEW_ITEMS_MASK 63

/* Options for View */
static EMFBSearchBarItem emfb_view_items[] = {
	{{ (gchar *) N_("All Messages"), VIEW_ALL_MESSAGES, 0 }, NULL},
	{{ (gchar *) N_("Unread Messages"), VIEW_UNREAD_MESSAGES, 0 }, "mail-unread"},
	{{ NULL, 0, 0 }, NULL},
	{{ (gchar *) N_("No Label"),VIEW_NO_LABEL, 0 }, NULL},
	{{ NULL, -1, 0 }, NULL}
};

/* TODO: Following options should be  customizable */
static EMFBSearchBarItem temp_view_items[] = {
	{{ NULL, 0, 0 }, NULL},
	{{ (gchar *) N_("Read Messages"), VIEW_READ_MESSAGES, 0 }, "mail-read"},
	{{ (gchar *) N_("Recent Messages"), VIEW_RECENT_MESSAGES, 0 }, NULL},
	{{ (gchar *) N_("Last 5 Days' Messages"), VIEW_LAST_FIVE_DAYS, 0 }, NULL},
	{{ (gchar *) N_("Messages with Attachments"), VIEW_WITH_ATTACHMENTS, 0 }, "mail-attachment"},
	{{ (gchar *) N_("Important Messages"), VIEW_MESSAGES_MARKED_AS_IMPORTANT, 0}, "emblem-important"},
	{{ (gchar *) N_("Messages Not Junk"), VIEW_NOT_JUNK, 0 }, "mail-mark-notjunk"},
	{{ NULL, -1, 0 }, NULL}
};

static ESearchBarItem emfb_search_scope_items[] = {
	E_FILTERBAR_CURRENT_FOLDER,
	E_FILTERBAR_CURRENT_ACCOUNT,
	E_FILTERBAR_ALL_ACCOUNTS,
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

static GtkWidget *
generate_viewoption_menu (GtkWidget *emfv)
{
	GtkWidget *menu, *menu_item;
	gint i = 0;
	GSList *l;

	menu = gtk_menu_new ();

	for (i = 0; emfb_view_items[i].search.id != -1; ++i) {
		if (emfb_view_items[i].search.text) {
			gchar *str;

			str = e_str_without_underscores (_(emfb_view_items[i].search.text));
			menu_item = gtk_image_menu_item_new_with_label (str);
			if (emfb_view_items[i].image) {
				GtkWidget *image;

				image = gtk_image_new_from_icon_name (
					emfb_view_items[i].image,
					GTK_ICON_SIZE_MENU);
				gtk_image_menu_item_set_image (
					GTK_IMAGE_MENU_ITEM (menu_item),
					image);
			}
			g_free (str);
		} else {
			menu_item = gtk_menu_item_new ();
			gtk_widget_set_sensitive (menu_item, FALSE);
		}

		g_object_set_data (G_OBJECT (menu_item), "EsbItemId",
				   GINT_TO_POINTER (emfb_view_items[i].search.id));

		gtk_widget_show (menu_item);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
	}

	/* Add the labels */
	for (l = mail_config_get_labels (), i = 0; l; l = l->next, i++) {
		EUtilLabel *label = l->data;
		if (label->name && *(label->name)) {
			gchar *str;
			GdkPixmap *pixmap;
			GdkColor colour;
			GdkGC *gc;
			GtkWidget *image;

			gdk_color_parse(label->colour, &colour);
			gdk_colormap_alloc_color(gdk_colormap_get_system(), &colour, FALSE, TRUE);

			pixmap = gdk_pixmap_new(((GtkWidget *)emfv)->window, 16, 16, -1);
			gc = gdk_gc_new(((GtkWidget *)emfv)->window);
			gdk_gc_set_foreground(gc, &colour);
			gdk_draw_rectangle(pixmap, gc, TRUE, 0, 0, 16, 16);
			g_object_unref(gc);

			image = gtk_image_new_from_pixmap(pixmap, NULL);
			str = e_str_without_underscores (e_util_labels_is_system (label->tag) ? _(label->name) : label->name);
			menu_item = gtk_image_menu_item_new_with_label (str);
			g_free (str);
			gtk_image_menu_item_set_image ((GtkImageMenuItem *)menu_item, image);
			g_object_set_data (G_OBJECT (menu_item), "EsbItemId",
					   GINT_TO_POINTER (VIEW_LABEL + (VIEW_ITEMS_MASK + 1) * i));

			g_object_set_data_full (G_OBJECT (menu_item), "LabelTag",
						g_strdup (strncmp (label->tag, "$Label", 6) == 0 ? label->tag + 6 : label->tag),
						g_free);

			gtk_widget_show (menu_item);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
		}
	}

	for (i = 0; temp_view_items[i].search.id != -1; ++i) {
		if (temp_view_items[i].search.text) {
			gchar *str;
			str = e_str_without_underscores (_(temp_view_items[i].search.text));
			menu_item = gtk_image_menu_item_new_with_label (str);
			if (temp_view_items[i].image) {
				GtkWidget *image;

				image = gtk_image_new_from_icon_name (
					temp_view_items[i].image,
					GTK_ICON_SIZE_MENU);
				gtk_image_menu_item_set_image (
					GTK_IMAGE_MENU_ITEM (menu_item),
					image);
			}
			g_free (str);
		} else {
			menu_item = gtk_menu_item_new ();
			gtk_widget_set_sensitive (menu_item, FALSE);
		}

		g_object_set_data (G_OBJECT (menu_item), "EsbItemId",
				   GINT_TO_POINTER (temp_view_items[i].search.id));

		gtk_widget_show (menu_item);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
	}

	return menu;
}

#if 0
static GArray *
viewoption_menu_generator ()
{
	GArray *menu = g_array_new (FALSE, FALSE, sizeof (ESearchBarItem));
	gint i = 0;
	ESearchBarItem dup_item;
	GSList *l;

	for (i = 0; emfb_view_items[i].search.id != -1; i++)
		g_array_append_vals (menu, &emfb_view_items[i], 1);

	for (l = mail_config_get_labels (); l; l = l->next) {
		ESearchBarItem item;
		EUtilLabel *label = l->data;

		item.text = label->name;
		item.id = VIEW_LABEL;

		g_array_append_vals (menu, &item, 1);
	}

	dup_item.id = -1;
	dup_item.text = NULL;
	g_array_append_vals (menu, &dup_item, 1);

	return menu;
}
#endif

static void
emfb_realize (GtkWidget *widget)
{
	GtkWidget *menu;
	EMFolderBrowser *emfb = (EMFolderBrowser *)widget;
	gint id;

	menu = generate_viewoption_menu (widget);
	id = e_search_bar_get_viewitem_id (E_SEARCH_BAR (emfb->search));

	e_search_bar_set_viewoption_menu (E_SEARCH_BAR (emfb->search), menu);

	/* restore last selected ID, if any */
	if (id != -1)
		e_search_bar_set_viewitem_id (E_SEARCH_BAR (emfb->search), id);
}

static void
html_scroll (GtkHTML *html,
	GtkOrientation orientation,
	GtkScrollType  scroll_type,
	gfloat         position,
	EMFolderBrowser *emfb)

{
	if (html->binding_handled || orientation != GTK_ORIENTATION_VERTICAL || !mail_config_get_enable_magic_spacebar ())
		return;

	if (scroll_type == GTK_SCROLL_PAGE_FORWARD) {
		gtk_widget_grab_focus ((GtkWidget *)((EMFolderView *) emfb)->list);
		message_list_select(((EMFolderView *) emfb)->list, MESSAGE_LIST_SELECT_NEXT, 0, CAMEL_MESSAGE_SEEN);
	} else if (scroll_type == GTK_SCROLL_PAGE_BACKWARD) {
		gtk_widget_grab_focus ((GtkWidget *)((EMFolderView *) emfb)->list);
		message_list_select(((EMFolderView *) emfb)->list, MESSAGE_LIST_SELECT_NEXT, 0, CAMEL_MESSAGE_SEEN);
	}
}

static gboolean
labels_changed_idle_cb (gpointer user_data)
{
	EMFolderBrowser *emfb = (EMFolderBrowser*) user_data;

	emfb_realize (GTK_WIDGET (emfb));

	emfb->priv->labels_change_idle_id = 0;

	return FALSE;
}

static void
gconf_labels_changed (GConfClient *client, guint cnxn_id,
		      GConfEntry *entry, gpointer user_data)
{
	EMFolderBrowser *emfb = (EMFolderBrowser*) user_data;

	/* regenerate menu option whenever something changed in labels */
	if (emfb && !emfb->priv->labels_change_idle_id)
		emfb->priv->labels_change_idle_id = g_idle_add (labels_changed_idle_cb, emfb);
}

static void
emfb_init(GObject *o)
{
	EMFolderBrowser *emfb = (EMFolderBrowser *)o;
	RuleContext *search_context = mail_component_peek_search_context (mail_component_peek ());
	struct _EMFolderBrowserPrivate *p;
	GtkWidget *html;

	EMEvent *eme;
	EMEventTargetFolderBrowser *target;

	p = emfb->priv = g_malloc0(sizeof(struct _EMFolderBrowserPrivate));

	emfb->view.preview_active = TRUE;
	emfb->view.list_active = TRUE;

	g_signal_connect_after (((EMFormatHTML *)(emfb->view.preview))->html, "scroll", G_CALLBACK (html_scroll), emfb);

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

	emfb->view.enable_map = g_slist_prepend(emfb->view.enable_map, (gpointer)emfb_enable_map);

	if (search_context) {
		const gchar *systemrules = g_object_get_data (G_OBJECT (search_context), "system");
		const gchar *userrules = g_object_get_data (G_OBJECT (search_context), "user");
		EFilterBar *efb;
		GConfClient *gconf;

		emfb->search = e_filter_bar_new(search_context, systemrules, userrules, emfb_search_config_search, emfb);
		efb = (EFilterBar *)emfb->search;
		efb->account_search_vf = NULL;
		efb->all_account_search_vf = NULL;
		efb->account_search_cancel = NULL;
		e_search_bar_set_menu ((ESearchBar *)emfb->search, emfb_search_items);
		e_search_bar_set_scopeoption ((ESearchBar *)emfb->search, emfb_search_scope_items);
		emfb->priv->scope_restricted = TRUE;
		g_signal_connect(emfb, "realize", G_CALLBACK(emfb_realize), NULL);
		gtk_widget_show((GtkWidget *)emfb->search);

		p->search_menu_activated_id = g_signal_connect(emfb->search, "menu_activated", G_CALLBACK(emfb_search_menu_activated), emfb);
		p->search_activated_id = g_signal_connect(emfb->search, "search_activated", G_CALLBACK(emfb_search_search_activated), emfb);
		g_signal_connect(emfb->search, "search_cleared", G_CALLBACK(emfb_search_search_cleared), NULL);

		gtk_box_pack_start((GtkBox *)emfb, (GtkWidget *)emfb->search, FALSE, TRUE, 0);

		gconf = mail_config_get_gconf_client ();
		emfb->priv->labels_change_notify_id = gconf_client_notify_add (gconf, E_UTIL_LABELS_GCONF_KEY, gconf_labels_changed, emfb, NULL, NULL);
	}

	emfb->priv->show_wide = gconf_client_get_bool(mail_config_get_gconf_client(), "/apps/evolution/mail/display/show_wide", NULL);
	emfb->vpane = emfb->priv->show_wide?gtk_hpaned_new():gtk_vpaned_new();

	g_signal_connect(emfb->vpane, "realize", G_CALLBACK(emfb_pane_realised), emfb);
	emfb->priv->vpane_resize_id = g_signal_connect(emfb->vpane, "button_release_event", G_CALLBACK(emfb_pane_button_release_event), emfb);

	gtk_widget_show(emfb->vpane);

	gtk_box_pack_start((GtkBox *)emfb, emfb->vpane, TRUE, TRUE, 0);

	gtk_paned_pack1 (GTK_PANED (emfb->vpane), GTK_WIDGET (emfb->view.list), FALSE, FALSE);
	gtk_widget_show((GtkWidget *)emfb->view.list);

	/* currently: just use a scrolledwindow for preview widget */
	p->scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy((GtkScrolledWindow *)p->scroll, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type((GtkScrolledWindow *)p->scroll, GTK_SHADOW_IN);
	gtk_widget_show(p->scroll);

	html = GTK_WIDGET (emfb->view.preview->formathtml.html);

	p->preview = gtk_vbox_new (FALSE, 1);
	p->search_bar = e_mail_search_bar_new (GTK_HTML (html));
	gtk_container_add((GtkContainer *)p->scroll, html);
	gtk_widget_show(html);
	gtk_box_pack_start ((GtkBox *)p->preview, p->scroll, TRUE, TRUE, 0);
	gtk_box_pack_start ((GtkBox *)p->preview, p->search_bar, FALSE, FALSE, 0);
	gtk_paned_pack2 (GTK_PANED (emfb->vpane), p->preview, TRUE, FALSE);
	gtk_widget_show(p->preview);

	g_signal_connect_swapped (
		p->search_bar, "changed",
		G_CALLBACK (em_format_redraw), emfb->view.preview);

	/** @HookPoint-EMFolderBrower: Folder Browser
	 * @Id: emfb.created
	 * @Class: org.gnome.evolution.mail.events:1.0
	 * @Target: EMFolderBrowser
	 */

	eme = em_event_peek();
	target = em_event_target_new_folder_browser (eme, emfb);

	e_event_emit((EEvent *)eme, "emfb.created", (EEventTarget *)target);

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

	if (emfb->priv->labels_change_notify_id) {
		GConfClient *gconf = mail_config_get_gconf_client ();

		if (gconf)
			gconf_client_notify_remove (gconf, emfb->priv->labels_change_notify_id);

		emfb->priv->labels_change_notify_id = 0;
	}

	if (emfb->priv->labels_change_idle_id) {
		g_source_remove (emfb->priv->labels_change_idle_id);

		emfb->priv->labels_change_idle_id = 0;
	}

	((GtkObjectClass *)emfb_parent)->destroy(o);
}

static void
emfb_show_search_bar (EMFolderView *folder_view)
{
	EMFolderBrowser *browser = (EMFolderBrowser *) folder_view;

	gtk_widget_show (browser->priv->search_bar);
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
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	folder_browser_signals[ACCOUNT_SEARCH_CLEARED] =
		g_signal_new ("account_search_cleared",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EMFolderBrowserClass, account_search_cleared),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	((GtkObjectClass *)klass)->destroy = emfb_destroy;
	((EMFolderViewClass *)klass)->set_folder = emfb_set_folder;
	((EMFolderViewClass *)klass)->activate = emfb_activate;
	((EMFolderViewClass *)klass)->show_search_bar = emfb_show_search_bar;
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
	    || emfb->view.list == NULL) {
		if (state && emfb->priv->scope_restricted && emfb->view.list->cursor_uid && *(emfb->view.list->cursor_uid)) {
			emfb->priv->scope_restricted = FALSE;
		}

		return;
	}

	emfb->view.preview_active = state;

	if (state) {
		GConfClient *gconf = mail_config_get_gconf_client ();
		gint paned_size /*, y*/;

		paned_size = gconf_client_get_int(gconf, emfb->priv->show_wide ? "/apps/evolution/mail/display/hpaned_size":"/apps/evolution/mail/display/paned_size", NULL);

		/*y = save_cursor_pos (emfb);*/
		gtk_paned_set_position (GTK_PANED (emfb->vpane), paned_size);
		gtk_widget_show (GTK_WIDGET (emfb->priv->preview));

		if (emfb->view.list->cursor_uid) {
			gchar *uid = g_alloca(strlen(emfb->view.list->cursor_uid)+1);

			emfb->priv->scope_restricted = FALSE;
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
		emfb->priv->scope_restricted = TRUE;
		/*
		mail_display_set_message (emfb->mail_display, NULL, NULL, NULL);
		emfb_ui_message_loaded (emfb);*/
	}

	/* FIXME: need to update menu's to reflect ui changes */
}

gboolean em_folder_browser_get_wide (EMFolderBrowser *emfb)
{
	return emfb->priv->show_wide;
}

void em_folder_browser_show_wide(EMFolderBrowser *emfb, gboolean state)
{
	GtkWidget *w;
	gint paned_size;

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

	gtk_box_pack_start((GtkBox *)emfb, w, TRUE, TRUE, 0);
	gtk_widget_reparent((GtkWidget *)emfb->view.list, w);
	gtk_widget_reparent((GtkWidget *)emfb->priv->preview, w);
	gtk_widget_destroy(emfb->vpane);
	gtk_container_child_set (GTK_CONTAINER (w), GTK_WIDGET (emfb->view.list),     "resize", FALSE, "shrink", FALSE, NULL);
	gtk_container_child_set (GTK_CONTAINER (w), GTK_WIDGET (emfb->priv->preview), "resize", TRUE,  "shrink", FALSE, NULL);
	gtk_container_resize_children ((GtkContainer *)w);
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
emfb_search_menu_activated(ESearchBar *esb, gint id, EMFolderBrowser *emfb)
{
	EFilterBar *efb = (EFilterBar *)esb;

	d(printf("menu activated\n"));

	switch (id) {
	case ESB_SAVE:
		d(printf("Save vfolder\n"));
		if (efb->current_query) {
			FilterRule *rule;
			gchar *name, *text;

			/* ensures vfolder is running */
			vfolder_load_storage ();

			rule = vfolder_clone_rule (efb->current_query);
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
emfb_search_config_search(EFilterBar *efb, FilterRule *rule, gint id, const gchar *query, gpointer data)
{
	EMFolderBrowser *emfb = data;
	EMailSearchBar *search_bar;
	ESearchingTokenizer *tokenizer;
	GList *partl;
	struct _camel_search_words *words;
	gint i;
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

			words = camel_search_words_split((guchar *)query);
			for (i=0;i<words->len;i++)
				strings = g_slist_prepend(strings, g_strdup(words->words[i]->word));
			camel_search_words_free (words);
		} else if (!strcmp(part->name, "sender")) {
			FilterInput *input = (FilterInput *)filter_part_find_element(part, "sender");
			if (input)
				filter_input_set_value(input, query);
		} else if (!strcmp(part->name, "to")) {
			FilterInput *input = (FilterInput *)filter_part_find_element(part, "recipient");
			if (input)
				filter_input_set_value(input, query);
		}

		partl = partl->next;
	}

	search_bar = E_MAIL_SEARCH_BAR (emfb->priv->search_bar);

	/* XXX This is a hack, but this code is on its way out anyway.
	 *     Function is called once before the search bar is created. */
	if (!E_IS_MAIL_SEARCH_BAR (search_bar))
		return;

	tokenizer = e_mail_search_bar_get_tokenizer (search_bar);

	e_searching_tokenizer_set_secondary_case_sensitivity (tokenizer, FALSE);
	e_searching_tokenizer_set_secondary_search_string (tokenizer, NULL);

	while (strings != NULL) {
		e_searching_tokenizer_add_secondary_search_string (
			tokenizer, strings->data);
		g_free (strings->data);
		strings = g_slist_delete_link (strings, strings);
	}

	e_mail_search_bar_changed (search_bar);
}

static const gchar *
get_view_query (ESearchBar *esb, CamelFolder *folder, const gchar *folder_uri)
{
	const gchar *view_sexp = NULL;
	gint id;
	GtkWidget *menu_item;
	gchar *tag;
	gboolean duplicate = TRUE;

	/* Get the current selected view */
	id = e_search_bar_get_viewitem_id (esb);
	menu_item = e_search_bar_get_selected_viewitem (esb);

	switch (id & VIEW_ITEMS_MASK) {
	case VIEW_ALL_MESSAGES:
		/* one space indicates no filtering */
		view_sexp = " ";
		break;

	/* README: All the sexp below are not rocket science but it is not straightforward as well.
	I believe it is better to document the assumptions and the conventions followed for the sexp,
	before I forget so that no one else again needs to read through the code  -- Sankar */

	case VIEW_UNREAD_MESSAGES:
		view_sexp = "(match-all (not (system-flag  \"Seen\")))";
		break;
	case VIEW_READ_MESSAGES:
		view_sexp = "(match-all (system-flag  \"Seen\" ))";
		break;
        case VIEW_RECENT_MESSAGES:
		if (!em_utils_folder_is_sent (folder, folder_uri))
			view_sexp = "(match-all (> (get-received-date) (- (get-current-date) 86400)))";
		else
			view_sexp = "(match-all (> (get-sent-date) (- (get-current-date) 86400)))";
		break;
	case VIEW_LAST_FIVE_DAYS:
		if (!em_utils_folder_is_sent (folder, folder_uri))
			view_sexp = " (match-all (> (get-received-date) (- (get-current-date) 432000)))";
		else
			view_sexp = " (match-all (> (get-sent-date) (- (get-current-date) 432000)))";
		break;
        case VIEW_WITH_ATTACHMENTS:
		view_sexp = "(match-all (system-flag \"Attachments\" ))";
		break;
	case VIEW_NOT_JUNK:
		view_sexp = "(match-all (not (system-flag \"junk\")))";
		break;
        case VIEW_NO_LABEL: {
		GSList *l;
		GString *s = g_string_new ("(and");

		for (l = mail_config_get_labels (); l; l = l->next) {
			EUtilLabel *label = (EUtilLabel *)l->data;

			if (label && label->tag) {
				const gchar *tag = label->tag;

				if (strncmp (tag, "$Label", 6) == 0)
					tag += 6;

				g_string_append_printf (s, " (match-all (not (or (= (user-tag \"label\") \"%s\") (user-flag \"$Label%s\") (user-flag \"%s\"))))", tag, tag, tag);
				/* FIXME: I dont see a way of mapping this kind of sexp into sql atm. I guess this option could be kicked out */
				/* May be we should copy what I did for system flags -- Sankar */
			}
		}

		g_string_append (s, ")");

		duplicate = FALSE;
		view_sexp = g_string_free (s, FALSE);
		} break;
        case VIEW_LABEL:
		tag = (gchar *)g_object_get_data (G_OBJECT (menu_item), "LabelTag");
		view_sexp = g_strdup_printf ("(match-all (or (= (user-tag \"label\") \"%s\") (user-flag \"$Label%s\") (user-flag \"%s\")))", tag, tag, tag);
		duplicate = FALSE;
		break;
	case VIEW_MESSAGES_MARKED_AS_IMPORTANT:
		view_sexp = "(match-all (system-flag  \"Flagged\" ))";
		break;
	case VIEW_ANY_FIELD_CONTAINS:
		break;

	case VIEW_CUSTOMIZE:
		/* one space indicates no filtering, so here use two */
		view_sexp = "  ";
		break;
	}

	if (duplicate)
		view_sexp = g_strdup (view_sexp);

	return view_sexp;
}

struct _setup_msg {
	MailMsg base;

	CamelFolder *folder;
	CamelOperation *cancel;
	GList *sources_uri;
	GList *sources_folder;
};

static gchar *
vfolder_setup_desc(struct _setup_msg *m)
{
	return g_strdup(_("Searching"));
}

static void
vfolder_setup_exec(struct _setup_msg *m)
{
	GList *l, *list = NULL;
	CamelFolder *folder;

	if (m->cancel)
		camel_operation_register (m->cancel);

	d(printf("Setting up Search Folder: %s\n", m->folder->full_name));

	l = m->sources_uri;
	while (l) {
		d(printf(" Adding uri: %s\n", (gchar *)l->data));
		folder = mail_tool_uri_to_folder (l->data, 0, &m->base.ex);
		if (folder) {
			list = g_list_append(list, folder);
		} else {
			g_warning("Could not open vfolder source: %s", (gchar *)l->data);
			camel_exception_clear(&m->base.ex);
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
vfolder_setup_done(struct _setup_msg *m)
{
}

static void
vfolder_setup_free (struct _setup_msg *m)
{
	GList *l;

	camel_object_unref(m->folder);

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

static MailMsgInfo vfolder_setup_info = {
	sizeof (struct _setup_msg),
	(MailMsgDescFunc) vfolder_setup_desc,
	(MailMsgExecFunc) vfolder_setup_exec,
	(MailMsgDoneFunc) vfolder_setup_done,
	(MailMsgFreeFunc) vfolder_setup_free
};

/* sources_uri should be camel uri's */
static gint
vfolder_setup (CamelFolder *folder, GList *sources_uri, GList *sources_folder, CamelOperation *cancel)
{
	struct _setup_msg *m;
	gint id;

	m = mail_msg_new(&vfolder_setup_info);
	m->folder = folder;
	camel_object_ref(folder);
	m->sources_uri = sources_uri;
	m->sources_folder = sources_folder;

	if (cancel) {
		m->cancel = cancel;
	}

	id = m->base.seq;
	mail_msg_slow_ordered_push (m);

	return id;
}

static void
emfb_search_search_activated(ESearchBar *esb, EMFolderBrowser *emfb)
{
	EMFolderView *emfv = (EMFolderView *) emfb;
	EFilterBar *efb = (EFilterBar *)esb;
	const gchar *view_sexp;
	gchar *search_state = NULL, *folder_uri=NULL;
	gchar *word = NULL, *storeuri = NULL, *search_word = NULL;
	gint id, i;
	CamelFolder *folder;
	CamelStore *store;
	GPtrArray *folders;
	GList *folder_list_account = NULL;
	GList *l, *folder_list = NULL;
	CamelException *ex;
	ex = camel_exception_new ();

	if (emfv->list == NULL || emfv->folder == NULL)
		return;

	id = e_search_bar_get_search_scope (esb);

	switch (id) {
	    case E_FILTERBAR_CURRENT_FOLDER_ID:
		    g_object_get (esb, "query", &search_word, NULL);
		    break;

	    case E_FILTERBAR_CURRENT_ACCOUNT_ID:
		    word = e_search_bar_get_text (esb);
		    if (!(word && *word)) {
			    if (efb->account_search_vf) {
				    camel_object_unref (efb->account_search_vf);
				    efb->account_search_vf = NULL;
				    if (efb->account_search_cancel) {
					    camel_operation_cancel (efb->account_search_cancel);
					    camel_operation_unref (efb->account_search_cancel);
					    efb->account_search_cancel = NULL;
				    }
			    }
			    g_signal_emit (emfb, folder_browser_signals [ACCOUNT_SEARCH_CLEARED], 0);
			    gtk_widget_set_sensitive (esb->scopeoption, TRUE);
			    g_free (word);
			    word = NULL;
			    break;
		    }

		    g_free (word);
		    word = NULL;
		    g_object_get (esb, "query", &search_word, NULL);
		    if (search_word && efb->account_search_vf && !strcmp (search_word, ((CamelVeeFolder *) efb->account_search_vf)->expression) ) {
			    break;
		    }
		    gtk_widget_set_sensitive (esb->scopeoption, FALSE);

		    /* Disable the folder tree */
		    g_signal_emit (emfb, folder_browser_signals [ACCOUNT_SEARCH_ACTIVATED], 0);

		    if (!efb->account_search_vf) {
			    store = emfv->folder->parent_store;
			    if (store->folders) {
				    folders = camel_object_bag_list(store->folders);
				    for (i=0;i<folders->len;i++) {
					    folder = folders->pdata[i];
					    folder_list_account = g_list_append(folder_list_account, folder);
				    }
			    }

			    /* Create a camel vee folder */
			    storeuri = g_strdup_printf("vfolder:%s/vfolder", mail_component_peek_base_directory (mail_component_peek ()));
			    vfolder_store = camel_session_get_store (session, storeuri, NULL);
			    efb->account_search_vf = (CamelVeeFolder *)camel_vee_folder_new (vfolder_store,_("Account Search"),CAMEL_STORE_VEE_FOLDER_AUTO);

			    /* Set the search expression  */
			    camel_vee_folder_set_expression (efb->account_search_vf, search_word);

			    efb->account_search_cancel = camel_operation_new (NULL, NULL);
			    vfolder_setup ((CamelFolder *)efb->account_search_vf, NULL, folder_list_account, efb->account_search_cancel);

			    folder_uri = mail_tools_folder_to_url ((CamelFolder *)efb->account_search_vf);
			    emfb_set_search_folder (emfv, (CamelFolder *)efb->account_search_vf, folder_uri);
			    g_free (folder_uri);
			    g_free (storeuri);
		    } else {
			    /* cancel previous filling first, if under way */
			    if (efb->account_search_cancel) {
				    camel_operation_cancel (efb->account_search_cancel);
				    camel_operation_unref (efb->account_search_cancel);
				    efb->account_search_cancel = NULL;
			    }

			    /* Reuse the existing search folder */
			    camel_vee_folder_set_expression((CamelVeeFolder *)efb->account_search_vf, search_word);
		    }

		    break;

	    case E_FILTERBAR_ALL_ACCOUNTS_ID:
		    word = e_search_bar_get_text (esb);
		    if (!(word && *word)) {
			    if (efb->all_account_search_vf) {
				    camel_object_unref (efb->all_account_search_vf);
				    efb->all_account_search_vf=NULL;
				    if (efb->account_search_cancel) {
					    camel_operation_cancel (efb->account_search_cancel);
					    camel_operation_unref (efb->account_search_cancel);
					    efb->account_search_cancel = NULL;
				    }
			    }
			    g_signal_emit (emfb, folder_browser_signals [ACCOUNT_SEARCH_CLEARED], 0);
			    gtk_widget_set_sensitive (esb->scopeoption, TRUE);
			    g_free (word);
			    word = NULL;
			    break;
		    }

		    g_free (word);
		    word = NULL;

		    g_object_get (esb, "query", &search_word, NULL);

		    if (search_word && efb->all_account_search_vf && !strcmp (search_word, ((CamelVeeFolder *) efb->all_account_search_vf)->expression) )  {
			    /* No real search apart from the existing one */
			    break;
		    }

		    gtk_widget_set_sensitive (esb->scopeoption, FALSE);
		    g_signal_emit (emfb, folder_browser_signals [ACCOUNT_SEARCH_ACTIVATED], 0);

		    if (!efb->all_account_search_vf) {
			    /* Create a camel vee folder */
			    storeuri = g_strdup_printf("vfolder:%s/vfolder", mail_component_peek_base_directory (mail_component_peek ()));
			    vfolder_store = camel_session_get_store (session, storeuri, NULL);
			    efb->all_account_search_vf = (CamelVeeFolder *)camel_vee_folder_new (vfolder_store,_("All Account Search"),CAMEL_STORE_VEE_FOLDER_AUTO);

			    /* Set sexp  */

			    /* FIXME: there got to be a better way :) */

			    /* Add the local folders */
			    l = mail_vfolder_get_sources_local ();
			    while (l) {
				    folder = mail_tool_uri_to_folder ((const gchar *)l->data, 0,ex);
				    if (folder)
					    folder_list = g_list_append(folder_list, folder);
				    else {
					    g_warning("Could not open vfolder source: %s", (gchar *)l->data);
					    camel_exception_clear(ex);
				    }
				    l = l->next;
			    }

			    /* Add the remote source folder */
			    l = mail_vfolder_get_sources_remote ();
			    while (l) {
				    folder = mail_tool_uri_to_folder ((const gchar *)l->data, 0,ex);
				    if (folder)
					    folder_list = g_list_append(folder_list, folder);
				    else {
					    g_warning("Could not open vfolder source: %s", (gchar *)l->data);
					    camel_exception_clear(ex);
				    }
				    l = l->next;
			    }

			    camel_vee_folder_set_expression (efb->all_account_search_vf, search_word);

			    efb->account_search_cancel = camel_operation_new (NULL, NULL);
			    vfolder_setup ((CamelFolder *)efb->all_account_search_vf, NULL, folder_list, efb->account_search_cancel);

			    folder_uri = mail_tools_folder_to_url ((CamelFolder *)efb->all_account_search_vf);

			    emfb_set_search_folder (emfv, (CamelFolder *)efb->all_account_search_vf, folder_uri);
			    g_free (folder_uri);
			    g_free (storeuri);
		    } else {
			    /* cancel previous filling first, if under way */
			    if (efb->account_search_cancel) {
				    camel_operation_cancel (efb->account_search_cancel);
				    camel_operation_unref (efb->account_search_cancel);
				    efb->account_search_cancel = NULL;
			    }

			    /* Reuse the existing search folder */
			    camel_vee_folder_set_expression((CamelVeeFolder *)efb->all_account_search_vf, search_word);
		    }

		    break;
	}
	g_object_get (esb, "state", &search_state, NULL);
	camel_object_meta_set (emfv->folder, "evolution:search_state", search_state);
	camel_object_state_write (emfv->folder);
	g_free (search_state);

	if (search_word) {
		g_free (search_word);
		search_word = NULL;
	}

	/* Merge the view and search expresion*/
	view_sexp = get_view_query (esb, emfv->folder, emfv->folder_uri);
	g_object_get (esb, "query", &search_word, NULL);

	word = search_word;

	if (search_word && *search_word)
		search_word = g_strconcat ("(and ", view_sexp, search_word, " )", NULL);
	else
		search_word = g_strdup (view_sexp);

	message_list_set_search(emfb->view.list, search_word);

	g_free (word);
	g_free (search_word);

	camel_exception_free (ex);
}

static void
emfb_search_search_cleared(ESearchBar *esb)
{
	/* FIXME: It should just cancel search.*/
	mail_cancel_all();
}

/* ********************************************************************** */

static gint
emfb_list_key_press(ETree *tree, gint row, ETreePath path, gint col, GdkEvent *ev, EMFolderBrowser *emfb)
{
	gboolean state, folder_choose = TRUE;
	if ((ev->key.state & GDK_CONTROL_MASK) != 0)
		return FALSE;

	switch (ev->key.keyval) {
	case GDK_space:
		if (!emfb->view.preview->caret_mode && mail_config_get_enable_magic_spacebar ()) {
			state = gtk_html_command(((EMFormatHTML *)((EMFolderView *) emfb)->preview)->html, "scroll-forward");
			if (!state) {
				folder_choose = message_list_select(((EMFolderView *) emfb)->list, MESSAGE_LIST_SELECT_NEXT, 0, CAMEL_MESSAGE_SEEN);
				if (!folder_choose)
					folder_choose = message_list_select(((EMFolderView *) emfb)->list,
								    MESSAGE_LIST_SELECT_NEXT | MESSAGE_LIST_SELECT_WRAP, 0, CAMEL_MESSAGE_SEEN);
			}

		} else
			em_utils_adjustment_page(gtk_scrolled_window_get_vadjustment((GtkScrolledWindow *)emfb->priv->scroll), TRUE);
		break;
	case GDK_BackSpace:
		if (!emfb->view.preview->caret_mode && mail_config_get_enable_magic_spacebar ()) {
			state = gtk_html_command(((EMFormatHTML *)((EMFolderView *) emfb)->preview)->html, "scroll-backward");
			if (!state) {
				folder_choose = message_list_select(((EMFolderView *) emfb)->list, MESSAGE_LIST_SELECT_PREVIOUS, 0, CAMEL_MESSAGE_SEEN);
				if (!folder_choose)
					folder_choose = message_list_select(((EMFolderView *) emfb)->list,
								    MESSAGE_LIST_SELECT_PREVIOUS | MESSAGE_LIST_SELECT_WRAP, 0, CAMEL_MESSAGE_SEEN);
			}

		} else
			em_utils_adjustment_page(gtk_scrolled_window_get_vadjustment((GtkScrolledWindow *)emfb->priv->scroll), FALSE);
		break;
	default:
		return FALSE;
	}

	if (!folder_choose && !emfb->view.preview->caret_mode && mail_config_get_enable_magic_spacebar ()) {
		/* check for unread messages. if yes .. rewindback to the folder */
		EMFolderTree *emft = g_object_get_data((GObject*)emfb, "foldertree");
		switch (ev->key.keyval) {
		    case GDK_space:
			    em_folder_tree_select_next_path (emft, TRUE);
			    break;
		    case GDK_BackSpace:
			    em_folder_tree_select_prev_path (emft, TRUE);
			    break;
		}
		gtk_widget_grab_focus ((GtkWidget *)((EMFolderView *) emfb)->list);
	}
	return TRUE;
}

static void
emfb_list_message_selected (MessageList *ml, const gchar *uid, EMFolderBrowser *emfb)
{
	EMFolderView *emfv = (EMFolderView *) emfb;

	if (emfv->folder == NULL)
		return;

	if (uid && *uid && emfb->priv->scope_restricted && emfb->view.preview_active) {
		emfb->priv->scope_restricted = FALSE;
	} else if ( !(uid && *uid) && !emfb->priv->scope_restricted) {
		emfb->priv->scope_restricted = TRUE;
	}

	camel_object_meta_set (emfv->folder, "evolution:selected_uid", uid);
	camel_object_state_write (emfv->folder);
	g_free (emfb->priv->select_uid);
	emfb->priv->select_uid = NULL;
}

/* ********************************************************************** */

static void
emfb_edit_cut(BonoboUIComponent *uid, gpointer data, const gchar *path)
{
	EMFolderBrowser *emfb = data;

	/* TODO: pity we can't sucblass this method, ugh, virtualise it? */

	if (GTK_WIDGET_HAS_FOCUS(((ESearchBar *)emfb->search)->entry))
		gtk_editable_cut_clipboard((GtkEditable *)((ESearchBar *)emfb->search)->entry);
	else if (GTK_WIDGET_HAS_FOCUS(emfb->view.preview->formathtml.html))
		em_format_html_display_cut(emfb->view.preview);
	else
		message_list_copy(emfb->view.list, TRUE);
}

static void
emfb_edit_copy(BonoboUIComponent *uid, gpointer data, const gchar *path)
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
emfb_edit_paste(BonoboUIComponent *uid, gpointer data, const gchar *path)
{
	EMFolderBrowser *emfb = data;

	if (GTK_WIDGET_HAS_FOCUS(((ESearchBar *)emfb->search)->entry))
		gtk_editable_paste_clipboard((GtkEditable *)((ESearchBar *)emfb->search)->entry);
	else
		message_list_paste(emfb->view.list);
}

static void
emfb_edit_invert_selection(BonoboUIComponent *uid, gpointer data, const gchar *path)
{
	EMFolderView *emfv = data;

	message_list_invert_selection(emfv->list);
}

static gboolean
emfb_select_all_daemon (MessageList *ml)
{
		message_list_select_all(ml);
		gtk_widget_grab_focus ((GtkWidget *)ml);
		return FALSE;
}

static void
emfb_edit_select_all(BonoboUIComponent *uid, gpointer data, const gchar *path)
{
		EMFolderView *emfv = data;

		if (emfv->list->threaded) {

				emfb_expand_all_threads (uid, data, path);

				/* The time out below is added so that the execution thread to
				   expand all conversations threads would've completed.

				   The timeout 505 is just to ensure that the value is a small delta
				   more than the timeout value in expand_all_threads thread. */

				g_timeout_add (505, (GSourceFunc) emfb_select_all_daemon, emfv->list);

		} else {
				/* If there is no threading, just select-all immediately */
				emfb_select_all_daemon (emfv->list);
		}
}

static void
emfb_edit_select_thread(BonoboUIComponent *uid, gpointer data, const gchar *path)
{
	EMFolderView *emfv = data;

	message_list_select_thread(emfv->list);
}

static void
emfb_edit_select_subthread(BonoboUIComponent *uid, gpointer data, const gchar *path)
{
	EMFolderView *emfv = data;

	message_list_select_subthread (emfv->list);
}

static void
emfb_folder_properties(BonoboUIComponent *uid, gpointer data, const gchar *path)
{
	EMFolderBrowser *emfb = data;

	if (emfb->view.folder_uri)
		em_folder_properties_show(NULL, emfb->view.folder, emfb->view.folder_uri);
}

/* VIEWTHREADED*/
static void
emfb_expand_all_threads(BonoboUIComponent *uid, gpointer data, const gchar *path)
{
	EMFolderView *emfv = data;

	message_list_set_threaded_expand_all(emfv->list);

}

static void
emfb_collapse_all_threads(BonoboUIComponent *uid, gpointer data, const gchar *path)
{
	EMFolderView *emfv = data;

	message_list_set_threaded_collapse_all(emfv->list);
}

static void
emfb_folder_copy(BonoboUIComponent *uid, gpointer data, const gchar *path)
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
emfb_folder_move(BonoboUIComponent *uid, gpointer data, const gchar *path)
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
emfb_folder_delete(BonoboUIComponent *uid, gpointer data, const gchar *path)
{
	EMFolderBrowser *emfb = data;

	em_folder_utils_delete_folder (emfb->view.folder);

	return;
}

static void
emfb_folder_refresh(BonoboUIComponent *uid, gpointer data, const gchar *path)
{
        EMFolderBrowser *emfb = data;
	EMFolderTree *tree = g_object_get_data (G_OBJECT (emfb), "foldertree");
        CamelFolder *folder;

        if ((folder = em_folder_tree_get_selected_folder (tree)) != NULL) {
		EMEvent *e = em_event_peek();
		EMEventTargetFolder *t = em_event_target_new_folder(e, folder->full_name, 0);
		e_event_emit((EEvent *)e, "folder.refresh", (EEventTarget *)t);
                mail_refresh_folder(folder, NULL, NULL);
	}
}

static void
emfb_folder_rename(BonoboUIComponent *uid, gpointer data, const gchar *path)
{
	EMFolderBrowser *emfb = data;

	em_folder_utils_rename_folder (emfb->view.folder);

	return;
}

static void
emfb_folder_create(BonoboUIComponent *uid, gpointer data, const gchar *path)
{
	EMFolderBrowser *emfb = data;
	CamelFolderInfo *fi = NULL;
	EMFolderTree *tree = g_object_get_data (G_OBJECT (emfb), "foldertree");

	/* FIXME: This function MUST be multithreaded
	   FIXME: This interface should NOT use a folderinfo */
	if (emfb->view.folder) {
		if ((fi = em_folder_tree_get_selected_folder_info (tree)) != NULL) {
			em_folder_utils_create_folder (fi, tree, NULL);
			camel_folder_info_free(fi);
		}
	} else {
		em_folder_utils_create_folder (NULL, tree, NULL);
	}

	return;
}

static void
emfb_folder_expunge(BonoboUIComponent *uid, gpointer data, const gchar *path)
{
	EMFolderBrowser *emfb = data;

	if (emfb->view.folder)
		em_utils_expunge_folder(gtk_widget_get_toplevel((GtkWidget *)emfb), emfb->view.folder);
}

static void
emfb_mark_all_read(BonoboUIComponent *uid, gpointer data, const gchar *path)
{
	/* FIXME: make a 'mark messages' function? */
	EMFolderView *emfv = data;
	GPtrArray *uids;
	gint i;

	if (emfv->folder == NULL)
		return;
	if ( em_utils_prompt_user((GtkWindow *)emfv, "/apps/evolution/mail/prompts/mark_all_read","mail:ask-mark-all-read", NULL)) {
		uids = message_list_get_uids(emfv->list);
		camel_folder_freeze(emfv->folder);
		for (i=0;i<uids->len;i++)
			camel_folder_set_message_flags(emfv->folder, uids->pdata[i], CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);
		camel_folder_thaw(emfv->folder);
		message_list_free_uids(emfv->list, uids);
	}
}

static void
emfb_view_hide_read(BonoboUIComponent *uid, gpointer data, const gchar *path)
{
	EMFolderView *emfv = data;

	message_list_hide_add(emfv->list, "(match-all (system-flag \"seen\"))", ML_HIDE_SAME, ML_HIDE_SAME);
}

static void
emfb_view_hide_selected(BonoboUIComponent *uid, gpointer data, const gchar *path)
{
	EMFolderView *emfv = data;
	GPtrArray *uids;

	/* TODO: perhaps this should sit directly on message_list? */
	/* is it worth it, it's so trivial */

	/* A new flag CAMEL_MESSAGE_HIDDEN is added by Sankar
	while extending the CAMEL_MESSAGE_FLAGS for proxy permissions.
	This can be used to hide messages.  */

	uids = message_list_get_selected(emfv->list);
	message_list_hide_uids(emfv->list, uids);
	message_list_free_uids(emfv->list, uids);
}

static void
emfb_view_show_all(BonoboUIComponent *uid, gpointer data, const gchar *path)
{
	EMFolderView *emfv = data;

	message_list_hide_clear(emfv->list);
}

/* ********************************************************************** */

static void
emfb_mail_stop(BonoboUIComponent *uid, gpointer data, const gchar *path)
{
	mail_cancel_all();
}

static void
emfb_tools_filters(BonoboUIComponent *uid, gpointer data, const gchar *path)
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
emfb_tools_subscriptions(BonoboUIComponent *uid, gpointer data, const gchar *path)
{
	EMFolderBrowser *emfb = data;

	if (emfb->priv->subscribe_editor) {
		gdk_window_show(emfb->priv->subscribe_editor->window);
	} else {
		emfb->priv->subscribe_editor = (GtkWidget *)em_subscribe_editor_new();
		e_dialog_set_transient_for ((GtkWindow *)emfb->priv->subscribe_editor, (GtkWidget *)emfb);
		g_signal_connect(emfb->priv->subscribe_editor, "destroy", G_CALLBACK(emfb_subscribe_editor_destroy), emfb);
		gtk_widget_show(emfb->priv->subscribe_editor);
	}
}

static void
emfb_focus_search(BonoboUIComponent *uid, gpointer data, const gchar *path)
{
	EMFolderBrowser *emfb = data;

	gtk_widget_grab_focus (((ESearchBar *)emfb->search)->entry);
}

static void
emfb_help_debug (BonoboUIComponent *uid, gpointer data, const gchar *path)
{
	mail_component_show_logger ((GtkWidget *) data);
}

static void
emfb_tools_vfolders(BonoboUIComponent *uid, gpointer data, const gchar *path)
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
	BONOBO_UI_UNSAFE_VERB ("EditSelectSubthread", emfb_edit_select_subthread),
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
	BONOBO_UI_UNSAFE_VERB ("FolderRefresh", emfb_folder_refresh),
	BONOBO_UI_UNSAFE_VERB ("FolderRename", emfb_folder_rename),
	BONOBO_UI_UNSAFE_VERB ("FolderCreate", emfb_folder_create),
	BONOBO_UI_UNSAFE_VERB ("HelpDebug", emfb_help_debug),

	BONOBO_UI_UNSAFE_VERB ("MailStop", emfb_mail_stop),
	BONOBO_UI_UNSAFE_VERB ("ToolsFilters", emfb_tools_filters),
	BONOBO_UI_UNSAFE_VERB ("ToolsSubscriptions", emfb_tools_subscriptions),
	BONOBO_UI_UNSAFE_VERB ("ToolsVFolders", emfb_tools_vfolders),
	BONOBO_UI_UNSAFE_VERB ("FocusSearch", emfb_focus_search),

	/* ViewPreview is a toggle */

	BONOBO_UI_VERB_END
};

static EPixmap emfb_pixmaps[] = {
	E_PIXMAP ("/commands/FolderCreate", "folder-new", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/ChangeFolderProperties", "document-properties", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/FolderCopy", "folder-copy", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/FolderMove", "folder-move", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MessageMarkAllAsRead", "mail-read", GTK_ICON_SIZE_MENU),

	E_PIXMAP_END
};

static void
emfb_hide_deleted(BonoboUIComponent *uic, const gchar *path, Bonobo_UIComponent_EventType type, const gchar *state, gpointer data)
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
emfb_view_threaded(BonoboUIComponent *uic, const gchar *path, Bonobo_UIComponent_EventType type, const gchar *state, gpointer data)
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
emfb_view_preview(BonoboUIComponent *uic, const gchar *path, Bonobo_UIComponent_EventType type, const gchar *state, gpointer data)
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
emfb_show_next(BonoboUIComponent *uic, const gchar *path, Bonobo_UIComponent_EventType type, const gchar *state, gpointer data)
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
emfb_show_below(BonoboUIComponent *uic, const gchar *path, Bonobo_UIComponent_EventType type, const gchar *state, gpointer data)
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
	gchar *state;

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
	gchar *state;

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
emfb_gui_folder_changed(CamelFolder *folder, gpointer dummy, EMFolderBrowser *emfb)
{
	if (emfb->priv->select_uid) {
		CamelMessageInfo *mi;

		mi = camel_folder_get_message_info(emfb->view.folder, emfb->priv->select_uid);
		if (mi) {
			/* because some sub-functions might free the pointer before it's done here */
			gchar *uid = g_strdup (emfb->priv->select_uid);

			camel_folder_free_message_info(emfb->view.folder, mi);
			em_folder_view_set_message (&emfb->view, uid, FALSE);
			g_free (uid);
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
emfb_set_search_folder(EMFolderView *emfv, CamelFolder *folder, const gchar *uri)
{
	EMFolderBrowser *emfb = (EMFolderBrowser *) emfv;
	const gchar *state;

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
	state = "<ETableState>"
		"<column source=\"0\"/> <column source=\"3\"/> <column source=\"1\"/>"
		"<column source=\"14\"/> <column source=\"5\"/>"
		"<column source=\"7\"/> <column source=\"13\"/> "
		"<grouping><leaf column=\"7\" ascending=\"false\"/> </grouping> </ETableState>";
	e_tree_set_state (((MessageList *)emfv->list)->tree, state);

	message_list_thaw(emfv->list);
}

static void
emfb_set_folder(EMFolderView *emfv, CamelFolder *folder, const gchar *uri)
{
	EMFolderBrowser *emfb = (EMFolderBrowser *) emfv;
	struct _EMFolderBrowserPrivate *p = emfb->priv;
	gboolean different_folder;

	message_list_freeze(emfv->list);

	if (emfb->priv->list_scrolled_id) {
		g_signal_handler_disconnect (emfv->list, emfb->priv->list_scrolled_id);
		emfb->priv->list_scrolled_id = 0;
	}

	if (emfb->priv->idle_scroll_id) {
		g_source_remove (emfb->priv->idle_scroll_id);
		emfb->priv->idle_scroll_id = 0;
	}

	if (emfb->view.folder && emfb->priv->folder_changed_id) {
		camel_object_remove_event(emfb->view.folder, emfb->priv->folder_changed_id);
		emfb->priv->folder_changed_id = 0;
	}

	different_folder =
		emfb->view.folder != NULL &&
		folder != emfb->view.folder;

	emfb_parent->set_folder(emfv, folder, uri);

	/* This is required since we get activated the first time
	   before the folder is open and need to override the
	   defaults */
	if (folder) {
		gchar *sstate;
		gint state;
		gboolean safe;
		GConfClient *gconf = mail_config_get_gconf_client();

		safe = gconf_client_get_bool (gconf, "/apps/evolution/mail/display/safe_list", NULL);
		if (safe) {
			if (camel_object_meta_set(emfv->folder, "evolution:show_preview", "0") &&
			    camel_object_meta_set(emfv->folder, "evolution:selected_uid", NULL)) {
				camel_object_state_write(emfv->folder);
				g_free (emfb->priv->select_uid);
				emfb->priv->select_uid = NULL;
			}
			gconf_client_set_bool (gconf, "/apps/evolution/mail/display/safe_list", FALSE, NULL);
		}

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
		sstate = camel_object_meta_get(folder, "evolution:search_state");
		if (sstate) {
			g_object_set(emfb->search, "state", sstate, NULL);
			g_free(sstate);
		} else {
			gboolean outgoing;
			outgoing = em_utils_folder_is_drafts (emfv->folder, emfv->folder_uri)
				|| em_utils_folder_is_sent (emfv->folder, emfv->folder_uri)
				|| em_utils_folder_is_outbox (emfv->folder, emfv->folder_uri);

			e_search_bar_clear_search ((ESearchBar *)emfb->search);

			if (outgoing) {
				e_search_bar_set_item_id ((ESearchBar *)emfb->search, 1);
				((ESearchBar *)emfb->search)->block_search = TRUE;
				e_search_bar_set_item_menu ((ESearchBar *)emfb->search, 1);
				((ESearchBar *)emfb->search)->block_search = FALSE;

			} else {
				e_search_bar_set_item_id ((ESearchBar *)emfb->search, 0);
				((ESearchBar *)emfb->search)->block_search = TRUE;
				e_search_bar_set_item_menu ((ESearchBar *)emfb->search, 0);
				((ESearchBar *)emfb->search)->block_search = FALSE;

			}
			e_search_bar_paint ((ESearchBar *)emfb->search);
		}

		/* This function gets triggered several times at startup,
		 * so we don't want to reset the message suppression state
		 * unless we're actually switching to a different folder. */
		if (different_folder)
			p->suppress_message_selection = FALSE;

		if (!p->suppress_message_selection)
			sstate = camel_object_meta_get (
				folder, "evolution:selected_uid");
		else
			sstate = NULL;

		g_free (p->select_uid);
		p->select_uid = sstate;

		if (emfv->list->cursor_uid == NULL && emfb->priv->list_built_id == 0)
			p->list_built_id = g_signal_connect(emfv->list, "message_list_built", G_CALLBACK (emfb_list_built), emfv);
	}

	message_list_thaw(emfv->list);
}

static void
emfb_activate(EMFolderView *emfv, BonoboUIComponent *uic, gint act)
{
	if (act) {
		GConfClient *gconf;
		gboolean state;
		gchar *sstate;
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
		state = mail_msg_active((guint)-1);
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
		if (emfb->priv->show_wide) {
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

void
em_folder_browser_suppress_message_selection (EMFolderBrowser *emfb)
{
	emfb->priv->suppress_message_selection = TRUE;
}
