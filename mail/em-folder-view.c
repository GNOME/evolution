/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Michael Zucchi <notzed@ximian.com>
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

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <gtk/gtkvbox.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkvpaned.h>
#include <gdk/gdkkeysyms.h>

#include <libgnome/gnome-url.h>

#include <libgnomeprintui/gnome-print-dialog.h>

#include <gconf/gconf-client.h>

#include <gal/menus/gal-view-etable.h>
#include <gal/menus/gal-view-factory-etable.h>
#include <gal/menus/gal-view-instance.h>
#include "widgets/menus/gal-view-menus.h"

#include <camel/camel-mime-message.h>
#include <camel/camel-stream.h>
#include <camel/camel-stream-filter.h>
#include <camel/camel-mime-filter.h>
#include <camel/camel-mime-filter-tohtml.h>
#include <camel/camel-mime-filter-enriched.h>
#include <camel/camel-multipart.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-url.h>

#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-ui-component.h>
#include <bonobo/bonobo-ui-util.h>

#include "widgets/misc/e-charset-picker.h"

#include <e-util/e-dialog-utils.h>
#include <e-util/e-icon-factory.h>

#include "em-format-html-display.h"
#include "em-format-html-print.h"
#include "em-folder-selection.h"
#include "em-folder-view.h"
#include "em-mailer-prefs.h"
#include "em-message-browser.h"
#include "message-list.h"
#include "em-utils.h"
#include "em-composer-utils.h"
#include "em-marshal.h"
#include "em-menu.h"

#include <gtkhtml/gtkhtml.h>
#include <gtkhtml/htmlobject.h>
#include <gtkhtml/htmlengine.h>
#include <gtkhtml/htmlengine-save.h>
#include <gtkhtml/htmlengine-edit-cut-and-paste.h>

#include "mail-mt.h"
#include "mail-ops.h"
#include "mail-config.h"
#include "mail-autofilter.h"
#include "mail-vfolder.h"
#include "mail-component.h"

#include "evolution-shell-component-utils.h" /* Pixmap stuff, sigh */

static void emfv_folder_changed(CamelFolder *folder, CamelFolderChangeInfo *changes, EMFolderView *emfv);

static void emfv_list_message_selected(MessageList *ml, const char *uid, EMFolderView *emfv);
static int emfv_list_right_click(ETree *tree, gint row, ETreePath path, gint col, GdkEvent *event, EMFolderView *emfv);
static void emfv_list_double_click(ETree *tree, gint row, ETreePath path, gint col, GdkEvent *event, EMFolderView *emfv);
static int emfv_list_key_press(ETree *tree, int row, ETreePath path, int col, GdkEvent *ev, EMFolderView *emfv);
static void emfv_list_selection_change(ETree *tree, EMFolderView *emfv);

static void emfv_format_link_clicked(EMFormatHTMLDisplay *efhd, const char *uri, EMFolderView *);
static int emfv_format_popup_event(EMFormatHTMLDisplay *efhd, GdkEventButton *event, const char *uri, CamelMimePart *part, EMFolderView *);

static void emfv_enable_menus(EMFolderView *emfv);

static void emfv_set_folder(EMFolderView *emfv, CamelFolder *folder, const char *uri);
static void emfv_set_folder_uri(EMFolderView *emfv, const char *uri);
static void emfv_set_message(EMFolderView *emfv, const char *uid, int nomarkseen);
static void emfv_activate(EMFolderView *emfv, BonoboUIComponent *uic, int state);

static void emfv_message_reply(EMFolderView *emfv, int mode);
static void vfolder_type_current (EMFolderView *emfv, int type);
static void filter_type_current (EMFolderView *emfv, int type);

static void emfv_setting_setup(EMFolderView *emfv);

static void emfv_on_url_cb(GObject *emitter, const char *url, EMFolderView *emfv);
static void emfv_on_url(EMFolderView *emfv, const char *uri, const char *nice_uri);

static gboolean emfv_popup_menu (GtkWidget *widget);

static const EMFolderViewEnable emfv_enable_map[];

struct _EMFolderViewPrivate {
	guint seen_id;
	guint setting_notify_id;
	int nomarkseen:1;
	int destroyed:1;

	CamelObjectHookID folder_changed_id;

	GtkWidget *invisible;
	char *selection_uri;

	GalViewInstance *view_instance;
	GalViewMenus *view_menus;
};

static GtkVBoxClass *emfv_parent;

enum {
	EMFV_ON_URL,
	EMFV_LOADED,
	EMFV_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void emfv_selection_get(GtkWidget *widget, GtkSelectionData *data, guint info, guint time_stamp, EMFolderView *emfv);
static void emfv_selection_clear_event(GtkWidget *widget, GdkEventSelection *event, EMFolderView *emfv);

static void
emfv_init(GObject *o)
{
	EMFolderView *emfv = (EMFolderView *)o;
	struct _EMFolderViewPrivate *p;
	extern CamelSession *session;
	
	gtk_box_set_homogeneous (GTK_BOX (emfv), FALSE);

	p = emfv->priv = g_malloc0(sizeof(struct _EMFolderViewPrivate));

	emfv->statusbar_active = TRUE;
	emfv->list_active = FALSE;
	
	emfv->ui_files = g_slist_append(NULL, EVOLUTION_UIDIR "/evolution-mail-message.xml");
	emfv->ui_app_name = "evolution-mail";

	emfv->enable_map = g_slist_prepend(NULL, (void *)emfv_enable_map);

	emfv->list = (MessageList *)message_list_new();
	g_signal_connect(emfv->list, "message_selected", G_CALLBACK(emfv_list_message_selected), emfv);

	/* FIXME: should this hang off message-list instead? */
	g_signal_connect(emfv->list->tree, "right_click", G_CALLBACK(emfv_list_right_click), emfv);
	g_signal_connect(emfv->list->tree, "double_click", G_CALLBACK(emfv_list_double_click), emfv);
	g_signal_connect(emfv->list->tree, "key_press", G_CALLBACK(emfv_list_key_press), emfv);
	g_signal_connect(emfv->list->tree, "selection_change", G_CALLBACK(emfv_list_selection_change), emfv);

	emfv->preview = (EMFormatHTMLDisplay *)em_format_html_display_new();
	/* FIXME: set_session should NOT be called here.  Should it be a constructor attribute? */
	em_format_set_session ((EMFormat *) emfv->preview, session);
	g_signal_connect(emfv->preview, "link_clicked", G_CALLBACK(emfv_format_link_clicked), emfv);
	g_signal_connect(emfv->preview, "popup_event", G_CALLBACK(emfv_format_popup_event), emfv);
	g_signal_connect (emfv->preview, "on_url", G_CALLBACK (emfv_on_url_cb), emfv);

	p->invisible = gtk_invisible_new();
	g_signal_connect(p->invisible, "selection_get", G_CALLBACK(emfv_selection_get), emfv);
	g_signal_connect(p->invisible, "selection_clear_event", G_CALLBACK(emfv_selection_clear_event), emfv);
	gtk_selection_add_target(p->invisible, GDK_SELECTION_PRIMARY, GDK_SELECTION_TYPE_STRING, 0);
	gtk_selection_add_target(p->invisible, GDK_SELECTION_CLIPBOARD, GDK_SELECTION_TYPE_STRING, 1);

	emfv->async = mail_async_event_new();

	emfv_setting_setup(emfv);
}

static void
emfv_finalise(GObject *o)
{
	EMFolderView *emfv = (EMFolderView *)o;
	struct _EMFolderViewPrivate *p = emfv->priv;

	g_slist_free(emfv->ui_files);
	g_slist_free(emfv->enable_map);

	g_free(p);

	((GObjectClass *)emfv_parent)->finalize(o);
}

static void
emfv_destroy (GtkObject *o)
{
	EMFolderView *emfv = (EMFolderView *) o;
	struct _EMFolderViewPrivate *p = emfv->priv;

	p->destroyed = TRUE;

	if (p->seen_id) {
		g_source_remove(p->seen_id);
		p->seen_id = 0;
	}

	if (p->setting_notify_id) {
		GConfClient *gconf = gconf_client_get_default();

		gconf_client_notify_remove(gconf, p->setting_notify_id);
		p->setting_notify_id = 0;
		g_object_unref(gconf);
	}

	if (emfv->folder) {
		if (p->folder_changed_id)
			camel_object_remove_event(emfv->folder, p->folder_changed_id);
		camel_object_unref(emfv->folder);
		g_free(emfv->folder_uri);
		emfv->folder = NULL;
		emfv->folder_uri = NULL;
	}

	if (emfv->async) {
		mail_async_event_destroy(emfv->async);
		emfv->async = NULL;
	}

	if (p->invisible) {
		gtk_object_destroy(p->invisible);
		p->invisible = NULL;
	}

	emfv->preview = NULL;
	emfv->list = NULL;
	emfv->preview_active = FALSE;
	emfv->uic = NULL;

	((GtkObjectClass *) emfv_parent)->destroy (o);
}

static void
emfv_class_init(GObjectClass *klass)
{
	klass->finalize = emfv_finalise;
	
	((GtkObjectClass *) klass)->destroy = emfv_destroy;
	
	((GtkWidgetClass *) klass)->popup_menu = emfv_popup_menu;
	
	((EMFolderViewClass *) klass)->update_message_style = TRUE;
	
	((EMFolderViewClass *)klass)->set_folder = emfv_set_folder;
	((EMFolderViewClass *)klass)->set_folder_uri = emfv_set_folder_uri;
	((EMFolderViewClass *)klass)->set_message = emfv_set_message;
	((EMFolderViewClass *)klass)->activate = emfv_activate;

	((EMFolderViewClass *)klass)->on_url = emfv_on_url;
	
	signals[EMFV_ON_URL] = g_signal_new ("on-url",
					     G_OBJECT_CLASS_TYPE (klass),
					     G_SIGNAL_RUN_LAST,
					     G_STRUCT_OFFSET (EMFolderViewClass, on_url),
					     NULL, NULL,
					     em_marshal_VOID__STRING_STRING,
					     G_TYPE_NONE,
					     2, G_TYPE_STRING, G_TYPE_STRING);

	signals[EMFV_LOADED] = g_signal_new("loaded",
					    G_OBJECT_CLASS_TYPE(klass),
					    G_SIGNAL_RUN_LAST,
					    G_STRUCT_OFFSET(EMFolderViewClass, loaded),
					    NULL, NULL,
					    g_cclosure_marshal_VOID__VOID,
					    G_TYPE_NONE,
					    0);

	signals[EMFV_CHANGED] = g_signal_new("changed",
					     G_OBJECT_CLASS_TYPE(klass),
					     G_SIGNAL_RUN_LAST,
					     G_STRUCT_OFFSET(EMFolderViewClass, changed),
					     NULL, NULL,
					     g_cclosure_marshal_VOID__VOID,
					     G_TYPE_NONE,
					     0);
}

GType
em_folder_view_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(EMFolderViewClass),
			NULL, NULL,
			(GClassInitFunc)emfv_class_init,
			NULL, NULL,
			sizeof(EMFolderView), 0,
			(GInstanceInitFunc)emfv_init
		};
		emfv_parent = g_type_class_ref(gtk_vbox_get_type());
		type = g_type_register_static(gtk_vbox_get_type(), "EMFolderView", &info, 0);
	}

	return type;
}

GtkWidget *em_folder_view_new(void)
{
	EMFolderView *emfv = g_object_new(em_folder_view_get_type(), 0);

	return (GtkWidget *)emfv;
}

/* flag all selected messages. Return number flagged */
/* FIXME: Should this be part of message-list instead? */
int
em_folder_view_mark_selected(EMFolderView *emfv, guint32 mask, guint32 set)
{
	GPtrArray *uids;
	int i;

	if (emfv->folder == NULL)
		return 0;
	
	uids = message_list_get_selected(emfv->list);
	camel_folder_freeze(emfv->folder);

	for (i=0; i<uids->len; i++)
		camel_folder_set_message_flags(emfv->folder, uids->pdata[i], mask, set);

	message_list_free_uids(emfv->list, uids);
	camel_folder_thaw(emfv->folder);
	
	return i;
}

