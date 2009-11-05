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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>

#ifdef G_OS_WIN32
/* Work around 'DATADIR' and 'interface' lossage in <windows.h> */
#define DATADIR crap_DATADIR
#include <windows.h>
#undef DATADIR
#undef interface
#endif

#include <gconf/gconf-client.h>

#include <camel/camel-mime-message.h>
#include <camel/camel-stream.h>
#include <camel/camel-stream-filter.h>
#include <camel/camel-mime-filter.h>
#include <camel/camel-mime-filter-tohtml.h>
#include <camel/camel-mime-filter-enriched.h>
#include <camel/camel-multipart.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-url.h>
#include <camel/camel-vee-folder.h>
#include <camel/camel-disco-store.h>
#include <camel/camel-offline-store.h>
#include <camel/camel-vee-store.h>

#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-ui-component.h>
#include <bonobo/bonobo-ui-util.h>

#include <gtkhtml/gtkhtml.h>
#include <gtkhtml/gtkhtml-embedded.h>
#include <gtkhtml/gtkhtml-stream.h>

#include <libedataserver/e-data-server-util.h>
#include <libedataserver/e-msgport.h>

#include "menus/gal-view-etable.h"
#include "menus/gal-view-factory-etable.h"
#include "menus/gal-view-instance.h"
#include "menus/gal-view-menus.h"

#include "misc/e-charset-picker.h"
#include <misc/e-filter-bar.h>
#include <misc/e-spinner.h>

#include "e-util/e-error.h"
#include "e-util/e-dialog-utils.h"
#include "e-util/e-icon-factory.h"
#include "e-util/e-print.h"
#include "e-util/e-profile-event.h"
#include "e-util/e-util-private.h"
#include "e-util/e-util-labels.h"

#include "filter/filter-rule.h"

#include "em-format-html-display.h"
#include "em-format-html-print.h"
#include "em-folder-selection.h"
#include "em-folder-view.h"
#include "em-folder-browser.h"
#include "em-mailer-prefs.h"
#include "em-folder-browser.h"
#include "em-message-browser.h"
#include "message-list.h"
#include "em-utils.h"
#include "em-composer-utils.h"
#include "em-menu.h"
#include "em-event.h"

#include "mail-mt.h"
#include "mail-ops.h"
#include "mail-config.h"
#include "mail-autofilter.h"
#include "mail-vfolder.h"
#include "mail-component.h"
#include "mail-tools.h"

#include "evolution-shell-component-utils.h" /* Pixmap stuff, sigh */

#ifdef HAVE_XFREE
#include <X11/XF86keysym.h>
#endif

static void emfv_list_message_selected(MessageList *ml, const gchar *uid, EMFolderView *emfv);
static void emfv_list_built(MessageList *ml, EMFolderView *emfv);
static gint emfv_list_right_click(ETree *tree, gint row, ETreePath path, gint col, GdkEvent *event, EMFolderView *emfv);
static void emfv_list_double_click(ETree *tree, gint row, ETreePath path, gint col, GdkEvent *event, EMFolderView *emfv);
static gint emfv_list_key_press(ETree *tree, gint row, ETreePath path, gint col, GdkEvent *ev, EMFolderView *emfv);
static void emfv_list_selection_change(ETree *tree, EMFolderView *emfv);

static void emfv_format_link_clicked(EMFormatHTMLDisplay *efhd, const gchar *uri, EMFolderView *);
static gint emfv_format_popup_event(EMFormatHTMLDisplay *efhd, GdkEventButton *event, const gchar *uri, CamelMimePart *part, EMFolderView *);

static void emfv_enable_menus(EMFolderView *emfv);

static void emfv_set_folder(EMFolderView *emfv, CamelFolder *folder, const gchar *uri);
static void emfv_set_folder_uri(EMFolderView *emfv, const gchar *uri);
static void emfv_set_message(EMFolderView *emfv, const gchar *uid, gint nomarkseen);
static void emfv_activate(EMFolderView *emfv, BonoboUIComponent *uic, gint state);

static void emfv_message_reply(EMFolderView *emfv, gint mode);
static void vfolder_type_current (EMFolderView *emfv, gint type);
static void filter_type_current (EMFolderView *emfv, gint type);

static void emfv_setting_setup(EMFolderView *emfv);

static void emfv_on_url_cb(GObject *emitter, const gchar *url, EMFolderView *emfv);
static void emfv_on_url(EMFolderView *emfv, const gchar *uri, const gchar *nice_uri);

static void emfv_set_seen (EMFolderView *emfv, const gchar *uid);
static gboolean emfv_on_html_button_released_cb (GtkHTML *html, GdkEventButton *button, EMFolderView *emfv);
static gboolean emfv_popup_menu (GtkWidget *widget);

/* this is added to emfv->enable_map in :init() */
static const EMFolderViewEnable emfv_enable_map[] = {
	{ "EditCut",                  EM_POPUP_SELECT_MANY },
	{ "EditCopy",                 EM_FOLDER_VIEW_SELECT_SELECTION },
	{ "EditPaste",                EM_POPUP_SELECT_FOLDER },
	{ "SelectAllText",            EM_POPUP_SELECT_ONE },

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
	{ "MessageDeleteKey",         EM_POPUP_SELECT_MANY},
	{ "MessageForward",           EM_POPUP_SELECT_MANY },
	{ "MessageForwardAttached",   EM_POPUP_SELECT_MANY },
	{ "MessageForwardInline",     EM_POPUP_SELECT_ONE },
	{ "MessageForwardQuoted",     EM_POPUP_SELECT_ONE },
	{ "MessageRedirect",          EM_POPUP_SELECT_ONE },
	{ "MessageMarkAsRead",        EM_POPUP_SELECT_MANY|EM_POPUP_SELECT_MARK_READ },
	{ "MessageMarkAsUnRead",      EM_POPUP_SELECT_MANY|EM_POPUP_SELECT_MARK_UNREAD },
	{ "MessageMarkAsImportant",   EM_POPUP_SELECT_MANY|EM_POPUP_SELECT_MARK_IMPORTANT },
	{ "MessageMarkAsUnimportant", EM_POPUP_SELECT_MANY|EM_POPUP_SELECT_MARK_UNIMPORTANT },
	{ "MessageMarkAsJunk",        EM_POPUP_SELECT_MANY|EM_POPUP_SELECT_JUNK },
	{ "MessageMarkAsNotJunk",     EM_POPUP_SELECT_MANY},
	{ "MessageFollowUpFlag",      EM_POPUP_SELECT_MANY|EM_POPUP_SELECT_FLAG_FOLLOWUP },
	{ "MessageFollowUpComplete",  EM_POPUP_SELECT_MANY|EM_POPUP_SELECT_FLAG_COMPLETED },
	{ "MessageFollowUpClear",     EM_POPUP_SELECT_MANY|EM_POPUP_SELECT_FLAG_CLEAR },
	{ "MessageMove",              EM_POPUP_SELECT_MANY },
	{ "MessageOpen",              EM_POPUP_SELECT_MANY },
	{ "MessageReplyAll",          EM_POPUP_SELECT_ONE },
	{ "MessageReplyList",         EM_POPUP_SELECT_ONE|EM_POPUP_SELECT_MAILING_LIST },
	{ "MessageReplySender",       EM_POPUP_SELECT_ONE },
	{ "MessageEdit",              EM_POPUP_SELECT_ONE },
	{ "MessageSaveAs",            EM_POPUP_SELECT_MANY },
	{ "MessageSearch",            EM_POPUP_SELECT_ONE| EM_FOLDER_VIEW_PREVIEW_PRESENT },
	{ "MessageUndelete",          EM_POPUP_SELECT_MANY|EM_POPUP_SELECT_UNDELETE },
	{ "PrintMessage",             EM_POPUP_SELECT_ONE },
	{ "PrintPreviewMessage",      EM_POPUP_SELECT_ONE },

	{ "TextZoomIn",		      EM_POPUP_SELECT_ONE },
	{ "TextZoomOut",	      EM_POPUP_SELECT_ONE },
	{ "TextZoomReset",	      EM_POPUP_SELECT_ONE },

	{ "ToolsFilterMailingList",   EM_POPUP_SELECT_ONE|EM_POPUP_SELECT_MAILING_LIST},
	{ "ToolsFilterRecipient",     EM_POPUP_SELECT_ONE },
	{ "ToolsFilterSender",        EM_POPUP_SELECT_ONE },
	{ "ToolsFilterSubject",       EM_POPUP_SELECT_ONE },
	{ "ToolsVFolderMailingList",  EM_POPUP_SELECT_ONE|EM_POPUP_SELECT_MAILING_LIST},
	{ "ToolsVFolderRecipient",    EM_POPUP_SELECT_ONE },
	{ "ToolsVFolderSender",       EM_POPUP_SELECT_ONE },
	{ "ToolsVFolderSubject",      EM_POPUP_SELECT_ONE },

	{ "ViewLoadImages",	      EM_POPUP_SELECT_ONE },
	{ "ViewSource",               EM_POPUP_SELECT_ONE },

	/* always enabled */
	{ "MailStop", 0 },

	{ NULL },
};

struct _EMFolderViewPrivate {
	guint setting_notify_id;
	guint selected_id;
	guint nomarkseen:1;
	guint destroyed:1;

	GtkWidget *invisible;
	gchar *selection_uri;

	GalViewInstance *view_instance;
	GalViewMenus *view_menus;

	gchar *selected_uid;
};

static GtkVBoxClass *emfv_parent;

enum {
	EMFV_ON_URL,
	EMFV_LOADED,
	EMFV_CHANGED,
	LAST_SIGNAL
};

extern CamelSession *session;

static guint signals[LAST_SIGNAL];

static void emfv_selection_get(GtkWidget *widget, GtkSelectionData *data, guint info, guint time_stamp, EMFolderView *emfv);
static void emfv_selection_clear_event(GtkWidget *widget, GdkEventSelection *event, EMFolderView *emfv);

#ifdef ENABLE_PROFILING
static void
emfv_format_complete(EMFormat *emf, EMFolderView *emfv)
{
	e_profile_event_emit("goto.done", emf->uid?emf->uid:"", 0);
}
#endif

static void
emfv_init(GObject *o)
{
	EMFolderView *emfv = (EMFolderView *)o;
	struct _EMFolderViewPrivate *p;

	gtk_box_set_homogeneous (GTK_BOX (emfv), FALSE);

	p = emfv->priv = g_malloc0(sizeof(struct _EMFolderViewPrivate));

	emfv->statusbar_active = TRUE;
	emfv->list_active = FALSE;

	emfv->ui_files = g_slist_append(NULL,
					g_build_filename (EVOLUTION_UIDIR,
							  "evolution-mail-message.xml",
							  NULL));

	emfv->ui_app_name = "evolution-mail";

	emfv->enable_map = g_slist_prepend(NULL, (gpointer)emfv_enable_map);

	emfv->list = (MessageList *)message_list_new();
	g_signal_connect(emfv->list, "message_selected", G_CALLBACK(emfv_list_message_selected), emfv);
	g_signal_connect(emfv->list, "message_list_built", G_CALLBACK(emfv_list_built), emfv);

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
	g_signal_connect (((EMFormatHTML *)emfv->preview)->html, "button-release-event", G_CALLBACK (emfv_on_html_button_released_cb), emfv);
#ifdef ENABLE_PROFILING
	g_signal_connect(emfv->preview, "complete", G_CALLBACK (emfv_format_complete), emfv);
#endif
	p->invisible = gtk_invisible_new();
	g_signal_connect(p->invisible, "selection_get", G_CALLBACK(emfv_selection_get), emfv);
	g_signal_connect(p->invisible, "selection_clear_event", G_CALLBACK(emfv_selection_clear_event), emfv);
	gtk_selection_add_target(p->invisible, GDK_SELECTION_PRIMARY, GDK_SELECTION_TYPE_STRING, 0);
	gtk_selection_add_target(p->invisible, GDK_SELECTION_CLIPBOARD, GDK_SELECTION_TYPE_STRING, 1);

	emfv->async = mail_async_event_new();

	emfv_setting_setup(emfv);
}

static void
free_one_ui_file (gpointer data,
		  gpointer user_data)
{
	g_free (data);
}

static void
emfv_finalise(GObject *o)
{
	EMFolderView *emfv = (EMFolderView *)o;
	struct _EMFolderViewPrivate *p = emfv->priv;

	g_slist_foreach (emfv->ui_files, free_one_ui_file, NULL);
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

	if (emfv->list && emfv->list->seen_id) {
		g_source_remove(emfv->list->seen_id);
		emfv->list->seen_id = 0;
	}

	if (p->setting_notify_id) {
		GConfClient *gconf = gconf_client_get_default();

		gconf_client_notify_remove(gconf, p->setting_notify_id);
		p->setting_notify_id = 0;
		g_object_unref(gconf);
	}

	if (emfv->folder) {
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
		gtk_object_destroy((GtkObject *)p->invisible);
		p->invisible = NULL;
	}

	if (p->selected_id != 0) {
		g_source_remove(p->selected_id);
		p->selected_id = 0;
	}

	g_free(p->selected_uid);
	p->selected_uid = NULL;

	g_free (emfv->displayed_uid);
	emfv->displayed_uid = NULL;

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
					     e_marshal_VOID__STRING_STRING,
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
	EMFolderView *emfv = g_object_new(em_folder_view_get_type(), NULL);

	return (GtkWidget *)emfv;
}

