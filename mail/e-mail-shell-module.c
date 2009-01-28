/*
 * e-mail-shell-module.c
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <glib/gi18n.h>
#include <camel/camel-disco-store.h>
#include <camel/camel-offline-store.h>
#include <camel/camel-session.h>
#include <camel/camel-url.h>

#include "e-util/e-account-utils.h"
#include "e-util/e-import.h"
#include "e-util/e-util.h"
#include "shell/e-shell.h"
#include "shell/e-shell-window.h"
#include "composer/e-msg-composer.h"
#include "widgets/misc/e-preferences-window.h"

#include "e-mail-shell-view.h"
#include "e-mail-shell-module.h"
#include "e-mail-shell-module-migrate.h"
#include "e-mail-shell-module-settings.h"

#include "e-mail-browser.h"
#include "e-mail-reader.h"
#include "em-account-prefs.h"
#include "em-composer-prefs.h"
#include "em-composer-utils.h"
#include "em-config.h"
#include "em-event.h"
#include "em-folder-tree-model.h"
#include "em-folder-utils.h"
#include "em-format-hook.h"
#include "em-format-html-display.h"
#include "em-junk-hook.h"
#include "em-mailer-prefs.h"
#include "em-network-prefs.h"
#include "em-utils.h"
#include "mail-config.h"
#include "mail-folder-cache.h"
#include "mail-mt.h"
#include "mail-ops.h"
#include "mail-send-recv.h"
#include "mail-session.h"
#include "mail-vfolder.h"
#include "importers/mail-importer.h"

#define MODULE_NAME		"mail"
#define MODULE_ALIASES		""
#define MODULE_SCHEMES		"mailto:email"
#define MODULE_SORT_ORDER	200

typedef struct _StoreInfo StoreInfo;

/* XXX Temporary */
CamelStore *vfolder_store;

struct _StoreInfo {
	CamelStore *store;
	gint ref_count;
	gchar *name;

	/* Keep a reference to these so they remain around for the session. */
	CamelFolder *vtrash;
	CamelFolder *vjunk;

	/* Initialization callback. */
	void (*done) (CamelStore *store,
	              CamelFolderInfo *info,
	              gpointer user_data);
	gpointer done_user_data;

	guint removed : 1;
};

/* Module Entry Point */
void e_shell_module_init (GTypeModule *type_module);

/* The array elements correspond to EMailFolderType. */
static struct {
	gchar *name;
	gchar *uri;
	CamelFolder *folder;
} default_local_folders[] = {
	{ N_("Inbox") },
	{ N_("Drafts") },
	{ N_("Outbox") },
	{ N_("Sent") },
	{ N_("Templates") },
	{ "Inbox" }  /* "always local" inbox */
};

/* XXX So many things need the shell module that it's
 *     just easier to make it globally available. */ 
EShellModule *mail_shell_module = NULL;

static GHashTable *store_hash;
static MailAsyncEvent *async_event;
static EMFolderTreeModel *folder_tree_model;
static CamelStore *local_store;

static gint mail_sync_in_progress;
static guint mail_sync_timeout_source_id;

extern gint camel_application_is_exiting;

static StoreInfo *
store_info_new (CamelStore *store,
                const gchar *name)
{
	CamelService *service;
	StoreInfo *si;

	g_return_val_if_fail (CAMEL_IS_STORE (store), NULL);

	service = CAMEL_SERVICE (store);

	si = g_slice_new0 (StoreInfo);
	si->ref_count = 1;

	if (name == NULL)
		si->name = camel_service_get_name (service, TRUE);
	else
		si->name = g_strdup (name);

	si->store = store;
	camel_object_ref (store);

	/* If these are vfolders then they need to be opened now,
	 * otherwise they won't keep track of all folders. */
	if (store->flags & CAMEL_STORE_VTRASH)
		si->vtrash = camel_store_get_trash (store, NULL);
	if (store->flags & CAMEL_STORE_VJUNK)
		si->vjunk = camel_store_get_junk (store, NULL);

	return si;
}

static StoreInfo *
store_info_ref (StoreInfo *si)
{
	g_return_val_if_fail (si != NULL, si);
	g_return_val_if_fail (si->ref_count > 0, si);

	g_atomic_int_add (&si->ref_count, 1);

	return si;
}