/* should this be elsewhere/take a uid list? */
int
em_folder_view_open_selected(EMFolderView *emfv)
{
	GPtrArray *uids;
	int i = 0;
	
	uids = message_list_get_selected(emfv->list);
	
	if (em_utils_folder_is_drafts(emfv->folder, emfv->folder_uri)
	    || em_utils_folder_is_outbox(emfv->folder, emfv->folder_uri)) {
		em_utils_edit_messages (emfv->folder, uids, TRUE);
	} else {
		/* TODO: have an em_utils_open_messages call? */
		
		/* FIXME: 'are you sure' for > 10 messages; is this even necessary? */
		
		for (i=0; i<uids->len; i++) {
			EMMessageBrowser *emmb;

			emmb = (EMMessageBrowser *)em_message_browser_window_new();
			message_list_set_threaded(((EMFolderView *)emmb)->list, emfv->list->threaded);
			em_folder_view_set_hide_deleted((EMFolderView *)emmb, emfv->hide_deleted);
			/* FIXME: session needs to be passed easier than this */
			em_format_set_session((EMFormat *)((EMFolderView *)emmb)->preview, ((EMFormat *)emfv->preview)->session);
			em_folder_view_set_folder((EMFolderView *)emmb, emfv->folder, emfv->folder_uri);
			em_folder_view_set_message((EMFolderView *)emmb, uids->pdata[i], FALSE);
			gtk_widget_show(emmb->window);
		}

		message_list_free_uids(emfv->list, uids);
	}

	return i;
}

/* ******************************************************************************** */
static void
emfv_list_display_view(GalViewInstance *instance, GalView *view, EMFolderView *emfv)
{
	if (GAL_IS_VIEW_ETABLE(view))
		gal_view_etable_attach_tree(GAL_VIEW_ETABLE(view), emfv->list->tree);
}

static void
emfv_setup_view_instance(EMFolderView *emfv)
{
	struct _EMFolderViewPrivate *p = emfv->priv;
	gboolean outgoing;
	char *id;
	static GalViewCollection *collection = NULL;

	g_assert(emfv->folder);
	g_assert(emfv->folder_uri);

	if (collection == NULL) {
		ETableSpecification *spec;
		GalViewFactory *factory;
		const char *evolution_dir;
		char *dir;

		collection = gal_view_collection_new ();
	
		gal_view_collection_set_title (collection, _("Mail"));
	
		evolution_dir = mail_component_peek_base_directory (mail_component_peek ());
		dir = g_build_filename (evolution_dir, "mail", "views", NULL);
		gal_view_collection_set_storage_directories (collection, EVOLUTION_GALVIEWSDIR "/mail/", dir);
		g_free (dir);
	
		spec = e_table_specification_new ();
		e_table_specification_load_from_file (spec, EVOLUTION_ETSPECDIR "/message-list.etspec");
	
		factory = gal_view_factory_etable_new (spec);
		g_object_unref (spec);
		gal_view_collection_add_factory (collection, factory);
		g_object_unref (factory);
	
		gal_view_collection_load (collection);
	}

	if (p->view_instance) {
		g_object_unref(p->view_instance);
		p->view_instance = NULL;
	}

	if (p->view_menus) {
		g_object_unref(p->view_menus);
		p->view_menus = NULL;
	}

	outgoing = em_utils_folder_is_drafts (emfv->folder, emfv->folder_uri)
		|| em_utils_folder_is_sent (emfv->folder, emfv->folder_uri)
		|| em_utils_folder_is_outbox (emfv->folder, emfv->folder_uri);
	
	/* TODO: should this go through mail-config api? */
	id = mail_config_folder_to_safe_url (emfv->folder);
	p->view_instance = gal_view_instance_new (collection, id);
	g_free (id);
	
	if (outgoing)
		gal_view_instance_set_default_view(p->view_instance, "As_Sent_Folder");
	
	gal_view_instance_load(p->view_instance);
	
	if (!gal_view_instance_exists(p->view_instance)) {
		struct stat st;
		char *path;
		
		path = mail_config_folder_to_cachename (emfv->folder, "et-header-");
		if (path && stat (path, &st) == 0 && st.st_size > 0 && S_ISREG (st.st_mode)) {
			ETableSpecification *spec;
			ETableState *state;
			GalView *view;
			
			spec = e_table_specification_new ();
			e_table_specification_load_from_file (spec, EVOLUTION_ETSPECDIR "/message-list.etspec");
			view = gal_view_etable_new (spec, "");
			g_object_unref (spec);
			
			state = e_table_state_new ();
			e_table_state_load_from_file (state, path);
			gal_view_etable_set_state (GAL_VIEW_ETABLE (view), state);
			g_object_unref (state);
			
			gal_view_instance_set_custom_view(p->view_instance, view);
			g_object_unref (view);
		}
		
		g_free (path);
	}

	g_signal_connect(p->view_instance, "display_view", G_CALLBACK(emfv_list_display_view), emfv);
	emfv_list_display_view(p->view_instance, gal_view_instance_get_current_view(p->view_instance), emfv);
	
	if (emfv->list_active && emfv->uic) {
		p->view_menus = gal_view_menus_new(p->view_instance);
		gal_view_menus_apply(p->view_menus, emfv->uic, NULL);
	}
}

/* ********************************************************************** */

static void
emfv_set_folder(EMFolderView *emfv, CamelFolder *folder, const char *uri)
{
	int isout = (folder && uri
		     && (em_utils_folder_is_drafts(folder, uri)
			 || em_utils_folder_is_sent(folder, uri)
			 || em_utils_folder_is_outbox(folder, uri)));
	
	if (folder == emfv->folder)
		return;
	
	if (emfv->preview)
		em_format_format ((EMFormat *) emfv->preview, NULL, NULL, NULL);
	
	message_list_set_folder(emfv->list, folder, uri, isout);
	g_free(emfv->folder_uri);
	emfv->folder_uri = g_strdup(uri);
	
	if (emfv->folder) {
		emfv->hide_deleted = emfv->list->hidedeleted; /* <- a bit nasty but makes it track the display better */
		mail_sync_folder (emfv->folder, NULL, NULL);
		camel_object_remove_event(emfv->folder, emfv->priv->folder_changed_id);
		camel_object_unref(emfv->folder);
	}

	emfv->folder = folder;
	if (folder) {
		emfv->priv->folder_changed_id = camel_object_hook_event(folder, "folder_changed",
									(CamelObjectEventHookFunc)emfv_folder_changed, emfv);
		camel_object_ref(folder);
		mail_refresh_folder(folder, NULL, NULL);
		/* We need to set this up to get the right view options for the message-list, even if we're not showing it */
		emfv_setup_view_instance(emfv);
	}
	
	emfv_enable_menus(emfv);

	/* TODO: should probably be called after all processing, not just this class's impl */
	g_signal_emit(emfv, signals[EMFV_LOADED], 0);
}

static void
emfv_got_folder(char *uri, CamelFolder *folder, void *data)
{
	EMFolderView *emfv = data;
	
	em_folder_view_set_folder(emfv, folder, uri);
}

static void
emfv_set_folder_uri(EMFolderView *emfv, const char *uri)
{
	mail_get_folder(uri, 0, emfv_got_folder, emfv, mail_thread_queued);
}

static void
emfv_set_message(EMFolderView *emfv, const char *uid, int nomarkseen)
{
	/* This could possible race with other set messages, but likelyhood is small */
	emfv->priv->nomarkseen = nomarkseen;
	message_list_select_uid(emfv->list, uid);
	/* force an update, since we may not get an updated event if we select the same uid */
	emfv_list_message_selected(emfv->list, uid, emfv);
}

/* ********************************************************************** */

static void
emfv_selection_get(GtkWidget *widget, GtkSelectionData *data, guint info, guint time_stamp, EMFolderView *emfv)
{
	struct _EMFolderViewPrivate *p = emfv->priv;

	if (p->selection_uri == NULL)
		return;

	gtk_selection_data_set(data, data->target, 8, p->selection_uri, strlen(p->selection_uri));
}

static void
emfv_selection_clear_event(GtkWidget *widget, GdkEventSelection *event, EMFolderView *emfv)
{
#if 0 /* do i care? */
	struct _EMFolderViewPrivate *p = emfv->priv;

	g_free(p->selection_uri);
	p->selection_uri = NULL;
#endif
}

/* ********************************************************************** */

/* Popup menu
   In many cases these are the functions called by the bonobo callbacks too */

static void
emfv_popup_open(EPopup *ep, EPopupItem *pitem, void *data)
{
	EMFolderView *emfv = data;
	em_folder_view_open_selected(emfv);
}

static void
emfv_popup_edit (EPopup *ep, EPopupItem *pitem, void *data)
{
	EMFolderView *emfv = data;
	GPtrArray *uids;

	if (!em_utils_check_user_can_send_mail((GtkWidget *)emfv))
		return;
	
	uids = message_list_get_selected(emfv->list);
	em_utils_edit_messages (emfv->folder, uids, FALSE);
}

static void
emfv_popup_saveas(EPopup *ep, EPopupItem *pitem, void *data)
{
	EMFolderView *emfv = data;
	GPtrArray *uids;
	
	uids = message_list_get_selected(emfv->list);
	em_utils_save_messages((GtkWidget *)emfv, emfv->folder, uids);
}

static void
emfv_popup_print(EPopup *ep, EPopupItem *pitem, void *data)
{
	EMFolderView *emfv = data;
	em_folder_view_print(emfv, FALSE);
}

static void
emfv_popup_reply_sender(EPopup *ep, EPopupItem *pitem, void *data)
{
	EMFolderView *emfv = data;
	emfv_message_reply(emfv, REPLY_MODE_SENDER);
}

static void
emfv_popup_reply_list(EPopup *ep, EPopupItem *pitem, void *data)
{
	EMFolderView *emfv = data;
	emfv_message_reply(emfv, REPLY_MODE_LIST);
}

static void
emfv_popup_reply_all(EPopup *ep, EPopupItem *pitem, void *data)
{
	EMFolderView *emfv = data;
	emfv_message_reply(emfv, REPLY_MODE_ALL);
}

static void
emfv_popup_forward(EPopup *ep, EPopupItem *pitem, void *data)
{
	EMFolderView *emfv = data;
	GPtrArray *uids;

	if (!em_utils_check_user_can_send_mail((GtkWidget *)emfv))
		return;

	uids = message_list_get_selected(emfv->list);
	em_utils_forward_messages (emfv->folder, uids, emfv->folder_uri);
}

static void
emfv_popup_flag_followup(EPopup *ep, EPopupItem *pitem, void *data)
{
	EMFolderView *emfv = data;
	GPtrArray *uids = message_list_get_selected(emfv->list);

	em_utils_flag_for_followup((GtkWidget *)emfv, emfv->folder, uids);
}

static void
emfv_popup_flag_completed(EPopup *ep, EPopupItem *pitem, void *data)
{
	EMFolderView *emfv = data;
	GPtrArray *uids;
	
	uids = message_list_get_selected(emfv->list);
	em_utils_flag_for_followup_completed((GtkWidget *)emfv, emfv->folder, uids);
}

static void
emfv_popup_flag_clear(EPopup *ep, EPopupItem *pitem, void *data)
{
	EMFolderView *emfv = data;
	GPtrArray *uids = message_list_get_selected(emfv->list);

	em_utils_flag_for_followup_clear((GtkWidget *)emfv, emfv->folder, uids);
}

static void
emfv_popup_mark_read(EPopup *ep, EPopupItem *pitem, void *data)
{
	EMFolderView *emfv = data;
	em_folder_view_mark_selected(emfv, CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);
}

static void
emfv_popup_mark_unread(EPopup *ep, EPopupItem *pitem, void *data)
{
	EMFolderView *emfv = data;
	em_folder_view_mark_selected(emfv, CAMEL_MESSAGE_SEEN|CAMEL_MESSAGE_DELETED, 0);
	
	if (emfv->priv->seen_id) {
		g_source_remove(emfv->priv->seen_id);
		emfv->priv->seen_id = 0;
	}
}

static void
emfv_popup_mark_important(EPopup *ep, EPopupItem *pitem, void *data)
{
	EMFolderView *emfv = data;
	em_folder_view_mark_selected(emfv, CAMEL_MESSAGE_FLAGGED|CAMEL_MESSAGE_DELETED, CAMEL_MESSAGE_FLAGGED);
}

static void
emfv_popup_mark_unimportant(EPopup *ep, EPopupItem *pitem, void *data)
{
	EMFolderView *emfv = data;
	em_folder_view_mark_selected(emfv, CAMEL_MESSAGE_FLAGGED, 0);
}

