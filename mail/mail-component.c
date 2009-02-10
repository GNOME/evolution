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
 *		Ettore Perazzoli <ettore@ximian.com>
 *	    Michael Zucchi <notzed@ximian.com>
 *	    Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <libedataserver/e-data-server-util.h>
#include "em-utils.h"
#include "em-composer-utils.h"
#include "em-format.h"
#include "em-folder-tree.h"
#include "em-folder-browser.h"
#include "em-message-browser.h"
#include "em-folder-selector.h"
#include "em-folder-selection.h"
#include "em-folder-utils.h"
#include "em-migrate.h"

#include "misc/e-info-label.h"
#include "e-util/e-util.h"
#include "e-util/e-error.h"
#include "e-util/e-util-private.h"
#include "e-util/e-logger.h"
#include "e-util/gconf-bridge.h"

#include "em-search-context.h"
#include "mail-config.h"
#include "mail-component.h"
#include "mail-folder-cache.h"
#include "mail-vfolder.h"
#include "mail-mt.h"
#include "mail-ops.h"
#include "mail-tools.h"
#include "mail-send-recv.h"
#include "mail-session.h"
#include "message-list.h"

#include "e-activity-handler.h"
#include "shell/e-user-creatable-items-handler.h"
#include "shell/e-component-view.h"

#include "composer/e-msg-composer.h"

#include "e-task-bar.h"

#include <gtk/gtk.h>

#include <e-util/e-mktemp.h>
#include <Evolution.h>

#include <table/e-tree.h>
#include <table/e-tree-memory.h>
#include <glib/gi18n-lib.h>

#include <camel/camel-file-utils.h>
#include <camel/camel-vtrash-folder.h>
#include <camel/camel-disco-store.h>
#include <camel/camel-offline-store.h>

#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-widget.h>

#include "e-util/e-non-intrusive-error-dialog.h"

#define MAILER_ERROR_LEVEL_KEY "/apps/evolution/mail/display/error_level"
#define MAILER_ERROR_TIME_OUT_KEY "/apps/evolution/mail/display/error_timeout"

#define d(x)

struct _MailComponentPrivate {
	GMutex *lock;

	/* states/data used during shutdown */
	enum { MC_QUIT_START, MC_QUIT_SYNC, MC_QUIT_THREADS } quit_state;
	int quit_count;
	int quit_expunge;	/* expunge on quit this time around? */

	char *base_directory;

	EMFolderTreeModel *model;

	EActivityHandler *activity_handler;

	MailAsyncEvent *async_event;
	GHashTable *store_hash; /* stores store_info objects by store */

	RuleContext *search_context;

	char *context_path;	/* current path for right-click menu */

	CamelStore *local_store;
	ELogger *logger;

	EComponentView *component_view;

	guint mail_sync_id; /* timeout id for sync call on the stores */
	guint mail_sync_in_progress; /* is greater than 0 if still waiting to finish sync on some store */
};

/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	MailComponentPrivate *priv = MAIL_COMPONENT (object)->priv;

	if (priv->mail_sync_id) {
		g_source_remove (priv->mail_sync_id);
		priv->mail_sync_id = 0;
	}

	if (priv->activity_handler != NULL) {
		g_object_unref (priv->activity_handler);
		priv->activity_handler = NULL;
	}

	if (priv->search_context != NULL) {
		g_object_unref (priv->search_context);
		priv->search_context = NULL;
	}

	if (priv->local_store != NULL) {
		camel_object_unref (priv->local_store);
		priv->local_store = NULL;
	}

	priv->component_view = NULL;

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	MailComponentPrivate *priv = MAIL_COMPONENT (object)->priv;

	g_free (priv->base_directory);

	g_hash_table_destroy (priv->store_hash);

	if (mail_async_event_destroy (priv->async_event) == -1) {
		g_warning("Cannot destroy async event: would deadlock");
		g_warning(" system may be unstable at exit");
	}

	g_free (priv->context_path);
	g_mutex_free(priv->lock);
	g_object_unref (priv->model);
	g_object_unref (priv->logger);
	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
view_on_url (GObject *emitter, const char *url, const char *nice_url, MailComponent *mail_component)
{
	MailComponentPrivate *priv = mail_component->priv;

	e_activity_handler_set_message (priv->activity_handler, nice_url);
}

