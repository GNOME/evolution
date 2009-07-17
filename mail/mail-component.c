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
#include "em-format/em-format.h"
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

#include "e-attachment-handler-mail.h"

#define MAILER_ERROR_LEVEL_KEY "/apps/evolution/mail/display/error_level"
#define MAILER_ERROR_TIME_OUT_KEY "/apps/evolution/mail/display/error_timeout"

#define d(x)

struct _MailComponentPrivate {
	GMutex *lock;

	/* states/data used during shutdown */
	enum { MC_QUIT_START, MC_QUIT_SYNC, MC_QUIT_THREADS } quit_state;
	gint quit_count;
	gint quit_expunge;	/* expunge on quit this time around? */

	gchar *base_directory;

	EMFolderTreeModel *model;

	EActivityHandler *activity_handler;

	MailAsyncEvent *async_event;
	GHashTable *store_hash; /* stores store_info objects by store */

	RuleContext *search_context;

	gchar *context_path;	/* current path for right-click menu */

	CamelStore *local_store;
	ELogger *logger;

	EComponentView *component_view;

	guint mail_sync_id; /* timeout id for sync call on the stores */
	guint mail_sync_in_progress; /* is greater than 0 if still waiting to finish sync on some store */
};

/* GObject methods.  */

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
view_on_url (GObject *emitter, const gchar *url, const gchar *nice_url, MailComponent *mail_component)
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
	gchar *uri;

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

	object_class->finalize = impl_finalize;

	epv->createView          = impl_createView;
//	epv->quit = impl_quit;
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
//		gchar *p = priv->base_directory;
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