static void
emfv_popup_mark_junk (EPopup *ep, EPopupItem *pitem, void *data)
{
	EMFolderView *emfv = data;
	GPtrArray *uids;
	
	uids = message_list_get_selected(emfv->list);
	em_folder_view_mark_selected(emfv,
				     CAMEL_MESSAGE_SEEN|CAMEL_MESSAGE_JUNK|CAMEL_MESSAGE_JUNK_LEARN,
				     CAMEL_MESSAGE_SEEN|CAMEL_MESSAGE_JUNK|CAMEL_MESSAGE_JUNK_LEARN);
	if (uids->len == 1)
		message_list_select(emfv->list, MESSAGE_LIST_SELECT_NEXT, 0, 0);

	message_list_free_uids(emfv->list, uids);
}

static void
emfv_popup_mark_nojunk (EPopup *ep, EPopupItem *pitem, void *data)
{
	EMFolderView *emfv = data;
	GPtrArray *uids;
	
	uids = message_list_get_selected(emfv->list);
	em_folder_view_mark_selected(emfv,
				     CAMEL_MESSAGE_JUNK|CAMEL_MESSAGE_JUNK_LEARN,
				     CAMEL_MESSAGE_JUNK_LEARN);
	if (uids->len == 1)
		message_list_select(emfv->list, MESSAGE_LIST_SELECT_NEXT, 0, 0);

	message_list_free_uids(emfv->list, uids);
}

static void
emfv_popup_delete(EPopup *ep, EPopupItem *pitem, void *data)
{
	EMFolderView *emfv = data;
	GPtrArray *uids;
	
	uids = message_list_get_selected(emfv->list);
	em_folder_view_mark_selected(emfv, CAMEL_MESSAGE_SEEN|CAMEL_MESSAGE_DELETED, CAMEL_MESSAGE_SEEN|CAMEL_MESSAGE_DELETED);
	
	if (uids->len == 1) {
		if (!message_list_select (emfv->list, MESSAGE_LIST_SELECT_NEXT, 0, 0) && emfv->hide_deleted)
			message_list_select (emfv->list, MESSAGE_LIST_SELECT_PREVIOUS, 0, 0);
	}
	em_utils_uids_free(uids);
}

static void
emfv_popup_undelete(EPopup *ep, EPopupItem *pitem, void *data)
{
	EMFolderView *emfv = data;
	em_folder_view_mark_selected(emfv, CAMEL_MESSAGE_DELETED, 0);
}

struct _move_data {
	EMFolderView *emfv;
	GPtrArray *uids;
	int delete;
};

static char *default_xfer_messages_uri = NULL;

static void
emfv_popup_move_cb(const char *uri, void *data)
{
	struct _move_data *d = data;
	
	if (uri) {
		g_free (default_xfer_messages_uri);
		default_xfer_messages_uri = g_strdup (uri);
		mail_transfer_messages(d->emfv->folder, d->uids, d->delete, uri, 0, NULL, NULL);
	} else
		em_utils_uids_free(d->uids);
	
	g_object_unref(d->emfv);
	g_free(d);
}

static void
emfv_popup_move(EPopup *ep, EPopupItem *pitem, void *data)
{
	EMFolderView *emfv = data;
	struct _move_data *d;
	
	d = g_malloc(sizeof(*d));
	d->emfv = emfv;
	g_object_ref(emfv);
	d->uids = message_list_get_selected(emfv->list);
	d->delete = TRUE;
	
	em_select_folder ((GtkWindow *) emfv, _("Select folder"), _("_Move"), default_xfer_messages_uri, emfv_popup_move_cb, d);
}

static void
emfv_popup_copy(EPopup *ep, EPopupItem *pitem, void *data)
{
	EMFolderView *emfv = data;
	struct _move_data *d;
	
	d = g_malloc(sizeof(*d));
	d->emfv = emfv;
	g_object_ref(emfv);
	d->uids = message_list_get_selected(emfv->list);
	d->delete = FALSE;
	
	em_select_folder ((GtkWindow *) emfv, _("Select folder"), _("C_opy"), default_xfer_messages_uri, emfv_popup_move_cb, d);
}

static void
emfv_set_label(EMFolderView *emfv, const char *label)
{
	GPtrArray *uids = message_list_get_selected(emfv->list);
	int i;

	for (i=0;i<uids->len;i++)
		camel_folder_set_message_user_tag(emfv->folder, uids->pdata[i], "label", label);

	message_list_free_uids(emfv->list, uids);
}

static void
emfv_popup_label_clear(EPopup *ep, EPopupItem *pitem, void *data)
{
	EMFolderView *emfv = data;
	emfv_set_label(emfv, NULL);
}

static void
emfv_popup_label_set(EPopup *ep, EPopupItem *pitem, void *data)
{
	EMFolderView *emfv = data;

	emfv_set_label(emfv, pitem->user_data);
}

static void
emfv_popup_add_sender(EPopup *ep, EPopupItem *pitem, void *data)
{
	EMFolderView *emfv = data;
	GPtrArray *uids = message_list_get_selected(emfv->list);
	CamelMessageInfo *info;
	const char *addr;

	if (uids->len == 1
	    && (info = camel_folder_get_message_info(emfv->folder, uids->pdata[0])) != NULL
	    && (addr = camel_message_info_from(info)) != NULL
	    && addr[0] != 0)
		em_utils_add_address((GtkWidget *)emfv, addr);

	em_utils_uids_free(uids);
}

static void
emfv_popup_apply_filters(EPopup *ep, EPopupItem *pitem, void *data)
{
	EMFolderView *emfv = data;
	GPtrArray *uids = message_list_get_selected(emfv->list);

	mail_filter_on_demand(emfv->folder, uids);
}

static void
emfv_popup_filter_junk(EPopup *ep, EPopupItem *pitem, void *data)
{
	EMFolderView *emfv = data;
	GPtrArray *uids = message_list_get_selected(emfv->list);

	mail_filter_junk(emfv->folder, uids);
}

/* filter callbacks, this will eventually be a wizard, see
   filter_type_current/vfolder_type_current for implementation */

#define EMFV_POPUP_AUTO_TYPE(autotype, name, type)	\
static void						\
name(EPopup *ep, EPopupItem *item, void *data)		\
{							\
	EMFolderView *emfv = data;			\
	autotype(emfv, type);				\
}

EMFV_POPUP_AUTO_TYPE(vfolder_type_current, emfv_popup_vfolder_subject, AUTO_SUBJECT)
EMFV_POPUP_AUTO_TYPE(vfolder_type_current, emfv_popup_vfolder_sender, AUTO_FROM)
EMFV_POPUP_AUTO_TYPE(vfolder_type_current, emfv_popup_vfolder_recipients, AUTO_TO)
EMFV_POPUP_AUTO_TYPE(vfolder_type_current, emfv_popup_vfolder_mlist, AUTO_MLIST)

EMFV_POPUP_AUTO_TYPE(filter_type_current, emfv_popup_filter_subject, AUTO_SUBJECT)
EMFV_POPUP_AUTO_TYPE(filter_type_current, emfv_popup_filter_sender, AUTO_FROM)
EMFV_POPUP_AUTO_TYPE(filter_type_current, emfv_popup_filter_recipients, AUTO_TO)
EMFV_POPUP_AUTO_TYPE(filter_type_current, emfv_popup_filter_mlist, AUTO_MLIST)

/* TODO: Move some of these to be 'standard' menu's */

static EPopupItem emfv_popup_items[] = {
	{ E_POPUP_ITEM, "00.emfv.00", N_("_Open"), emfv_popup_open, NULL, NULL, 0 },
	{ E_POPUP_ITEM, "00.emfv.01", N_("_Edit as New Message..."), emfv_popup_edit, NULL, NULL, EM_POPUP_SELECT_EDIT },
	{ E_POPUP_ITEM, "00.emfv.02", N_("_Save As..."), emfv_popup_saveas, NULL, "stock_save-as", 0 },
	{ E_POPUP_ITEM, "00.emfv.03", N_("_Print"), emfv_popup_print, NULL, "stock_print", 0 },

	{ E_POPUP_BAR, "10.emfv" },
	{ E_POPUP_ITEM, "10.emfv.00", N_("_Reply to Sender"), emfv_popup_reply_sender, NULL, "stock_mail-reply", EM_POPUP_SELECT_ONE },
	{ E_POPUP_ITEM, "10.emfv.01", N_("Reply to _List"), emfv_popup_reply_list, NULL, NULL, EM_POPUP_SELECT_ONE|EM_POPUP_SELECT_MAILING_LIST },
	{ E_POPUP_ITEM, "10.emfv.02", N_("Reply to _All"), emfv_popup_reply_all, NULL, "stock_mail-reply-to-all", EM_POPUP_SELECT_ONE },
	{ E_POPUP_ITEM, "10.emfv.03", N_("_Forward"), emfv_popup_forward, NULL, "stock_mail-forward", EM_POPUP_SELECT_MANY },

	{ E_POPUP_BAR, "20.emfv", NULL, NULL, NULL, NULL, EM_POPUP_SELECT_FLAG_FOLLOWUP|EM_POPUP_SELECT_FLAG_COMPLETED|EM_POPUP_SELECT_FLAG_CLEAR },
	{ E_POPUP_ITEM, "20.emfv.00", N_("Follo_w Up..."), emfv_popup_flag_followup, NULL, "stock_mail-flag-for-followup",  EM_POPUP_SELECT_FLAG_FOLLOWUP },
	{ E_POPUP_ITEM, "20.emfv.01", N_("Fla_g Completed"), emfv_popup_flag_completed, NULL, NULL, EM_POPUP_SELECT_FLAG_COMPLETED },
	{ E_POPUP_ITEM, "20.emfv.02", N_("Cl_ear Flag"), emfv_popup_flag_clear, NULL, NULL, EM_POPUP_SELECT_FLAG_CLEAR },
	
	{ E_POPUP_BAR, "30.emfv" },
	{ E_POPUP_ITEM, "30.emfv.00", N_("Mar_k as Read"), emfv_popup_mark_read, NULL, "stock_mail-open", EM_POPUP_SELECT_MARK_READ },
	{ E_POPUP_ITEM,  "30.emfv.01", N_("Mark as _Unread"), emfv_popup_mark_unread, NULL, "stock_mail-unread", EM_POPUP_SELECT_MARK_UNREAD },
	{ E_POPUP_ITEM, "30.emfv.02", N_("Mark as _Important"), emfv_popup_mark_important, NULL, "stock_mail-priority-high", EM_POPUP_SELECT_MARK_IMPORTANT },
	{ E_POPUP_ITEM, "30.emfv.03", N_("_Mark as Unimportant"), emfv_popup_mark_unimportant, NULL, NULL, EM_POPUP_SELECT_MARK_UNIMPORTANT },
	{ E_POPUP_ITEM, "30.emfv.04", N_("Mark as _Junk"), emfv_popup_mark_junk, NULL, "stock_spam", EM_POPUP_SELECT_MARK_JUNK },
	{ E_POPUP_ITEM, "30.emfv.05", N_("Mark as _Not Junk"), emfv_popup_mark_nojunk, NULL, "stock_not-spam", EM_POPUP_SELECT_MARK_NOJUNK },
	
	{ E_POPUP_BAR, "40.emfv" },
	{ E_POPUP_ITEM, "40.emfv.00", N_("_Delete"), emfv_popup_delete, NULL, "stock_delete", EM_POPUP_SELECT_DELETE },
	{ E_POPUP_ITEM, "40.emfv.01", N_("U_ndelete"), emfv_popup_undelete, NULL, "stock_undelete", EM_POPUP_SELECT_UNDELETE },

	{ E_POPUP_BAR, "50.emfv" },
	{ E_POPUP_ITEM, "50.emfv.00", N_("Mo_ve to Folder..."), emfv_popup_move },
	{ E_POPUP_ITEM, "50.emfv.01", N_("_Copy to Folder..."), emfv_popup_copy },

	{ E_POPUP_BAR, "60.label" },
	{ E_POPUP_SUBMENU, "60.label.00", N_("Label") },
	{ E_POPUP_IMAGE, "60.label.00/00.label", N_("None"), emfv_popup_label_clear },
	{ E_POPUP_BAR, "60.label.00/00.label.00" },

	{ E_POPUP_BAR, "70.emfv", NULL, NULL, NULL, NULL, EM_POPUP_SELECT_ADD_SENDER },
	{ E_POPUP_ITEM, "70.emfv.00", N_("Add Sender to Address_book"), emfv_popup_add_sender, NULL, NULL, EM_POPUP_SELECT_ADD_SENDER },

	{ E_POPUP_BAR, "80.emfv" },	
	{ E_POPUP_ITEM, "80.emfv.00", N_("Appl_y Filters"), emfv_popup_apply_filters, NULL, "stock_mail-filters-apply" },
	{ E_POPUP_ITEM, "80.emfv.01", N_("F_ilter Junk"), emfv_popup_filter_junk, NULL, "stock_spam" },

	{ E_POPUP_BAR, "90.filter", NULL, NULL, NULL, NULL, EM_POPUP_SELECT_ONE },
	{ E_POPUP_SUBMENU, "90.filter.00", N_("Crea_te Rule From Message"), NULL, NULL, NULL, EM_POPUP_SELECT_ONE },
	{ E_POPUP_ITEM, "90.filter.00/00.00", N_("VFolder on _Subject"), emfv_popup_vfolder_subject, NULL, NULL, EM_POPUP_SELECT_ONE },
	{ E_POPUP_ITEM, "90.filter.00/00.01", N_("VFolder on Se_nder"), emfv_popup_vfolder_sender, NULL, NULL, EM_POPUP_SELECT_ONE },
	{ E_POPUP_ITEM, "90.filter.00/00.02", N_("VFolder on _Recipients"), emfv_popup_vfolder_recipients, NULL, NULL, EM_POPUP_SELECT_ONE },
	{ E_POPUP_ITEM, "90.filter.00/00.03", N_("VFolder on Mailing _List"),
	  emfv_popup_vfolder_mlist, NULL, NULL, EM_POPUP_SELECT_ONE|EM_POPUP_SELECT_MAILING_LIST },
	
	{ E_POPUP_BAR, "90.filter.00/10", NULL, NULL, NULL, NULL, EM_POPUP_SELECT_ONE },
	{ E_POPUP_ITEM, "90.filter.00/10.00", N_("Filter on Sub_ject"), emfv_popup_filter_subject, NULL, NULL, EM_POPUP_SELECT_ONE },
	{ E_POPUP_ITEM, "90.filter.00/10.01", N_("Filter on Sen_der"), emfv_popup_filter_sender, NULL, NULL, EM_POPUP_SELECT_ONE },
	{ E_POPUP_ITEM, "90.filter.00/10.02", N_("Filter on Re_cipients"), emfv_popup_filter_recipients,  NULL, NULL, EM_POPUP_SELECT_ONE },
	{ E_POPUP_ITEM, "90.filter.00/10.03", N_("Filter on _Mailing List"),
	  emfv_popup_filter_mlist, NULL, NULL, EM_POPUP_SELECT_ONE|EM_POPUP_SELECT_MAILING_LIST },
};