static void
store_info_unref (StoreInfo *si)
{
	g_return_if_fail (si != NULL);
	g_return_if_fail (si->ref_count > 0);

	if (g_atomic_int_exchange_and_add (&si->ref_count, -1) > 1)
		return;

	if (si->vtrash != NULL)
		camel_object_unref (si->vtrash);
	if (si->vjunk != NULL)
		camel_object_unref (si->vjunk);
	camel_object_unref (si->store);
	g_free (si->name);

	g_slice_free (StoreInfo, si);
}

static void
store_hash_free (StoreInfo *si)
{
	si->removed = 1;
	store_info_unref (si);
}

static gboolean
mail_shell_module_add_store_done (CamelStore *store,
                                  CamelFolderInfo *info,
                                  gpointer user_data)
{
	StoreInfo *si = user_data;

	if (si->done != NULL)
		si->done (store, info, si);

	if (!si->removed) {
		/* Let the counters know about the already-opened
		 * junk and trash folders. */
		if (si->vtrash != NULL)
			mail_note_folder (si->vtrash);
		if (si->vjunk != NULL)
			mail_note_folder (si->vjunk);
	}

	store_info_unref (si);

	return TRUE;
}

static void
mail_shell_module_add_store (EShellModule *shell_module,
                             CamelStore *store,
                             const gchar *name,
                             void (*done) (CamelStore *store,
                                           CamelFolderInfo *info,
                                           gpointer user_data))
{
	StoreInfo *si;

	si = store_info_new (store, name);
	si->done = done;
	g_hash_table_insert (store_hash, store, si);

	em_folder_tree_model_add_store (folder_tree_model, store, si->name);

	mail_note_store (
		shell_module, store, NULL,
		mail_shell_module_add_store_done, store_info_ref (si));
}

static void
mail_shell_module_add_local_store_done (CamelStore *store,
                                        CamelFolderInfo *info,
                                        gpointer unused)
{
	gint ii;

	for (ii = 0; ii < G_N_ELEMENTS (default_local_folders); ii++) {
		if (default_local_folders[ii].folder != NULL)
			mail_note_folder (default_local_folders[ii].folder);
	}
}

static void
mail_shell_module_add_local_store (EShellModule *shell_module,
                                   CamelStore *local_store,
                                   const gchar *name)
{
	mail_shell_module_add_store (
		shell_module, local_store, name,
		mail_shell_module_add_local_store_done);
}

static void
mail_shell_module_init_hooks (void)
{
	e_plugin_hook_register_type (em_config_hook_get_type ());
	e_plugin_hook_register_type (em_event_hook_get_type ());
	e_plugin_hook_register_type (em_format_hook_get_type ());
	e_plugin_hook_register_type (em_junk_hook_get_type ());

	em_format_hook_register_type (em_format_get_type ());
	em_format_hook_register_type (em_format_html_get_type ());
	em_format_hook_register_type (em_format_html_display_get_type ());

	em_junk_hook_register_type (emj_get_type ());
}

static void
mail_shell_module_init_importers (void)
{
	EImportClass *import_class;
	EImportImporter *importer;

	import_class = g_type_class_ref (e_import_get_type ());

	importer = mbox_importer_peek ();
	e_import_class_add_importer (import_class, importer, NULL, NULL);

	importer = elm_importer_peek ();
	e_import_class_add_importer (import_class, importer, NULL, NULL);

	importer = pine_importer_peek ();
	e_import_class_add_importer (import_class, importer, NULL, NULL);
}

static void
mail_shell_module_init_local_store (EShellModule *shell_module)
{
	CamelException ex;
	CamelService *service;
	CamelURL *url;
	const gchar *data_dir;
	gchar *temp;
	gint ii;

	camel_exception_init (&ex);

	url = camel_url_new ("mbox:", NULL);
	data_dir = e_shell_module_get_data_dir (shell_module);
	temp = g_build_filename (data_dir, "local", NULL);
	camel_url_set_path (url, temp);
	g_free (temp);

	temp = camel_url_to_string (url, 0);
	service = camel_session_get_service (
		session, temp, CAMEL_PROVIDER_STORE, &ex);
	g_free (temp);

	if (service == NULL)
		goto fail;

	for (ii = 0; ii < G_N_ELEMENTS (default_local_folders); ii++) {
		/* FIXME Should this URI be account relative? */
		camel_url_set_fragment (url, default_local_folders[ii].name);
		default_local_folders[ii].uri = camel_url_to_string (url, 0);
		default_local_folders[ii].folder = camel_store_get_folder (
			CAMEL_STORE (service), default_local_folders[ii].name,
			CAMEL_STORE_FOLDER_CREATE, &ex);
		camel_exception_clear (&ex);
	}

	camel_url_free (url);

	camel_object_ref (service);
	g_object_ref (shell_module);

	mail_async_event_emit (
		async_event, MAIL_ASYNC_GUI,
		(MailAsyncFunc) mail_shell_module_add_local_store,
		shell_module, service, _("On This Computer"));

	local_store = CAMEL_STORE (service);

	return;

fail:
	g_warning ("Could not initialize local store/folder: %s", ex.desc);

	camel_exception_clear (&ex);
	camel_url_free (url);
}

