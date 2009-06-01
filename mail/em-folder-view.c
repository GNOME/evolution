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

#include <glib/gi18n.h>
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

#include <gtkhtml/gtkhtml.h>
#include <gtkhtml/gtkhtml-embedded.h>
#include <gtkhtml/gtkhtml-stream.h>

#include <libedataserver/e-data-server-util.h>
#include <libedataserver/e-msgport.h>

#include "menus/gal-view-etable.h"
#include "menus/gal-view-factory-etable.h"
#include "menus/gal-view-instance.h"

#include "misc/e-charset-picker.h"
#include <misc/e-spinner.h>

#include "e-util/e-error.h"
#include "e-util/e-dialog-utils.h"
#include "e-util/e-icon-factory.h"
#include "e-util/e-print.h"
#include "e-util/e-profile-event.h"
#include "e-util/e-util-private.h"
#include "e-util/e-util-labels.h"
#include "shell/e-shell.h"

#include "filter/filter-rule.h"

#include "em-format-html-display.h"
#include "em-format-html-print.h"
#include "em-folder-selection.h"
#include "em-folder-view.h"
#include "em-folder-browser.h"
#include "em-mailer-prefs.h"
#include "em-folder-browser.h"
#include "message-list.h"
#include "em-utils.h"
#include "em-composer-utils.h"
#include "em-menu.h"
#include "em-event.h"
#include "e-mail-shell-backend.h"

#include "mail-mt.h"
#include "mail-ops.h"
#include "mail-config.h"
#include "mail-autofilter.h"
#include "mail-vfolder.h"
#include "mail-tools.h"

#ifdef HAVE_XFREE
#include <X11/XF86keysym.h>
#endif