static void
emfv_popup_labels_free(EPopup *ep, GSList *l, void *data)
{
	while (l) {
		GSList *n = l->next;
		EPopupItem *item = l->data;

		g_free(item->path);
		g_free(item);

		g_slist_free_1(l);
		l = n;
	}
}

static void
emfv_popup_items_free(EPopup *ep, GSList *items, void *data)
{
	g_slist_free(items);
}
       
static void
emfv_popup(EMFolderView *emfv, GdkEvent *event)
{
	GSList *menus = NULL, *l, *label_list = NULL;
	GtkMenu *menu;
	EMPopup *emp;
	EMPopupTargetSelect *target;
	int i;

	emp = em_popup_new("com.ximian.mail.folderview.popup.select");
	target = em_folder_view_get_popup_target(emfv, emp);

	for (i=0;i<sizeof(emfv_popup_items)/sizeof(emfv_popup_items[0]);i++)
		menus = g_slist_prepend(menus, &emfv_popup_items[i]);

	e_popup_add_items((EPopup *)emp, menus, emfv_popup_items_free, emfv);

	i = 1;
	for (l = mail_config_get_labels(); l; l = l->next) {
		EPopupItem *item;
		MailConfigLabel *label = l->data;
		GdkPixmap *pixmap;
		GdkColor colour;
		GdkGC *gc;
		
		item = g_malloc0(sizeof(*item));
		item->type = E_POPUP_IMAGE;
		item->path = g_strdup_printf("60.label.00/00.label.%02d", i++);
		item->label = label->name;
		item->activate = emfv_popup_label_set;
		item->user_data = label->tag;

		gdk_color_parse(label->colour, &colour);
		gdk_color_alloc(gdk_colormap_get_system(), &colour);
		
		pixmap = gdk_pixmap_new(((GtkWidget *)emfv)->window, 16, 16, -1);
		gc = gdk_gc_new(((GtkWidget *)emfv)->window);
		gdk_gc_set_foreground(gc, &colour);
		gdk_draw_rectangle(pixmap, gc, TRUE, 0, 0, 16, 16);
		gdk_gc_unref(gc);

		item->image = gtk_image_new_from_pixmap(pixmap, NULL);
		gtk_widget_show(item->image);

		label_list = g_slist_prepend(label_list, item);
	}

	e_popup_add_items((EPopup *)emp, label_list, emfv_popup_labels_free, emfv);

	menu = e_popup_create_menu_once((EPopup *)emp, (EPopupTarget *)target, 0);

	if (event == NULL || event->type == GDK_KEY_PRESS) {
		/* FIXME: menu pos function */
		gtk_menu_popup(menu, NULL, NULL, NULL, NULL, 0, event ? event->key.time : gtk_get_current_event_time());
	} else {
		gtk_menu_popup(menu, NULL, NULL, NULL, NULL, event->button.button, event->button.time);
	}
}

/* ********************************************************************** */

/* Bonobo menu's */

/* a lot of stuff maps directly to the popup menu equivalent */
#define EMFV_MAP_CALLBACK(from, to)				\
static void							\
from(BonoboUIComponent *uid, void *data, const char *path)	\
{								\
	to(NULL, NULL, data);					\
}

EMFV_MAP_CALLBACK(emfv_add_sender_addressbook, emfv_popup_add_sender)
EMFV_MAP_CALLBACK(emfv_message_apply_filters, emfv_popup_apply_filters)
EMFV_MAP_CALLBACK(emfv_message_filter_junk, emfv_popup_filter_junk)
EMFV_MAP_CALLBACK(emfv_message_copy, emfv_popup_copy)
EMFV_MAP_CALLBACK(emfv_message_move, emfv_popup_move)
EMFV_MAP_CALLBACK(emfv_message_forward, emfv_popup_forward)
EMFV_MAP_CALLBACK(emfv_message_reply_all, emfv_popup_reply_all)
EMFV_MAP_CALLBACK(emfv_message_reply_list, emfv_popup_reply_list)
EMFV_MAP_CALLBACK(emfv_message_reply_sender, emfv_popup_reply_sender)
EMFV_MAP_CALLBACK(emfv_message_mark_read, emfv_popup_mark_read)
EMFV_MAP_CALLBACK(emfv_message_mark_unread, emfv_popup_mark_unread)
EMFV_MAP_CALLBACK(emfv_message_mark_important, emfv_popup_mark_important)
EMFV_MAP_CALLBACK(emfv_message_mark_unimportant, emfv_popup_mark_unimportant)
EMFV_MAP_CALLBACK(emfv_message_mark_junk, emfv_popup_mark_junk)
EMFV_MAP_CALLBACK(emfv_message_mark_nojunk, emfv_popup_mark_nojunk)
EMFV_MAP_CALLBACK(emfv_message_delete, emfv_popup_delete)
EMFV_MAP_CALLBACK(emfv_message_undelete, emfv_popup_undelete)
EMFV_MAP_CALLBACK(emfv_message_followup_flag, emfv_popup_flag_followup)
/*EMFV_MAP_CALLBACK(emfv_message_followup_clear, emfv_popup_flag_clear)
  EMFV_MAP_CALLBACK(emfv_message_followup_completed, emfv_popup_flag_completed)*/
EMFV_MAP_CALLBACK(emfv_message_open, emfv_popup_open)
EMFV_MAP_CALLBACK(emfv_message_edit, emfv_popup_edit)
EMFV_MAP_CALLBACK(emfv_message_saveas, emfv_popup_saveas)
EMFV_MAP_CALLBACK(emfv_print_message, emfv_popup_print)

static void
emfv_edit_cut(BonoboUIComponent *uid, void *data, const char *path)
{
	EMFolderView *emfv = data;

	if (GTK_WIDGET_HAS_FOCUS(emfv->preview->formathtml.html))
		em_format_html_display_cut(emfv->preview);
	else
		message_list_copy(emfv->list, TRUE);
}

static void
emfv_edit_copy(BonoboUIComponent *uid, void *data, const char *path)
{
	EMFolderView *emfv = data;

	if (GTK_WIDGET_HAS_FOCUS(emfv->preview->formathtml.html))
		em_format_html_display_copy(emfv->preview);
	else
		message_list_copy(emfv->list, FALSE);
}

static void
emfv_edit_paste(BonoboUIComponent *uid, void *data, const char *path)
{
	EMFolderView *emfv = data;

	message_list_paste(emfv->list);
}

static void
emfv_mail_next(BonoboUIComponent *uid, void *data, const char *path)
{
	EMFolderView *emfv = data;

	message_list_select(emfv->list, MESSAGE_LIST_SELECT_NEXT, 0, 0);
}

static void
emfv_mail_next_flagged(BonoboUIComponent *uid, void *data, const char *path)
{
	EMFolderView *emfv = data;
	
	message_list_select(emfv->list, MESSAGE_LIST_SELECT_NEXT|MESSAGE_LIST_SELECT_WRAP, CAMEL_MESSAGE_FLAGGED, CAMEL_MESSAGE_FLAGGED);
}

static void
emfv_mail_next_unread(BonoboUIComponent *uid, void *data, const char *path)
{
	EMFolderView *emfv = data;

	message_list_select(emfv->list, MESSAGE_LIST_SELECT_NEXT|MESSAGE_LIST_SELECT_WRAP, 0, CAMEL_MESSAGE_SEEN);
}

static void
emfv_mail_next_thread(BonoboUIComponent *uid, void *data, const char *path)
{
	EMFolderView *emfv = data;

	message_list_select_next_thread(emfv->list);
}

static void
emfv_mail_previous(BonoboUIComponent *uid, void *data, const char *path)
{
	EMFolderView *emfv = data;

	message_list_select(emfv->list, MESSAGE_LIST_SELECT_PREVIOUS, 0, 0);
}

static void
emfv_mail_previous_flagged(BonoboUIComponent *uid, void *data, const char *path)
{
	EMFolderView *emfv = data;

	message_list_select(emfv->list, MESSAGE_LIST_SELECT_PREVIOUS|MESSAGE_LIST_SELECT_WRAP, CAMEL_MESSAGE_FLAGGED, CAMEL_MESSAGE_FLAGGED);
}

static void
emfv_mail_previous_unread(BonoboUIComponent *uid, void *data, const char *path)
{
	EMFolderView *emfv = data;

	message_list_select(emfv->list, MESSAGE_LIST_SELECT_PREVIOUS|MESSAGE_LIST_SELECT_WRAP, 0, CAMEL_MESSAGE_SEEN);
}

static void
emfv_message_forward_attached (BonoboUIComponent *uic, void *data, const char *path)
{
	EMFolderView *emfv = data;
	GPtrArray *uids;
	
	if (!em_utils_check_user_can_send_mail ((GtkWidget *) emfv))
		return;
	
	uids = message_list_get_selected (emfv->list);
	em_utils_forward_attached (emfv->folder, uids, emfv->folder_uri);
}

static void
emfv_message_forward_inline (BonoboUIComponent *uic, void *data, const char *path)
{
	EMFolderView *emfv = data;
	GPtrArray *uids;
	
	if (!em_utils_check_user_can_send_mail ((GtkWidget *) emfv))
		return;
	
	uids = message_list_get_selected (emfv->list);
	em_utils_forward_inline (emfv->folder, uids, emfv->folder_uri);
}

