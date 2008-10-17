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
#include <camel/camel-session.h>
#include <camel/camel-url.h>

#include "e-util/e-import.h"
#include "e-util/e-util.h"
#include "shell/e-shell.h"
#include "shell/e-shell-window.h"

#include "e-mail-shell-view.h"
#include "e-mail-shell-module.h"
#include "e-mail-shell-module-migrate.h"

#include "em-config.h"
#include "em-event.h"
#include "em-folder-tree-model.h"
#include "em-format-hook.h"
#include "em-format-html-display.h"
#include "em-junk-hook.h"
#include "mail-config.h"
#include "mail-folder-cache.h"
#include "mail-mt.h"
#include "mail-session.h"
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

G_LOCK_DEFINE_STATIC (local_store);

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

	account_list = mail_config_get_accounts ();

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
mail_shell_module_new_mail_cb (EShellWindow *shell_window)
{
	GtkAction *action;

	action = e_shell_window_get_shell_view_action (
		shell_window, MODULE_NAME);
	g_object_set (action, "icon-name", "mail-unread", NULL);

	g_print ("Shell Event: new-mail\n");
}

static void
action_mail_folder_new_cb (GtkAction *action,
                           EShellWindow *shell_window)
{
	/* FIXME */
}

static void
action_mail_message_new_cb (GtkAction *action,
                            EShellWindow *shell_window)
{
	/* FIXME */
}

static GtkActionEntry item_entries[] = {

	{ "mail-message-new",
	  "mail-message-new",
	  N_("_Mail Message"),  /* XXX C_() here */
	  "<Shift><Control>m",
	  N_("Compose a new mail message"),
	  G_CALLBACK (action_mail_message_new_cb) }
};

static GtkActionEntry source_entries[] = {

	{ "mail-folder-new",
	  "folder-new",
	  N_("Mail _Folder"),
	  NULL,
	  N_("Create a new mail folder"),
	  G_CALLBACK (action_mail_folder_new_cb) }
};

static gboolean
mail_module_handle_uri (EShellModule *shell_module,
                        const gchar *uri)
{
	/* FIXME */
	return FALSE;
}

static void
mail_module_window_created (EShellModule *shell_module,
                            EShellWindow *shell_window)
{
	EShell *shell;
	const gchar *module_name;

	shell = e_shell_module_get_shell (shell_module);
	module_name = G_TYPE_MODULE (shell_module)->name;

	e_shell_window_register_new_item_actions (
		shell_window, module_name,
		item_entries, G_N_ELEMENTS (item_entries));

	e_shell_window_register_new_source_actions (
		shell_window, module_name,
		source_entries, G_N_ELEMENTS (source_entries));

	g_signal_connect_swapped (
		shell, "event::new-mail",
		G_CALLBACK (mail_shell_module_new_mail_cb), shell_window);
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

	shell_module = E_SHELL_MODULE (type_module);
	shell = e_shell_module_get_shell (shell_module);

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

	g_signal_connect_swapped (
		shell, "handle-uri",
		G_CALLBACK (mail_module_handle_uri), shell_module);

	g_signal_connect_swapped (
		shell, "window-created",
		G_CALLBACK (mail_module_window_created), shell_module);

	mail_config_init ();
	mail_msg_init ();

	mail_shell_module_init_local_store (shell_module);
	mail_shell_module_load_accounts (shell_module);
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