/* flag all selected messages. Return number flagged */
/* FIXME: Should this be part of message-list instead? */
gint
em_folder_view_mark_selected(EMFolderView *emfv, guint32 mask, guint32 set)
{
	GPtrArray *uids;
	gint i;

	if (emfv->folder == NULL)
		return 0;

	uids = message_list_get_selected(emfv->list);
	if (!CAMEL_IS_VEE_FOLDER(emfv->folder))
		camel_folder_freeze(emfv->folder);

	for (i=0; i<uids->len; i++)
		camel_folder_set_message_flags(emfv->folder, uids->pdata[i], mask, set);

	message_list_free_uids(emfv->list, uids);
	if (!CAMEL_IS_VEE_FOLDER(emfv->folder))
		camel_folder_thaw(emfv->folder);

	return i;
}

/* should this be elsewhere/take a uid list? */
gint
em_folder_view_open_selected(EMFolderView *emfv)
{
	GPtrArray *uids, *views;
	gint i = 0;

	uids = message_list_get_selected(emfv->list);

	if (uids->len >= 10) {
		gchar *num = g_strdup_printf("%d", uids->len);
		gint doit;

		doit = em_utils_prompt_user((GtkWindow *)gtk_widget_get_toplevel((GtkWidget *)emfv),
					    "/apps/evolution/mail/prompts/open_many",
					    "mail:ask-open-many", num, NULL);
		g_free(num);
		if (!doit) {
			message_list_free_uids(emfv->list, uids);
			return 0;
		}
	}

        if (em_utils_folder_is_drafts(emfv->folder, emfv->folder_uri)
	    || em_utils_folder_is_templates(emfv->folder, emfv->folder_uri)
	    || em_utils_folder_is_outbox(emfv->folder, emfv->folder_uri)) {
		em_utils_edit_messages(emfv->folder, uids, TRUE);
		return uids->len;
	}

	/* for vfolders we need to edit the *original*, not the vfolder copy */
	views = g_ptr_array_new();
	for (i=0;i<uids->len;i++) {
		if (camel_object_is((CamelObject *)emfv->folder, camel_vee_folder_get_type())) {
			CamelVeeMessageInfo *vinfo = (CamelVeeMessageInfo *)camel_folder_get_message_info(emfv->folder, uids->pdata[i]);

			if (vinfo) {
				gchar *uid;
				/* TODO: get_location shouldn't strdup the uid */
				CamelFolder *f = camel_vee_folder_get_location((CamelVeeFolder *)emfv->folder, vinfo, &uid);
				gchar *uri = mail_tools_folder_to_url(f);

				if (em_utils_folder_is_drafts(f, uri) || em_utils_folder_is_outbox(f, uri)) {
					GPtrArray *edits = g_ptr_array_new();

					g_ptr_array_add(edits, uid);
					em_utils_edit_messages(f, edits, TRUE);
				} else {
					g_free(uid);
					g_ptr_array_add(views, g_strdup(uids->pdata[i]));
				}
				g_free(uri);
			}
		} else {
			g_ptr_array_add(views, g_strdup(uids->pdata[i]));
		}
	}

	/* TODO: have an em_utils_open_messages call? */
	for (i=0; i<views->len; i++) {
		EMMessageBrowser *emmb;

		emmb = (EMMessageBrowser *)em_message_browser_window_new();
		message_list_set_threaded(((EMFolderView *)emmb)->list, emfv->list->threaded);
		/* always keep actual message in a list view, even it doesn't belong to the filter anymore */
		message_list_ensure_message (((EMFolderView *)emmb)->list, views->pdata[i]);
		message_list_set_search (((EMFolderView *)emmb)->list, emfv->list->search);
		em_folder_view_set_hide_deleted((EMFolderView *)emmb, emfv->hide_deleted);
		/* FIXME: session needs to be passed easier than this */
		em_format_set_session((EMFormat *)((EMFolderView *)emmb)->preview, ((EMFormat *)emfv->preview)->session);
		em_folder_view_set_folder((EMFolderView *)emmb, emfv->folder, emfv->folder_uri);
		em_folder_view_set_message((EMFolderView *)emmb, views->pdata[i], FALSE);
		gtk_widget_show(emmb->window);
		/* TODO: this loads the message twice (!) */
		em_utils_handle_receipt (emfv->folder, uids->pdata[i], NULL);
		g_free(views->pdata[i]);
	}
	g_ptr_array_free(views, TRUE);

	message_list_free_uids(emfv->list, uids);

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
	static GalViewCollection *collection = NULL;
	struct _EMFolderViewPrivate *p = emfv->priv;
	gboolean outgoing, show_wide=FALSE;
	gchar *id;

	g_return_if_fail (emfv->folder);
	g_return_if_fail (emfv->folder_uri);

	if (collection == NULL) {
		ETableSpecification *spec;
		GalViewFactory *factory;
		const gchar *evolution_dir;
		gchar *dir;
		gchar *galviewsmaildir;
		gchar *etspecfile;

		collection = gal_view_collection_new ();

		gal_view_collection_set_title (collection, _("Mail"));

		evolution_dir = mail_component_peek_base_directory (mail_component_peek ());
		galviewsmaildir = g_build_filename (EVOLUTION_GALVIEWSDIR,
						    "mail",
						    NULL);
		dir = g_build_filename (evolution_dir, "views", NULL);
		gal_view_collection_set_storage_directories (collection, galviewsmaildir, dir);
		g_free (dir);
		g_free (galviewsmaildir);

		spec = e_table_specification_new ();
		etspecfile = g_build_filename (EVOLUTION_ETSPECDIR,
					       "message-list.etspec",
					       NULL);
		if (!e_table_specification_load_from_file (spec, etspecfile))
			g_error ("Unable to load ETable specification file "
				 "for mail");
		g_free (etspecfile);

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

	/* TODO: should this go through mail-config api? */
	id = mail_config_folder_to_safe_url (emfv->folder);
	p->view_instance = gal_view_instance_new (collection, id);

	show_wide = emfv->list_active ? em_folder_browser_get_wide ((EMFolderBrowser *) emfv):FALSE;
	if (show_wide) {
		gchar *safe_id, *filename;

		/* Force to use the wide view */
		g_free (p->view_instance->custom_filename);
		g_free (p->view_instance->current_view_filename);
		safe_id = g_strdup (id);
		e_filename_make_safe (safe_id);
		filename = g_strdup_printf ("custom_wide_view-%s.xml", safe_id);
		p->view_instance->custom_filename = g_build_filename (collection->local_dir, filename, NULL);
		g_free (filename);
		filename = g_strdup_printf ("current_wide_view-%s.xml", safe_id);
		p->view_instance->current_view_filename = g_build_filename (collection->local_dir, filename, NULL);
		g_free (filename);
		g_free (safe_id);
	}
	g_free (id);

	outgoing = em_utils_folder_is_drafts (emfv->folder, emfv->folder_uri)
		|| em_utils_folder_is_sent (emfv->folder, emfv->folder_uri)
		|| em_utils_folder_is_outbox (emfv->folder, emfv->folder_uri);

	if (outgoing) {
		if (show_wide)
			gal_view_instance_set_default_view(p->view_instance, "Wide_View_Sent");
		else
			gal_view_instance_set_default_view(p->view_instance, "As_Sent_Folder");
	} else if (show_wide) {
		gal_view_instance_set_default_view(p->view_instance, "Wide_View_Normal");
	}

	gal_view_instance_load(p->view_instance);

	if (!gal_view_instance_exists(p->view_instance)) {
		struct stat st;
		gchar *path;

		path = mail_config_folder_to_cachename (emfv->folder, "et-header-");
		if (path && g_stat (path, &st) == 0 && st.st_size > 0 && S_ISREG (st.st_mode)) {
			ETableSpecification *spec;
			ETableState *state;
			GalView *view;
			gchar *etspecfile;

			spec = e_table_specification_new ();
			etspecfile = g_build_filename (EVOLUTION_ETSPECDIR,
						       "message-list.etspec",
						       NULL);
			e_table_specification_load_from_file (spec, etspecfile);
			g_free (etspecfile);
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

void em_folder_view_setup_view_instance (EMFolderView *emfv)
{
	emfv_setup_view_instance (emfv);
}

/* ********************************************************************** */

static void
emfv_set_folder(EMFolderView *emfv, CamelFolder *folder, const gchar *uri)
{
	gint isout = (folder && uri
		     && (em_utils_folder_is_drafts(folder, uri)
			 || em_utils_folder_is_sent(folder, uri)
			 || em_utils_folder_is_outbox(folder, uri)));

	if (folder == emfv->folder)
		return;

	if (emfv->priv->selected_id)
		g_source_remove(emfv->priv->selected_id);

	if (emfv->preview)
		em_format_format ((EMFormat *) emfv->preview, NULL, NULL, NULL);

	message_list_set_folder(emfv->list, folder, uri, isout);
	g_free(emfv->folder_uri);
	emfv->folder_uri = uri ? g_strdup(uri):NULL;

	if (emfv->folder) {
		emfv->hide_deleted = emfv->list->hidedeleted; /* <- a bit nasty but makes it track the display better */
		mail_sync_folder (emfv->folder, NULL, NULL);
		camel_object_unref(emfv->folder);
	}

	emfv->folder = folder;
	if (folder) {
		/* We need to set this up to get the right view options for the message-list,
		 * even if we're not showing it */
		emfv_setup_view_instance(emfv);
		camel_object_ref(folder);
	}

	emfv_enable_menus(emfv);

	/* TODO: should probably be called after all processing, not just this class's impl */
	g_signal_emit(emfv, signals[EMFV_LOADED], 0);
}

static void
emfv_got_folder(gchar *uri, CamelFolder *folder, gpointer data)
{
	EMFolderView *emfv = data;

	em_folder_view_set_folder(emfv, folder, uri);
}

static void
emfv_set_folder_uri(EMFolderView *emfv, const gchar *uri)
{
	mail_get_folder(uri, 0, emfv_got_folder, emfv, mail_msg_fast_ordered_push);
}

static void
emfv_set_message(EMFolderView *emfv, const gchar *uid, gint nomarkseen)
{
	e_profile_event_emit("goto.uid", uid?uid:"<none>", 0);

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

	gtk_selection_data_set(data, data->target, 8, (guchar *)p->selection_uri, strlen(p->selection_uri));
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
emfv_popup_open(EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderView *emfv = data;
	em_folder_view_open_selected(emfv);
}

static void
emfv_popup_edit (EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderView *emfv = data;
	GPtrArray *uids;

	if (!em_utils_check_user_can_send_mail((GtkWidget *)emfv))
		return;

	uids = message_list_get_selected(emfv->list);
	em_utils_edit_messages (emfv->folder, uids, FALSE);
}

static void
emfv_popup_saveas(EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderView *emfv = data;
	GPtrArray *uids;

	uids = message_list_get_selected(emfv->list);
	em_utils_save_messages((GtkWidget *)emfv, emfv->folder, uids);
}

static void
emfv_view_load_images(BonoboUIComponent *uic, gpointer data, const gchar *path)
{
	EMFolderView *emfv = data;

	if (emfv->preview)
		em_format_html_load_http((EMFormatHTML *)emfv->preview);
}

static void
emfv_popup_print(EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderView *emfv = data;
	em_folder_view_print(emfv, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG);
}

static void
emfv_popup_copy_text(EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderView *emfv = data;
	gtk_html_copy (((EMFormatHTML *)emfv->preview)->html);
}

static void
emfv_popup_source(EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderView *emfv = data;
	EMMessageBrowser *emmb;
	GPtrArray *uids;

	uids = message_list_get_selected(emfv->list);

	emmb = (EMMessageBrowser *)em_message_browser_window_new();
	em_format_set_session((EMFormat *)((EMFolderView *)emmb)->preview, ((EMFormat *)emfv->preview)->session);
	message_list_ensure_message (((EMFolderView *)emmb)->list, uids->pdata[0]);
	em_folder_view_set_folder((EMFolderView *)emmb, emfv->folder, emfv->folder_uri);
	em_format_set_mode((EMFormat *)((EMFolderView *)emmb)->preview, EM_FORMAT_SOURCE);
	em_folder_view_set_message((EMFolderView *)emmb, uids->pdata[0], FALSE);
	gtk_widget_show(emmb->window);

	message_list_free_uids(emfv->list, uids);
}

static void
emfv_mail_compose(BonoboUIComponent *uid, gpointer data, const gchar *path)
{
	EMFolderView *emfv = data;

	if (!em_utils_check_user_can_send_mail((GtkWidget *)emfv))
		return;

	em_utils_compose_new_message(emfv->folder_uri);
}

static void
emfv_popup_reply_sender(EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderView *emfv = data;
	emfv_message_reply(emfv, REPLY_MODE_SENDER);
}

static void
emfv_popup_reply_list(EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderView *emfv = data;
	emfv_message_reply(emfv, REPLY_MODE_LIST);
}

static void
emfv_popup_reply_all(EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderView *emfv = data;
	emfv_message_reply(emfv, REPLY_MODE_ALL);
}

static void
emfv_popup_forward(EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderView *emfv = data;
	GPtrArray *uids;

	if (!em_utils_check_user_can_send_mail((GtkWidget *)emfv))
		return;

	uids = message_list_get_selected(emfv->list);
	em_utils_forward_messages (emfv->folder, uids, emfv->folder_uri);
}

static void
emfv_popup_flag_followup(EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderView *emfv = data;
	GPtrArray *uids = message_list_get_selected(emfv->list);

	em_utils_flag_for_followup((GtkWidget *)emfv, emfv->folder, uids);
}

static void
emfv_popup_flag_completed(EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderView *emfv = data;
	GPtrArray *uids;

	uids = message_list_get_selected(emfv->list);
	em_utils_flag_for_followup_completed((GtkWidget *)emfv, emfv->folder, uids);

	if (emfv->preview)
		em_format_redraw (EM_FORMAT (emfv->preview));
}

static void
emfv_popup_flag_clear(EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderView *emfv = data;
	GPtrArray *uids = message_list_get_selected(emfv->list);

	em_utils_flag_for_followup_clear((GtkWidget *)emfv, emfv->folder, uids);

	if (emfv->preview)
		em_format_redraw (EM_FORMAT (emfv->preview));
}

static void
emfv_popup_mark_read(EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderView *emfv = data;
	em_folder_view_mark_selected(emfv, CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);
}

static void
emfv_popup_mark_unread(EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderView *emfv = data;
	em_folder_view_mark_selected(emfv, CAMEL_MESSAGE_SEEN|CAMEL_MESSAGE_DELETED, 0);

	if (emfv->list->seen_id) {
		g_source_remove(emfv->list->seen_id);
		emfv->list->seen_id = 0;
	}
}

static void
emfv_popup_mark_important(EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderView *emfv = data;
	em_folder_view_mark_selected(emfv, CAMEL_MESSAGE_FLAGGED|CAMEL_MESSAGE_DELETED, CAMEL_MESSAGE_FLAGGED);
}

static void
emfv_popup_mark_unimportant(EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderView *emfv = data;
	em_folder_view_mark_selected(emfv, CAMEL_MESSAGE_FLAGGED, 0);
}

static void
emfv_select_next_message (EMFolderView *emfv, gint count, gboolean always_can_previous)
{
	if (emfv && count == 1) {
		if (!message_list_select (emfv->list, MESSAGE_LIST_SELECT_NEXT, 0, 0) && (emfv->hide_deleted || always_can_previous))
			message_list_select (emfv->list, MESSAGE_LIST_SELECT_PREVIOUS, 0, 0);
	}
}

static void
emfv_popup_mark_junk (EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderView *emfv = data;
	gint count;

	count = em_folder_view_mark_selected(emfv,
					     CAMEL_MESSAGE_SEEN|CAMEL_MESSAGE_JUNK|CAMEL_MESSAGE_NOTJUNK|CAMEL_MESSAGE_JUNK_LEARN,
					     CAMEL_MESSAGE_SEEN|CAMEL_MESSAGE_JUNK|CAMEL_MESSAGE_JUNK_LEARN);

	emfv_select_next_message (emfv, count, TRUE);
}

static void
emfv_popup_mark_nojunk (EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderView *emfv = data;
	gint count;

	count = em_folder_view_mark_selected(emfv,
					     CAMEL_MESSAGE_JUNK|CAMEL_MESSAGE_NOTJUNK|CAMEL_MESSAGE_JUNK_LEARN,
					     CAMEL_MESSAGE_NOTJUNK|CAMEL_MESSAGE_JUNK_LEARN);

	emfv_select_next_message (emfv, count, TRUE);
}

#define DelInVFolderCheckName  "DelInVFolderCheck"
#define DelInVFolderKey        "/apps/evolution/mail/prompts/delete_in_vfolder"

static void
emfv_delete_msg_response (GtkWidget *dialog, gint response, gpointer data)
{
	if (response == GTK_RESPONSE_OK) {
		EMFolderView *emfv = data;
		gint count;
		GPtrArray *uids;

		if (dialog) {
			GList *children, *l;
			GtkWidget *check = NULL;

			children = gtk_container_get_children (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox));
			for (l = children; l; l = l->next) {
				if (GTK_IS_ALIGNMENT (l->data)) {
					check =  gtk_bin_get_child (GTK_BIN (l->data));

					if (check && GTK_IS_CHECK_BUTTON (check) &&
					    !strcmp (gtk_widget_get_name (check), DelInVFolderCheckName))
						break;

					check = NULL;
				}
			}

			if (check) {
				GConfClient *gconf = gconf_client_get_default ();
				gconf_client_set_bool (gconf, DelInVFolderKey, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check)), NULL);
				g_object_unref (gconf);
			}

			g_list_free (children);
		}

		uids = message_list_get_selected(emfv->list);
		if (!CAMEL_IS_VEE_FOLDER(emfv->folder))
			camel_folder_freeze(emfv->folder);

		for (count=0; count < uids->len; count++) {
			if (camel_folder_get_message_flags (emfv->folder, uids->pdata[count]) & CAMEL_MESSAGE_USER_NOT_DELETABLE) {
				if (emfv->preview_active) {
					GtkHTMLStream *hstream = gtk_html_begin(((EMFormatHTML *)emfv->preview)->html);

					gtk_html_stream_printf(hstream, "<h2>%s</h2><p>%s</p>",
							_("Mail Deletion Failed"),
							_("You do not have sufficient permissions to delete this mail."));
					gtk_html_stream_close(hstream, GTK_HTML_STREAM_OK);
				} else {
					GtkWidget *w = e_error_new (NULL, "mail:no-delete-permission", "", NULL);
					em_utils_show_error_silent (w);
				}

				count = -1;
				break;
			} else
				camel_folder_set_message_flags(emfv->folder, uids->pdata[count], CAMEL_MESSAGE_SEEN|CAMEL_MESSAGE_DELETED, CAMEL_MESSAGE_SEEN|CAMEL_MESSAGE_DELETED );
		}

		message_list_free_uids(emfv->list, uids);
		if (!CAMEL_IS_VEE_FOLDER(emfv->folder))
			camel_folder_thaw(emfv->folder);

		emfv_select_next_message (emfv, count, FALSE);
	}

	if (dialog)
		gtk_widget_destroy (dialog);
}

static void
emfv_popup_delete (EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderView *emfv = data;
	GConfClient *gconf = gconf_client_get_default ();

	if (emfv->folder && emfv->folder->parent_store && CAMEL_IS_VEE_STORE (emfv->folder->parent_store)
	    && !gconf_client_get_bool (gconf, DelInVFolderKey, NULL)) {
		GtkWidget *dialog, *checkbox, *align;

		dialog = e_error_new (NULL, "mail:ask-delete-vfolder-msg", emfv->folder->full_name, NULL);
		g_signal_connect (dialog, "response", G_CALLBACK (emfv_delete_msg_response), emfv);
		checkbox = gtk_check_button_new_with_label (_("Do not ask me again."));
		gtk_widget_set_name (checkbox, DelInVFolderCheckName);
		align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
		gtk_container_add (GTK_CONTAINER (align), checkbox);
		gtk_widget_show (checkbox);
		gtk_box_pack_end (GTK_BOX (GTK_DIALOG (dialog)->vbox), align, TRUE, TRUE, 6);
		gtk_widget_show (align);
		gtk_widget_show (dialog);
	} else {
		emfv_delete_msg_response (NULL, GTK_RESPONSE_OK, emfv);
	}

	g_object_unref (gconf);
}
#undef DelInVFolderCheckName
#undef DelInVFolderKey

static void
emfv_popup_undelete(EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderView *emfv = data;
	em_folder_view_mark_selected(emfv, CAMEL_MESSAGE_DELETED, 0);
}

struct _move_data {
	EMFolderView *emfv;
	GPtrArray *uids;
	gint delete;
};

static gchar *default_xfer_messages_uri = NULL;

static void
emfv_popup_move_cb(const gchar *uri, gpointer data)
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
emfv_popup_move(EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderView *emfv = data;
	struct _move_data *d;

	d = g_malloc(sizeof(*d));
	d->emfv = emfv;
	g_object_ref(emfv);
	d->uids = message_list_get_selected(emfv->list);
	d->delete = TRUE;

	em_select_folder ((GtkWindow *) emfv, _("Select folder"), _("_Move"), default_xfer_messages_uri, NULL, emfv_popup_move_cb, d);
}

static void
emfv_popup_copy(EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderView *emfv = data;
	struct _move_data *d;

	d = g_malloc(sizeof(*d));
	d->emfv = emfv;
	g_object_ref(emfv);
	d->uids = message_list_get_selected(emfv->list);
	d->delete = FALSE;

	em_select_folder ((GtkWindow *) emfv, _("Select folder"), _("C_opy"), default_xfer_messages_uri, NULL, emfv_popup_move_cb, d);
}

static void
emfv_set_label (EMFolderView *emfv, const gchar *label)
{
	GPtrArray *uids = message_list_get_selected (emfv->list);
	gint i;

	for (i = 0; i < uids->len; i++)
		camel_folder_set_message_user_flag (emfv->folder, uids->pdata[i], label, TRUE);

	message_list_free_uids (emfv->list, uids);
}

static void
emfv_unset_label (EMFolderView *emfv, const gchar *label)
{
	GPtrArray *uids = message_list_get_selected (emfv->list);
	gint i;

	for (i = 0; i < uids->len; i++) {
		camel_folder_set_message_user_flag (emfv->folder, uids->pdata[i], label, FALSE);
		camel_folder_set_message_user_tag (emfv->folder, uids->pdata[i], "label", NULL);
	}

	message_list_free_uids (emfv->list, uids);
}

static void
emfv_popup_label_clear(EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderView *emfv = data;
	GSList *l;
	EUtilLabel *label;

	for (l = mail_config_get_labels (); l; l = l->next) {
		label = l->data;
		emfv_unset_label(emfv, label->tag);
	}
}

static void
emfv_popup_label_set(EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderView *emfv = data;

	if (pitem->type & E_POPUP_ACTIVE)
		emfv_set_label (emfv, pitem->user_data);
	else
		emfv_unset_label (emfv, pitem->user_data);
}

static void
emfv_popup_label_new (EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderView *emfv = data;
	gchar *tag = e_util_labels_add_with_dlg (NULL, NULL);

	if (tag) {
		emfv_set_label (emfv, tag);
		g_free (tag);
	}
}

static void
emfv_popup_add_sender(EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderView *emfv = data;
	GPtrArray *uids = message_list_get_selected(emfv->list);
	CamelMessageInfo *info;
	const gchar *addr;

	if (uids->len == 1
	    && (info = camel_folder_get_message_info(emfv->folder, uids->pdata[0])) != NULL
	    && (addr = camel_message_info_from(info)) != NULL
	    && addr[0] != 0)
		em_utils_add_address((GtkWidget *)emfv, addr);

	em_utils_uids_free(uids);
}

static void
emfv_popup_apply_filters(EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderView *emfv = data;
	GPtrArray *uids = message_list_get_selected(emfv->list);

	mail_filter_on_demand(emfv->folder, uids);
}

static void
emfv_popup_filter_junk(EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderView *emfv = data;
	GPtrArray *uids = message_list_get_selected(emfv->list);

	mail_filter_junk(emfv->folder, uids);
}

/* filter callbacks, this will eventually be a wizard, see
   filter_type_current/vfolder_type_current for implementation */

#define EMFV_POPUP_AUTO_TYPE(autotype, name, type)	\
static void						\
name(EPopup *ep, EPopupItem *item, gpointer data)		\
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
	{ E_POPUP_ITEM, (gchar *) "00.emfv.00", (gchar *) N_("_Copy"), emfv_popup_copy_text, NULL, (gchar *) "edit-copy", EM_FOLDER_VIEW_SELECT_DISPLAY|EM_FOLDER_VIEW_SELECT_SELECTION },

	{ E_POPUP_BAR, (gchar *) "10.emfv", NULL, NULL, NULL, NULL },

	{ E_POPUP_ITEM, (gchar *) "10.emfv.00", (gchar *) N_("_Reply to Sender"), emfv_popup_reply_sender, NULL, (gchar *) "mail-reply-sender", EM_POPUP_SELECT_ONE },
	{ E_POPUP_ITEM, (gchar *) "10.emfv.01", (gchar *) N_("Reply to _All"), emfv_popup_reply_all, NULL, (gchar *) "mail-reply-all", EM_POPUP_SELECT_ONE },
	{ E_POPUP_ITEM, (gchar *) "10.emfv.02", (gchar *) N_("_Forward"), emfv_popup_forward, NULL, (gchar *) "mail-forward", EM_POPUP_SELECT_MANY },

	{ E_POPUP_BAR, (gchar *) "20.emfv", NULL, NULL, NULL, NULL },
	/* EM_POPUP_EDIT was used here. This is changed to EM_POPUP_SELECT_ONE as Edit-as-new-messaeg need not be restricted to Sent-Items folder alone */
	{ E_POPUP_ITEM, (gchar *) "20.emfv.00", (gchar *) N_("_Edit as New Message..."), emfv_popup_edit, NULL, NULL, EM_POPUP_SELECT_ONE },
	{ E_POPUP_ITEM, (gchar *) "20.emfv.01", (gchar *) N_("_Save As..."), emfv_popup_saveas, NULL, (gchar *) "document-save-as", EM_POPUP_SELECT_MANY },
	{ E_POPUP_ITEM, (gchar *) "20.emfv.02", (gchar *) N_("_Print..."), emfv_popup_print, NULL, (gchar *) "document-print", EM_POPUP_SELECT_ONE },

	{ E_POPUP_BAR, (gchar *) "40.emfv", NULL, NULL, NULL, NULL },
	{ E_POPUP_ITEM, (gchar *) "40.emfv.00", (gchar *) N_("_Delete"), emfv_popup_delete, NULL, (gchar *) "edit-delete", EM_POPUP_SELECT_DELETE|EM_FOLDER_VIEW_SELECT_LISTONLY },
	{ E_POPUP_ITEM, (gchar *) "40.emfv.01", (gchar *) N_("U_ndelete"), emfv_popup_undelete, NULL, NULL, EM_POPUP_SELECT_UNDELETE|EM_FOLDER_VIEW_SELECT_LISTONLY },
	{ E_POPUP_ITEM, (gchar *) "40.emfv.02", (gchar *) N_("_Move to Folder..."), emfv_popup_move, NULL, (gchar *) "mail-move", EM_POPUP_SELECT_MANY|EM_FOLDER_VIEW_SELECT_LISTONLY },
	{ E_POPUP_ITEM, (gchar *) "40.emfv.03", (gchar *) N_("_Copy to Folder..."), emfv_popup_copy, NULL, (gchar *) "mail-copy", EM_POPUP_SELECT_MANY|EM_FOLDER_VIEW_SELECT_LISTONLY },

	{ E_POPUP_BAR, (gchar *) "50.emfv", NULL, NULL, NULL, NULL },
	{ E_POPUP_ITEM, (gchar *) "50.emfv.00", (gchar *) N_("Mar_k as Read"), emfv_popup_mark_read, NULL, (gchar *) "mail-mark-read", EM_POPUP_SELECT_MARK_READ|EM_FOLDER_VIEW_SELECT_LISTONLY },
	{ E_POPUP_ITEM, (gchar *) "50.emfv.01", (gchar *) N_("Mark as _Unread"), emfv_popup_mark_unread, NULL, (gchar *) "mail-mark-unread", EM_POPUP_SELECT_MARK_UNREAD|EM_FOLDER_VIEW_SELECT_LISTONLY },
	{ E_POPUP_ITEM, (gchar *) "50.emfv.02", (gchar *) N_("Mark as _Important"), emfv_popup_mark_important, NULL, (gchar *) "mail-mark-important", EM_POPUP_SELECT_MARK_IMPORTANT|EM_FOLDER_VIEW_SELECT_LISTONLY },
	{ E_POPUP_ITEM, (gchar *) "50.emfv.03", (gchar *) N_("Mark as Un_important"), emfv_popup_mark_unimportant, NULL, NULL, EM_POPUP_SELECT_MARK_UNIMPORTANT|EM_FOLDER_VIEW_SELECT_LISTONLY },
	{ E_POPUP_ITEM, (gchar *) "50.emfv.04", (gchar *) N_("Mark as _Junk"), emfv_popup_mark_junk, NULL, (gchar *) "mail-mark-junk", EM_POPUP_SELECT_MANY|EM_FOLDER_VIEW_SELECT_LISTONLY|EM_POPUP_SELECT_JUNK },
	{ E_POPUP_ITEM, (gchar *) "50.emfv.05", (gchar *) N_("Mark as _Not Junk"), emfv_popup_mark_nojunk, NULL, (gchar *) "mail-mark-notjunk", EM_POPUP_SELECT_MANY|EM_FOLDER_VIEW_SELECT_LISTONLY|EM_POPUP_SELECT_NOT_JUNK },
	{ E_POPUP_ITEM, (gchar *) "50.emfv.06", (gchar *) N_("Mark for Follo_w Up..."), emfv_popup_flag_followup, NULL, (gchar *) "stock_mail-flag-for-followup",  EM_POPUP_SELECT_FLAG_FOLLOWUP|EM_FOLDER_VIEW_SELECT_LISTONLY },

	{ E_POPUP_SUBMENU, (gchar *) "60.label.00", (gchar *) N_("_Label"), NULL, NULL, NULL, EM_POPUP_SELECT_MANY|EM_FOLDER_VIEW_SELECT_LISTONLY },
	{ E_POPUP_ITEM, (gchar *) "60.label.00/00.label", (gchar *) N_("_None"), emfv_popup_label_clear, NULL, NULL, EM_POPUP_SELECT_MANY|EM_FOLDER_VIEW_SELECT_LISTONLY },
	{ E_POPUP_BAR, (gchar *) "60.label.00/00.label.00", NULL, NULL, NULL, NULL },
	{ E_POPUP_BAR, (gchar *) "60.label.00/01.label", NULL, NULL, NULL, NULL },
	{ E_POPUP_ITEM, (gchar *) "60.label.00/01.label.00", (gchar *) N_("_New Label"), emfv_popup_label_new, NULL, NULL, EM_POPUP_SELECT_MANY|EM_FOLDER_VIEW_SELECT_LISTONLY },

	{ E_POPUP_BAR, (gchar *) "70.emfv.06", NULL, NULL, NULL, NULL },

	{ E_POPUP_ITEM, (gchar *) "70.emfv.07", (gchar *) N_("Fla_g Completed"), emfv_popup_flag_completed, NULL, (gchar *) "stock_mail-flag-for-followup-done", EM_POPUP_SELECT_FLAG_COMPLETED|EM_FOLDER_VIEW_SELECT_LISTONLY },
	{ E_POPUP_ITEM, (gchar *) "70.emfv.08", (gchar *) N_("Cl_ear Flag"), emfv_popup_flag_clear, NULL, NULL, EM_POPUP_SELECT_FLAG_CLEAR|EM_FOLDER_VIEW_SELECT_LISTONLY },

	{ E_POPUP_BAR, (gchar *) "90.filter", NULL, NULL, NULL, NULL },
	{ E_POPUP_SUBMENU, (gchar *) "90.filter.00", (gchar *) N_("Crea_te Rule From Message"), NULL, NULL, NULL, EM_POPUP_SELECT_ONE|EM_FOLDER_VIEW_SELECT_LISTONLY },
	/* Translators: The following strings are used while creating a new search folder, to specify what parameter the search folder would be based on. */
	{ E_POPUP_ITEM, (gchar *) "90.filter.00/00.00", (gchar *) N_("Search Folder based on _Subject"), emfv_popup_vfolder_subject, NULL, NULL, EM_POPUP_SELECT_ONE|EM_FOLDER_VIEW_SELECT_LISTONLY },
	{ E_POPUP_ITEM, (gchar *) "90.filter.00/00.01", (gchar *) N_("Search Folder based on Se_nder"), emfv_popup_vfolder_sender, NULL, NULL, EM_POPUP_SELECT_ONE|EM_FOLDER_VIEW_SELECT_LISTONLY },
	{ E_POPUP_ITEM, (gchar *) "90.filter.00/00.02", (gchar *) N_("Search Folder based on _Recipients"), emfv_popup_vfolder_recipients, NULL, NULL, EM_POPUP_SELECT_ONE|EM_FOLDER_VIEW_SELECT_LISTONLY },
	{ E_POPUP_ITEM, (gchar *) "90.filter.00/00.03", (gchar *) N_("Search Folder based on Mailing _List"),
	  emfv_popup_vfolder_mlist, NULL, NULL, EM_POPUP_SELECT_ONE|EM_POPUP_SELECT_MAILING_LIST|EM_FOLDER_VIEW_SELECT_LISTONLY },

	{ E_POPUP_BAR, (gchar *) "90.filter.00/10", NULL, NULL, NULL, NULL },
	/* Translators: The following strings are used while creating a new message filter, to specify what parameter the filter would be based on. */
	{ E_POPUP_ITEM, (gchar *) "90.filter.00/10.00", (gchar *) N_("Filter based on Sub_ject"), emfv_popup_filter_subject, NULL, NULL, EM_POPUP_SELECT_ONE|EM_FOLDER_VIEW_SELECT_LISTONLY },
	{ E_POPUP_ITEM, (gchar *) "90.filter.00/10.01", (gchar *) N_("Filter based on Sen_der"), emfv_popup_filter_sender, NULL, NULL, EM_POPUP_SELECT_ONE|EM_FOLDER_VIEW_SELECT_LISTONLY },
	{ E_POPUP_ITEM, (gchar *) "90.filter.00/10.02", (gchar *) N_("Filter based on Re_cipients"), emfv_popup_filter_recipients,  NULL, NULL, EM_POPUP_SELECT_ONE|EM_FOLDER_VIEW_SELECT_LISTONLY },
	{ E_POPUP_ITEM, (gchar *) "90.filter.00/10.03", (gchar *) N_("Filter based on _Mailing List"),
	  emfv_popup_filter_mlist, NULL, NULL, EM_POPUP_SELECT_ONE|EM_POPUP_SELECT_MAILING_LIST|EM_FOLDER_VIEW_SELECT_LISTONLY },
};

static enum _e_popup_t
emfv_popup_labels_get_state_for_tag (EMFolderView *emfv, GPtrArray *uids, const gchar *label_tag)
{
	enum _e_popup_t state = 0;
	gint i;
	gboolean exists = FALSE, not_exists = FALSE;

	g_return_val_if_fail (emfv != 0, state);
	g_return_val_if_fail (label_tag != NULL, state);

	for (i = 0; i < uids->len && (!exists || !not_exists); i++) {
		if (camel_folder_get_message_user_flag (emfv->folder, uids->pdata[i], label_tag))
			exists = TRUE;
		else {
			const gchar *label = e_util_labels_get_new_tag (camel_folder_get_message_user_tag (emfv->folder, uids->pdata[i], "label"));

			/* backward compatibility... */
			if (label && !strcmp (label, label_tag))
				exists = TRUE;
			else
				not_exists = TRUE;
		}
	}

	if (exists && not_exists)
		state = E_POPUP_INCONSISTENT;
	else if (exists)
		state = E_POPUP_ACTIVE;

	return state;
}

static void
emfv_popup_labels_free(EPopup *ep, GSList *l, gpointer data)
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
emfv_popup_items_free(EPopup *ep, GSList *items, gpointer data)
{
	g_slist_free(items);
}

static void
emfv_popup(EMFolderView *emfv, GdkEvent *event, gint on_display)
{
	GSList *menus = NULL, *l, *label_list = NULL;
	GtkMenu *menu;
	EMPopup *emp;
	EMPopupTargetSelect *target;
	gint i;

	/** @HookPoint-EMPopup: Message List Context Menu
	 * @Id: org.gnome.evolution.mail.folderview.popup.select
	 * @Type: EMPopup
	 * @Target: EMPopupTargetSelect
	 *
	 * This is the context menu shown on the message list or over a message.
	 */
	emp = em_popup_new("org.gnome.evolution.mail.folderview.popup");
	target = em_folder_view_get_popup_target(emfv, emp, on_display);

	for (i=0;i<sizeof(emfv_popup_items)/sizeof(emfv_popup_items[0]);i++)
		menus = g_slist_prepend(menus, &emfv_popup_items[i]);

	e_popup_add_items((EPopup *)emp, menus, NULL, emfv_popup_items_free, emfv);

	i = 1;
	if (!on_display) {
		GPtrArray *uids = message_list_get_selected (emfv->list);

		for (l = mail_config_get_labels (); l; l = l->next) {
			EPopupItem *item;
			EUtilLabel *label = l->data;
			GdkPixmap *pixmap;
			GdkColor colour;
			GdkGC *gc;

			item = g_malloc0(sizeof(*item));
			item->type = E_POPUP_TOGGLE | emfv_popup_labels_get_state_for_tag (emfv, uids, label->tag);
			item->path = g_strdup_printf("60.label.00/00.label.%02d", i++);
			item->label = label->name;
			item->activate = emfv_popup_label_set;
			item->user_data = label->tag;

			item->visible = EM_POPUP_SELECT_MANY|EM_FOLDER_VIEW_SELECT_LISTONLY;

			gdk_color_parse (label->colour, &colour);
			gdk_colormap_alloc_color(gdk_colormap_get_system(), &colour, FALSE, TRUE);

			pixmap = gdk_pixmap_new(((GtkWidget *)emfv)->window, 16, 16, -1);
			gc = gdk_gc_new(((GtkWidget *)emfv)->window);
			gdk_gc_set_foreground(gc, &colour);
			gdk_draw_rectangle(pixmap, gc, TRUE, 0, 0, 16, 16);
			g_object_unref(gc);

			item->image = gtk_image_new_from_pixmap(pixmap, NULL);
			gtk_widget_show(item->image);

			label_list = g_slist_prepend(label_list, item);
		}

		message_list_free_uids (emfv->list, uids);
	}

	e_popup_add_items((EPopup *)emp, label_list, NULL, emfv_popup_labels_free, emfv);

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
from(BonoboUIComponent *uid, gpointer data, const gchar *path)	\
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
EMFV_MAP_CALLBACK(emfv_message_followup_clear, emfv_popup_flag_clear)
EMFV_MAP_CALLBACK(emfv_message_followup_completed, emfv_popup_flag_completed)
EMFV_MAP_CALLBACK(emfv_message_open, emfv_popup_open)
EMFV_MAP_CALLBACK(emfv_message_edit, emfv_popup_edit)
EMFV_MAP_CALLBACK(emfv_message_saveas, emfv_popup_saveas)
EMFV_MAP_CALLBACK(emfv_print_message, emfv_popup_print)
EMFV_MAP_CALLBACK(emfv_message_source, emfv_popup_source)

static void
emfv_empty_trash(BonoboUIComponent *uid, gpointer data, const gchar *path)
{
	EMFolderView *emfv = data;

	em_utils_empty_trash (gtk_widget_get_toplevel ((GtkWidget *) emfv));
}

static void
prepare_offline(gpointer key, gpointer value, gpointer data)
{
	CamelService *service = key;

	if (CAMEL_IS_DISCO_STORE(service)
	    || CAMEL_IS_OFFLINE_STORE(service)) {
		mail_store_prepare_offline((CamelStore *)service);
	}
}

static void
emfv_prepare_offline(BonoboUIComponent *uid, gpointer data, const gchar *path)
{
	mail_component_stores_foreach(mail_component_peek(), prepare_offline, NULL);
}

static void
emfv_edit_cut(BonoboUIComponent *uid, gpointer data, const gchar *path)
{
	EMFolderView *emfv = data;

	if (GTK_WIDGET_HAS_FOCUS(emfv->preview->formathtml.html))
		em_format_html_display_cut(emfv->preview);
	else
		message_list_copy(emfv->list, TRUE);
}

static void
emfv_edit_copy(BonoboUIComponent *uid, gpointer data, const gchar *path)
{
	EMFolderView *emfv = data;

	if (GTK_WIDGET_HAS_FOCUS(emfv->preview->formathtml.html))
		em_format_html_display_copy(emfv->preview);
	else
		message_list_copy(emfv->list, FALSE);
}

static void
emfv_edit_paste(BonoboUIComponent *uid, gpointer data, const gchar *path)
{
	EMFolderView *emfv = data;

	message_list_paste(emfv->list);
}

static void
emfv_select_all_text(BonoboUIComponent *uid, gpointer data, const gchar *path)
{
	EMFolderView *emfv = data;
	gboolean selected;

	gtk_html_select_all (((EMFormatHTML *)emfv->preview)->html);
	selected = gtk_html_command (((EMFormatHTML *)emfv->preview)->html, "is-selection-active");
	bonobo_ui_component_set_prop(emfv->uic, "/commands/EditCopy", "sensitive", selected?"1":"0", NULL);

}

static void
emfv_mail_next(BonoboUIComponent *uid, gpointer data, const gchar *path)
{
	EMFolderView *emfv = data;

	e_profile_event_emit("goto.next", "", 0);

	message_list_select(emfv->list, MESSAGE_LIST_SELECT_NEXT, 0, 0);
}

static void
emfv_mail_next_flagged(BonoboUIComponent *uid, gpointer data, const gchar *path)
{
	EMFolderView *emfv = data;

	message_list_select(emfv->list, MESSAGE_LIST_SELECT_NEXT|MESSAGE_LIST_SELECT_WRAP, CAMEL_MESSAGE_FLAGGED, CAMEL_MESSAGE_FLAGGED);
}

static void
emfv_mail_next_unread(BonoboUIComponent *uid, gpointer data, const gchar *path)
{
	EMFolderView *emfv = data;

	gtk_widget_grab_focus((GtkWidget *) emfv->list);
	message_list_select(emfv->list, MESSAGE_LIST_SELECT_NEXT|MESSAGE_LIST_SELECT_WRAP, 0, CAMEL_MESSAGE_SEEN);
}

static void
emfv_mail_next_thread(BonoboUIComponent *uid, gpointer data, const gchar *path)
{
	EMFolderView *emfv = data;

	message_list_select_next_thread(emfv->list);
}

static void
emfv_mail_previous(BonoboUIComponent *uid, gpointer data, const gchar *path)
{
	EMFolderView *emfv = data;

	message_list_select(emfv->list, MESSAGE_LIST_SELECT_PREVIOUS, 0, 0);
}

static void
emfv_mail_previous_flagged(BonoboUIComponent *uid, gpointer data, const gchar *path)
{
	EMFolderView *emfv = data;

	message_list_select(emfv->list, MESSAGE_LIST_SELECT_PREVIOUS|MESSAGE_LIST_SELECT_WRAP, CAMEL_MESSAGE_FLAGGED, CAMEL_MESSAGE_FLAGGED);
}

static void
emfv_mail_previous_unread(BonoboUIComponent *uid, gpointer data, const gchar *path)
{
	EMFolderView *emfv = data;

	gtk_widget_grab_focus((GtkWidget *) emfv->list);
	message_list_select(emfv->list, MESSAGE_LIST_SELECT_PREVIOUS|MESSAGE_LIST_SELECT_WRAP, 0, CAMEL_MESSAGE_SEEN);
}

static void
emfv_message_forward_attached (BonoboUIComponent *uic, gpointer data, const gchar *path)
{
	EMFolderView *emfv = data;
	GPtrArray *uids;

	if (!em_utils_check_user_can_send_mail ((GtkWidget *) emfv))
		return;

	uids = message_list_get_selected (emfv->list);
	em_utils_forward_attached (emfv->folder, uids, emfv->folder_uri);
}

static void
emfv_message_forward_inline (BonoboUIComponent *uic, gpointer data, const gchar *path)
{
	EMFolderView *emfv = data;
	GPtrArray *uids;

	if (!em_utils_check_user_can_send_mail ((GtkWidget *) emfv))
		return;

	uids = message_list_get_selected (emfv->list);
	em_utils_forward_inline (emfv->folder, uids, emfv->folder_uri);
}

static void
emfv_message_forward_quoted (BonoboUIComponent *uic, gpointer data, const gchar *path)
{
	EMFolderView *emfv = data;
	GPtrArray *uids;

	if (!em_utils_check_user_can_send_mail ((GtkWidget *) emfv))
		return;

	uids = message_list_get_selected (emfv->list);
	em_utils_forward_quoted (emfv->folder, uids, emfv->folder_uri);
}

static void
emfv_message_redirect (BonoboUIComponent *uic, gpointer data, const gchar *path)
{
	EMFolderView *emfv = data;

	if (emfv->list->cursor_uid == NULL)
		return;

	if (!em_utils_check_user_can_send_mail ((GtkWidget *) emfv))
		return;

	em_utils_redirect_message_by_uid (emfv->folder, emfv->list->cursor_uid);
}

static gboolean
html_contains_nonwhitespace (const gchar *html, gint len)
{
	const gchar *p;
	gunichar c = 0;

	if (!html || len<=0)
		return FALSE;

	p = html;

	while (p && p - html < len) {
		c = g_utf8_get_char (p);
		if (!c)
			break;

		if (c == '<') {
			/* skip until next '>' */
			while (c = g_utf8_get_char (p), c && c != '>' && p - html < len)
				p = g_utf8_next_char (p);
			if (!c)
				break;
		}else if (c == '&') {
			/* sequence '&nbsp;' is a space */
			if (g_ascii_strncasecmp (p, "&nbsp;", 6) == 0)
				p = p + 5;
			else
				break;
		}else if (!g_unichar_isspace (c)) {
			break;
		}

		p = g_utf8_next_char (p);
	}

	return p - html < len - 1 && c != 0;
}

static void
emfv_message_reply(EMFolderView *emfv, gint mode)
{
	gchar *html = NULL;
	gint len;

	if (emfv->list->cursor_uid == NULL)
		return;

	if (!em_utils_check_user_can_send_mail ((GtkWidget *) emfv))
		return;

	if (gtk_html_command(((EMFormatHTML *)emfv->preview)->html, "is-selection-active")
	    && (html = gtk_html_get_selection_html (((EMFormatHTML *)emfv->preview)->html, &len))
	    && len && html[0] && html_contains_nonwhitespace (html, len)) {
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
					    html, len, "text/html");
		em_utils_reply_to_message (emfv->folder, emfv->list->cursor_uid, msg, mode, NULL);
		camel_object_unref(msg);
	} else {
		em_utils_reply_to_message (emfv->folder, emfv->list->cursor_uid, NULL, mode, (EMFormat *)emfv->preview);
	}

	g_free (html);
}

static void
emfv_message_search(BonoboUIComponent *uic, gpointer data, const gchar *path)
{
	EMFolderView *emfv = data;

	em_folder_view_show_search_bar (emfv);
}

static void
emfv_print_preview_message(BonoboUIComponent *uic, gpointer data, const gchar *path)
{
	EMFolderView *emfv = data;

	em_folder_view_print(emfv, GTK_PRINT_OPERATION_ACTION_PREVIEW);
}

static void
emfv_text_zoom_in(BonoboUIComponent *uic, gpointer data, const gchar *path)
{
	EMFolderView *emfv = data;

	if (emfv->preview)
		em_format_html_display_zoom_in(emfv->preview);
}

static void
emfv_text_zoom_out(BonoboUIComponent *uic, gpointer data, const gchar *path)
{
	EMFolderView *emfv = data;

	if (emfv->preview)
		em_format_html_display_zoom_out(emfv->preview);
}

static void
emfv_text_zoom_reset(BonoboUIComponent *uic, gpointer data, const gchar *path)
{
	EMFolderView *emfv = data;

	if (emfv->preview)
		em_format_html_display_zoom_reset(emfv->preview);
}

/* ********************************************************************** */

struct _filter_data {
	const gchar *source;
	gchar *uri;
	gint type;
};

static void
filter_data_free (struct _filter_data *fdata)
{
	g_free (fdata->uri);
	g_free (fdata);
}

static void
filter_type_got_message (CamelFolder *folder, const gchar *uid, CamelMimeMessage *msg, gpointer user_data)
{
	struct _filter_data *data = user_data;

	if (msg)
		filter_gui_add_from_message (msg, data->source, data->type);

	filter_data_free (data);
}

static void
filter_type_uid (CamelFolder *folder, const gchar *uid, const gchar *source, gint type)
{
	struct _filter_data *data;

	data = g_malloc0 (sizeof (*data));
	data->type = type;
	data->source = source;

	mail_get_message (folder, uid, filter_type_got_message, data, mail_msg_unordered_push);
}

static void
filter_type_current (EMFolderView *emfv, gint type)
{
	const gchar *source;
	GPtrArray *uids;

	if (em_utils_folder_is_sent (emfv->folder, emfv->folder_uri)
	    || em_utils_folder_is_outbox (emfv->folder, emfv->folder_uri))
		source = FILTER_SOURCE_OUTGOING;
	else
		source = FILTER_SOURCE_INCOMING;

	uids = message_list_get_selected (emfv->list);

	if (uids->len == 1)
		filter_type_uid (emfv->folder, (gchar *) uids->pdata[0], source, type);

	em_utils_uids_free (uids);
}

EMFV_MAP_CALLBACK(emfv_tools_filter_subject, emfv_popup_filter_subject)
EMFV_MAP_CALLBACK(emfv_tools_filter_sender, emfv_popup_filter_sender)
EMFV_MAP_CALLBACK(emfv_tools_filter_recipient, emfv_popup_filter_recipients)
EMFV_MAP_CALLBACK(emfv_tools_filter_mlist, emfv_popup_filter_mlist)

static void
vfolder_type_got_message (CamelFolder *folder, const gchar *uid, CamelMimeMessage *msg, gpointer user_data)
{
	struct _filter_data *data = user_data;

	if (msg)
		vfolder_gui_add_from_message (msg, data->type, data->uri);

	filter_data_free (data);
}

static void
emp_uri_popup_vfolder_sender(EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderView *emfv = data;
	EMPopupTargetURI *t = (EMPopupTargetURI *)ep->target;
	CamelURL *url;
	CamelInternetAddress *addr;

	url = camel_url_new(t->uri, NULL);
	if (url == NULL) {
		g_warning("cannot parse url '%s'", t->uri);
		return;
	}

	if (url->path && url->path[0]) {
		/* ensures vfolder is running */
		vfolder_load_storage ();

		addr = camel_internet_address_new ();
		camel_address_decode (CAMEL_ADDRESS (addr), url->path);
		vfolder_gui_add_from_address (addr, AUTO_FROM, emfv->folder_uri);
		camel_object_unref (addr);
	}

	camel_url_free(url);

}

static void
emp_uri_popup_vfolder_recipient(EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderView *emfv = data;
	EMPopupTargetURI *t = (EMPopupTargetURI *)ep->target;
	CamelURL *url;
	CamelInternetAddress *addr;

	url = camel_url_new(t->uri, NULL);
	if (url == NULL) {
		g_warning("cannot parse url '%s'", t->uri);
		return;
	}

	if (url->path && url->path[0]) {
		/* ensures vfolder is running */
		vfolder_load_storage ();

		addr = camel_internet_address_new ();
		camel_address_decode (CAMEL_ADDRESS (addr), url->path);
		vfolder_gui_add_from_address (addr, AUTO_TO, emfv->folder_uri);
		camel_object_unref (addr);
	}

	camel_url_free(url);
}

static void
vfolder_type_uid (CamelFolder *folder, const gchar *uid, const gchar *uri, gint type)
{
	struct _filter_data *data;

	data = g_malloc0 (sizeof (*data));
	data->type = type;
	data->uri = g_strdup (uri);

	mail_get_message (folder, uid, vfolder_type_got_message, data, mail_msg_unordered_push);
}

static void
vfolder_type_current (EMFolderView *emfv, gint type)
{
	GPtrArray *uids;

	uids = message_list_get_selected (emfv->list);

	if (uids->len == 1) {
		/* ensures vfolder is running */
		vfolder_load_storage ();

		vfolder_type_uid (emfv->folder, (gchar *) uids->pdata[0], emfv->folder_uri, type);
	}

	em_utils_uids_free (uids);
}

EMFV_MAP_CALLBACK(emfv_tools_vfolder_subject, emfv_popup_vfolder_subject)
EMFV_MAP_CALLBACK(emfv_tools_vfolder_sender, emfv_popup_vfolder_sender)
EMFV_MAP_CALLBACK(emfv_tools_vfolder_recipient, emfv_popup_vfolder_recipients)
EMFV_MAP_CALLBACK(emfv_tools_vfolder_mlist, emfv_popup_vfolder_mlist)

/* ********************************************************************** */

static BonoboUIVerb emfv_message_verbs[] = {
	BONOBO_UI_UNSAFE_VERB ("EmptyTrash", emfv_empty_trash),
	BONOBO_UI_UNSAFE_VERB ("PrepareForOffline", emfv_prepare_offline),
	BONOBO_UI_UNSAFE_VERB ("EditCut", emfv_edit_cut),
	BONOBO_UI_UNSAFE_VERB ("EditCopy", emfv_edit_copy),
	BONOBO_UI_UNSAFE_VERB ("EditPaste", emfv_edit_paste),

	BONOBO_UI_UNSAFE_VERB ("SelectAllText", emfv_select_all_text),

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
	BONOBO_UI_UNSAFE_VERB ("MessageDeleteKey", emfv_message_delete),
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
	BONOBO_UI_UNSAFE_VERB ("MessageFollowUpComplete", emfv_message_followup_completed),
	BONOBO_UI_UNSAFE_VERB ("MessageFollowUpClear", emfv_message_followup_clear),
	BONOBO_UI_UNSAFE_VERB ("MessageMove", emfv_message_move),
	BONOBO_UI_UNSAFE_VERB ("MessageOpen", emfv_message_open),
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

	BONOBO_UI_UNSAFE_VERB ("ViewSource", emfv_message_source),

	BONOBO_UI_UNSAFE_VERB ("MailCompose", emfv_mail_compose),

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

	E_PIXMAP ("/commands/EditCopy", "edit-copy", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/EditCut", "edit-cut", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/EditPaste", "edit-paste", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MailCompose", "mail-message-new", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MessageApplyFilters", "stock_mail-filters-apply", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MessageCopy", "mail-copy", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MessageDelete", "user-trash", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MessageFilterJunk", "mail-mark-junk", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MessageFollowUpFlag", "stock_mail-flag-for-followup", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MessageForward", "mail-forward", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MessageMarkAsImportant", "mail-mark-important", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MessageMarkAsJunk", "mail-mark-junk", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MessageMarkAsNotJunk", "mail-mark-notjunk", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MessageMarkAsRead", "mail-mark-read", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MessageMarkAsUnRead", "mail-mark-unread", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MessageMove", "mail-move", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MessageReplyAll", "mail-reply-all", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MessageReplySender", "mail-reply-sender", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MessageSaveAs", "document-save-as", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MessageSearch", "edit-find", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/PrintMessage", "document-print", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/PrintPreviewMessage", "document-print-preview", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/TextZoomIn", "zoom-in", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/TextZoomOut", "zoom-out", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/TextZoomReset", "zoom-original", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/ViewLoadImages", "image-x-generic", GTK_ICON_SIZE_MENU),

	E_PIXMAP ("/menu/MessagePlaceholder/Message/MessageNavigation/GoTo", "go-jump", GTK_ICON_SIZE_MENU),

	E_PIXMAP ("/Toolbar/MailMessageToolbar/MessageReplySender", "mail-reply-sender", GTK_ICON_SIZE_LARGE_TOOLBAR),
	E_PIXMAP ("/Toolbar/MailMessageToolbar/MessageReplyAll", "mail-reply-all", GTK_ICON_SIZE_LARGE_TOOLBAR),
	E_PIXMAP ("/Toolbar/MailMessageToolbar/MessageForward", "mail-forward", GTK_ICON_SIZE_LARGE_TOOLBAR),
	E_PIXMAP ("/Toolbar/MailMessageToolbar/PrintMessage", "document-print", GTK_ICON_SIZE_LARGE_TOOLBAR),
	E_PIXMAP ("/Toolbar/MailMessageToolbar/MessageMove", "mail-move", GTK_ICON_SIZE_LARGE_TOOLBAR),
	E_PIXMAP ("/Toolbar/MailMessageToolbar/MessageCopy", "mail-copy", GTK_ICON_SIZE_LARGE_TOOLBAR),
	E_PIXMAP ("/Toolbar/MailMessageToolbar/MessageDelete", "edit-delete", GTK_ICON_SIZE_LARGE_TOOLBAR),
	E_PIXMAP ("/Toolbar/MailMessageToolbar/MessageMarkAsJunk", "mail-mark-junk", GTK_ICON_SIZE_LARGE_TOOLBAR),
	E_PIXMAP ("/Toolbar/MailMessageToolbar/MessageMarkAsNotJunk", "mail-mark-notjunk", GTK_ICON_SIZE_LARGE_TOOLBAR),
	E_PIXMAP ("/Toolbar/MailNextButtons/MailNext", "go-next", GTK_ICON_SIZE_LARGE_TOOLBAR),
	E_PIXMAP ("/Toolbar/MailNextButtons/MailPrevious", "go-previous", GTK_ICON_SIZE_LARGE_TOOLBAR),

	E_PIXMAP_END
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
		if (emfv->menu && emfv->folder) {
			EMMenuTargetSelect *t;

			t = em_menu_target_new_select(emfv->menu, emfv->folder, emfv->folder_uri, message_list_get_selected(emfv->list));
			t->target.widget = GTK_WIDGET (emfv);
			e_menu_update_target((EMenu *)emfv->menu, t);
		}
	}

	if (emfv->folder) {
		EMPopup *emp = em_popup_new("dummy");
		EMPopupTargetSelect *t;

		t = em_folder_view_get_popup_target(emfv, emp, FALSE);
		disable_mask = t->target.mask;
		e_popup_target_free((EPopup *)emp, t);
		g_object_unref(emp);
	} else {
		disable_mask = ~0;
	}

	name = g_string_new("");
	for (l = emfv->enable_map; l; l = l->next) {
		EMFolderViewEnable *map = l->data;
		gint i;

		for (i=0;map[i].name;i++) {
			gint state = (map[i].mask & disable_mask) == 0;

			g_string_printf(name, "/commands/%s", map[i].name);
			bonobo_ui_component_set_prop(emfv->uic, name->str, "sensitive", state?"1":"0", NULL);
		}
	}

	g_string_free(name, TRUE);
}

static void
emfv_view_mode(BonoboUIComponent *uic, const gchar *path, Bonobo_UIComponent_EventType type, const gchar *state, gpointer data)
{
	EMFolderView *emfv = data;
	gint i;

	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	/* TODO: I don't like this stuff much, is there any way we can move listening for such events
	   elsehwere?  Probably not I guess, unless there's a EMFolderViewContainer for bonobo usage
	   of a folder view */

	i = state[0] != '0';

			em_format_set_mode((EMFormat *)emfv->preview, i);

			if (EM_FOLDER_VIEW_GET_CLASS (emfv)->update_message_style) {
				GConfClient *gconf = mail_config_get_gconf_client ();

				gconf_client_set_int (gconf, "/apps/evolution/mail/display/message_style", i, NULL);
			}
}

static void
emfv_caret_mode(BonoboUIComponent *uic, const gchar *path, Bonobo_UIComponent_EventType type, const gchar *state, gpointer data)
{
	EMFolderView *emfv = data;

	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	em_format_html_display_set_caret_mode(emfv->preview, state[0] != '0');

	gconf_client_set_bool(mail_config_get_gconf_client(), "/apps/evolution/mail/display/caret_mode", state[0] != '0', NULL);
}

static void
emfv_charset_changed(BonoboUIComponent *uic, const gchar *path, Bonobo_UIComponent_EventType type, const gchar *state, gpointer data)
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
emfv_activate(EMFolderView *emfv, BonoboUIComponent *uic, gint act)
{
	struct _EMFolderViewPrivate *p = emfv->priv;

	if (act) {
		em_format_mode_t style;
		gboolean state;
		GSList *l;

		emfv->uic = uic;

		for (l = emfv->ui_files;l;l = l->next)
			bonobo_ui_util_set_ui(uic, PREFIX, (gchar *)l->data, emfv->ui_app_name, NULL);

		bonobo_ui_component_add_verb_list_with_data(uic, emfv_message_verbs, emfv);
		e_pixmaps_update(uic, emfv_message_pixmaps);

		/* must do plugin menu's after main ones because of bonobo bustedness */
		if (emfv->menu)
			e_menu_activate((EMenu *)emfv->menu, uic, act);

		state = emfv->preview->caret_mode;
		bonobo_ui_component_set_prop(uic, "/commands/CaretMode", "state", state?"1":"0", NULL);
		bonobo_ui_component_add_listener(uic, "CaretMode", emfv_caret_mode, emfv);

		style = ((EMFormat *)emfv->preview)->mode?EM_FORMAT_ALLHEADERS:EM_FORMAT_NORMAL;
		if (style)
			bonobo_ui_component_set_prop(uic, "/commands/ViewFullHeaders", "state", "1", NULL);
		bonobo_ui_component_add_listener(uic, "ViewFullHeaders", emfv_view_mode, emfv);
		em_format_set_mode((EMFormat *)emfv->preview, style);

		if (emfv->folder)
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

		if (emfv->menu)
			e_menu_activate((EMenu *)emfv->menu, uic, act);

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

gint
em_folder_view_print (EMFolderView *emfv, GtkPrintOperationAction action)
{
	EMFormatHTMLPrint *efhp;
	GPtrArray *uids;

	if (emfv->folder == NULL)
		return 0;

	uids = message_list_get_selected (emfv->list);
	if (uids->len != 1)
		goto exit;

	efhp = em_format_html_print_new (
		(EMFormatHTML *) emfv->preview, action);
	em_format_set_session (
		(EMFormat *) efhp,
		((EMFormat *) emfv->preview)->session);
	em_format_merge_handler ((EMFormat *) efhp,
		(EMFormat *) emfv->preview);

	em_format_html_print_message (
		efhp, emfv->folder, uids->pdata[0]);
	g_object_unref (efhp);

exit:
	message_list_free_uids (emfv->list, uids);

	return 0;
}

EMPopupTargetSelect *
em_folder_view_get_popup_target(EMFolderView *emfv, EMPopup *emp, gint on_display)
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

	if (on_display)
		t->target.mask &= ~EM_FOLDER_VIEW_SELECT_DISPLAY;
	else
		t->target.mask &= ~EM_FOLDER_VIEW_SELECT_LISTONLY;

	if (gtk_html_command (((EMFormatHTML *)emfv->preview)->html, "is-selection-active"))
		t->target.mask &= ~EM_FOLDER_VIEW_SELECT_SELECTION;
	else
		t->target.mask &= ~EM_FOLDER_VIEW_SELECT_NOSELECTION;

	if (emfv->preview_active)
		t->target.mask &= ~EM_FOLDER_VIEW_PREVIEW_PRESENT;

	/* See bug 352980 */
	/* See bug #54770 */
	/* if (!emfv->hide_deleted)
		t->target.mask &= ~EM_POPUP_SELECT_DELETE;*/

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
	gchar *uid;
};

static void
mst_free (struct mst_t *mst)
{
	mst->emfv->list->seen_id = 0;

	g_free (mst->uid);
	g_free (mst);
}

static gint
do_mark_seen (gpointer user_data)
{
	struct mst_t *mst = user_data;
	EMFolderView *emfv = mst->emfv;
	MessageList *list = emfv->list;

	if (mst->uid && list->cursor_uid && !strcmp (mst->uid, list->cursor_uid))
		emfv_set_seen (emfv, mst->uid);

	return FALSE;
}

static void
emfv_list_done_message_selected(CamelFolder *folder, const gchar *uid, CamelMimeMessage *msg, gpointer data, CamelException *ex)
{
	EMFolderView *emfv = data;
	EMEvent *eme;
	EMEventTargetMessage *target;

	if (emfv->preview == NULL) {
		emfv->priv->nomarkseen = FALSE;
		emfv_enable_menus(emfv);
		g_object_unref (emfv);
		return;

	}

	e_profile_event_emit("goto.loaded", emfv->displayed_uid, 0);

	mail_indicate_new_mail (FALSE);

	/** @Event: message.reading
	 * @Title: Viewing a message
	 * @Target: EMEventTargetMessage
	 *
	 * message.reading is emitted whenever a user views a message.
	 */
	/* TODO: do we emit a message.reading with no message when we're looking at nothing or don't care? */
	eme = em_event_peek();
	target = em_event_target_new_message(eme, folder, msg, uid, 0, NULL);
	e_event_emit((EEvent *)eme, "message.reading", (EEventTarget *)target);

	em_format_format((EMFormat *)emfv->preview, folder, uid, msg);

	if (emfv->list->seen_id)
		g_source_remove(emfv->list->seen_id);

	if (msg && emfv->mark_seen && !emfv->priv->nomarkseen) {
		if (emfv->mark_seen_timeout > 0) {
			struct mst_t *mst;

			mst = g_new (struct mst_t, 1);
			mst->emfv = emfv;
			mst->uid = g_strdup (uid);

			emfv->list->seen_id = g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE, emfv->mark_seen_timeout,
								 (GSourceFunc)do_mark_seen, mst, (GDestroyNotify)mst_free);
		} else {
			emfv_set_seen (emfv, uid);
		}
	} else if (camel_exception_is_set(ex)) {
		GtkHTMLStream *hstream = gtk_html_begin(((EMFormatHTML *)emfv->preview)->html);

		/* Display the error inline rather than popping up an annoying box.
		   We also clear the exception, this stops the box popping up */

		gtk_html_stream_printf(hstream, "<h2>%s</h2><p>%s</p>",
				       _("Unable to retrieve message"),
				       ex->desc);
		gtk_html_stream_close(hstream, GTK_HTML_STREAM_OK);
		camel_exception_clear(ex);
	}

	emfv->priv->nomarkseen = FALSE;
	emfv_enable_menus(emfv);
	g_object_unref (emfv);
}

static gboolean
emfv_message_selected_timeout(gpointer data)
{
	EMFolderView *emfv = data;

	if (emfv->priv->selected_uid) {
		if (emfv->displayed_uid == NULL || strcmp(emfv->displayed_uid, emfv->priv->selected_uid) != 0) {
			/*GtkHTMLStream *hstream;*/

			g_free(emfv->displayed_uid);
			emfv->displayed_uid = emfv->priv->selected_uid;
			emfv->priv->selected_uid = NULL;
			g_object_ref (emfv);
			/* TODO: we should manage our own thread stuff, would make cancelling outstanding stuff easier */
			e_profile_event_emit("goto.load", emfv->displayed_uid, 0);
			mail_get_messagex(emfv->folder, emfv->displayed_uid, emfv_list_done_message_selected, emfv, mail_msg_fast_ordered_push);
		} else {
			e_profile_event_emit("goto.empty", "", 0);
			g_free(emfv->priv->selected_uid);
			emfv->priv->selected_uid = NULL;
		}
	} else {
		e_profile_event_emit("goto.empty", "", 0);
		g_free(emfv->displayed_uid);
		emfv->displayed_uid = NULL;
		em_format_format((EMFormat *)emfv->preview, NULL, NULL, NULL);
		emfv->priv->nomarkseen = FALSE;
	}

	emfv->priv->selected_id = 0;

	return FALSE;
}

static void
emfv_list_message_selected(MessageList *ml, const gchar *uid, EMFolderView *emfv)
{
	e_profile_event_emit("goto.listuid", uid, 0);

	if (emfv->preview_active) {
		if (emfv->priv->selected_id != 0)
			g_source_remove(emfv->priv->selected_id);

		emfv->priv->selected_id = g_timeout_add(100, emfv_message_selected_timeout, emfv);

		g_free(emfv->priv->selected_uid);
		emfv->priv->selected_uid = g_strdup(uid);
	}

	emfv_enable_menus(emfv);

	g_signal_emit(emfv, signals[EMFV_CHANGED], 0);
}

static void
emfv_list_built(MessageList *ml, EMFolderView *emfv)
{
	if (!emfv->priv->destroyed) {
		emfv_enable_menus(emfv);
		g_signal_emit(emfv, signals[EMFV_LOADED], 0);
	}
}

static void
emfv_list_double_click(ETree *tree, gint row, ETreePath path, gint col, GdkEvent *event, EMFolderView *emfv)
{
	/* Ignore double-clicks on columns that handle thier own state */
	if (MESSAGE_LIST_COLUMN_IS_ACTIVE (col))
		return;

	em_folder_view_open_selected(emfv);
}

static gint
emfv_list_right_click(ETree *tree, gint row, ETreePath path, gint col, GdkEvent *event, EMFolderView *emfv)
{
	emfv_popup(emfv, event, FALSE);

	return TRUE;
}

static gint
emfv_list_key_press(ETree *tree, gint row, ETreePath path, gint col, GdkEvent *ev, EMFolderView *emfv)
{
	GPtrArray *uids;
	gint i;
	guint32 flags;

	if ((ev->key.state & GDK_CONTROL_MASK) != 0)
		return FALSE;

	switch (ev->key.keyval) {
	case GDK_Return:
	case GDK_KP_Enter:
	case GDK_ISO_Enter:
		em_folder_view_open_selected(emfv);
		break;
#ifdef HAVE_XFREE
	case XF86XK_Reply:
		emfv_message_reply(emfv, REPLY_MODE_ALL);
		break;
	case XF86XK_MailForward:
		uids = message_list_get_selected(emfv->list);
		em_utils_forward_messages (emfv->folder, uids, emfv->folder_uri);
		break;
#endif /* HAVE_XFREE */
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
	gboolean ret = FALSE;
	EMFolderView *emfv = (EMFolderView *)widget;

	/* Try to bring up menu for preview html object.
	   Currently we cannot directly connect to html's "popup_menu" signal
	   since it doesn't work.
	*/

	if (GTK_WIDGET_HAS_FOCUS (emfv->preview->formathtml.html))
		ret = em_format_html_display_popup_menu (emfv->preview);

	if (!ret)
		emfv_popup (emfv, NULL, FALSE);

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
emfv_format_link_clicked(EMFormatHTMLDisplay *efhd, const gchar *uri, EMFolderView *emfv)
{
	if (!strncmp (uri, "##", 2))
		return;

	if (!g_ascii_strncasecmp (uri, "mailto:", 7)) {
		em_utils_compose_new_message_with_mailto (uri, emfv->folder_uri);
	} else if (*uri == '#') {
		gtk_html_jump_to_anchor (((EMFormatHTML *) efhd)->html, uri + 1);
	} else if (!g_ascii_strncasecmp (uri, "thismessage:", 12)) {
		/* ignore */
	} else if (!g_ascii_strncasecmp (uri, "cid:", 4)) {
		/* ignore */
	} else {
		/* FIXME Pass a parent window. */
		e_show_uri (NULL, uri);
	}
}

static void
emp_uri_popup_link_copy(EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EMFolderView *emfv = data;
	struct _EMFolderViewPrivate *p = emfv->priv;

	g_free(p->selection_uri);
	p->selection_uri = g_strdup(pitem->user_data);

	gtk_selection_owner_set(p->invisible, GDK_SELECTION_PRIMARY, gtk_get_current_event_time());
	gtk_selection_owner_set(p->invisible, GDK_SELECTION_CLIPBOARD, gtk_get_current_event_time());
}

static EPopupItem emfv_uri_popups[] = {
	{ E_POPUP_ITEM, (gchar *) "00.uri.15", (gchar *) N_("_Copy Link Location"), emp_uri_popup_link_copy, NULL, (gchar *) "edit-copy", EM_POPUP_URI_NOT_MAILTO },

	{ E_POPUP_SUBMENU, (gchar *) "99.uri.00", (gchar *) N_("Create _Search Folder"), NULL, NULL, NULL, EM_POPUP_URI_MAILTO },
	{ E_POPUP_ITEM, (gchar *) "99.uri.00/00.10", (gchar *) N_("_From this Address"), emp_uri_popup_vfolder_sender, NULL, NULL, EM_POPUP_URI_MAILTO },
	{ E_POPUP_ITEM, (gchar *) "99.uri.00/00.00", (gchar *) N_("_To this Address"), emp_uri_popup_vfolder_recipient, NULL, NULL, EM_POPUP_URI_MAILTO },
};

static void
emfv_uri_popup_free(EPopup *ep, GSList *list, gpointer data)
{
	while (list) {
		GSList *n = list->next;
		struct _EPopupItem *item = list->data;

		g_free(item->user_data);
		item->user_data = NULL;
		g_free (item);
		g_slist_free_1(list);

		list = n;
	}
}

static void
emfv_free_em_popup (gpointer emp)
{
	EPopup *ep = (EPopup *)emp;

	if (!ep)
		return;

	if (ep->target) {
		/* without this the next unref on ep does nothing */
		e_popup_target_free (ep, ep->target);
		ep->target = NULL;
	}

	g_object_unref (ep);
}

static GtkMenu *
emfv_append_menu (EMPopup *des_emp, GtkMenu *des_menu, EMPopup *src_emp, GtkMenu *src_menu)
{
	GtkWidget *separator;
	GList *children, *p;
	gchar *name;

	if (!src_menu)
		return des_menu;

	if (!des_menu)
		return src_menu;

	separator = gtk_separator_menu_item_new ();
	gtk_widget_show (separator);
	gtk_menu_shell_append (GTK_MENU_SHELL (des_menu), separator);

	children = gtk_container_get_children (GTK_CONTAINER (src_menu));
	for (p = children; p; p = p->next) {
		g_object_ref (p->data);
		gtk_container_remove (GTK_CONTAINER (src_menu), p->data);
		gtk_menu_shell_append (GTK_MENU_SHELL (des_menu), p->data);
		g_object_unref (p->data);
	}

	g_list_free (children);
	gtk_widget_destroy (GTK_WIDGET (src_menu));

	/* free src_emp together with des_emp; name contains unique identifier */
	name = g_strdup_printf ("emp_%p", (gpointer) src_emp);
	g_object_set_data_full (G_OBJECT (des_emp), name, src_emp, emfv_free_em_popup);
	g_free (name);

	return des_menu;
}

static gint
emfv_format_popup_event(EMFormatHTMLDisplay *efhd, GdkEventButton *event, const gchar *uri, CamelMimePart *part, EMFolderView *emfv)
{
	GtkMenu *menu = NULL;
	EMPopup *main_emp = NULL;

	if (uri == NULL && part == NULL) {
		/* So we don't try and popup with nothing selected - rather odd result! */
		GPtrArray *uids = message_list_get_selected(emfv->list);
		gint doit = uids->len > 0;

		message_list_free_uids(emfv->list, uids);
		if (doit)
			emfv_popup(emfv, (GdkEvent *)event, TRUE);
		return doit;
	}

	/* FIXME: this maybe should just fit on em-html-display, it has access to the
	   snooped part type */

	/** @HookPoint-EMPopup: Inline URI Context Menu
	 * @Id: org.gnome.evolution.mail.folderview.popup
	 * @Class: org.gnome.evolution.mail.popup:1.0
	 * @Target: EMPopupTargetURI
	 *
	 * This is the context menu shown when clicking on inline URIs,
	 * including addresses or normal HTML links that are displayed inside
	 * the message view.
	 */

	/** @HookPoint-EMPopup: Inline Object Context Menu
	 * @Id: org.gnome.evolution.mail.folderview.popup
	 * @Class: org.gnome.evolution.mail.popup:1.0
	 * @Target: EMPopupTargetPart
	 *
	 * This is the context menu shown when clicking on inline
	 * content such as a picture.
	 */

	if (uri) {
		gboolean have_more_uris = strchr (uri, '\n') != NULL;
		const gchar *act, *next;

		for (act = uri; act; act = next) {
			gchar *u;
			next = strchr (act, '\n');
			if (next) {
				u = g_strndup (act, next - act);
				next++;
			} else
				u = g_strdup (act);

			if (u && *u) {
				GSList *menus = NULL;
				gint i;
				EMPopupTargetURI *t;
				EMPopup *emp;
				EPopupTarget *target;
				GtkMenu *mymenu;

				emp = em_popup_new ("org.gnome.evolution.mail.folderview.popup");
				t = em_popup_target_new_uri(emp, u);
				target = (EPopupTarget *)t;

				for (i = 0; i < sizeof (emfv_uri_popups)/sizeof (emfv_uri_popups[0]); i++) {
					EPopupItem *itm = g_malloc0 (sizeof (EPopupItem));

					memcpy (itm, &emfv_uri_popups[i], sizeof (EPopupItem));
					itm->user_data = g_strdup (t->uri);
					menus = g_slist_prepend (menus, itm);
				}
				e_popup_add_items ((EPopup *)emp, menus, NULL, emfv_uri_popup_free, emfv);
				mymenu = e_popup_create_menu_once ((EPopup *)emp, target, 0);

				if (have_more_uris) {
					GtkWidget *item;

					if (strlen (u) > 100) {
						GString *str;
						gchar *c;

						/* the url should be in the form of http://a.b.c/... and we want to
						   see where the image comes from, so skip first 10 characters and
						   find the first '/' there */
						c = strchr (u + 10, '/');
						if (!c)
							str = g_string_new_len (u, 40);
						else
							str = g_string_new_len (u, MAX (c - u + 1, 40));

						g_string_append (str, "...");
						g_string_append (str, u + strlen (u) - 40);

						item = gtk_menu_item_new_with_label (str->str);

						g_string_free (str, TRUE);
					} else
						item = gtk_menu_item_new_with_label (u);

					gtk_widget_set_sensitive (item, FALSE);
					gtk_widget_show (item);
					gtk_menu_shell_insert (GTK_MENU_SHELL (mymenu), item, 0);
				}

				menu = emfv_append_menu (main_emp, menu, emp, mymenu);
				if (!main_emp)
					main_emp = emp;
			}

			g_free (u);
		}
	}

	if (part) {
		EMPopup *emp;
		EPopupTarget *target;

		emp = em_popup_new ("org.gnome.evolution.mail.folderview.popup");
		target = (EPopupTarget *)em_popup_target_new_part(emp, part, NULL);

		menu = emfv_append_menu (main_emp, menu, emp, e_popup_create_menu_once ((EPopup *)emp, target, 0));
		if (!main_emp)
			main_emp = emp;
	}

	if (event == NULL)
		gtk_menu_popup (menu, NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time());
	else
		gtk_menu_popup (menu, NULL, NULL, NULL, NULL, event->button, event->time);

	return TRUE;
}

static void
emfv_set_seen(EMFolderView *emfv, const gchar *uid)
{
	guint32 old_flags = camel_folder_get_message_flags(emfv->folder, uid);

	/* If we're setting the SEEN flag on a message, handle receipt requests */
	if (!(old_flags & CAMEL_MESSAGE_SEEN))
		em_utils_handle_receipt(emfv->folder, uid, (CamelMimeMessage *)((EMFormat *)emfv->preview)->message);

	camel_folder_set_message_flags(emfv->folder, uid, CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);
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
	EMFV_SHOW_PREVIEW,
	EMFV_SHOW_DELETED,
	EMFV_THREAD_LIST,
	EMFV_PANED_SIZE,
	EMFV_SENDER_PHOTO,
	EMFV_PHOTO_LOCAL,
	EMFV_SHOW_REAL_DATE,
	EMFV_SETTINGS		/* last, for loop count */
};

/* IF these get too long, update key field */
static const gchar * const emfv_display_keys[] = {
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
	"show_preview",
	"show_deleted",
	"thread_list",
	"paned_size",
	"sender_photo",
	"photo_local",
	"show_real_date"
};

static GHashTable *emfv_setting_key;

static void
emfv_setting_notify(GConfClient *gconf, guint cnxn_id, GConfEntry *entry, EMFolderView *emfv)
{
	GConfValue *value;
	gchar *tkey;

	g_return_if_fail (gconf_entry_get_key (entry) != NULL);

	if (!(value = gconf_entry_get_value (entry)))
		return;

	tkey = strrchr(entry->key, '/');
	g_return_if_fail (tkey != NULL);

	switch (GPOINTER_TO_INT(g_hash_table_lookup(emfv_setting_key, tkey+1))) {
	case EMFV_ANIMATE_IMAGES:
		em_format_html_display_set_animate(emfv->preview, gconf_value_get_bool (value));
		break;
	case EMFV_CHARSET:
		em_format_set_default_charset((EMFormat *)emfv->preview, gconf_value_get_string (value));
		break;
	case EMFV_CITATION_COLOUR: {
		const gchar *s;
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
			gint style = gconf_value_get_int (value);

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

		header_config_list = gconf_client_get_list(gconf, "/apps/evolution/mail/display/headers", GCONF_VALUE_STRING, NULL);
		em_format_clear_headers((EMFormat *)emfv->preview);
		p = header_config_list;
		while (p) {
			EMMailerPrefsHeader *h;
			gchar *xml = (gchar *)p->data;

			h = em_mailer_prefs_header_from_xml(xml);
			if (h && h->enabled) {
				em_format_add_header(emf, h->name, EM_FORMAT_HEADER_BOLD);
			}
			em_mailer_prefs_header_free(h);
			p = g_slist_next(p);
		}
		g_slist_foreach(header_config_list, (GFunc) g_free, NULL);
		g_slist_free(header_config_list);
		/* force a redraw */
		if (emf->message)
			em_format_redraw(emf);
		break; }
	case EMFV_SENDER_PHOTO: {
		EMFormat *emf = (EMFormat *)emfv->preview;

		emf->show_photo = gconf_value_get_bool (value);
		if (emf->message)
			em_format_redraw(emf);

		break; }
	case EMFV_PHOTO_LOCAL: {
		EMFormat *emf = (EMFormat *)emfv->preview;

		emf->photo_local = gconf_value_get_bool (value);

		break; }
        case EMFV_SHOW_PREVIEW: {
		gboolean state_gconf, state_camel;
		gchar *ret;

		/* If emfv->folder hasn't been initialized, do nothing */
		if (!emfv->folder)
			return;

		state_gconf = gconf_value_get_bool (value);
		if (state_gconf == FALSE)
			emfv_enable_menus (emfv);

		if ((ret = camel_object_meta_get (emfv->folder, "evolution:show_preview"))) {
			state_camel = (ret[0] != '0');
			g_free (ret);
			if (state_gconf == state_camel)
				return;
		}

		if (camel_object_meta_set (emfv->folder, "evolution:show_preview", state_gconf ? "1" : "0"))
			camel_object_state_write (emfv->folder);
		if (emfv->list_active)
			em_folder_browser_show_preview ((EMFolderBrowser *)emfv, state_gconf);
		bonobo_ui_component_set_prop (emfv->uic, "/commands/ViewPreview", "state", state_gconf ? "1" : "0", NULL);
		break; }
	case EMFV_SHOW_DELETED: {
		gboolean state;

		state = gconf_value_get_bool (value);
		em_folder_view_set_hide_deleted (emfv, !state);
		/* Set the prop only if the component has already been
		 * activated. */
		if (emfv->uic)
			bonobo_ui_component_set_prop (emfv->uic, "/commands/HideDeleted", "state", state ? "0" : "1", NULL);
		break; }
	case EMFV_THREAD_LIST: {
		gboolean state_gconf, state_camel;
		gchar *ret;

		/* If emfv->folder or emfv->list hasn't been initialized, do nothing */
		if (!emfv->folder || !emfv->list)
			return;

		state_gconf = gconf_value_get_bool (value);
		if ((ret = camel_object_meta_get (emfv->folder, "evolution:thread_list"))) {
			state_camel = (ret[0] != '0');
			g_free (ret);
			if (state_gconf == state_camel)
				return;
		}

		if (camel_object_meta_set (emfv->folder, "evolution:thread_list", state_gconf ? "1" : "0"))
			camel_object_state_write (emfv->folder);
		message_list_set_threaded (emfv->list, state_gconf);
		bonobo_ui_component_set_prop (emfv->uic, "/commands/ViewThreaded", "state", state_gconf ? "1" : "0", NULL);
		break; }
	case EMFV_PANED_SIZE: {
		EMFolderBrowser *emfb = (EMFolderBrowser *)emfv;
		gint paned_size;

		if (!emfv->list_active || !emfb->vpane || !emfv->preview_active)
			return;

		paned_size = gconf_value_get_int (value);
		if (paned_size == gtk_paned_get_position (GTK_PANED (emfb->vpane)))
			return;

		gtk_paned_set_position (GTK_PANED (emfb->vpane), paned_size);
		break; }
	case EMFV_SHOW_REAL_DATE: {
		EMFormat *emf = (EMFormat *)emfv->preview;

		emf->show_real_date = gconf_value_get_bool (value);
		if (emf->message)
			em_format_redraw (emf);

		break; }
	}
}

static void
emfv_setting_setup(EMFolderView *emfv)
{
	GConfClient *gconf = gconf_client_get_default();
	GConfEntry *entry;
	GError *err = NULL;
	gint i;
	gchar key[64];

	if (emfv_setting_key == NULL) {
		emfv_setting_key = g_hash_table_new(g_str_hash, g_str_equal);
		for (i=1;i<EMFV_SETTINGS;i++)
			g_hash_table_insert(emfv_setting_key, (gpointer)emfv_display_keys[i-1], GINT_TO_POINTER(i));
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
emfv_on_url (EMFolderView *emfv, const gchar *uri, const gchar *nice_uri)
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
emfv_on_url_cb (GObject *emitter, const gchar *url, EMFolderView *emfv)
{
	gchar *nice_url = NULL;

	if (url) {
		if (strncmp (url, "mailto:", 7) == 0) {
			CamelInternetAddress *cia = camel_internet_address_new();
			CamelURL *curl;
			gchar *addr;

			curl = camel_url_new(url, NULL);
			camel_address_decode((CamelAddress *)cia, curl->path);
			addr = camel_address_format((CamelAddress *)cia);
			nice_url = g_strdup_printf (_("Click to mail %s"), addr&&addr[0]?addr:(url + 7));
			g_free(addr);
			camel_url_free(curl);
			camel_object_unref(cia);
		} else if (strncmp (url, "callto:", 7) == 0 || strncmp (url, "h323:", 5) == 0 || strncmp (url, "sip:", 4) == 0) {
			CamelInternetAddress *cia = camel_internet_address_new();
			CamelURL *curl;
			gchar *addr;

			curl = camel_url_new(url, NULL);
			camel_address_decode((CamelAddress *)cia, curl->path);
			addr = camel_address_format((CamelAddress *)cia);
			nice_url = g_strdup_printf (_("Click to call %s"), addr&&addr[0]?addr:(url + 7));
			g_free(addr);
			camel_url_free(curl);
			camel_object_unref(cia);
		} else if (!strncmp (url, "##", 2)) {
			nice_url = g_strdup (_("Click to hide/unhide addresses"));
		} else
			nice_url = g_strdup_printf (_("Click to open %s"), url);
	}

	g_signal_emit (emfv, signals[EMFV_ON_URL], 0, url, nice_url);

	g_free (nice_url);
}

static gboolean
emfv_on_html_button_released_cb (GtkHTML *html, GdkEventButton *button, EMFolderView *emfv)
{
	gboolean selected;

	selected = gtk_html_command (html, "is-selection-active");
	bonobo_ui_component_set_prop(emfv->uic, "/commands/EditCopy", "sensitive", selected?"1":"0", NULL);

	return FALSE;
}

void
em_folder_view_show_search_bar (EMFolderView *emfv)
{
	EMFolderViewClass *class;

	g_return_if_fail (EM_IS_FOLDER_VIEW (emfv));

	class = EM_FOLDER_VIEW_GET_CLASS (emfv);
	g_return_if_fail (class->show_search_bar != NULL);

	class->show_search_bar (emfv);
}