static void
emfv_message_forward_quoted (BonoboUIComponent *uic, void *data, const char *path)
{
	EMFolderView *emfv = data;
	GPtrArray *uids;
	
	if (!em_utils_check_user_can_send_mail ((GtkWidget *) emfv))
		return;
	
	uids = message_list_get_selected (emfv->list);
	em_utils_forward_quoted (emfv->folder, uids, emfv->folder_uri);
}

static void
emfv_message_redirect (BonoboUIComponent *uic, void *data, const char *path)
{
	EMFolderView *emfv = data;
	
	if (emfv->list->cursor_uid == NULL)
		return;
	
	if (!em_utils_check_user_can_send_mail ((GtkWidget *) emfv))
		return;
	
	em_utils_redirect_message_by_uid (emfv->folder, emfv->list->cursor_uid);
}

static void
emfv_message_post_reply (BonoboUIComponent *uic, void *data, const char *path)
{
	EMFolderView *emfv = data;
	
	if (emfv->list->cursor_uid == NULL)
		return;
	
	if (!em_utils_check_user_can_send_mail ((GtkWidget *) emfv))
		return;
	
	em_utils_post_reply_to_message_by_uid (emfv->folder, emfv->list->cursor_uid);
}

static void
emfv_message_reply(EMFolderView *emfv, int mode)
{
	HTMLObject *selection;
	guint len;
	
	if (emfv->list->cursor_uid == NULL)
		return;
	
	if (!em_utils_check_user_can_send_mail ((GtkWidget *) emfv))
		return;
	
	html_engine_copy_object (((EMFormatHTML *)emfv->preview)->html->engine, &selection, &len);
	if (selection != NULL) {
		HTMLEngineSaveState *state;
		
		state = html_engine_save_buffer_new(((EMFormatHTML *)emfv->preview)->html->engine, TRUE);
		html_object_save (selection, state);
		html_object_destroy (selection);
		if (state->user_data && ((GString *)state->user_data)->len) {
			CamelMimeMessage *msg, *src;
			struct _camel_header_raw *header;

			src = (CamelMimeMessage *)((EMFormat *)emfv->preview)->message;
			msg = camel_mime_message_new();

			/* need to strip content- headers */
			header = ((CamelMimePart *)src)->headers;
			while (header) {
				if (g_ascii_strncasecmp(header->name, "content-", 8) != 0)
					camel_medium_add_header((CamelMedium *)msg, header->name, header->value);
				header = header->next;
			}
			camel_mime_part_set_encoding((CamelMimePart *)msg, CAMEL_TRANSFER_ENCODING_8BIT);
			camel_mime_part_set_content((CamelMimePart *)msg,
						    ((GString *)state->user_data)->str,
						    ((GString *)state->user_data)->len,
						    "text/html");
			em_utils_reply_to_message (emfv->folder, emfv->list->cursor_uid, msg, mode, NULL);
			camel_object_unref(msg);
		} else {
			em_utils_reply_to_message (emfv->folder, emfv->list->cursor_uid, NULL, mode, (EMFormat *)emfv->preview);
		}

		html_engine_save_buffer_free(state);
	} else {
		em_utils_reply_to_message(emfv->folder, emfv->list->cursor_uid, NULL, mode, (EMFormat *)emfv->preview);
	}
}

static void
emfv_message_search(BonoboUIComponent *uic, void *data, const char *path)
{
	EMFolderView *emfv = data;

	em_format_html_display_search(emfv->preview);
}

static void
emfv_print_preview_message(BonoboUIComponent *uic, void *data, const char *path)
{
	EMFolderView *emfv = data;

	em_folder_view_print(emfv, TRUE);
}

static void
emfv_text_zoom_in(BonoboUIComponent *uic, void *data, const char *path)
{
	EMFolderView *emfv = data;

	if (emfv->preview)
		em_format_html_display_zoom_in(emfv->preview);
}

static void
emfv_text_zoom_out(BonoboUIComponent *uic, void *data, const char *path)
{
	EMFolderView *emfv = data;

	if (emfv->preview)
		em_format_html_display_zoom_out(emfv->preview);
}

static void
emfv_text_zoom_reset(BonoboUIComponent *uic, void *data, const char *path)
{
	EMFolderView *emfv = data;

	if (emfv->preview)
		em_format_html_display_zoom_reset(emfv->preview);
}

/* ********************************************************************** */

struct _filter_data {
	const char *source;
	char *uri;
	int type;
};

static void
filter_data_free (struct _filter_data *fdata)
{
	g_free (fdata->uri);
	g_free (fdata);
}

static void
filter_type_got_message (CamelFolder *folder, const char *uid, CamelMimeMessage *msg, void *user_data)
{
	struct _filter_data *data = user_data;
	
	if (msg)
		filter_gui_add_from_message (msg, data->source, data->type);
	
	filter_data_free (data);
}

static void
filter_type_uid (CamelFolder *folder, const char *uid, const char *source, int type)
{
	struct _filter_data *data;
	
	data = g_malloc0 (sizeof (*data));
	data->type = type;
	data->source = source;
	
	mail_get_message (folder, uid, filter_type_got_message, data, mail_thread_new);
}

static void
filter_type_current (EMFolderView *emfv, int type)
{
	const char *source;
	GPtrArray *uids;
	
	if (em_utils_folder_is_sent (emfv->folder, emfv->folder_uri)
	    || em_utils_folder_is_outbox (emfv->folder, emfv->folder_uri))
		source = FILTER_SOURCE_OUTGOING;
	else
		source = FILTER_SOURCE_INCOMING;
	
	uids = message_list_get_selected (emfv->list);
	
	if (uids->len == 1)
		filter_type_uid (emfv->folder, (char *) uids->pdata[0], source, type);
	
	em_utils_uids_free (uids);
}

EMFV_MAP_CALLBACK(emfv_tools_filter_subject, emfv_popup_filter_subject)
EMFV_MAP_CALLBACK(emfv_tools_filter_sender, emfv_popup_filter_sender)
EMFV_MAP_CALLBACK(emfv_tools_filter_recipient, emfv_popup_filter_recipients)
EMFV_MAP_CALLBACK(emfv_tools_filter_mlist, emfv_popup_filter_mlist)

static void
vfolder_type_got_message (CamelFolder *folder, const char *uid, CamelMimeMessage *msg, void *user_data)
{
	struct _filter_data *data = user_data;
	
	if (msg)
		vfolder_gui_add_from_message (msg, data->type, data->uri);
	
	filter_data_free (data);
}

static void
vfolder_type_uid (CamelFolder *folder, const char *uid, const char *uri, int type)
{
	struct _filter_data *data;
	
	data = g_malloc0 (sizeof (*data));
	data->type = type;
	data->uri = g_strdup (uri);
	
	mail_get_message (folder, uid, vfolder_type_got_message, data, mail_thread_new);
}

static void
vfolder_type_current (EMFolderView *emfv, int type)
{
	GPtrArray *uids;
	
	uids = message_list_get_selected (emfv->list);
	
	if (uids->len == 1)
		vfolder_type_uid (emfv->folder, (char *) uids->pdata[0], emfv->folder_uri, type);
	
	em_utils_uids_free (uids);
}

EMFV_MAP_CALLBACK(emfv_tools_vfolder_subject, emfv_popup_vfolder_subject)
EMFV_MAP_CALLBACK(emfv_tools_vfolder_sender, emfv_popup_vfolder_sender)
EMFV_MAP_CALLBACK(emfv_tools_vfolder_recipient, emfv_popup_vfolder_recipients)
EMFV_MAP_CALLBACK(emfv_tools_vfolder_mlist, emfv_popup_vfolder_mlist)

/* ********************************************************************** */

static void
emfv_view_load_images(BonoboUIComponent *uic, void *data, const char *path)
{
	EMFolderView *emfv = data;

	if (emfv->preview)
		em_format_html_load_http((EMFormatHTML *)emfv->preview);
}

static BonoboUIVerb emfv_message_verbs[] = {
	BONOBO_UI_UNSAFE_VERB ("EditCut", emfv_edit_cut),
	BONOBO_UI_UNSAFE_VERB ("EditCopy", emfv_edit_copy),
	BONOBO_UI_UNSAFE_VERB ("EditPaste", emfv_edit_paste),

	BONOBO_UI_UNSAFE_VERB ("MailNext", emfv_mail_next),
	BONOBO_UI_UNSAFE_VERB ("MailNextFlagged", emfv_mail_next_flagged),
	BONOBO_UI_UNSAFE_VERB ("MailNextUnread", emfv_mail_next_unread),
	BONOBO_UI_UNSAFE_VERB ("MailNextThread", emfv_mail_next_thread),
	BONOBO_UI_UNSAFE_VERB ("MailPrevious", emfv_mail_previous),
	BONOBO_UI_UNSAFE_VERB ("MailPreviousFlagged", emfv_mail_previous_flagged),
	BONOBO_UI_UNSAFE_VERB ("MailPreviousUnread", emfv_mail_previous_unread),

	BONOBO_UI_UNSAFE_VERB ("AddSenderToAddressbook", emfv_add_sender_addressbook),

	BONOBO_UI_UNSAFE_VERB ("MessageApplyFilters", emfv_message_apply_filters),
	BONOBO_UI_UNSAFE_VERB ("MessageFilterJunk", emfv_message_filter_junk),
	BONOBO_UI_UNSAFE_VERB ("MessageCopy", emfv_message_copy),
	BONOBO_UI_UNSAFE_VERB ("MessageDelete", emfv_message_delete),
	BONOBO_UI_UNSAFE_VERB ("MessageForward", emfv_message_forward),
	BONOBO_UI_UNSAFE_VERB ("MessageForwardAttached", emfv_message_forward_attached),
	BONOBO_UI_UNSAFE_VERB ("MessageForwardInline", emfv_message_forward_inline),
	BONOBO_UI_UNSAFE_VERB ("MessageForwardQuoted", emfv_message_forward_quoted),
	BONOBO_UI_UNSAFE_VERB ("MessageRedirect", emfv_message_redirect),
	BONOBO_UI_UNSAFE_VERB ("MessageMarkAsRead", emfv_message_mark_read),
	BONOBO_UI_UNSAFE_VERB ("MessageMarkAsUnRead", emfv_message_mark_unread),
	BONOBO_UI_UNSAFE_VERB ("MessageMarkAsImportant", emfv_message_mark_important),
	BONOBO_UI_UNSAFE_VERB ("MessageMarkAsUnimportant", emfv_message_mark_unimportant),
	BONOBO_UI_UNSAFE_VERB ("MessageMarkAsJunk", emfv_message_mark_junk),
	BONOBO_UI_UNSAFE_VERB ("MessageMarkAsNotJunk", emfv_message_mark_nojunk),
	BONOBO_UI_UNSAFE_VERB ("MessageFollowUpFlag", emfv_message_followup_flag),
	BONOBO_UI_UNSAFE_VERB ("MessageMove", emfv_message_move),
	BONOBO_UI_UNSAFE_VERB ("MessageOpen", emfv_message_open),
	BONOBO_UI_UNSAFE_VERB ("MessagePostReply", emfv_message_post_reply),
	BONOBO_UI_UNSAFE_VERB ("MessageReplyAll", emfv_message_reply_all),
	BONOBO_UI_UNSAFE_VERB ("MessageReplyList", emfv_message_reply_list),
	BONOBO_UI_UNSAFE_VERB ("MessageReplySender", emfv_message_reply_sender),
	BONOBO_UI_UNSAFE_VERB ("MessageEdit", emfv_message_edit),
	BONOBO_UI_UNSAFE_VERB ("MessageSaveAs", emfv_message_saveas),
	BONOBO_UI_UNSAFE_VERB ("MessageSearch", emfv_message_search),
	BONOBO_UI_UNSAFE_VERB ("MessageUndelete", emfv_message_undelete),

	BONOBO_UI_UNSAFE_VERB ("PrintMessage", emfv_print_message),
	BONOBO_UI_UNSAFE_VERB ("PrintPreviewMessage", emfv_print_preview_message),

	BONOBO_UI_UNSAFE_VERB ("TextZoomIn", emfv_text_zoom_in),
	BONOBO_UI_UNSAFE_VERB ("TextZoomOut", emfv_text_zoom_out),
	BONOBO_UI_UNSAFE_VERB ("TextZoomReset", emfv_text_zoom_reset),

	/* TODO: This stuff should just be 1 item that runs a wizard */
	BONOBO_UI_UNSAFE_VERB ("ToolsFilterMailingList", emfv_tools_filter_mlist),
	BONOBO_UI_UNSAFE_VERB ("ToolsFilterRecipient", emfv_tools_filter_recipient),
	BONOBO_UI_UNSAFE_VERB ("ToolsFilterSender", emfv_tools_filter_sender),
	BONOBO_UI_UNSAFE_VERB ("ToolsFilterSubject", emfv_tools_filter_subject),
	BONOBO_UI_UNSAFE_VERB ("ToolsVFolderMailingList", emfv_tools_vfolder_mlist),
	BONOBO_UI_UNSAFE_VERB ("ToolsVFolderRecipient", emfv_tools_vfolder_recipient),
	BONOBO_UI_UNSAFE_VERB ("ToolsVFolderSender", emfv_tools_vfolder_sender),
	BONOBO_UI_UNSAFE_VERB ("ToolsVFolderSubject", emfv_tools_vfolder_subject),

	BONOBO_UI_UNSAFE_VERB ("ViewLoadImages", emfv_view_load_images),
	/* ViewHeaders stuff is a radio */
	/* CaretMode is a toggle */

	BONOBO_UI_VERB_END
};
static EPixmap emfv_message_pixmaps[] = {
	E_PIXMAP ("/commands/EditCut", "stock_cut", E_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/EditCopy", "stock_copy", E_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/EditPaste", "stock_paste", E_ICON_SIZE_MENU),

	E_PIXMAP ("/commands/PrintMessage", "stock_print", E_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/PrintPreviewMessage", "stock_print-preview", E_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MessageDelete", "stock_delete", E_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MessageUndelete", "stock_undelete", E_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MessageCopy", "stock_mail-copy", E_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MessageMove", "stock_mail-move", E_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MessageReplyAll", "stock_mail-reply-to-all", E_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MessageReplySender", "stock_mail-reply", E_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MessageForward", "stock_mail-forward", E_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MessageApplyFilters", "stock_mail-filters-apply", E_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MessageFilterJunk", "stock_spam", E_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MessageSearch", "stock_search", E_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MessageSaveAs", "stock_save-as", E_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MessageMarkAsRead", "stock_mail-open", E_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MessageMarkAsUnRead", "stock_mail-unread", E_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MessageMarkAsImportant", "stock_mail-priority-high", E_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MessageMarkAsJunk", "stock_spam", E_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MessageMarkAsNotJunk", "stock_not-spam", E_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MessageFollowUpFlag", "stock_mail-flag-for-followup", E_ICON_SIZE_MENU),

	E_PIXMAP ("/Toolbar/MailMessageToolbar/MessageReplySender", "stock_mail-reply", E_ICON_SIZE_LARGE_TOOLBAR),
	E_PIXMAP ("/Toolbar/MailMessageToolbar/MessageReplyAll", "stock_mail-reply-to-all", E_ICON_SIZE_LARGE_TOOLBAR),
	E_PIXMAP ("/Toolbar/MailMessageToolbar/MessageForward", "stock_mail-forward", E_ICON_SIZE_LARGE_TOOLBAR),
	E_PIXMAP ("/Toolbar/MailMessageToolbar/PrintMessage", "stock_print", E_ICON_SIZE_LARGE_TOOLBAR),
	E_PIXMAP ("/Toolbar/MailMessageToolbar/MessageMove", "stock_mail-move", E_ICON_SIZE_LARGE_TOOLBAR),
	E_PIXMAP ("/Toolbar/MailMessageToolbar/MessageCopy", "stock_mail-copy", E_ICON_SIZE_LARGE_TOOLBAR),
	E_PIXMAP ("/Toolbar/MailMessageToolbar/MessageDelete", "stock_delete", E_ICON_SIZE_LARGE_TOOLBAR),
	E_PIXMAP ("/Toolbar/MailMessageToolbar/MessageMarkAsJunk", "stock_spam", E_ICON_SIZE_LARGE_TOOLBAR),
	E_PIXMAP ("/Toolbar/MailMessageToolbar/MessageMarkAsNotJunk", "stock_not-spam", E_ICON_SIZE_LARGE_TOOLBAR),

	E_PIXMAP ("/Toolbar/MailNextButtons/MailNext", "stock_next", E_ICON_SIZE_LARGE_TOOLBAR),
	E_PIXMAP ("/Toolbar/MailNextButtons/MailPrevious", "stock_previous", E_ICON_SIZE_LARGE_TOOLBAR),

	E_PIXMAP_END
};

