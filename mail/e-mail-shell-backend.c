/*
 * e-mail-shell-backend.c
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

#include "e-mail-shell-backend.h"

#include <glib/gi18n.h>
#include <camel/camel-disco-store.h>
#include <camel/camel-offline-store.h>
#include <camel/camel-session.h>
#include <camel/camel-url.h>

#include "e-util/e-account-utils.h"
#include "e-util/e-binding.h"
#include "e-util/e-import.h"
#include "e-util/e-util.h"
#include "shell/e-shell.h"
#include "shell/e-shell-window.h"
#include "composer/e-msg-composer.h"
#include "widgets/misc/e-preferences-window.h"

#include "e-mail-shell-migrate.h"
#include "e-mail-shell-settings.h"
#include "e-mail-shell-sidebar.h"
#include "e-mail-shell-view.h"

#include "e-attachment-handler-mail.h"
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

#define E_MAIL_SHELL_BACKEND_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_SHELL_BACKEND, EMailShellBackendPrivate))

#define BACKEND_NAME "mail"

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

struct _EMailShellBackendPrivate {
	GHashTable *store_hash;
	MailAsyncEvent *async_event;
	EMFolderTreeModel *folder_tree_model;
	CamelStore *local_store;

	gint mail_sync_in_progress;
	guint mail_sync_timeout_source_id;
};

static gpointer parent_class;
static GType mail_shell_backend_type;

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

/* XXX So many things need the shell backend that it's
 *     just easier for now to make it globally available.
 *     We should fix this, though. */
EMailShellBackend *global_mail_shell_backend = NULL;

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
mail_shell_backend_add_store_done (CamelStore *store,
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
mail_shell_backend_add_store (EMailShellBackend *mail_shell_backend,
                              CamelStore *store,
                              const gchar *name,
                              void (*done) (CamelStore *store,
                                            CamelFolderInfo *info,
                                            gpointer user_data))
{
	EMFolderTreeModel *folder_tree_model;
	GHashTable *store_hash;
	StoreInfo *si;

	store_hash = mail_shell_backend->priv->store_hash;
	folder_tree_model = mail_shell_backend->priv->folder_tree_model;

	si = store_info_new (store, name);
	si->done = done;
	g_hash_table_insert (store_hash, store, si);

	em_folder_tree_model_add_store (folder_tree_model, store, si->name);

	mail_note_store (
		mail_shell_backend, store, NULL,
		mail_shell_backend_add_store_done, store_info_ref (si));
}

static void
mail_shell_backend_add_local_store_done (CamelStore *store,
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
mail_shell_backend_add_local_store (EMailShellBackend *mail_shell_backend,
                                    CamelStore *local_store,
                                    const gchar *name)
{
	mail_shell_backend_add_store (
		mail_shell_backend, local_store, name,
		mail_shell_backend_add_local_store_done);
}

static void
mail_shell_backend_init_hooks (void)
{
	e_plugin_hook_register_type (em_config_hook_get_type ());
	e_plugin_hook_register_type (em_event_hook_get_type ());
	e_plugin_hook_register_type (em_junk_hook_get_type ());

	/* EMFormat classes must be registered before EMFormatHook. */
	em_format_hook_register_type (em_format_get_type ());
	em_format_hook_register_type (em_format_html_get_type ());
	em_format_hook_register_type (em_format_html_display_get_type ());
	e_plugin_hook_register_type (em_format_hook_get_type ());

	em_junk_hook_register_type (emj_get_type ());
}

static void
mail_shell_backend_init_importers (void)
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
mail_shell_backend_init_local_store (EShellBackend *shell_backend)
{
	EMailShellBackendPrivate *priv;
	CamelException ex;
	CamelService *service;
	CamelURL *url;
	MailAsyncEvent *async_event;
	const gchar *data_dir;
	gchar *temp;
	gint ii;

	priv = E_MAIL_SHELL_BACKEND_GET_PRIVATE (shell_backend);

	camel_exception_init (&ex);

	async_event = priv->async_event;
	data_dir = e_shell_backend_get_data_dir (shell_backend);

	url = camel_url_new ("mbox:", NULL);
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
	g_object_ref (shell_backend);

	mail_async_event_emit (
		async_event, MAIL_ASYNC_GUI,
		(MailAsyncFunc) mail_shell_backend_add_local_store,
		shell_backend, service, _("On This Computer"));

	priv->local_store = CAMEL_STORE (service);

	return;

fail:
	g_warning ("Could not initialize local store/folder: %s", ex.desc);

	camel_exception_clear (&ex);
	camel_url_free (url);
}

static void
mail_shell_backend_load_accounts (EShellBackend *shell_backend)
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

		e_mail_shell_backend_load_store_by_uri (
			E_MAIL_SHELL_BACKEND (shell_backend), url, name);
	}

	g_object_unref (iter);
}