static void
mail_shell_module_load_accounts (EShellModule *shell_module)
{
	EAccountList *account_list;
	EIterator *iter;

	account_list = e_get_account_list ();

	for (iter = e_list_get_iterator ((EList *) account_list);
		e_iterator_is_valid (iter); e_iterator_next (iter)) {

		EAccountService *service;
		EAccount *account;
		const gchar *name;
		const gchar *url;

		account = (EAccount *) e_iterator_get (iter);
		service = account->source;
		name = account->name;
		url = service->url;

		if (!account->enabled)
			continue;

		if (url == NULL || *url == '\0')
			continue;

		/* HACK: mbox URL's are handled by the local store setup
		 *       above.  Any that come through as account sources
		 *       are really movemail sources! */
		if (g_str_has_prefix (url, "mbox:"))
			continue;

		e_mail_shell_module_load_store_by_uri (
			shell_module, url, name);
	}

	g_object_unref (iter);
}

static void
mail_shell_module_mail_icon_cb (EShellWindow *shell_window,
                                const gchar *icon_name)
{
	GtkAction *action;

	action = e_shell_window_get_shell_view_action (
		shell_window, MODULE_NAME);
	g_object_set (action, "icon-name", icon_name, NULL);
}

static void
action_mail_folder_new_cb (GtkAction *action,
                           EShellWindow *shell_window)
{
	EMFolderTree *folder_tree = NULL;
	EMailShellSidebar *mail_shell_sidebar;
	EShellSidebar *shell_sidebar;
	EShellView *shell_view;
	const gchar *view_name;

	/* Take care not to unnecessarily load the mail shell view. */
	view_name = e_shell_window_get_active_view (shell_window);
	if (g_strcmp0 (view_name, MODULE_NAME) != 0)
		goto exit;

	shell_view = e_shell_window_get_shell_view (shell_window, view_name);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

	mail_shell_sidebar = E_MAIL_SHELL_SIDEBAR (shell_sidebar);
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);

exit:
	em_folder_utils_create_folder (NULL, folder_tree);
}

static void
action_mail_message_new_cb (GtkAction *action,
                            EShellWindow *shell_window)
{
	GtkWindow *window = GTK_WINDOW (shell_window);
	EMailShellSidebar *mail_shell_sidebar;
	EShellSidebar *shell_sidebar;
	EShellView *shell_view;
	EMFolderTree *folder_tree;
	const gchar *view_name;
	gchar *uri = NULL;

	if (!em_utils_check_user_can_send_mail (window))
		return;

	/* Take care not to unnecessarily load the mail shell view. */
	view_name = e_shell_window_get_active_view (shell_window);
	if (g_strcmp0 (view_name, MODULE_NAME) != 0)
		goto exit;

	shell_view = e_shell_window_get_shell_view (shell_window, view_name);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

	mail_shell_sidebar = E_MAIL_SHELL_SIDEBAR (shell_sidebar);
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);
	uri = em_folder_tree_get_selected_uri (folder_tree);

exit:
	em_utils_compose_new_message (uri);

	g_free (uri);
}

static GtkActionEntry item_entries[] = {

	{ "mail-message-new",
	  "mail-message-new",
	  NC_("New", "_Mail Message"),
	  "<Shift><Control>m",
	  N_("Compose a new mail message"),
	  G_CALLBACK (action_mail_message_new_cb) }
};

static GtkActionEntry source_entries[] = {

	{ "mail-folder-new",
	  "folder-new",
	  NC_("New", "Mail _Folder"),
	  NULL,
	  N_("Create a new mail folder"),
	  G_CALLBACK (action_mail_folder_new_cb) }
};