/* this is added to emfv->enable_map in :init() */
static const EMFolderViewEnable emfv_enable_map[] = {
	{ "EditCut",                  EM_POPUP_SELECT_MANY },
	{ "EditCopy",                 EM_POPUP_SELECT_MANY },
	{ "EditPaste",                EM_POPUP_SELECT_FOLDER },

	/* FIXME: should these be single-selection? */
	{ "MailNext",                 EM_POPUP_SELECT_MANY|EM_FOLDER_VIEW_SELECT_NEXT_MSG },
	{ "MailNextFlagged",          EM_POPUP_SELECT_MANY },
	{ "MailNextUnread",           EM_POPUP_SELECT_MANY },
	{ "MailNextThread",           EM_POPUP_SELECT_MANY },
	{ "MailPrevious",             EM_POPUP_SELECT_MANY|EM_FOLDER_VIEW_SELECT_PREV_MSG },
	{ "MailPreviousFlagged",      EM_POPUP_SELECT_MANY },
	{ "MailPreviousUnread",       EM_POPUP_SELECT_MANY },

	{ "AddSenderToAddressbook",   EM_POPUP_SELECT_ADD_SENDER },

	{ "MessageApplyFilters",      EM_POPUP_SELECT_MANY },
	{ "MessageFilterJunk",        EM_POPUP_SELECT_MANY },
	{ "MessageCopy",              EM_POPUP_SELECT_MANY },
	{ "MessageDelete",            EM_POPUP_SELECT_MANY|EM_POPUP_SELECT_DELETE },
	{ "MessageForward",           EM_POPUP_SELECT_MANY },
	{ "MessageForwardAttached",   EM_POPUP_SELECT_MANY },
	{ "MessageForwardInline",     EM_POPUP_SELECT_ONE },
	{ "MessageForwardQuoted",     EM_POPUP_SELECT_ONE },
	{ "MessageRedirect",          EM_POPUP_SELECT_ONE },
	{ "MessageMarkAsRead",        EM_POPUP_SELECT_MANY|EM_POPUP_SELECT_MARK_READ },
	{ "MessageMarkAsUnRead",      EM_POPUP_SELECT_MANY|EM_POPUP_SELECT_MARK_UNREAD },
	{ "MessageMarkAsImportant",   EM_POPUP_SELECT_MANY|EM_POPUP_SELECT_MARK_IMPORTANT },
	{ "MessageMarkAsUnimportant", EM_POPUP_SELECT_MANY|EM_POPUP_SELECT_MARK_UNIMPORTANT },
	{ "MessageMarkAsJunk",        EM_POPUP_SELECT_MANY|EM_POPUP_SELECT_MARK_JUNK },
	{ "MessageMarkAsNotJunk",     EM_POPUP_SELECT_MANY|EM_POPUP_SELECT_MARK_NOJUNK },
	{ "MessageFollowUpFlag",      EM_POPUP_SELECT_MANY },
	{ "MessageMove",              EM_POPUP_SELECT_MANY },
	{ "MessageOpen",              EM_POPUP_SELECT_MANY },
	{ "MessagePostReply",         EM_POPUP_SELECT_ONE },
	{ "MessageReplyAll",          EM_POPUP_SELECT_ONE },
	{ "MessageReplyList",         EM_POPUP_SELECT_ONE|EM_POPUP_SELECT_MAILING_LIST },
	{ "MessageReplySender",       EM_POPUP_SELECT_ONE },
	{ "MessageEdit",              EM_POPUP_SELECT_EDIT },
	{ "MessageSaveAs",            EM_POPUP_SELECT_MANY },
	{ "MessageSearch",            EM_POPUP_SELECT_ONE },
	{ "MessageUndelete",          EM_POPUP_SELECT_MANY|EM_POPUP_SELECT_UNDELETE },
	{ "PrintMessage",             EM_POPUP_SELECT_ONE },
	{ "PrintPreviewMessage",      EM_POPUP_SELECT_ONE },

	{ "TextZoomIn",		      EM_POPUP_SELECT_ONE },
	{ "TextZoomOut",	      EM_POPUP_SELECT_ONE },
	{ "TextZoomReset",	      EM_POPUP_SELECT_ONE },

	{ "ToolsFilterMailingList",   EM_POPUP_SELECT_ONE },
	{ "ToolsFilterRecipient",     EM_POPUP_SELECT_ONE },
	{ "ToolsFilterSender",        EM_POPUP_SELECT_ONE },
	{ "ToolsFilterSubject",       EM_POPUP_SELECT_ONE },
	{ "ToolsVFolderMailingList",  EM_POPUP_SELECT_ONE },
	{ "ToolsVFolderRecipient",    EM_POPUP_SELECT_ONE },
	{ "ToolsVFolderSender",       EM_POPUP_SELECT_ONE },
	{ "ToolsVFolderSubject",      EM_POPUP_SELECT_ONE },

	{ "ViewLoadImages",	      EM_POPUP_SELECT_ONE },

	{ NULL },

	/* always enabled

	{ "ViewFullHeaders", IS_0MESSAGE, 0 },
	{ "ViewNormal",      IS_0MESSAGE, 0 },
	{ "ViewSource",      IS_0MESSAGE, 0 },
	{ "CaretMode",       IS_0MESSAGE, 0 }, */
};

static void
emfv_enable_menus(EMFolderView *emfv)
{
	guint32 disable_mask;
	GString *name;
	GSList *l;

	if (emfv->uic == NULL)
		return;

	{
		if (emfv->menu) {
			if (emfv->folder) {
				EMMenuTargetSelect *t;

				t = em_menu_target_new_select(emfv->menu, emfv->folder, emfv->folder_uri, message_list_get_selected(emfv->list));
				e_menu_update_target((EMenu *)emfv->menu, t);
			} else {
				e_menu_update_target((EMenu *)emfv->menu, NULL);
			}
		}
	}

	if (emfv->folder) {
		EMPopup *emp = em_popup_new("dummy");
		EMPopupTargetSelect *t;

		t = em_folder_view_get_popup_target(emfv, emp);
		disable_mask = t->target.mask;
		e_popup_target_free((EPopup *)emp, t);
		g_object_unref(emp);
	} else {
		disable_mask = ~0;
	}

	name = g_string_new("");
	for (l = emfv->enable_map; l; l = l->next) {
		EMFolderViewEnable *map = l->data;
		int i;

		for (i=0;map[i].name;i++) {
			int state = (map[i].mask & disable_mask) == 0;

			g_string_printf(name, "/commands/%s", map[i].name);
			bonobo_ui_component_set_prop(emfv->uic, name->str, "sensitive", state?"1":"0", NULL);
		}
	}

	g_string_free(name, TRUE);
}

/* must match em_format_mode_t order */
static const char * const emfv_display_styles[] = {
	"/commands/ViewNormal",
	"/commands/ViewFullHeaders",
	"/commands/ViewSource"
};

static void
emfv_view_mode(BonoboUIComponent *uic, const char *path, Bonobo_UIComponent_EventType type, const char *state, void *data)
{
	EMFolderView *emfv = data;
	int i;

	if (type != Bonobo_UIComponent_STATE_CHANGED
	    || state[0] == '0')
		return;

	/* TODO: I don't like this stuff much, is there any way we can move listening for such events
	   elsehwere?  Probably not I guess, unless there's a EMFolderViewContainer for bonobo usage
	   of a folder view */

	for (i=0;i<= EM_FORMAT_SOURCE;i++) {
		if (strcmp(emfv_display_styles[i]+strlen("/commands/"), path) == 0) {
			em_format_set_mode((EMFormat *)emfv->preview, i);

			if (EM_FOLDER_VIEW_GET_CLASS (emfv)->update_message_style) {
				GConfClient *gconf = mail_config_get_gconf_client ();
				
				gconf_client_set_int (gconf, "/apps/evolution/mail/display/message_style", i, NULL);
			}
			break;
		}
	}
}