/* this is added to emfv->enable_map in :init() */
static const EMFolderViewEnable emfv_enable_map[] = {
	{ "EditCut",                  EM_POPUP_SELECT_MANY },
	{ "EditCopy",                 EM_FOLDER_VIEW_SELECT_SELECTION },
	{ "EditPaste",                EM_POPUP_SELECT_FOLDER },
//	{ "SelectAllText",            EM_POPUP_SELECT_ONE },

	/* FIXME: should these be single-selection? */
	{ "MailNext",                 EM_POPUP_SELECT_MANY|EM_FOLDER_VIEW_SELECT_NEXT_MSG },
//	{ "MailNextFlagged",          EM_POPUP_SELECT_MANY },
//	{ "MailNextUnread",           EM_POPUP_SELECT_MANY },
//	{ "MailNextThread",           EM_POPUP_SELECT_MANY },
	{ "MailPrevious",             EM_POPUP_SELECT_MANY|EM_FOLDER_VIEW_SELECT_PREV_MSG },
//	{ "MailPreviousFlagged",      EM_POPUP_SELECT_MANY },
//	{ "MailPreviousUnread",       EM_POPUP_SELECT_MANY },

	{ "AddSenderToAddressbook",   EM_POPUP_SELECT_ADD_SENDER },

//	{ "MessageApplyFilters",      EM_POPUP_SELECT_MANY },
//	{ "MessageFilterJunk",        EM_POPUP_SELECT_MANY },
//	{ "MessageCopy",              EM_POPUP_SELECT_MANY },
//	{ "MessageDelete",            EM_POPUP_SELECT_MANY|EM_POPUP_SELECT_DELETE },
//	{ "MessageDeleteKey",         EM_POPUP_SELECT_MANY},
//	{ "MessageForward",           EM_POPUP_SELECT_MANY },
//	{ "MessageForwardAttached",   EM_POPUP_SELECT_MANY },
//	{ "MessageForwardInline",     EM_POPUP_SELECT_ONE },
//	{ "MessageForwardQuoted",     EM_POPUP_SELECT_ONE },
//	{ "MessageRedirect",          EM_POPUP_SELECT_ONE },
//	{ "MessageMarkAsRead",        EM_POPUP_SELECT_MANY|EM_POPUP_SELECT_MARK_READ },
//	{ "MessageMarkAsUnRead",      EM_POPUP_SELECT_MANY|EM_POPUP_SELECT_MARK_UNREAD },
//	{ "MessageMarkAsImportant",   EM_POPUP_SELECT_MANY|EM_POPUP_SELECT_MARK_IMPORTANT },
//	{ "MessageMarkAsUnimportant", EM_POPUP_SELECT_MANY|EM_POPUP_SELECT_MARK_UNIMPORTANT },
//	{ "MessageMarkAsJunk",        EM_POPUP_SELECT_MANY|EM_POPUP_SELECT_JUNK },
//	{ "MessageMarkAsNotJunk",     EM_POPUP_SELECT_MANY},
	{ "MessageFollowUpFlag",      EM_POPUP_SELECT_MANY|EM_POPUP_SELECT_FLAG_FOLLOWUP },
	{ "MessageFollowUpComplete",  EM_POPUP_SELECT_MANY|EM_POPUP_SELECT_FLAG_COMPLETED },
	{ "MessageFollowUpClear",     EM_POPUP_SELECT_MANY|EM_POPUP_SELECT_FLAG_CLEAR },
//	{ "MessageMove",              EM_POPUP_SELECT_MANY },
//	{ "MessageOpen",              EM_POPUP_SELECT_MANY },
//	{ "MessageReplyAll",          EM_POPUP_SELECT_ONE },
//	{ "MessageReplyList",         EM_POPUP_SELECT_ONE|EM_POPUP_SELECT_MAILING_LIST },
//	{ "MessageReplySender",       EM_POPUP_SELECT_ONE },
//	{ "MessageEdit",              EM_POPUP_SELECT_ONE },
//	{ "MessageSaveAs",            EM_POPUP_SELECT_MANY },
	{ "MessageSearch",            EM_POPUP_SELECT_ONE| EM_FOLDER_VIEW_PREVIEW_PRESENT },
//	{ "MessageUndelete",          EM_POPUP_SELECT_MANY|EM_POPUP_SELECT_UNDELETE },
//	{ "PrintMessage",             EM_POPUP_SELECT_ONE },
//	{ "PrintPreviewMessage",      EM_POPUP_SELECT_ONE },

//	{ "TextZoomIn",		      EM_POPUP_SELECT_ONE },
//	{ "TextZoomOut",	      EM_POPUP_SELECT_ONE },
//	{ "TextZoomReset",	      EM_POPUP_SELECT_ONE },

	{ "ToolsFilterMailingList",   EM_POPUP_SELECT_ONE|EM_POPUP_SELECT_MAILING_LIST},
	{ "ToolsFilterRecipient",     EM_POPUP_SELECT_ONE },
	{ "ToolsFilterSender",        EM_POPUP_SELECT_ONE },
	{ "ToolsFilterSubject",       EM_POPUP_SELECT_ONE },
	{ "ToolsVFolderMailingList",  EM_POPUP_SELECT_ONE|EM_POPUP_SELECT_MAILING_LIST},
	{ "ToolsVFolderRecipient",    EM_POPUP_SELECT_ONE },
	{ "ToolsVFolderSender",       EM_POPUP_SELECT_ONE },
	{ "ToolsVFolderSubject",      EM_POPUP_SELECT_ONE },

//	{ "ViewLoadImages",	      EM_POPUP_SELECT_ONE },
//	{ "ViewSource",               EM_POPUP_SELECT_ONE },

//	/* always enabled */
//	{ "MailStop", 0 },

	{ NULL },
};

struct _EMFolderViewPrivate {
	guint setting_notify_id;
	guint selected_id;
	guint nomarkseen:1;
	guint destroyed:1;