static void
mail_shell_module_init_preferences (EShell *shell)
{
	GtkWidget *preferences_window;

	preferences_window = e_shell_get_preferences_window ();

	e_preferences_window_add_page (
		E_PREFERENCES_WINDOW (preferences_window),
		"mail-accounts",
		"preferences-mail-accounts",
		_("Mail Accounts"),
		em_account_prefs_new (),
		100);

	e_preferences_window_add_page (
		E_PREFERENCES_WINDOW (preferences_window),
		"mail",
		"preferences-mail",
		_("Mail Preferences"),
		em_mailer_prefs_new (shell),
		300);

	e_preferences_window_add_page (
		E_PREFERENCES_WINDOW (preferences_window),
		"composer",
		"preferences-composer",
		_("Composer Preferences"),
		em_composer_prefs_new (shell),
		400);

	e_preferences_window_add_page (
		E_PREFERENCES_WINDOW (preferences_window),
		"system-network-proxy",
		"preferences-system-network-proxy",
		_("Network Preferences"),
		em_network_prefs_new (),
		500);
}

static void
mail_shell_module_sync_store_done_cb (CamelStore *store,
                                      gpointer user_data)
{
	mail_sync_in_progress--;
}

static void
mail_shell_module_sync_store_cb (CamelStore *store)
{
	if (!camel_application_is_exiting) {
		mail_sync_in_progress++;
		mail_sync_store (
			store, FALSE,
			mail_shell_module_sync_store_done_cb, NULL);
	}
}

static gboolean
mail_shell_module_mail_sync (EShellModule *shell_module)
{
	if (camel_application_is_exiting)
		return FALSE;

	if (mail_sync_in_progress)
		goto exit;

	if (session == NULL || !camel_session_is_online (session))
		goto exit;

	e_mail_shell_module_stores_foreach (
		shell_module, (GHFunc)
		mail_shell_module_sync_store_cb, NULL);

exit:
	return !camel_application_is_exiting;
}

static void
mail_shell_module_notify_online_mode_cb (EShell *shell,
                                         GParamSpec *pspec,
                                         EShellModule *shell_module)
{
	gboolean online;

	online = e_shell_get_online_mode (shell);
	camel_session_set_online (session, online);
}

static void
mail_shell_module_handle_email_uri_cb (gchar *folder_uri,
                                       CamelFolder *folder,
                                       gpointer user_data)
{
	CamelURL *url = user_data;
	const gchar *forward;
	const gchar *reply;
	const gchar *uid;

	if (folder == NULL) {
		g_warning ("Could not open folder '%s'", folder_uri);
		goto exit;
	}

	forward = camel_url_get_param (url, "forward");
	reply = camel_url_get_param (url, "reply");
	uid = camel_url_get_param (url, "uid");

	if (reply != NULL) {
		gint mode;

		if (g_strcmp0 (reply, "all") == 0)
			mode = REPLY_MODE_ALL;
		else if (g_strcmp0 (reply, "list") == 0)
			mode = REPLY_MODE_LIST;
		else
			mode = REPLY_MODE_SENDER;

		em_utils_reply_to_message (folder, uid, NULL, mode, NULL);

	} else if (forward != NULL) {
		GPtrArray *uids;

		uids = g_ptr_array_new ();
		g_ptr_array_add (uids, g_strdup (uid));

		if (g_strcmp0 (forward, "attached") == 0)
			em_utils_forward_attached (folder, uids, folder_uri);
		else if (g_strcmp0 (forward, "inline") == 0)
			em_utils_forward_inline (folder, uids, folder_uri);
		else if (g_strcmp0 (forward, "quoted") == 0)
			em_utils_forward_quoted (folder, uids, folder_uri);
		else
			em_utils_forward_messages (folder, uids, folder_uri);

	} else {
		GtkWidget *browser;

		/* FIXME Should pass in the shell module. */
		browser = e_mail_browser_new (mail_shell_module);
		e_mail_reader_set_folder (
			E_MAIL_READER (browser), folder, folder_uri);
		e_mail_reader_set_message (
			E_MAIL_READER (browser), uid, FALSE);
		gtk_widget_show (browser);
	}

exit:
	camel_url_free (url);
}