static void
disable_folder_tree (gpointer *emfb, GtkWidget *widget)
{
	gtk_widget_set_sensitive (widget, FALSE);
}

static void
enable_folder_tree (GtkWidget *emfb, GtkWidget *emft)
{
	EMFolderView *emfv = (EMFolderView *) emfb;
	CamelURL *selected_curl, *current_curl;
	CamelFolder *selected_folder;
	gchar *uri;

	/* Get the currently displayed folder. */
	uri = mail_tools_folder_to_url (emfv->list->folder);
	current_curl = uri ? camel_url_new (uri, NULL) : NULL;
	g_free (uri);

	/* Get the selected folder in the folder tree. */
	selected_folder = em_folder_tree_get_selected_folder(EM_FOLDER_TREE (emft));
	uri = mail_tools_folder_to_url (selected_folder);

	selected_curl = uri ? camel_url_new (uri, NULL) : NULL;

	if (current_curl && selected_curl && !camel_url_equal (selected_curl, current_curl)) {

		g_signal_emit_by_name (
			emft, "folder-selected", emft, uri,
			selected_folder->full_name, uri, selected_folder->folder_flags);
	}

	gtk_widget_set_sensitive (emft, TRUE);

	camel_url_free (current_curl);
	camel_url_free (selected_curl);
	g_free (uri);
}

/* Evolution::Component CORBA methods.  */

static GNOME_Evolution_ComponentView
impl_createView (PortableServer_Servant servant,
		 GNOME_Evolution_ShellView parent,
		 CORBA_boolean select_item,
		 CORBA_Environment *ev)
{
	MailComponent *mail_component = MAIL_COMPONENT (bonobo_object_from_servant (servant));
	MailComponentPrivate *priv = mail_component->priv;
	EComponentView *component_view;
	GtkWidget *tree_widget, *vbox, *info;
	GtkWidget *view_widget;
	GtkWidget *statusbar_widget;
	char *uri;

	mail_session_set_interactive(TRUE);
	mc_startup(mail_component);

	view_widget = em_folder_browser_new ();

	if (!select_item)
		em_folder_browser_suppress_message_selection (
			(EMFolderBrowser *) view_widget);

//	tree_widget = (GtkWidget *) em_folder_tree_new_with_model (priv->model);
//	em_folder_tree_set_excluded ((EMFolderTree *) tree_widget, 0);
//	em_folder_tree_enable_drag_and_drop ((EMFolderTree *) tree_widget);

//	if ((uri = em_folder_tree_model_get_selected (priv->model))) {
//		gboolean expanded;
//
//		expanded = em_folder_tree_model_get_expanded_uri (priv->model, uri);
//		em_folder_tree_set_selected ((EMFolderTree *) tree_widget, uri, FALSE);
//		em_folder_view_set_folder_uri ((EMFolderView *) view_widget, uri);
//
//		if (!expanded)
//			em_folder_tree_model_set_expanded_uri (priv->model, uri, expanded);
//
//		g_free (uri);
//	}

	em_format_set_session ((EMFormat *) ((EMFolderView *) view_widget)->preview, session);

	g_signal_connect (view_widget, "on-url", G_CALLBACK (view_on_url), mail_component);
	em_folder_view_set_statusbar ((EMFolderView*)view_widget, FALSE);

//	statusbar_widget = e_task_bar_new ();
//	e_activity_handler_attach_task_bar (priv->activity_handler, E_TASK_BAR (statusbar_widget));

	gtk_widget_show (tree_widget);
	gtk_widget_show (view_widget);
	gtk_widget_show (statusbar_widget);

//	vbox = gtk_vbox_new(FALSE, 0);
//	info = e_info_label_new("evolution-mail");
//	e_info_label_set_info((EInfoLabel *)info, _("Mail"), "");
//	gtk_box_pack_start((GtkBox *)vbox, info, FALSE, TRUE, 0);
//	gtk_box_pack_start((GtkBox *)vbox, tree_widget, TRUE, TRUE, 0);

	gtk_widget_show(info);
	gtk_widget_show(vbox);

//	component_view = e_component_view_new(parent, "mail", vbox, view_widget, statusbar_widget);
//
//	g_object_set_data((GObject *)component_view, "info-label", info);
//
//	g_object_set_data_full((GObject *)view_widget, "e-creatable-items-handler",
//			       e_user_creatable_items_handler_new("mail", create_local_item_cb, tree_widget),
//			       (GDestroyNotify)g_object_unref);


	g_signal_connect (component_view->view_control, "activate", G_CALLBACK (view_control_activate_cb), view_widget);
//	g_signal_connect (tree_widget, "folder-selected", G_CALLBACK (folder_selected_cb), view_widget);

	g_signal_connect((EMFolderBrowser *)view_widget, "account_search_cleared", G_CALLBACK (enable_folder_tree), tree_widget);
	g_signal_connect(((EMFolderBrowser *)view_widget), "account_search_activated", G_CALLBACK (disable_folder_tree), tree_widget);
//	g_signal_connect(view_widget, "changed", G_CALLBACK(view_changed_cb), component_view);
//	g_signal_connect(view_widget, "loaded", G_CALLBACK(view_changed_cb), component_view);

	g_object_set_data((GObject*)info, "folderview", view_widget);
	g_object_set_data((GObject*)view_widget, "foldertree", tree_widget);

	priv->component_view = component_view;

	return BONOBO_OBJREF(component_view);
}