static void
emfv_caret_mode(BonoboUIComponent *uic, const char *path, Bonobo_UIComponent_EventType type, const char *state, void *data)
{
	EMFolderView *emfv = data;

	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	em_format_html_display_set_caret_mode(emfv->preview, state[0] != '0');

	gconf_client_set_bool(mail_config_get_gconf_client(), "/apps/evolution/mail/display/caret_mode", state[0] != '0', NULL);
}

static void
emfv_charset_changed(BonoboUIComponent *uic, const char *path, Bonobo_UIComponent_EventType type, const char *state, void *data)
{
	EMFolderView *emfv = data;

	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	/* menu items begin with "Charset-" = 8 characters */
	if (state[0] != '0' && strlen(path) > 8) {
		path += 8;
		/* default charset used in mail view */
		if (!strcmp(path, _("Default")))
			path = NULL;

		em_format_set_charset((EMFormat *)emfv->preview, path);
	}
}

static void
emfv_activate(EMFolderView *emfv, BonoboUIComponent *uic, int act)
{
	struct _EMFolderViewPrivate *p = emfv->priv;

	if (emfv->menu)
		e_menu_activate((EMenu *)emfv->menu, uic, act);

	if (act) {
		em_format_mode_t style;
		gboolean state;
		GSList *l;

		emfv->uic = uic;

		for (l = emfv->ui_files;l;l = l->next)
			bonobo_ui_util_set_ui(uic, PREFIX, (char *)l->data, emfv->ui_app_name, NULL);

		bonobo_ui_component_add_verb_list_with_data(uic, emfv_message_verbs, emfv);
		e_pixmaps_update(uic, emfv_message_pixmaps);

		state = emfv->preview->caret_mode;
		bonobo_ui_component_set_prop(uic, "/commands/CaretMode", "state", state?"1":"0", NULL);
		bonobo_ui_component_add_listener(uic, "CaretMode", emfv_caret_mode, emfv);

		style = ((EMFormat *)emfv->preview)->mode;
		bonobo_ui_component_set_prop(uic, emfv_display_styles[style], "state", "1", NULL);
		bonobo_ui_component_add_listener(uic, "ViewNormal", emfv_view_mode, emfv);
		bonobo_ui_component_add_listener(uic, "ViewFullHeaders", emfv_view_mode, emfv);
		bonobo_ui_component_add_listener(uic, "ViewSource", emfv_view_mode, emfv);
		em_format_set_mode((EMFormat *)emfv->preview, style);
		
		if (emfv->folder && !em_utils_folder_is_sent(emfv->folder, emfv->folder_uri))
			bonobo_ui_component_set_prop(uic, "/commands/MessageEdit", "sensitive", "0", NULL);
		
		/* default charset used in mail view */
		e_charset_picker_bonobo_ui_populate (uic, "/menu/View", _("Default"), emfv_charset_changed, emfv);

		emfv_enable_menus(emfv);
		if (emfv->statusbar_active)
			bonobo_ui_component_set_translate (uic, "/", "<status><item name=\"main\"/></status>", NULL);

		/* We need to set this up to get the right view options for the message-list, even if we're not showing it */
		if (emfv->folder)
			emfv_setup_view_instance(emfv);
	} else {
		const BonoboUIVerb *v;

		/* TODO: Should this just rm /? */
		for (v = &emfv_message_verbs[0]; v->cname; v++)
			bonobo_ui_component_remove_verb(uic, v->cname);

		if (p->view_instance) {
			g_object_unref(p->view_instance);
			p->view_instance = NULL;
		}

		if (p->view_menus) {
			g_object_unref(p->view_menus);
			p->view_menus = NULL;
		}

		if (emfv->folder)
			mail_sync_folder(emfv->folder, NULL, NULL);

		emfv->uic = NULL;
	}
}

struct _print_data {
	EMFolderView *emfv;

	int preview;
	CamelFolder *folder;
	char *uid;
};

static void
emfv_print_response(GtkWidget *w, int resp, struct _print_data *data)
{
	EMFormatHTMLPrint *print;
	GnomePrintConfig *config = NULL;

	switch (resp) {
	case GNOME_PRINT_DIALOG_RESPONSE_PREVIEW:
		data->preview = TRUE;
	case GNOME_PRINT_DIALOG_RESPONSE_PRINT:
		if (w)
			config = gnome_print_dialog_get_config((GnomePrintDialog *)w);
		print = em_format_html_print_new();
		em_format_set_session((EMFormat *)print, ((EMFormat *)data->emfv->preview)->session);
		em_format_html_print_message(print, (EMFormatHTML *)data->emfv->preview, config, data->folder, data->uid, data->preview);
		g_object_unref(print);
		if (config)
			g_object_unref(config);
		break;
	}

	if (w)
		gtk_widget_destroy(w);

	g_object_unref(data->emfv);
	camel_object_unref(data->folder);
	g_free(data->uid);
	g_free(data);
}

int em_folder_view_print(EMFolderView *emfv, int preview)
{
	struct _print_data *data;
	GPtrArray *uids;

	if (emfv->folder == NULL)
		return 0;

	uids = message_list_get_selected(emfv->list);
	if (uids->len != 1) {
		message_list_free_uids(emfv->list, uids);
		return 0;
	}

	data = g_malloc0(sizeof(*data));
	data->emfv = emfv;
	g_object_ref(emfv);
	data->preview = preview;
	data->folder = emfv->folder;
	camel_object_ref(data->folder);
	data->uid = g_strdup(uids->pdata[0]);
	message_list_free_uids(emfv->list, uids);

	if (preview) {
		emfv_print_response(NULL, GNOME_PRINT_DIALOG_RESPONSE_PREVIEW, data);
	} else {
		GtkDialog *dialog = (GtkDialog *)gnome_print_dialog_new(NULL, _("Print Message"), GNOME_PRINT_DIALOG_COPIES);

		gtk_dialog_set_default_response(dialog, GNOME_PRINT_DIALOG_RESPONSE_PRINT);
		e_dialog_set_transient_for ((GtkWindow *) dialog, (GtkWidget *) emfv);
		g_signal_connect(dialog, "response", G_CALLBACK(emfv_print_response), data);
		gtk_widget_show((GtkWidget *)dialog);
	}

	return 0;
}

EMPopupTargetSelect *
em_folder_view_get_popup_target(EMFolderView *emfv, EMPopup *emp)
{
	EMPopupTargetSelect *t;

	t = em_popup_target_new_select(emp, emfv->folder, emfv->folder_uri, message_list_get_selected(emfv->list));
	t->target.widget = (GtkWidget *)emfv;

	if (emfv->list->threaded)
		t->target.mask &= ~EM_FOLDER_VIEW_SELECT_THREADED;

	if (message_list_hidden(emfv->list) != 0)
		t->target.mask &= ~EM_FOLDER_VIEW_SELECT_HIDDEN;

	if (message_list_can_select(emfv->list, MESSAGE_LIST_SELECT_NEXT, 0, 0))
		t->target.mask &= ~EM_FOLDER_VIEW_SELECT_NEXT_MSG;

	if (message_list_can_select(emfv->list, MESSAGE_LIST_SELECT_PREVIOUS, 0, 0))
		t->target.mask &= ~EM_FOLDER_VIEW_SELECT_PREV_MSG;

	/* See bug #54770 */
	if (!emfv->hide_deleted)
		t->target.mask &= ~EM_POPUP_SELECT_DELETE;
	
	return t;
}

void
em_folder_view_set_statusbar (EMFolderView *emfv, gboolean statusbar)
{
	g_return_if_fail (emfv);
	
	emfv->statusbar_active = statusbar;

	if (statusbar && emfv->uic)
		bonobo_ui_component_set_translate (emfv->uic, "/",
						   "<status><item name=\"main\"/></status>", NULL);
}

void
em_folder_view_set_hide_deleted(EMFolderView *emfv, gboolean status)
{
	if (emfv->folder && (emfv->folder->folder_flags & CAMEL_FOLDER_IS_TRASH))
		status = FALSE;

	emfv->hide_deleted = status;

	if (emfv->folder) {
		message_list_set_hidedeleted(emfv->list, status);
		g_signal_emit(emfv, signals[EMFV_CHANGED], 0);
	}
}

/* ********************************************************************** */

struct mst_t {
	EMFolderView *emfv;
	char *uid;
};

static void
mst_free (struct mst_t *mst)
{
	mst->emfv->priv->seen_id = 0;
	
	g_free (mst->uid);
	g_free (mst);
}

static int
do_mark_seen (gpointer user_data)
{
	struct mst_t *mst = user_data;
	EMFolderView *emfv = mst->emfv;
	MessageList *list = emfv->list;
	
	if (mst->uid && list->cursor_uid && !strcmp (mst->uid, list->cursor_uid))
		camel_folder_set_message_flags (emfv->folder, mst->uid, CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);
	
	return FALSE;
}

static void
emfv_list_done_message_selected(CamelFolder *folder, const char *uid, CamelMimeMessage *msg, void *data)
{
	EMFolderView *emfv = data;
	
	if (emfv->preview == NULL) {
		emfv->priv->nomarkseen = FALSE;
		g_object_unref (emfv);
		emfv_enable_menus(emfv);
		return;
	}
	
	em_format_format((EMFormat *)emfv->preview, folder, uid, msg);
	
	if (emfv->priv->seen_id)
		g_source_remove(emfv->priv->seen_id);
	
	if (msg && emfv->mark_seen && !emfv->priv->nomarkseen) {
		if (emfv->mark_seen_timeout > 0) {
			struct mst_t *mst;
		
			mst = g_new (struct mst_t, 1);
			mst->emfv = emfv;
			mst->uid = g_strdup (uid);
		
			emfv->priv->seen_id = g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE, emfv->mark_seen_timeout,
								 (GSourceFunc)do_mark_seen, mst, (GDestroyNotify)mst_free);
		} else {
			camel_folder_set_message_flags(emfv->folder, uid, CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);
		}
	}
	
	g_object_unref (emfv);
	emfv->priv->nomarkseen = FALSE;

	emfv_enable_menus(emfv);
}

static void
emfv_list_message_selected(MessageList *ml, const char *uid, EMFolderView *emfv)
{
	if (emfv->preview_active) {
		if (uid) {
			if (emfv->displayed_uid == NULL || strcmp(emfv->displayed_uid, uid) != 0) {
				g_free(emfv->displayed_uid);
				emfv->displayed_uid = g_strdup(uid);
				g_object_ref (emfv);
				/* TODO: we should manage our own thread stuff, would make cancelling outstanding stuff easier */
				mail_get_message(emfv->folder, uid, emfv_list_done_message_selected, emfv, mail_thread_queued);
			}
		} else {
			g_free(emfv->displayed_uid);
			emfv->displayed_uid = NULL;
			em_format_format((EMFormat *)emfv->preview, NULL, NULL, NULL);
			emfv->priv->nomarkseen = FALSE;
		}
	}

	emfv_enable_menus(emfv);

	g_signal_emit(emfv, signals[EMFV_CHANGED], 0);
}

static void
emfv_list_double_click(ETree *tree, gint row, ETreePath path, gint col, GdkEvent *event, EMFolderView *emfv)
{
	/* Ignore double-clicks on columns that handle thier own state */
	if (MESSAGE_LIST_COLUMN_IS_ACTIVE (col))
		return;

	em_folder_view_open_selected(emfv);
}

static int
emfv_list_right_click(ETree *tree, gint row, ETreePath path, gint col, GdkEvent *event, EMFolderView *emfv)
{
	emfv_popup(emfv, event);

	return TRUE;
}

static int
emfv_list_key_press(ETree *tree, int row, ETreePath path, int col, GdkEvent *ev, EMFolderView *emfv)
{
	GPtrArray *uids;
	int i;
	guint32 flags;

	if ((ev->key.state & GDK_CONTROL_MASK) != 0)
		return FALSE;

	switch (ev->key.keyval) {
	case GDK_Return:
	case GDK_KP_Enter:
	case GDK_ISO_Enter:
		em_folder_view_open_selected(emfv);
		break;
	case '!':
		uids = message_list_get_selected(emfv->list);

		camel_folder_freeze(emfv->folder);
		for (i = 0; i < uids->len; i++) {
			flags = camel_folder_get_message_flags(emfv->folder, uids->pdata[i]) ^ CAMEL_MESSAGE_FLAGGED;
			if (flags & CAMEL_MESSAGE_FLAGGED)
				flags &= ~CAMEL_MESSAGE_DELETED;
			camel_folder_set_message_flags(emfv->folder, uids->pdata[i],
						       CAMEL_MESSAGE_FLAGGED|CAMEL_MESSAGE_DELETED, flags);
		}
		camel_folder_thaw(emfv->folder);

		message_list_free_uids(emfv->list, uids);
		break;
	default:
		return FALSE;
	}
	
	return TRUE;
}