static gboolean
mail_shell_module_handle_uri_cb (EShell *shell,
                                 const gchar *uri,
                                 EShellModule *shell_module)
{
	gboolean handled = TRUE;

	if (g_str_has_prefix (uri, "mailto:")) {
		if (em_utils_check_user_can_send_mail (NULL))
			em_utils_compose_new_message_with_mailto (uri, NULL);

	} else if (g_str_has_prefix (uri, "email:")) {
		CamelURL *url;

		url = camel_url_new (uri, NULL);
		if (camel_url_get_param (url, "uid") != NULL) {
			gchar *curi = em_uri_to_camel (uri);

			mail_get_folder (
				curi, 0,
				mail_shell_module_handle_email_uri_cb,
				url, mail_msg_unordered_push);
			g_free (curi);

		} else {
			g_warning ("Email URI's must include a uid parameter");
			camel_url_free (url);
		}
	} else
		handled = FALSE;

	return TRUE;
}

/* Helper for mail_shell_module_prepare_for_[off|on]line_cb() */
static void
mail_shell_store_line_transition_done_cb (CamelStore *store,
                                          gpointer user_data)
{
	EActivity *activity = user_data;

	g_object_unref (activity);
}

/* Helper for mail_shell_module_prepare_for_offline_cb() */
static void
mail_shell_store_prepare_for_offline_cb (CamelService *service,
                                         gpointer unused,
                                         EActivity *activity)
{
	if (CAMEL_IS_DISCO_STORE (service) || CAMEL_IS_OFFLINE_STORE (service))
		mail_store_set_offline (
			CAMEL_STORE (service), TRUE,
			mail_shell_store_line_transition_done_cb,
			g_object_ref (activity));
}

static void
mail_shell_module_prepare_for_offline_cb (EShell *shell,
                                          EActivity *activity,
                                          EShellModule *shell_module)
{
	GList *watched_windows;
	GtkWidget *parent = NULL;
	gboolean synchronize = FALSE;

	watched_windows = e_shell_get_watched_windows (shell);
	if (watched_windows != NULL)
		parent = GTK_WIDGET (watched_windows->data);

	if (e_shell_get_network_available (shell))
		synchronize = em_utils_prompt_user (
			GTK_WINDOW (parent),
			"/apps/evolution/mail/prompts/quick_offline",
			"mail:ask-quick-offline", NULL);

	if (!synchronize) {
		mail_cancel_all ();
		camel_session_set_network_state (session, FALSE);
	}

	e_mail_shell_module_stores_foreach (
		shell_module, (GHFunc)
		mail_shell_store_prepare_for_offline_cb, activity);
}

/* Helper for mail_shell_module_prepare_for_online_cb() */
static void
mail_shell_store_prepare_for_online_cb (CamelService *service,
                                        gpointer unused,
                                        EActivity *activity)
{
	if (CAMEL_IS_DISCO_STORE (service) || CAMEL_IS_OFFLINE_STORE (service))
		mail_store_set_offline (
			CAMEL_STORE (service), FALSE,
			mail_shell_store_line_transition_done_cb,
			g_object_ref (activity));
}

static void
mail_shell_module_prepare_for_online_cb (EShell *shell,
                                         EActivity *activity,
                                         EShellModule *shell_module)
{
	camel_session_set_online (session, TRUE);

	e_mail_shell_module_stores_foreach (
		shell_module, (GHFunc)
		mail_shell_store_prepare_for_online_cb, activity);
}

static void
mail_shell_module_send_receive_cb (EShell *shell,
                                   GtkWindow *parent,
                                   EShellModule *shell_module)
{
	em_utils_clear_get_password_canceled_accounts_flag ();
	mail_send_receive (parent);
}

static void
mail_shell_module_window_weak_notify_cb (EShell *shell,
                                         GObject *where_the_object_was)
{
	g_signal_handlers_disconnect_by_func (
		shell, mail_shell_module_mail_icon_cb,
		where_the_object_was);
}

static void
mail_shell_module_window_created_cb (EShell *shell,
                                     GtkWindow *window,
                                     EShellModule *shell_module)
{
	static gboolean first_time = TRUE;
	const gchar *module_name;

	if (E_IS_MSG_COMPOSER (window)) {
		/* Integrate the new composer into the mail module. */
		em_configure_new_composer (E_MSG_COMPOSER (window));
		return;
	}

	if (!E_IS_SHELL_WINDOW (window))
		return;

	module_name = G_TYPE_MODULE (shell_module)->name;

	e_shell_window_register_new_item_actions (
		E_SHELL_WINDOW (window), module_name,
		item_entries, G_N_ELEMENTS (item_entries));

	e_shell_window_register_new_source_actions (
		E_SHELL_WINDOW (window), module_name,
		source_entries, G_N_ELEMENTS (source_entries));

	g_signal_connect_swapped (
		shell, "event::mail-icon",
		G_CALLBACK (mail_shell_module_mail_icon_cb), window);

	g_object_weak_ref (
		G_OBJECT (window), (GWeakNotify)
		mail_shell_module_window_weak_notify_cb, shell);

	if (first_time) {
		g_signal_connect (
			window, "map-event",
			G_CALLBACK (e_msg_composer_check_autosave), NULL);
		first_time = FALSE;
	}
}