static CORBA_boolean
impl_requestQuit(PortableServer_Servant servant, CORBA_Environment *ev)
{
	/*MailComponent *mc = MAIL_COMPONENT(bonobo_object_from_servant(servant));*/
	CamelFolder *folder;
	guint32 unsent;

	if (!e_msg_composer_request_close_all())
		return FALSE;

	folder = mc_default_folders[MAIL_COMPONENT_FOLDER_OUTBOX].folder;
	if (folder != NULL
	    && camel_session_is_online(session)
	    && camel_object_get(folder, NULL, CAMEL_FOLDER_VISIBLE, &unsent, 0) == 0
	    && unsent > 0
	    && e_error_run(NULL, "mail:exit-unsaved", NULL) != GTK_RESPONSE_YES)
		return FALSE;

	return TRUE;
}

static void
mc_quit_sync_done(CamelStore *store, void *data)
{
	MailComponent *mc = data;

	mc->priv->quit_count--;
}

static void
mc_quit_sync(CamelStore *store, struct _store_info *si, MailComponent *mc)
{
	mc->priv->quit_count++;
	mail_sync_store(store, mc->priv->quit_expunge, mc_quit_sync_done, mc);
}

static void
mc_quit_delete (CamelStore *store, struct _store_info *si, MailComponent *mc)
{
	CamelFolder *folder = camel_store_get_junk (store, NULL);

	if (folder) {
		GPtrArray *uids;
		int i;

		uids =  camel_folder_get_uids (folder);
		camel_folder_freeze(folder);
		for (i=0;i<uids->len;i++)
			camel_folder_set_message_flags(folder, uids->pdata[i], CAMEL_MESSAGE_DELETED|CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_DELETED|CAMEL_MESSAGE_SEEN);
		camel_folder_thaw(folder);
		camel_folder_free_uids (folder, uids);
	}
}

static CORBA_boolean
impl_quit(PortableServer_Servant servant, CORBA_Environment *ev)
{
	MailComponent *mc = MAIL_COMPONENT(bonobo_object_from_servant(servant));
	EAccountList *account_list;

	if (mc->priv->quit_state == -1)
		mc->priv->quit_state = MC_QUIT_START;

	account_list = e_get_account_list ();
	e_account_list_prune_proxies (account_list);

	switch (mc->priv->quit_state) {
	case MC_QUIT_START: {
		extern int camel_application_is_exiting;
		int now = time(NULL)/60/60/24, days;
		gboolean empty_junk;

		GConfClient *gconf = mail_config_get_gconf_client();

		camel_application_is_exiting = TRUE;

		mail_vfolder_shutdown();

		mc->priv->quit_expunge = gconf_client_get_bool(gconf, "/apps/evolution/mail/trash/empty_on_exit", NULL)
			&& ((days = gconf_client_get_int(gconf, "/apps/evolution/mail/trash/empty_on_exit_days", NULL)) == 0
			    || (days + gconf_client_get_int(gconf, "/apps/evolution/mail/trash/empty_date", NULL)) <= now);

		empty_junk = gconf_client_get_bool(gconf, "/apps/evolution/mail/junk/empty_on_exit", NULL)
			&& ((days = gconf_client_get_int(gconf, "/apps/evolution/mail/junk/empty_on_exit_days", NULL)) == 0
			    || (days + gconf_client_get_int(gconf, "/apps/evolution/mail/junk/empty_date", NULL)) <= now);

		if (empty_junk) {
			g_hash_table_foreach(mc->priv->store_hash, (GHFunc)mc_quit_delete, mc);
			gconf_client_set_int(gconf, "/apps/evolution/mail/junk/empty_date", now, NULL);
		}

		g_hash_table_foreach(mc->priv->store_hash, (GHFunc)mc_quit_sync, mc);

		if (mc->priv->quit_expunge)
			gconf_client_set_int(gconf, "/apps/evolution/mail/trash/empty_date", now, NULL);

		mc->priv->quit_state = MC_QUIT_SYNC;
	}
		/* Falls through */
	case MC_QUIT_SYNC:
		if (mc->priv->quit_count > 0 || mc->priv->mail_sync_in_progress > 0)
			return FALSE;

		mail_cancel_all();
		mc->priv->quit_state = MC_QUIT_THREADS;

		/* Falls through */
	case MC_QUIT_THREADS:
		/* should we keep cancelling? */
		if (mail_msg_active((unsigned int)-1))
			return FALSE;

		mail_session_shutdown ();
		return TRUE;
	}

	return TRUE;
}