	GtkWidget *invisible;
	gchar *selection_uri;

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

static void
emfv_init(GObject *o)
{
	EMFolderView *emfv = (EMFolderView *)o;
	struct _EMFolderViewPrivate *p;

//	gtk_box_set_homogeneous (GTK_BOX (emfv), FALSE);
//
//	p = emfv->priv = g_malloc0(sizeof(struct _EMFolderViewPrivate));
//
//	emfv->statusbar_active = TRUE;
//	emfv->list_active = FALSE;
//
//	emfv->ui_files = g_slist_append(NULL,
//					g_build_filename (EVOLUTION_UIDIR,
//							  "evolution-mail-message.xml",
//							  NULL));
//
//	emfv->ui_app_name = "evolution-mail";

	emfv->enable_map = g_slist_prepend(NULL, (gpointer)emfv_enable_map);

//	emfv->list = (MessageList *)message_list_new();
//	g_signal_connect(emfv->list, "message_selected", G_CALLBACK(emfv_list_message_selected), emfv);
//	g_signal_connect(emfv->list, "message_list_built", G_CALLBACK(emfv_list_built), emfv);
//
//	/* FIXME: should this hang off message-list instead? */
//	g_signal_connect(emfv->list->tree, "right_click", G_CALLBACK(emfv_list_right_click), emfv);
//	g_signal_connect(emfv->list->tree, "double_click", G_CALLBACK(emfv_list_double_click), emfv);
//	g_signal_connect(emfv->list->tree, "key_press", G_CALLBACK(emfv_list_key_press), emfv);
//	g_signal_connect(emfv->list->tree, "selection_change", G_CALLBACK(emfv_list_selection_change), emfv);
//
//	emfv->preview = (EMFormatHTMLDisplay *)em_format_html_display_new();
//	/* FIXME: set_session should NOT be called here.  Should it be a constructor attribute? */
//	em_format_set_session ((EMFormat *) emfv->preview, session);
//	g_signal_connect(emfv->preview, "link_clicked", G_CALLBACK(emfv_format_link_clicked), emfv);
	g_signal_connect(emfv->preview, "popup_event", G_CALLBACK(emfv_format_popup_event), emfv);
//	g_signal_connect (emfv->preview, "on_url", G_CALLBACK (emfv_on_url_cb), emfv);
//	g_signal_connect (((EMFormatHTML *)emfv->preview)->html, "button-release-event", G_CALLBACK (emfv_on_html_button_released_cb), emfv);
//#ifdef ENABLE_PROFILING
//	g_signal_connect(emfv->preview, "complete", G_CALLBACK (emfv_format_complete), emfv);
//#endif
//	p->invisible = gtk_invisible_new();
//	g_signal_connect(p->invisible, "selection_get", G_CALLBACK(emfv_selection_get), emfv);
//	g_signal_connect(p->invisible, "selection_clear_event", G_CALLBACK(emfv_selection_clear_event), emfv);
//	gtk_selection_add_target(p->invisible, GDK_SELECTION_PRIMARY, GDK_SELECTION_TYPE_STRING, 0);
//	gtk_selection_add_target(p->invisible, GDK_SELECTION_CLIPBOARD, GDK_SELECTION_TYPE_STRING, 1);

	emfv->async = mail_async_event_new();
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

//	((EMFolderViewClass *)klass)->on_url = emfv_on_url;

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

//static void
//emfv_selection_get(GtkWidget *widget, GtkSelectionData *data, guint info, guint time_stamp, EMFolderView *emfv)
//{
//	struct _EMFolderViewPrivate *p = emfv->priv;
//
//	if (p->selection_uri == NULL)
//		return;
//
//	gtk_selection_data_set(data, data->target, 8, (guchar *)p->selection_uri, strlen(p->selection_uri));
//}

//static void
//emfv_selection_clear_event(GtkWidget *widget, GdkEventSelection *event, EMFolderView *emfv)
//{
//#if 0 /* do i care? */
//	struct _EMFolderViewPrivate *p = emfv->priv;
//
//	g_free(p->selection_uri);
//	p->selection_uri = NULL;
//#endif
//}

/* ********************************************************************** */

/* Popup menu
   In many cases these are the functions called by the bonobo callbacks too */

/* ********************************************************************** */

/* Bonobo menu's */

/* a lot of stuff maps directly to the popup menu equivalent */
#define EMFV_MAP_CALLBACK(from, to)				\
static void							\
from(BonoboUIComponent *uid, gpointer data, const gchar *path)	\
{								\
	to(NULL, NULL, data);					\
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

//static void
//emp_uri_popup_vfolder_sender(EPopup *ep, EPopupItem *pitem, gpointer data)
//{
//	EMFolderView *emfv = data;
//	EMPopupTargetURI *t = (EMPopupTargetURI *)ep->target;
//	CamelURL *url;
//	CamelInternetAddress *addr;
//
//	url = camel_url_new(t->uri, NULL);
//	if (url == NULL) {
//		g_warning("cannot parse url '%s'", t->uri);
//		return;
//	}
//
//	if (url->path && url->path[0]) {
//		/* ensures vfolder is running */
//		vfolder_load_storage ();
//
//		addr = camel_internet_address_new ();
//		camel_address_decode (CAMEL_ADDRESS (addr), url->path);
//		vfolder_gui_add_from_address (addr, AUTO_FROM, emfv->folder_uri);
//		camel_object_unref (addr);
//	}
//
//	camel_url_free(url);
//
//}


//static void
//emp_uri_popup_vfolder_recipient(EPopup *ep, EPopupItem *pitem, gpointer data)
//{
//	EMFolderView *emfv = data;
//	EMPopupTargetURI *t = (EMPopupTargetURI *)ep->target;
//	CamelURL *url;
//	CamelInternetAddress *addr;
//
//	url = camel_url_new(t->uri, NULL);
//	if (url == NULL) {
//		g_warning("cannot parse url '%s'", t->uri);
//		return;
//	}
//
//	if (url->path && url->path[0]) {
//		/* ensures vfolder is running */
//		vfolder_load_storage ();
//
//		addr = camel_internet_address_new ();
//		camel_address_decode (CAMEL_ADDRESS (addr), url->path);
//		vfolder_gui_add_from_address (addr, AUTO_TO, emfv->folder_uri);
//		camel_object_unref (addr);
//	}
//
//	camel_url_free(url);
//}

/* ********************************************************************** */

static BonoboUIVerb emfv_message_verbs[] = {
	BONOBO_UI_UNSAFE_VERB ("EditCut", emfv_edit_cut),
	BONOBO_UI_UNSAFE_VERB ("EditCopy", emfv_edit_copy),
	BONOBO_UI_UNSAFE_VERB ("EditPaste", emfv_edit_paste),

//	BONOBO_UI_UNSAFE_VERB ("SelectAllText", emfv_select_all_text),

//	BONOBO_UI_UNSAFE_VERB ("MessageDelete", emfv_message_delete),
//	BONOBO_UI_UNSAFE_VERB ("MessageDeleteKey", emfv_message_delete),
//	BONOBO_UI_UNSAFE_VERB ("MessageOpen", emfv_message_open),
//	BONOBO_UI_UNSAFE_VERB ("MessageSearch", emfv_message_search),

//	BONOBO_UI_UNSAFE_VERB ("ViewSource", emfv_message_source),

	BONOBO_UI_VERB_END
};

static void
emfv_activate(EMFolderView *emfv, BonoboUIComponent *uic, gint act)
{

	if (act) {
		em_format_mode_t style;
		gboolean state;
		GSList *l;

		emfv->uic = uic;

		for (l = emfv->ui_files;l;l = l->next)
			bonobo_ui_util_set_ui(uic, PREFIX, (gchar *)l->data, emfv->ui_app_name, NULL);

		bonobo_ui_component_add_verb_list_with_data(uic, emfv_message_verbs, emfv);
		/* must do plugin menu's after main ones because of bonobo bustedness */
		if (emfv->menu)
			e_menu_activate((EMenu *)emfv->menu, uic, act);

//		state = emfv->preview->caret_mode;
//		bonobo_ui_component_set_prop(uic, "/commands/CaretMode", "state", state?"1":"0", NULL);
//		bonobo_ui_component_add_listener(uic, "CaretMode", emfv_caret_mode, emfv);

//		style = ((EMFormat *)emfv->preview)->mode?EM_FORMAT_ALLHEADERS:EM_FORMAT_NORMAL;
//		if (style)
//			bonobo_ui_component_set_prop(uic, "/commands/ViewFullHeaders", "state", "1", NULL);
//		bonobo_ui_component_add_listener(uic, "ViewFullHeaders", emfv_view_mode, emfv);
//		em_format_set_mode((EMFormat *)emfv->preview, style);

		if (emfv->folder)
			bonobo_ui_component_set_prop(uic, "/commands/MessageEdit", "sensitive", "0", NULL);

//		/* default charset used in mail view */
//		e_charset_picker_bonobo_ui_populate (uic, "/menu/View", _("Default"), emfv_charset_changed, emfv);

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

		if (emfv->folder)
			mail_sync_folder(emfv->folder, NULL, NULL);

		emfv->uic = NULL;
	}
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
	EShell *shell;

	if (emfv->preview == NULL) {
		emfv->priv->nomarkseen = FALSE;
		emfv_enable_menus(emfv);
		g_object_unref (emfv);
		return;

	}

	e_profile_event_emit("goto.loaded", emfv->displayed_uid, 0);

	shell = e_shell_backend_get_shell (mail_shell_backend);
	e_shell_event (shell, "mail-icon", "evolution-mail");

	/** @Event: message.reading
	 * @Title: Viewing a message
	 * @Target: EMEventTargetMessage
	 *
	 * message.reading is emitted whenever a user views a message.
	 */
	/* TODO: do we emit a message.reading with no message when we're looking at nothing or don't care? */
	eme = em_event_peek();
	target = em_event_target_new_message(eme, folder, msg, uid, 0);
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

//static void
//emp_uri_popup_link_copy(EPopup *ep, EPopupItem *pitem, gpointer data)
//{
//	EMFolderView *emfv = data;
//	struct _EMFolderViewPrivate *p = emfv->priv;
//
//	g_free(p->selection_uri);
//	p->selection_uri = em_utils_url_unescape_amp(pitem->user_data);
//
//	gtk_selection_owner_set(p->invisible, GDK_SELECTION_PRIMARY, gtk_get_current_event_time());
//	gtk_selection_owner_set(p->invisible, GDK_SELECTION_CLIPBOARD, gtk_get_current_event_time());
//}

static EPopupItem emfv_uri_popups[] = {
//	{ E_POPUP_ITEM, (gchar *) "00.uri.15", (gchar *) N_("_Copy Link Location"), emp_uri_popup_link_copy, NULL, (gchar *) "edit-copy", EM_POPUP_URI_NOT_MAILTO },

//	{ E_POPUP_SUBMENU, (gchar *) "99.uri.00", (gchar *) N_("Create _Search Folder"), NULL, NULL, NULL, EM_POPUP_URI_MAILTO },
//	{ E_POPUP_ITEM, (gchar *) "99.uri.00/00.10", (gchar *) N_("_From this Address"), emp_uri_popup_vfolder_sender, NULL, NULL, EM_POPUP_URI_MAILTO },
//	{ E_POPUP_ITEM, (gchar *) "99.uri.00/00.00", (gchar *) N_("_To this Address"), emp_uri_popup_vfolder_recipient, NULL, NULL, EM_POPUP_URI_MAILTO },
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
	EMFV_CHARSET = 1,
	EMFV_HEADERS,
	EMFV_SHOW_DELETED,
	EMFV_SETTINGS		/* last, for loop count */
};

/* IF these get too long, update key field */
static const gchar * const emfv_display_keys[] = {
	"charset",
	"headers",
	"show_deleted",
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

	switch(GPOINTER_TO_INT(g_hash_table_lookup(emfv_setting_key, tkey+1))) {
	case EMFV_CHARSET:
		em_format_set_default_charset((EMFormat *)emfv->preview, gconf_value_get_string (value));
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
	case EMFV_SHOW_DELETED: {
		gboolean state;

		state = gconf_value_get_bool (value);
		em_folder_view_set_hide_deleted (emfv, !state);
		/* Set the prop only if the component has already been
		 * activated. */
		if (emfv->uic)
			bonobo_ui_component_set_prop (emfv->uic, "/commands/HideDeleted", "state", state ? "0" : "1", NULL);
		break; }
	}
}