static EShellModuleInfo module_info = {

	MODULE_NAME,
	MODULE_ALIASES,
	MODULE_SCHEMES,
	MODULE_SORT_ORDER,

	/* is_busy */ NULL,
	/* shutdown */ NULL,
	e_mail_shell_module_migrate
};

void
e_shell_module_init (GTypeModule *type_module)
{
	EShell *shell;
	EShellModule *shell_module;
	EShellSettings *shell_settings;
	gboolean enable_search_folders;

	shell_module = E_SHELL_MODULE (type_module);
	shell = e_shell_module_get_shell (shell_module);
	shell_settings = e_shell_get_shell_settings (shell);

	e_shell_module_set_info (
		shell_module, &module_info,
		e_mail_shell_view_get_type (type_module));

	/* This also initializes Camel, so it needs to happen early. */
	mail_session_init (shell_module);

	mail_shell_module_init_hooks ();
	mail_shell_module_init_importers ();

	/* XXX This never gets unreffed. */
	mail_shell_module = g_object_ref (shell_module);

	store_hash = g_hash_table_new_full (
		g_direct_hash, g_direct_equal,
		(GDestroyNotify) NULL,
		(GDestroyNotify) store_hash_free);

	async_event = mail_async_event_new ();

	folder_tree_model = em_folder_tree_model_new (shell_module);

	g_signal_connect (
		shell, "notify::online-mode",
		G_CALLBACK (mail_shell_module_notify_online_mode_cb),
		shell_module);

	g_signal_connect (
		shell, "handle-uri",
		G_CALLBACK (mail_shell_module_handle_uri_cb),
		shell_module);

	g_signal_connect (
		shell, "prepare-for-offline",
		G_CALLBACK (mail_shell_module_prepare_for_offline_cb),
		shell_module);

	g_signal_connect (
		shell, "prepare-for-online",
		G_CALLBACK (mail_shell_module_prepare_for_online_cb),
		shell_module);

	g_signal_connect (
		shell, "send-receive",
		G_CALLBACK (mail_shell_module_send_receive_cb),
		shell_module);

	g_signal_connect (
		shell, "window-created",
		G_CALLBACK (mail_shell_module_window_created_cb),
		shell_module);

	mail_config_init ();
	mail_msg_init ();

	mail_shell_module_init_local_store (shell_module);
	mail_shell_module_load_accounts (shell_module);

	/* Initialize settings before initializing preferences,
	 * since the preferences bind to the shell settings. */
	e_mail_shell_module_init_settings (shell);
	mail_shell_module_init_preferences (shell);

	enable_search_folders = e_shell_settings_get_boolean (
		shell_settings, "mail-enable-search-folders");
	if (enable_search_folders)
		vfolder_load_storage ();

	mail_autoreceive_init (session);

	if (g_getenv ("CAMEL_FLUSH_CHANGES") != NULL)
		mail_sync_timeout_source_id = g_timeout_add_seconds (
			mail_config_get_sync_timeout (),
			(GSourceFunc) mail_shell_module_mail_sync,
			shell_module);
}

/******************************** Public API *********************************/

CamelFolder *
e_mail_shell_module_get_folder (EShellModule *shell_module,
                                EMailFolderType folder_type)
{
	g_return_val_if_fail (E_IS_SHELL_MODULE (shell_module), NULL);

	return default_local_folders[folder_type].folder;
}

const gchar *
e_mail_shell_module_get_folder_uri (EShellModule *shell_module,
                                    EMailFolderType folder_type)
{
	g_return_val_if_fail (E_IS_SHELL_MODULE (shell_module), NULL);

	return default_local_folders[folder_type].uri;
}

EMFolderTreeModel *
e_mail_shell_module_get_folder_tree_model (EShellModule *shell_module)
{
	/* Require a shell module in case we need it in the future. */
	g_return_val_if_fail (E_IS_SHELL_MODULE (shell_module), NULL);

	return folder_tree_model;
}