static void
mail_shell_backend_mail_icon_cb (EShellWindow *shell_window,
                                const gchar *icon_name)
{
	GtkAction *action;

	action = e_shell_window_get_shell_view_action (
		shell_window, BACKEND_NAME);
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
	if (g_strcmp0 (view_name, BACKEND_NAME) != 0)
		goto exit;

	shell_view = e_shell_window_get_shell_view (shell_window, view_name);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

	mail_shell_sidebar = E_MAIL_SHELL_SIDEBAR (shell_sidebar);
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);

exit:
	em_folder_utils_create_folder (
		NULL, folder_tree, GTK_WINDOW (shell_window));
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
	if (g_strcmp0 (view_name, BACKEND_NAME) != 0)
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
mail_shell_backend_init_preferences (EShell *shell)
{
	GtkWidget *preferences_window;

	preferences_window = e_shell_get_preferences_window (shell);

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
mail_shell_backend_sync_store_done_cb (CamelStore *store,
                                       gpointer user_data)
{
	EMailShellBackend *mail_shell_backend = user_data;

	mail_shell_backend->priv->mail_sync_in_progress--;
}

static void
mail_shell_backend_sync_store_cb (CamelStore *store,
                                  EMailShellBackend *mail_shell_backend)
{
	if (!camel_application_is_exiting) {
		mail_shell_backend->priv->mail_sync_in_progress++;
		mail_sync_store (
			store, FALSE,
			mail_shell_backend_sync_store_done_cb,
			mail_shell_backend);
	}
}

static gboolean
mail_shell_backend_mail_sync (EMailShellBackend *mail_shell_backend)
{
	if (camel_application_is_exiting)
		return FALSE;

	if (mail_shell_backend->priv->mail_sync_in_progress)
		goto exit;

	if (session == NULL || !camel_session_is_online (session))
		goto exit;

	e_mail_shell_backend_stores_foreach (
		mail_shell_backend, (GHFunc)
		mail_shell_backend_sync_store_cb,
		mail_shell_backend);

exit:
	return !camel_application_is_exiting;
}

static void
mail_shell_backend_notify_online_cb (EShell *shell,
                                    GParamSpec *pspec,
                                    EShellBackend *shell_backend)
{
	gboolean online;

	online = e_shell_get_online (shell);
	camel_session_set_online (session, online);
}

static void
mail_shell_backend_handle_email_uri_cb (gchar *folder_uri,
                                        CamelFolder *folder,
                                        gpointer user_data)
{
	EMailShellBackend *mail_shell_backend = user_data;
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
		browser = e_mail_browser_new (mail_shell_backend);
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
mail_shell_backend_handle_uri_cb (EShell *shell,
                                  const gchar *uri,
                                  EMailShellBackend *mail_shell_backend)
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
				mail_shell_backend_handle_email_uri_cb,
				mail_shell_backend, mail_msg_unordered_push);
			g_free (curi);

		} else {
			g_warning ("Email URI's must include a uid parameter");
			camel_url_free (url);
		}
	} else
		handled = FALSE;

	return TRUE;
}