/* Initialization.  */

static void
mail_component_class_init (MailComponentClass *class)
{
	POA_GNOME_Evolution_Component__epv *epv = &((EvolutionComponentClass *)class)->epv;
	POA_GNOME_Evolution_MailComponent__epv *mepv = &class->epv;
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	bindtextdomain (GETTEXT_PACKAGE, EVOLUTION_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;

	epv->createView          = impl_createView;
	epv->requestQuit = impl_requestQuit;
	epv->quit = impl_quit;
//	epv->_get_userCreatableItems = impl__get_userCreatableItems;
//	epv->requestCreateItem       = impl_requestCreateItem;
//	epv->handleURI               = impl_handleURI;
//	epv->sendAndReceive          = impl_sendAndReceive;
//	epv->upgradeFromVersion      = impl_upgradeFromVersion;
//	epv->setLineStatus	     = impl_setLineStatus;

//	mepv->test = impl_mail_test;
}

//static void
//store_hash_free (struct _store_info *si)
//{
//	si->removed = 1;
//	store_info_unref(si);
//}

static void
mail_component_init (MailComponent *component)
{
	MailComponentPrivate *priv;

	priv = g_new0 (MailComponentPrivate, 1);
	component->priv = priv;

	priv->lock = g_mutex_new();
	priv->quit_state = -1;

//	/* FIXME This is used as both a filename and URI path throughout
//	 *       the mail code.  Need to clean this up; maybe provide a
//	 *       mail_component_get_base_uri() function. */
//	priv->base_directory = g_build_filename (e_get_user_data_dir (), "mail", NULL);
//#ifdef G_OS_WIN32
//	{
//		char *p = priv->base_directory;
//		while ((p = strchr(p, '\\')))
//			*p++ = '/';
//	}
//#endif

//	if (g_mkdir_with_parents (e_get_user_data_dir (), 0777) == -1 && errno != EEXIST)
//		abort ();

//	priv->model = em_folder_tree_model_new (e_get_user_data_dir ());
	priv->logger = e_logger_create ("mail");
	priv->activity_handler = e_activity_handler_new ();
	e_activity_handler_set_logger (priv->activity_handler, priv->logger);
	e_activity_handler_set_error_flush_time (priv->activity_handler, mail_config_get_error_timeout ()*1000);

//	mail_session_init (e_get_user_data_dir ());

//	priv->async_event = mail_async_event_new();
//	priv->store_hash = g_hash_table_new_full (
//		NULL, NULL,
//		(GDestroyNotify) NULL,
//		(GDestroyNotify) store_hash_free);

//	mail_autoreceive_init (session);

//	priv->mail_sync_in_progress = 0;
//	if (g_getenv("CAMEL_FLUSH_CHANGES"))
//		priv->mail_sync_id = g_timeout_add_seconds (mail_config_get_sync_timeout (), call_mail_sync, component);
//	else 
//		priv->mail_sync_id = 0;
}

void
mail_component_show_logger (gpointer top)
{
	MailComponent *mc = mail_component_peek ();
	ELogger *logger = mc->priv->logger;

	eni_show_logger(logger, top, MAILER_ERROR_TIME_OUT_KEY, MAILER_ERROR_LEVEL_KEY);
}