void
e_mail_shell_module_add_store (EShellModule *shell_module,
                               CamelStore *store,
                               const gchar *name)
{
	g_return_if_fail (E_IS_SHELL_MODULE (shell_module));
	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (name != NULL);

	mail_shell_module_add_store (shell_module, store, name, NULL);
}

CamelStore *
e_mail_shell_module_get_local_store (EShellModule *shell_module)
{
	g_return_val_if_fail (E_IS_SHELL_MODULE (shell_module), NULL);
	g_return_val_if_fail (local_store != NULL, NULL);

	return local_store;
}

CamelStore *
e_mail_shell_module_load_store_by_uri (EShellModule *shell_module,
                                       const gchar *uri,
                                       const gchar *name)
{
	CamelStore *store;
	CamelProvider *provider;
	CamelException ex;

	g_return_val_if_fail (E_IS_SHELL_MODULE (shell_module), NULL);
	g_return_val_if_fail (uri != NULL, NULL);
	g_return_val_if_fail (name != NULL, NULL);

	camel_exception_init (&ex);

	/* Load the service, but don't connect.  Check its provider,
	 * and if this belongs in the shell's folder list, add it. */

	provider = camel_provider_get (uri, &ex);
	if (provider == NULL)
		goto fail;

	if (!(provider->flags & CAMEL_PROVIDER_IS_STORAGE))
		return NULL;

	store = (CamelStore *) camel_session_get_service (
		session, uri, CAMEL_PROVIDER_STORE, &ex);
	if (store == NULL)
		goto fail;

	e_mail_shell_module_add_store (shell_module, store, name);

	camel_object_unref (store);

	return store;

fail:
	/* FIXME: Show an error dialog. */
	g_warning (
		"Couldn't get service: %s: %s", uri,
		camel_exception_get_description (&ex));
	camel_exception_clear (&ex);

	return NULL;
}

/* Helper for e_mail_shell_module_remove_store() */
static void
mail_shell_module_remove_store_cb (CamelStore *store,
                                   gpointer event_data,
                                   gpointer user_data)
{
	camel_service_disconnect (CAMEL_SERVICE (store), TRUE, NULL);
	camel_object_unref (store);
}

void
e_mail_shell_module_remove_store (EShellModule *shell_module,
                                  CamelStore *store)
{
	g_return_if_fail (E_IS_SHELL_MODULE (shell_module));
	g_return_if_fail (CAMEL_IS_STORE (store));

	/* Because the store hash holds a reference to each store used
	 * as a key in it, none of them will ever be gc'ed, meaning any
	 * call to camel_session_get_{service,store} with the same URL
	 * will always return the same object.  So this works. */

	if (g_hash_table_lookup (store_hash, store) == NULL)
		return;

	camel_object_ref (store);
	g_hash_table_remove (store_hash, store);
	mail_note_store_remove (store);
	em_folder_tree_model_remove_store (folder_tree_model, store);

	mail_async_event_emit (
		async_event, MAIL_ASYNC_THREAD,
		(MailAsyncFunc) mail_shell_module_remove_store_cb,
		store, NULL, NULL);
}

void
e_mail_shell_module_remove_store_by_uri (EShellModule *shell_module,
                                         const gchar *uri)
{
	CamelStore *store;
	CamelProvider *provider;

	g_return_if_fail (E_IS_SHELL_MODULE (shell_module));
	g_return_if_fail (uri != NULL);

	provider = camel_provider_get (uri, NULL);
	if (provider == NULL)
		return;

	if (!(provider->flags & CAMEL_PROVIDER_IS_STORAGE))
		return;

	store = (CamelStore *) camel_session_get_service (
		session, uri, CAMEL_PROVIDER_STORE, NULL);
	if (store != NULL) {
		e_mail_shell_module_remove_store (shell_module, store);
		camel_object_unref (store);
	}
}

void
e_mail_shell_module_stores_foreach (EShellModule *shell_module,
                                    GHFunc func,
                                    gpointer user_data)
{
	GHashTableIter iter;
	gpointer key, value;

	g_return_if_fail (E_IS_SHELL_MODULE (shell_module));
	g_return_if_fail (func != NULL);

	g_hash_table_iter_init (&iter, store_hash);

	while (g_hash_table_iter_next (&iter, &key, &value))
		func (key, ((StoreInfo *) value)->name, user_data);
}