/* Helper for mail_shell_backend_prepare_for_[off|on]line_cb() */
static void
mail_shell_store_line_transition_done_cb (CamelStore *store,
                                          gpointer user_data)
{
	EActivity *activity = user_data;

	g_object_unref (activity);
}

/* Helper for mail_shell_backend_prepare_for_offline_cb() */
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
mail_shell_backend_prepare_for_offline_cb (EShell *shell,
                                           EActivity *activity,
                                           EMailShellBackend *mail_shell_backend)
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

	e_mail_shell_backend_stores_foreach (
		mail_shell_backend, (GHFunc)
		mail_shell_store_prepare_for_offline_cb, activity);
}

/* Helper for mail_shell_backend_prepare_for_online_cb() */
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
mail_shell_backend_prepare_for_online_cb (EShell *shell,
                                          EActivity *activity,
                                          EMailShellBackend *mail_shell_backend)
{
	camel_session_set_online (session, TRUE);

	e_mail_shell_backend_stores_foreach (
		mail_shell_backend, (GHFunc)
		mail_shell_store_prepare_for_online_cb, activity);
}

static void
mail_shell_backend_send_receive_cb (EShell *shell,
                                   GtkWindow *parent,
                                   EShellBackend *shell_backend)
{
	em_utils_clear_get_password_canceled_accounts_flag ();
	mail_send_receive (parent);
}

static void
mail_shell_backend_window_weak_notify_cb (EShell *shell,
                                         GObject *where_the_object_was)
{
	g_signal_handlers_disconnect_by_func (
		shell, mail_shell_backend_mail_icon_cb,
		where_the_object_was);
}

static void
mail_shell_backend_window_created_cb (EShell *shell,
                                     GtkWindow *window,
                                     EShellBackend *shell_backend)
{
	EShellSettings *shell_settings;
	static gboolean first_time = TRUE;
	const gchar *backend_name;

	shell_settings = e_shell_get_shell_settings (shell);

	/* This applies to both the composer and signature editor. */
	if (GTKHTML_IS_EDITOR (window)) {
		GList *spell_languages;

		e_binding_new (
			G_OBJECT (shell_settings), "composer-inline-spelling",
			G_OBJECT (window), "inline-spelling");

		e_binding_new (
			G_OBJECT (shell_settings), "composer-magic-links",
			G_OBJECT (window), "magic-links");

		e_binding_new (
			G_OBJECT (shell_settings), "composer-magic-smileys",
			G_OBJECT (window), "magic-smileys");

		spell_languages = e_load_spell_languages ();
		gtkhtml_editor_set_spell_languages (
			GTKHTML_EDITOR (window), spell_languages);
		g_list_free (spell_languages);
	}

	if (E_IS_MSG_COMPOSER (window)) {
		/* Integrate the new composer into the mail module. */
		em_configure_new_composer (E_MSG_COMPOSER (window));
		return;
	}

	if (!E_IS_SHELL_WINDOW (window))
		return;

	backend_name = E_SHELL_BACKEND_GET_CLASS (shell_backend)->name;

	e_shell_window_register_new_item_actions (
		E_SHELL_WINDOW (window), backend_name,
		item_entries, G_N_ELEMENTS (item_entries));

	e_shell_window_register_new_source_actions (
		E_SHELL_WINDOW (window), backend_name,
		source_entries, G_N_ELEMENTS (source_entries));

	g_signal_connect_swapped (
		shell, "event::mail-icon",
		G_CALLBACK (mail_shell_backend_mail_icon_cb), window);

	g_object_weak_ref (
		G_OBJECT (window), (GWeakNotify)
		mail_shell_backend_window_weak_notify_cb, shell);

	if (first_time) {
		g_signal_connect (
			window, "map-event",
			G_CALLBACK (e_msg_composer_check_autosave), NULL);
		first_time = FALSE;
	}
}