static gboolean
emfv_popup_menu (GtkWidget *widget)
{
	EMFolderView *emfv = (EMFolderView *)widget;

	emfv_popup (emfv, NULL);

	return TRUE;
}

static void
emfv_list_selection_change(ETree *tree, EMFolderView *emfv)
{
	/* we can't just listen to the message-list message selected thing, since we dont get them
	   in all cases.  blah */
	g_signal_emit(emfv, signals[EMFV_CHANGED], 0);
}

static void
emfv_format_link_clicked(EMFormatHTMLDisplay *efhd, const char *uri, EMFolderView *emfv)
{
	if (!strncasecmp (uri, "mailto:", 7)) {
		em_utils_compose_new_message_with_mailto (uri, emfv->folder_uri);
	} else if (*uri == '#') {
		gtk_html_jump_to_anchor (((EMFormatHTML *) efhd)->html, uri + 1);
	} else if (!strncasecmp (uri, "thismessage:", 12)) {
		/* ignore */
	} else if (!strncasecmp (uri, "cid:", 4)) {
		/* ignore */
	} else {
		GError *err = NULL;
		
		gnome_url_show (uri, &err);
		
		if (err) {
			g_warning ("gnome_url_show: %s", err->message);
			g_error_free (err);
		}
	}
}

static void
emp_uri_popup_link_copy(EPopup *ep, EPopupItem *pitem, void *data)
{
	EMFolderView *emfv = data;
	struct _EMFolderViewPrivate *p = emfv->priv;

	g_free(p->selection_uri);
	p->selection_uri = g_strdup(pitem->user_data);

	gtk_selection_owner_set(p->invisible, GDK_SELECTION_PRIMARY, gtk_get_current_event_time());
	gtk_selection_owner_set(p->invisible, GDK_SELECTION_CLIPBOARD, gtk_get_current_event_time());
}

static EPopupItem emfv_uri_popups[] = {
	{ E_POPUP_ITEM, "00.uri.01", N_("_Copy Link Location"), emp_uri_popup_link_copy, NULL, NULL, EM_POPUP_URI_NOT_MAILTO },
};

static void
emfv_uri_popup_free(EPopup *ep, GSList *list, void *data)
{
	while (list) {
		GSList *n = list->next;
		struct _EPopupItem *item = list->data;

		g_free(item->user_data);
		item->user_data = NULL;
		g_slist_free_1(list);

		list = n;
	}
}

static int
emfv_format_popup_event(EMFormatHTMLDisplay *efhd, GdkEventButton *event, const char *uri, CamelMimePart *part, EMFolderView *emfv)
{
	EMPopup *emp;
	EPopupTarget *target;
	GtkMenu *menu;

	if (uri == NULL && part == NULL) {
		/* So we don't try and popup with nothing selected - rather odd result! */
		GPtrArray *uids = message_list_get_selected(emfv->list);
		int doit = uids->len > 0;

		message_list_free_uids(emfv->list, uids);
		if (doit)
			emfv_popup(emfv, (GdkEvent *)event);
		return doit;
	}

	/* FIXME: this maybe should just fit on em-html-display, it has access to the
	   snooped part type */

	emp = em_popup_new("com.ximian.mail.folderview.popup.uri");
	if (part)
		target = (EPopupTarget *)em_popup_target_new_part(emp, part, NULL);
	else {
		GSList *menus = NULL;
		int i;
		EMPopupTargetURI *t;

		t = em_popup_target_new_uri(emp, uri);
		target = (EPopupTarget *)t;

		for (i=0;i<sizeof(emfv_uri_popups)/sizeof(emfv_uri_popups[0]);i++) {
			emfv_uri_popups[i].user_data = g_strdup(t->uri);
			menus = g_slist_prepend(menus, &emfv_uri_popups[i]);
		}
		e_popup_add_items((EPopup *)emp, menus, emfv_uri_popup_free, emfv);
	}

	menu = e_popup_create_menu_once((EPopup *)emp, target, 0);
	gtk_menu_popup(menu, NULL, NULL, NULL, NULL, event->button, event->time);

	return TRUE;
}

static void
emfv_gui_folder_changed(CamelFolder *folder, void *dummy, EMFolderView *emfv)
{
	if (!emfv->priv->destroyed) {
		emfv_enable_menus(emfv);
		g_signal_emit(emfv, signals[EMFV_LOADED], 0);
	}
	g_object_unref(emfv);
}

static void
emfv_folder_changed(CamelFolder *folder, CamelFolderChangeInfo *changes, EMFolderView *emfv)
{
	g_object_ref(emfv);
	mail_async_event_emit(emfv->async, MAIL_ASYNC_GUI, (MailAsyncFunc)emfv_gui_folder_changed, folder, NULL, emfv);
}

/* keep these two tables in sync */
enum {
	EMFV_ANIMATE_IMAGES = 1,
	EMFV_CHARSET,
	EMFV_CITATION_COLOUR,
	EMFV_CITATION_MARK,
	EMFV_CARET_MODE,
	EMFV_MESSAGE_STYLE,
	EMFV_MARK_SEEN,
	EMFV_MARK_SEEN_TIMEOUT,
	EMFV_LOAD_HTTP,
	EMFV_HEADERS,
	EMFV_SETTINGS		/* last, for loop count */
};

/* IF these get too long, update key field */
static const char * const emfv_display_keys[] = {
	"animate_images",
	"charset",
	"citation_colour",
	"mark_citations",
	"caret_mode",
	"message_style",
	"mark_seen",
	"mark_seen_timeout",
	"load_http_images",
	"headers",
};

static GHashTable *emfv_setting_key;

static void
emfv_setting_notify(GConfClient *gconf, guint cnxn_id, GConfEntry *entry, EMFolderView *emfv)
{
	GConfValue *value;
	char *tkey;

	g_return_if_fail (gconf_entry_get_key (entry) != NULL);
	
	if (!(value = gconf_entry_get_value (entry)))
		return;
	
	tkey = strrchr(entry->key, '/');
	g_return_if_fail (tkey != NULL);
	
	switch(GPOINTER_TO_INT(g_hash_table_lookup(emfv_setting_key, tkey+1))) {
	case EMFV_ANIMATE_IMAGES:
		em_format_html_display_set_animate(emfv->preview, gconf_value_get_bool (value));
		break;
	case EMFV_CHARSET:
		em_format_set_default_charset((EMFormat *)emfv->preview, gconf_value_get_string (value));
		break;
	case EMFV_CITATION_COLOUR: {
		const char *s;
		GdkColor colour;
		guint32 rgb;

		s = gconf_value_get_string (value);
		gdk_color_parse(s?s:"#737373", &colour);
		rgb = ((colour.red & 0xff00) << 8) | (colour.green & 0xff00) | ((colour.blue & 0xff00) >> 8);
		em_format_html_set_mark_citations((EMFormatHTML *)emfv->preview,
						  ((EMFormatHTML *)emfv->preview)->mark_citations, rgb);
		break; }
	case EMFV_CITATION_MARK:
		em_format_html_set_mark_citations((EMFormatHTML *)emfv->preview,
						  gconf_value_get_bool (value),
						  ((EMFormatHTML *)emfv->preview)->citation_colour);
		break;
	case EMFV_CARET_MODE:
		em_format_html_display_set_caret_mode(emfv->preview, gconf_value_get_bool (value));
		break;
	case EMFV_MESSAGE_STYLE:
		if (EM_FOLDER_VIEW_GET_CLASS (emfv)->update_message_style) {
			int style = gconf_value_get_int (value);
			
			if (style < EM_FORMAT_NORMAL || style > EM_FORMAT_SOURCE)
				style = EM_FORMAT_NORMAL;
			em_format_set_mode((EMFormat *)emfv->preview, style);
		}
		break;
	case EMFV_MARK_SEEN:
		emfv->mark_seen = gconf_value_get_bool (value);
		break;
	case EMFV_MARK_SEEN_TIMEOUT:
		emfv->mark_seen_timeout = gconf_value_get_int (value);
		break;
	case EMFV_LOAD_HTTP:
		em_format_html_set_load_http((EMFormatHTML *)emfv->preview, gconf_value_get_int(value));
		break;
	case EMFV_HEADERS: {
		GSList *header_config_list, *p;
		EMFormat *emf = (EMFormat *)emfv->preview;
		int added_headers = 0;

		header_config_list = gconf_client_get_list(gconf, "/apps/evolution/mail/display/headers", GCONF_VALUE_STRING, NULL);
      		em_format_clear_headers((EMFormat *)emfv->preview);
		p = header_config_list;
		while (p) {
			EMMailerPrefsHeader *h;
			char *xml = (char *)p->data;
			
			h = em_mailer_prefs_header_from_xml(xml);
			if (h && h->enabled) {
				em_format_add_header(emf, h->name, EM_FORMAT_HEADER_BOLD);
				added_headers++;
			}
			em_mailer_prefs_header_free(h);
			p = g_slist_next(p);
		}
		g_slist_foreach(header_config_list, (GFunc) g_free, NULL);
		g_slist_free(header_config_list);
		if (added_headers == 0)
			em_format_default_headers(emf);
		/* force a redraw */
		if (emf->message)
			em_format_redraw(emf);
		break; }
	}
}

static void
emfv_setting_setup(EMFolderView *emfv)
{
	GConfClient *gconf = gconf_client_get_default();
	GConfEntry *entry;
	GError *err = NULL;
	int i;
	char key[64];

	if (emfv_setting_key == NULL) {
		emfv_setting_key = g_hash_table_new(g_str_hash, g_str_equal);
		for (i=1;i<EMFV_SETTINGS;i++)
			g_hash_table_insert(emfv_setting_key, (void *)emfv_display_keys[i-1], GINT_TO_POINTER(i));
	}

	gconf_client_add_dir(gconf, "/apps/evolution/mail/display", GCONF_CLIENT_PRELOAD_NONE, NULL);

	for (i=1;err == NULL && i<EMFV_SETTINGS;i++) {
		sprintf(key, "/apps/evolution/mail/display/%s", emfv_display_keys[i-1]);
		entry = gconf_client_get_entry(gconf, key, NULL, TRUE, &err);
		if (entry) {
			emfv_setting_notify(gconf, 0, entry, emfv);
			gconf_entry_free(entry);
		}
	}

	if (err) {
		g_warning("Could not load display settings: %s", err->message);
		g_error_free(err);
	}

	emfv->priv->setting_notify_id = gconf_client_notify_add(gconf, "/apps/evolution/mail/display",
								(GConfClientNotifyFunc)emfv_setting_notify,
								emfv, NULL, NULL);
	g_object_unref(gconf);
}

static void
emfv_on_url (EMFolderView *emfv, const char *uri, const char *nice_uri)
{
	if (emfv->statusbar_active) {
		if (emfv->uic) {
			bonobo_ui_component_set_status (emfv->uic, nice_uri, NULL);
			/* Make sure the node keeps existing if nice_url == NULL */
			if (!nice_uri)
				bonobo_ui_component_set_translate (
					emfv->uic, "/", "<status><item name=\"main\"/></status>", NULL);
		}
	}
}

static void
emfv_on_url_cb (GObject *emitter, const char *url, EMFolderView *emfv)
{
	char *nice_url = NULL;

	if (url) {
		if (strncmp (url, "mailto:", 7) == 0) {
			CamelInternetAddress *cia = camel_internet_address_new();
			CamelURL *curl;
			char *addr;

			curl = camel_url_new(url, NULL);
			camel_address_decode((CamelAddress *)cia, curl->path);
			addr = camel_address_format((CamelAddress *)cia);
			nice_url = g_strdup_printf (_("Click to mail %s"), addr&&addr[0]?addr:(url + 7));
			g_free(addr);
			camel_url_free(curl);
			camel_object_unref(cia);
		} else
			nice_url = g_strdup_printf (_("Click to open %s"), url);
	}
	
	g_signal_emit (emfv, signals[EMFV_ON_URL], 0, url, nice_url);
	
	g_free (nice_url);
}