static void
mail_shell_backend_dispose (GObject *object)
{
	EMailShellBackendPrivate *priv;

	priv = E_MAIL_SHELL_BACKEND_GET_PRIVATE (object);

	g_hash_table_remove_all (priv->store_hash);

	if (priv->folder_tree_model != NULL) {
		g_object_unref (priv->folder_tree_model);
		priv->folder_tree_model = NULL;
	}

	if (priv->local_store != NULL) {
		camel_object_unref (priv->local_store);
		priv->local_store = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
mail_shell_backend_finalize (GObject *object)
{
	EMailShellBackendPrivate *priv;

	priv = E_MAIL_SHELL_BACKEND_GET_PRIVATE (object);

	g_hash_table_destroy (priv->store_hash);
	mail_async_event_destroy (priv->async_event);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
mail_shell_backend_constructed (GObject *object)
{
	EMailShellBackendPrivate *priv;
	EShell *shell;
	EShellBackend *shell_backend;

	priv = E_MAIL_SHELL_BACKEND_GET_PRIVATE (object);

	shell_backend = E_SHELL_BACKEND (object);
	shell = e_shell_backend_get_shell (shell_backend);

	/* This also initializes Camel, so it needs to happen early. */
	mail_session_init (E_MAIL_SHELL_BACKEND (shell_backend));

	mail_shell_backend_init_hooks ();
	mail_shell_backend_init_importers ();

	e_attachment_handler_mail_get_type ();

	/* XXX This never gets unreffed. */
	global_mail_shell_backend = g_object_ref (shell_backend);

	priv->store_hash = g_hash_table_new_full (
		g_direct_hash, g_direct_equal,
		(GDestroyNotify) NULL,
		(GDestroyNotify) store_hash_free);

	priv->async_event = mail_async_event_new ();

	priv->folder_tree_model = em_folder_tree_model_new (
		E_MAIL_SHELL_BACKEND (shell_backend));

	g_signal_connect (
		shell, "notify::online",
		G_CALLBACK (mail_shell_backend_notify_online_cb),
		shell_backend);

	g_signal_connect (
		shell, "handle-uri",
		G_CALLBACK (mail_shell_backend_handle_uri_cb),
		shell_backend);

	g_signal_connect (
		shell, "prepare-for-offline",
		G_CALLBACK (mail_shell_backend_prepare_for_offline_cb),
		shell_backend);

	g_signal_connect (
		shell, "prepare-for-online",
		G_CALLBACK (mail_shell_backend_prepare_for_online_cb),
		shell_backend);

	g_signal_connect (
		shell, "send-receive",
		G_CALLBACK (mail_shell_backend_send_receive_cb),
		shell_backend);

	g_signal_connect (
		shell, "window-created",
		G_CALLBACK (mail_shell_backend_window_created_cb),
		shell_backend);

	mail_config_init ();
	mail_msg_init ();

	mail_shell_backend_init_local_store (shell_backend);
	mail_shell_backend_load_accounts (shell_backend);

	/* Initialize settings before initializing preferences,
	 * since the preferences bind to the shell settings. */
	e_mail_shell_settings_init (shell);
	mail_shell_backend_init_preferences (shell);
}

static void
mail_shell_backend_start (EShellBackend *shell_backend)
{
	EMailShellBackendPrivate *priv;
	EShell *shell;
	EShellSettings *shell_settings;
	gboolean enable_search_folders;

	priv = E_MAIL_SHELL_BACKEND_GET_PRIVATE (shell_backend);

	shell = e_shell_backend_get_shell (shell_backend);
	shell_settings = e_shell_get_shell_settings (shell);

	/* XXX Do we really still need this flag? */
	mail_session_set_interactive (TRUE);

	enable_search_folders = e_shell_settings_get_boolean (
		shell_settings, "mail-enable-search-folders");
	if (enable_search_folders)
		vfolder_load_storage ();

	mail_autoreceive_init (shell_backend, session);

	if (g_getenv ("CAMEL_FLUSH_CHANGES") != NULL)
		priv->mail_sync_timeout_source_id = g_timeout_add_seconds (
			mail_config_get_sync_timeout (),
			(GSourceFunc) mail_shell_backend_mail_sync,
			shell_backend);
}

static void
mail_shell_backend_class_init (EMailShellBackendClass *class)
{
	GObjectClass *object_class;
	EShellBackendClass *shell_backend_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMailShellBackendPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = mail_shell_backend_dispose;
	object_class->finalize = mail_shell_backend_finalize;
	object_class->constructed = mail_shell_backend_constructed;

	shell_backend_class = E_SHELL_BACKEND_CLASS (class);
	shell_backend_class->shell_view_type = E_TYPE_MAIL_SHELL_VIEW;
	shell_backend_class->name = BACKEND_NAME;
	shell_backend_class->aliases = "";
	shell_backend_class->schemes = "mailto:email";
	shell_backend_class->sort_order = 200;
	shell_backend_class->start = mail_shell_backend_start;
	shell_backend_class->is_busy = NULL;
	shell_backend_class->shutdown = NULL;
	shell_backend_class->migrate = e_mail_shell_migrate;
}

static void
mail_shell_backend_init (EMailShellBackend *mail_shell_backend)
{
	mail_shell_backend->priv =
		E_MAIL_SHELL_BACKEND_GET_PRIVATE (mail_shell_backend);
}

GType
e_mail_shell_backend_get_type (void)
{
	return mail_shell_backend_type;
}

void
e_mail_shell_backend_register_type (GTypeModule *type_module)
{
	const GTypeInfo type_info = {
		sizeof (EMailShellBackendClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) mail_shell_backend_class_init,
		(GClassFinalizeFunc) NULL,
		NULL,  /* class_data */
		sizeof (EMailShellBackend),
		0,     /* n_preallocs */
		(GInstanceInitFunc) mail_shell_backend_init,
		NULL   /* value_table */
	};

	mail_shell_backend_type = g_type_module_register_type (
		type_module, E_TYPE_SHELL_BACKEND,
		"EMailShellBackend", &type_info, 0);
}

/******************************** Public API *********************************/

CamelFolder *
e_mail_shell_backend_get_folder (EMailShellBackend *mail_shell_backend,
                                 EMailFolderType folder_type)
{
	g_return_val_if_fail (
		E_IS_MAIL_SHELL_BACKEND (mail_shell_backend), NULL);

	return default_local_folders[folder_type].folder;
}

const gchar *
e_mail_shell_backend_get_folder_uri (EMailShellBackend *mail_shell_backend,
                                     EMailFolderType folder_type)
{
	g_return_val_if_fail (
		E_IS_MAIL_SHELL_BACKEND (mail_shell_backend), NULL);

	return default_local_folders[folder_type].uri;
}

EMFolderTreeModel *
e_mail_shell_backend_get_folder_tree_model (EMailShellBackend *mail_shell_backend)
{
	g_return_val_if_fail (
		E_IS_MAIL_SHELL_BACKEND (mail_shell_backend), NULL);

	return mail_shell_backend->priv->folder_tree_model;
}

void
e_mail_shell_backend_add_store (EMailShellBackend *mail_shell_backend,
                                CamelStore *store,
                                const gchar *name)
{
	g_return_if_fail (E_IS_MAIL_SHELL_BACKEND (mail_shell_backend));
	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (name != NULL);

	mail_shell_backend_add_store (mail_shell_backend, store, name, NULL);
}

CamelStore *
e_mail_shell_backend_get_local_store (EMailShellBackend *mail_shell_backend)
{
	g_return_val_if_fail (
		E_IS_MAIL_SHELL_BACKEND (mail_shell_backend), NULL);

	return mail_shell_backend->priv->local_store;
}

CamelStore *
e_mail_shell_backend_load_store_by_uri (EMailShellBackend *mail_shell_backend,
                                        const gchar *uri,
                                        const gchar *name)
{
	CamelStore *store;
	CamelProvider *provider;
	CamelException ex;

	g_return_val_if_fail (
		E_IS_MAIL_SHELL_BACKEND (mail_shell_backend), NULL);
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

	e_mail_shell_backend_add_store (mail_shell_backend, store, name);

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

/* Helper for e_mail_shell_backend_remove_store() */
static void
mail_shell_backend_remove_store_cb (CamelStore *store,
                                    gpointer event_data,
                                    gpointer user_data)
{
	camel_service_disconnect (CAMEL_SERVICE (store), TRUE, NULL);
	camel_object_unref (store);
}

void
e_mail_shell_backend_remove_store (EMailShellBackend *mail_shell_backend,
                                   CamelStore *store)
{
	GHashTable *store_hash;
	MailAsyncEvent *async_event;
	EMFolderTreeModel *folder_tree_model;

	g_return_if_fail (E_IS_MAIL_SHELL_BACKEND (mail_shell_backend));
	g_return_if_fail (CAMEL_IS_STORE (store));

	store_hash = mail_shell_backend->priv->store_hash;
	async_event = mail_shell_backend->priv->async_event;
	folder_tree_model = mail_shell_backend->priv->folder_tree_model;

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
		(MailAsyncFunc) mail_shell_backend_remove_store_cb,
		store, NULL, NULL);
}

void
e_mail_shell_backend_remove_store_by_uri (EMailShellBackend *mail_shell_backend,
                                          const gchar *uri)
{
	CamelStore *store;
	CamelProvider *provider;

	g_return_if_fail (E_IS_MAIL_SHELL_BACKEND (mail_shell_backend));
	g_return_if_fail (uri != NULL);

	provider = camel_provider_get (uri, NULL);
	if (provider == NULL)
		return;

	if (!(provider->flags & CAMEL_PROVIDER_IS_STORAGE))
		return;

	store = (CamelStore *) camel_session_get_service (
		session, uri, CAMEL_PROVIDER_STORE, NULL);
	if (store != NULL) {
		e_mail_shell_backend_remove_store (mail_shell_backend, store);
		camel_object_unref (store);
	}
}

void
e_mail_shell_backend_stores_foreach (EMailShellBackend *mail_shell_backend,
                                     GHFunc func,
                                     gpointer user_data)
{
	GHashTable *store_hash;
	GHashTableIter iter;
	gpointer key, value;

	g_return_if_fail (E_IS_MAIL_SHELL_BACKEND (mail_shell_backend));
	g_return_if_fail (func != NULL);

	store_hash = mail_shell_backend->priv->store_hash;

	g_hash_table_iter_init (&iter, store_hash);

	while (g_hash_table_iter_next (&iter, &key, &value))
		func (key, ((StoreInfo *) value)->name, user_data);
}

/******************* Code below here belongs elsewhere. *******************/

#include "filter/filter-option.h"
#include "shell/e-shell-settings.h"
#include "mail/e-mail-label-list-store.h"

GSList *
e_mail_labels_get_filter_options (void)
{
	EShell *shell;
	EShellSettings *shell_settings;
	EMailLabelListStore *list_store;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GSList *list = NULL;
	gboolean valid;

	shell = e_shell_get_default ();
	shell_settings = e_shell_get_shell_settings (shell);
	list_store = e_shell_settings_get_object (
		shell_settings, "mail-label-list-store");

	model = GTK_TREE_MODEL (list_store);
	valid = gtk_tree_model_get_iter_first (model, &iter);

	while (valid) {
		struct _filter_option *option;
		gchar *name, *tag;

		name = e_mail_label_list_store_get_name (list_store, &iter);
		tag = e_mail_label_list_store_get_tag (list_store, &iter);

		option = g_new0 (struct _filter_option, 1);
		option->title = e_str_without_underscores (name);
		option->value = tag;  /* takes ownership */

		g_free (name);

		valid = gtk_tree_model_iter_next (model, &iter);
	}

	g_object_unref (list_store);

	return list;
}
