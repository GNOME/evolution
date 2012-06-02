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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-mail-shell-backend.h"

#include <glib/gi18n.h>

#include <e-util/e-import.h>
#include <e-util/e-util.h>

#include <shell/e-shell.h>
#include <shell/e-shell-window.h>

#ifdef WITH_CAPPLET
#include <capplet/settings/mail-capplet-shell.h>
#endif

#include <composer/e-msg-composer.h>

#include <widgets/misc/e-preferences-window.h>
#include <widgets/misc/e-signature-editor.h>
#include <widgets/misc/e-web-view.h>

#include <libemail-engine/e-mail-folder-utils.h>
#include <libemail-engine/e-mail-session.h>
#include <libemail-engine/mail-config.h>
#include <libemail-engine/mail-ops.h>

#include <mail/e-mail-browser.h>
#include <mail/e-mail-reader.h>
#include <mail/em-account-editor.h>
#include <mail/em-composer-utils.h>
#include <mail/em-folder-utils.h>
#include <mail/em-format-hook.h>
#include <mail/em-format-html-display.h>
#include <mail/em-utils.h>
#include <mail/mail-send-recv.h>
#include <mail/mail-vfolder-ui.h>
#include <mail/importers/mail-importer.h>
#include <mail/e-mail-ui-session.h>

#include "e-mail-shell-settings.h"
#include "e-mail-shell-sidebar.h"
#include "e-mail-shell-view.h"
#include "em-account-prefs.h"
#include "em-composer-prefs.h"
#include "em-mailer-prefs.h"
#include "em-network-prefs.h"

#define E_MAIL_SHELL_BACKEND_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_SHELL_BACKEND, EMailShellBackendPrivate))

#define E_MAIL_SHELL_BACKEND_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_SHELL_BACKEND, EMailShellBackendPrivate))

#define BACKEND_NAME "mail"

struct _EMailShellBackendPrivate {
	gint mail_sync_in_progress;
	guint mail_sync_source_id;
	gpointer assistant; /* weak pointer, when adding new mail account */
	gpointer editor;    /* weak pointer, when editing a mail account */
};

static void mbox_create_preview_cb (GObject *preview, GtkWidget **preview_widget);
static void mbox_fill_preview_cb (GObject *preview, CamelMimeMessage *msg);

G_DEFINE_DYNAMIC_TYPE (
	EMailShellBackend,
	e_mail_shell_backend,
	E_TYPE_MAIL_BACKEND)

static void
mail_shell_backend_init_importers (void)
{
	EImportClass *import_class;
	EImportImporter *importer;

	import_class = g_type_class_ref (e_import_get_type ());

	importer = mbox_importer_peek ();
	e_import_class_add_importer (import_class, importer, NULL, NULL);
	mbox_importer_set_preview_funcs (
		mbox_create_preview_cb, mbox_fill_preview_cb);

	importer = elm_importer_peek ();
	e_import_class_add_importer (import_class, importer, NULL, NULL);

	importer = pine_importer_peek ();
	e_import_class_add_importer (import_class, importer, NULL, NULL);
}

static void
mail_shell_backend_mail_icon_cb (EShellWindow *shell_window,
                                 const gchar *icon_name)
{
	GtkAction *action;

	action = e_shell_window_get_shell_view_action (
		shell_window, BACKEND_NAME);
	gtk_action_set_icon_name (action, icon_name);
}

static void
action_mail_folder_new_cb (GtkAction *action,
                           EShellWindow *shell_window)
{
	EMFolderTree *folder_tree = NULL;
	EMailShellSidebar *mail_shell_sidebar;
	EMailSession *session;
	EShellSidebar *shell_sidebar;
	EShellView *shell_view;
	const gchar *view_name;

	/* Take care not to unnecessarily load the mail shell view. */
	view_name = e_shell_window_get_active_view (shell_window);
	if (g_strcmp0 (view_name, BACKEND_NAME) != 0) {
		EShell *shell;
		EShellBackend *shell_backend;
		EMailBackend *backend;

		shell = e_shell_window_get_shell (shell_window);

		shell_backend =
			e_shell_get_backend_by_name (shell, BACKEND_NAME);
		g_return_if_fail (E_IS_MAIL_BACKEND (shell_backend));

		backend = E_MAIL_BACKEND (shell_backend);
		session = e_mail_backend_get_session (backend);

		goto exit;
	}

	shell_view = e_shell_window_get_shell_view (shell_window, view_name);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

	mail_shell_sidebar = E_MAIL_SHELL_SIDEBAR (shell_sidebar);
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);
	session = em_folder_tree_get_session (folder_tree);

exit:
	em_folder_utils_create_folder (
		GTK_WINDOW (shell_window), session, folder_tree, NULL);
}

static void
action_mail_account_new_cb (GtkAction *action,
                            EShellWindow *shell_window)
{
	EShell *shell;
	EShellBackend *shell_backend;

	g_return_if_fail (shell_window != NULL);

	shell = e_shell_window_get_shell (shell_window);
	shell_backend = e_shell_get_backend_by_name (shell, BACKEND_NAME);
	g_return_if_fail (E_IS_MAIL_SHELL_BACKEND (shell_backend));

	e_mail_shell_backend_new_account (
		E_MAIL_SHELL_BACKEND (shell_backend),
		GTK_WINDOW (shell_window));
}

static void
action_mail_message_new_cb (GtkAction *action,
                            EShellWindow *shell_window)
{
	EMailShellSidebar *mail_shell_sidebar;
	EShellSidebar *shell_sidebar;
	EShellView *shell_view;
	EShell *shell;
	EMFolderTree *folder_tree;
	CamelFolder *folder = NULL;
	CamelStore *store;
	const gchar *view_name;
	gchar *folder_name;

	shell = e_shell_window_get_shell (shell_window);

	/* Take care not to unnecessarily load the mail shell view. */
	view_name = e_shell_window_get_active_view (shell_window);
	if (g_strcmp0 (view_name, BACKEND_NAME) != 0)
		goto exit;

	shell_view = e_shell_window_get_shell_view (shell_window, view_name);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

	mail_shell_sidebar = E_MAIL_SHELL_SIDEBAR (shell_sidebar);
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);

	if (em_folder_tree_get_selected (folder_tree, &store, &folder_name)) {

		/* FIXME This blocks and is not cancellable. */
		folder = camel_store_get_folder_sync (
			store, folder_name, 0, NULL, NULL);

		g_object_unref (store);
		g_free (folder_name);
	}

exit:
	em_utils_compose_new_message (shell, folder);
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

	{ "mail-account-new",
	  "evolution-mail",
	  NC_("New", "Mail Acco_unt"),
	  NULL,
	  N_("Create a new mail account"),
	  G_CALLBACK (action_mail_account_new_cb) },

	{ "mail-folder-new",
	  "folder-new",
	  NC_("New", "Mail _Folder"),
	  NULL,
	  N_("Create a new mail folder"),
	  G_CALLBACK (action_mail_folder_new_cb) }
};

static void
mail_shell_backend_sync_store_done_cb (CamelStore *store,
                                       gpointer user_data)
{
	EMailShellBackend *mail_shell_backend = user_data;

	mail_shell_backend->priv->mail_sync_in_progress--;
}

static gboolean
mail_shell_backend_mail_sync (EMailShellBackend *mail_shell_backend)
{
	EShell *shell;
	EShellBackend *shell_backend;
	EMailBackend *backend;
	EMailSession *session;
	GList *list, *link;

	shell_backend = E_SHELL_BACKEND (mail_shell_backend);
	shell = e_shell_backend_get_shell (shell_backend);

	/* Obviously we can only sync in online mode. */
	if (!e_shell_get_online (shell))
		goto exit;

	/* If a sync is still in progress, skip this round. */
	if (mail_shell_backend->priv->mail_sync_in_progress)
		goto exit;

	backend = E_MAIL_BACKEND (mail_shell_backend);
	session = e_mail_backend_get_session (backend);

	list = camel_session_list_services (CAMEL_SESSION (session));

	for (link = list; link != NULL; link = g_list_next (link)) {
		CamelService *service;

		service = CAMEL_SERVICE (link->data);

		if (!CAMEL_IS_STORE (service))
			continue;

		mail_shell_backend->priv->mail_sync_in_progress++;

		mail_sync_store (
			CAMEL_STORE (service), FALSE,
			mail_shell_backend_sync_store_done_cb,
			mail_shell_backend);
	}

	g_list_free (list);

exit:
	return TRUE;
}

static gboolean
mail_shell_backend_handle_uri_cb (EShell *shell,
                                  const gchar *uri,
                                  EMailShellBackend *mail_shell_backend)
{
	gboolean handled = FALSE;

	if (g_str_has_prefix (uri, "mailto:")) {
		em_utils_compose_new_message_with_mailto (shell, uri, NULL);
		handled = TRUE;
	}

	return handled;
}

static void
mail_shell_backend_prepare_for_quit_cb (EShell *shell,
                                        EActivity *activity,
                                        EShellBackend *shell_backend)
{
	EMailShellBackendPrivate *priv;

	priv = E_MAIL_SHELL_BACKEND_GET_PRIVATE (shell_backend);

	/* Prevent a sync from starting while trying to shutdown. */
	if (priv->mail_sync_source_id > 0) {
		g_source_remove (priv->mail_sync_source_id);
		priv->mail_sync_source_id = 0;
	}
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
mail_shell_backend_window_added_cb (GtkApplication *application,
                                    GtkWindow *window,
                                    EShellBackend *shell_backend)
{
	EShell *shell = E_SHELL (application);
	EMailBackend *backend;
	EMailSession *session;
	const gchar *backend_name;

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);

	/* This applies to both the composer and signature editor. */
	if (GTKHTML_IS_EDITOR (window)) {
		GList *spell_languages;
		gboolean active = TRUE;

		spell_languages = e_load_spell_languages ();
		gtkhtml_editor_set_spell_languages (
			GTKHTML_EDITOR (window), spell_languages);
		g_list_free (spell_languages);

		if (!E_IS_SIGNATURE_EDITOR (window) ||
		    !e_signature_editor_get_editing_old (E_SIGNATURE_EDITOR (window))) {
			EShellSettings *shell_settings;

			shell_settings = e_shell_get_shell_settings (shell);

			/* Express mode does not honor this setting. */
			if (!e_shell_get_express_mode (shell))
				active = e_shell_settings_get_boolean (
					shell_settings, "composer-format-html");

			gtkhtml_editor_set_html_mode (GTKHTML_EDITOR (window), active);
		}
	}

	if (E_IS_MSG_COMPOSER (window)) {
		/* Start the mail backend if it isn't already.  This
		 * may be necessary when opening a new composer window
		 * from a shell view other than mail. */
		e_shell_backend_start (shell_backend);

		/* Integrate the new composer into the mail module. */
		em_configure_new_composer (
			E_MSG_COMPOSER (window), session);
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
}

static void
mail_shell_backend_constructed (GObject *object)
{
	EShell *shell;
	EShellSettings *shell_settings;
	EShellBackend *shell_backend;
	EMailSession *mail_session;
	CamelService *vstore;
	GtkWidget *preferences_window;

	shell_backend = E_SHELL_BACKEND (object);
	shell = e_shell_backend_get_shell (shell_backend);
	shell_settings = e_shell_get_shell_settings (shell);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_shell_backend_parent_class)->constructed (object);

	/* Register format types for EMFormatHook. */
	em_format_hook_register_type (em_format_get_type ());
	em_format_hook_register_type (em_format_html_get_type ());
	em_format_hook_register_type (em_format_html_display_get_type ());

	/* Register plugin hook types. */
	em_format_hook_get_type ();

	mail_shell_backend_init_importers ();

	g_signal_connect (
		shell, "handle-uri",
		G_CALLBACK (mail_shell_backend_handle_uri_cb),
		shell_backend);

	g_signal_connect (
		shell, "prepare-for-quit",
		G_CALLBACK (mail_shell_backend_prepare_for_quit_cb),
		shell_backend);

	g_signal_connect (
		shell, "window-added",
		G_CALLBACK (mail_shell_backend_window_added_cb),
		shell_backend);

	e_mail_shell_settings_init (shell_backend);

	/* Setup preference widget factories */
	preferences_window = e_shell_get_preferences_window (shell);

	e_preferences_window_add_page (
		E_PREFERENCES_WINDOW (preferences_window),
		"mail-accounts",
		"preferences-mail-accounts",
		_("Mail Accounts"),
		"mail-account-management",
		em_account_prefs_new,
		100);

	e_preferences_window_add_page (
		E_PREFERENCES_WINDOW (preferences_window),
		"mail",
		"preferences-mail",
		_("Mail Preferences"),
		"index#mail-basic",
		em_mailer_prefs_new,
		300);

	e_preferences_window_add_page (
		E_PREFERENCES_WINDOW (preferences_window),
		"composer",
		"preferences-composer",
		_("Composer Preferences"),
		"index#mail-composing",
		em_composer_prefs_new,
		400);

	e_preferences_window_add_page (
		E_PREFERENCES_WINDOW (preferences_window),
		"system-network-proxy",
		"preferences-system-network-proxy",
		_("Network Preferences"),
		NULL,
		em_network_prefs_new,
		500);

	mail_session = e_mail_backend_get_session (E_MAIL_BACKEND (object));
	vstore = camel_session_get_service (CAMEL_SESSION (mail_session), E_MAIL_SESSION_VFOLDER_UID);
	g_object_bind_property (
		shell_settings, "mail-enable-unmatched-search-folder",
		vstore, "unmatched-enabled",
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
}

static void
mail_shell_backend_start (EShellBackend *shell_backend)
{
	EMailShellBackendPrivate *priv;
	EShell *shell;
	EShellSettings *shell_settings;
	EMailBackend *backend;
	EMailSession *session;
	EMailAccountStore *account_store;
	gboolean enable_search_folders;
	GError *error = NULL;

	priv = E_MAIL_SHELL_BACKEND_GET_PRIVATE (shell_backend);

	shell = e_shell_backend_get_shell (shell_backend);
	shell_settings = e_shell_get_shell_settings (shell);

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);
	account_store = e_mail_ui_session_get_account_store (E_MAIL_UI_SESSION (session));

	enable_search_folders = e_shell_settings_get_boolean (
		shell_settings, "mail-enable-search-folders");
	if (enable_search_folders)
		vfolder_load_storage (session);

	if (!e_mail_account_store_load_sort_order (account_store, &error)) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}

	mail_autoreceive_init (session);

	if (g_getenv ("CAMEL_FLUSH_CHANGES") != NULL)
		priv->mail_sync_source_id = g_timeout_add_seconds (
			mail_config_get_sync_timeout (),
			(GSourceFunc) mail_shell_backend_mail_sync,
			shell_backend);
}

static gboolean
mail_shell_backend_delete_junk_policy_decision (EMailBackend *backend)
{
	EShell *shell;
	EShellSettings *shell_settings;
	GSettings *settings;
	gboolean delete_junk;
	gint empty_date;
	gint empty_days;
	gint now;

	shell = e_shell_backend_get_shell (E_SHELL_BACKEND (backend));

	settings = g_settings_new ("org.gnome.evolution.mail");
	shell_settings = e_shell_get_shell_settings (shell);

	now = time (NULL) / 60 / 60 / 24;

	delete_junk = e_shell_settings_get_boolean (
		shell_settings, "mail-empty-junk-on-exit");

	/* XXX No EShellSettings properties for these keys. */

	empty_date = empty_days = 0;

	if (delete_junk) {
		empty_days = g_settings_get_int (settings, "junk-empty-on-exit-days");
		empty_date = g_settings_get_int (settings, "junk-empty-date");
	}

	delete_junk &= (empty_days == 0) || (empty_date + empty_days <= now);

	if (delete_junk) {
		g_settings_set_int (settings, "junk-empty-date", now);
	}

	g_object_unref (settings);

	return delete_junk;
}

static gboolean
mail_shell_backend_empty_trash_policy_decision (EMailBackend *backend)
{
	EShell *shell;
	EShellSettings *shell_settings;
	GSettings *settings;
	gboolean empty_trash;
	gint empty_date;
	gint empty_days;
	gint now;

	shell = e_shell_backend_get_shell (E_SHELL_BACKEND (backend));

	settings = g_settings_new ("org.gnome.evolution.mail");
	shell_settings = e_shell_get_shell_settings (shell);

	now = time (NULL) / 60 / 60 / 24;

	empty_trash = e_shell_settings_get_boolean (
		shell_settings, "mail-empty-trash-on-exit");

	/* XXX No EShellSettings properties for these keys. */

	empty_date = empty_days = 0;

	if (empty_trash) {
		empty_days = g_settings_get_int (settings, "trash-empty-on-exit-days");
		empty_date = g_settings_get_int (settings, "trash-empty-date");
	}

	empty_trash &= (empty_days == 0) || (empty_date + empty_days <= now);

	if (empty_trash) {
		g_settings_set_int (settings, "trash-empty-date", now);
	}

	g_object_unref (settings);

	return empty_trash;
}

static void
mail_shell_backend_dispose (GObject *object)
{
	EMailShellBackendPrivate *priv;

	priv = E_MAIL_SHELL_BACKEND (object)->priv;

	if (priv->assistant != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (priv->assistant), &priv->assistant);
		priv->assistant = NULL;
	}

	if (priv->editor != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (priv->editor), &priv->editor);
		priv->editor = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_shell_backend_parent_class)->dispose (object);
}

static void
e_mail_shell_backend_class_init (EMailShellBackendClass *class)
{
	GObjectClass *object_class;
	EShellBackendClass *shell_backend_class;
	EMailBackendClass *mail_backend_class;

	g_type_class_add_private (class, sizeof (EMailShellBackendPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = mail_shell_backend_constructed;
	object_class->dispose = mail_shell_backend_dispose;

	shell_backend_class = E_SHELL_BACKEND_CLASS (class);
	shell_backend_class->shell_view_type = E_TYPE_MAIL_SHELL_VIEW;
	shell_backend_class->name = BACKEND_NAME;
	shell_backend_class->aliases = "";
	shell_backend_class->schemes = "mailto:email";
	shell_backend_class->sort_order = 200;
	shell_backend_class->preferences_page = "mail-accounts";
	shell_backend_class->start = mail_shell_backend_start;

	mail_backend_class = E_MAIL_BACKEND_CLASS (class);
	mail_backend_class->delete_junk_policy_decision =
		mail_shell_backend_delete_junk_policy_decision;
	mail_backend_class->empty_trash_policy_decision =
		mail_shell_backend_empty_trash_policy_decision;
}

static void
e_mail_shell_backend_class_finalize (EMailShellBackendClass *class)
{
}

static void
e_mail_shell_backend_init (EMailShellBackend *mail_shell_backend)
{
	mail_shell_backend->priv =
		E_MAIL_SHELL_BACKEND_GET_PRIVATE (mail_shell_backend);
}

void
e_mail_shell_backend_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_mail_shell_backend_register_type (type_module);
}

void
e_mail_shell_backend_new_account (EMailShellBackend *mail_shell_backend,
                                  GtkWindow *parent)
{
#ifdef WITH_CAPPLET
	EShell *shell;
	EShellBackend *shell_backend;
#endif /* WITH_CAPPLET */
	EMailShellBackendPrivate *priv;

	g_return_if_fail (mail_shell_backend != NULL);
	g_return_if_fail (E_IS_MAIL_SHELL_BACKEND (mail_shell_backend));

	priv = mail_shell_backend->priv;

	if (priv->assistant != NULL) {
		gtk_window_present (GTK_WINDOW (priv->assistant));
		return;
	}

#ifdef WITH_CAPPLET
	shell_backend = E_SHELL_BACKEND (mail_shell_backend);
	shell = e_shell_backend_get_shell (shell_backend);

	if (e_shell_get_express_mode (shell))
		priv->assistant = mail_capplet_shell_new (0, TRUE, FALSE);
#endif /* WITH_CAPPLET */

	if (priv->assistant == NULL) {
		EMAccountEditor *emae;

		/** @HookPoint-EMConfig: New Mail Account Assistant
		 * @Id: org.gnome.evolution.mail.config.accountAssistant
		 * @Type: E_CONFIG_ASSISTANT
		 * @Class: org.gnome.evolution.mail.config:1.0
		 * @Target: EMConfigTargetAccount
		 *
		 * The new mail account assistant.
		 */
		emae = em_account_editor_new (
			NULL, EMAE_ASSISTANT, E_MAIL_BACKEND (mail_shell_backend),
			"org.gnome.evolution.mail.config.accountAssistant");
		e_config_create_window (
			E_CONFIG (emae->config), NULL,
			_("Evolution Account Assistant"));
		priv->assistant = E_CONFIG (emae->config)->window;
		g_object_set_data_full (
			G_OBJECT (priv->assistant), "AccountEditor",
			emae, (GDestroyNotify) g_object_unref);
	}

	g_object_add_weak_pointer (
		G_OBJECT (priv->assistant), &priv->assistant);
	gtk_window_set_transient_for (GTK_WINDOW (priv->assistant), parent);
	gtk_widget_show (priv->assistant);
}

void
e_mail_shell_backend_edit_account (EMailShellBackend *mail_shell_backend,
                                   GtkWindow *parent,
                                   EAccount *account)
{
	EMailShellBackendPrivate *priv;
	EMAccountEditor *emae;

	g_return_if_fail (mail_shell_backend != NULL);
	g_return_if_fail (E_IS_MAIL_SHELL_BACKEND (mail_shell_backend));
	g_return_if_fail (account != NULL);

	priv = mail_shell_backend->priv;

	if (priv->editor != NULL) {
		gtk_window_present (GTK_WINDOW (priv->editor));
		return;
	}

	/** @HookPoint-EMConfig: Mail Account Editor
	 * @Id: org.gnome.evolution.mail.config.accountEditor
	 * @Type: E_CONFIG_BOOK
	 * @Class: org.gnome.evolution.mail.config:1.0
	 * @Target: EMConfigTargetAccount
	 *
	 * The account editor window.
	 */
	emae = em_account_editor_new (
		account, EMAE_NOTEBOOK, E_MAIL_BACKEND (mail_shell_backend),
		"org.gnome.evolution.mail.config.accountEditor");
	e_config_create_window (
		E_CONFIG (emae->config), parent, _("Account Editor"));
	priv->editor = E_CONFIG (emae->config)->window;
	g_object_set_data_full (
		G_OBJECT (priv->editor), "AccountEditor",
		emae, (GDestroyNotify) g_object_unref);

	g_object_add_weak_pointer (G_OBJECT (priv->editor), &priv->editor);
	gtk_widget_show (priv->editor);
}

/******************* Code below here belongs elsewhere. *******************/

#include "filter/e-filter-option.h"
#include "shell/e-shell-settings.h"

GSList *
e_mail_labels_get_filter_options (void)
{
	EShell *shell;
	EShellBackend *shell_backend;
	EMailBackend *backend;
	EMailSession *session;
	EMailLabelListStore *label_store;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GSList *list = NULL;
	gboolean valid;

	shell = e_shell_get_default ();
	shell_backend = e_shell_get_backend_by_name (shell, "mail");

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);
	label_store = e_mail_ui_session_get_label_store (
		E_MAIL_UI_SESSION (session));

	model = GTK_TREE_MODEL (label_store);
	valid = gtk_tree_model_get_iter_first (model, &iter);

	while (valid) {
		struct _filter_option *option;
		gchar *name, *tag;

		name = e_mail_label_list_store_get_name (label_store, &iter);
		tag = e_mail_label_list_store_get_tag (label_store, &iter);

		if (g_str_has_prefix (tag, "$Label")) {
			gchar *tmp = tag;

			tag = g_strdup (tag + 6);

			g_free (tmp);
		}

		option = g_new0 (struct _filter_option, 1);
		option->title = e_str_without_underscores (name);
		option->value = tag;  /* takes ownership */
		list = g_slist_prepend (list, option);

		g_free (name);

		valid = gtk_tree_model_iter_next (model, &iter);
	}

	return g_slist_reverse (list);
}

static void
message_parsed_cb (GObject *source_object,
                   GAsyncResult *res,
                   gpointer user_data)
{
	EMFormatHTML *formatter = EM_FORMAT_HTML (source_object);
	GObject *preview = user_data;
	EMailDisplay *display;

	display = g_object_get_data (preview, "mbox-imp-display");
	e_mail_display_set_formatter (display, formatter);
	e_mail_display_load (display, EM_FORMAT (formatter)->uri_base);
}

/* utility functions for mbox importer */
static void
mbox_create_preview_cb (GObject *preview,
                        GtkWidget **preview_widget)
{
	EMailDisplay *display;

	g_return_if_fail (preview != NULL);
	g_return_if_fail (preview_widget != NULL);

	display = g_object_new (E_TYPE_MAIL_DISPLAY, NULL);
	g_object_set_data_full (preview, "mbox-imp-display",
				g_object_ref (display), g_object_unref);

	*preview_widget = GTK_WIDGET (display);
}

static void
mbox_fill_preview_cb (GObject *preview,
                      CamelMimeMessage *msg)
{
	EMailDisplay *display;
	EMFormat *formatter;
	GHashTable *formatters;
	SoupSession *soup_session;
	EMailSession *mail_session;
	gchar *mail_uri;

	g_return_if_fail (preview != NULL);
	g_return_if_fail (msg != NULL);

	display = g_object_get_data (preview, "mbox-imp-display");
	g_return_if_fail (display != NULL);

	soup_session = webkit_get_default_session ();
	formatters = g_object_get_data (G_OBJECT (soup_session), "formatters");
	if (!formatters) {
		formatters = g_hash_table_new_full (g_str_hash, g_str_equal,
			(GDestroyNotify) g_free, NULL);
		g_object_set_data (
			G_OBJECT (soup_session), "formatters", formatters);
	}

	mail_uri = em_format_build_mail_uri (NULL, msg->message_id, NULL, NULL);

	mail_session = e_mail_session_new ();

	formatter = EM_FORMAT (
		em_format_html_display_new (
		CAMEL_SESSION (mail_session)));
	formatter->message_uid = g_strdup (msg->message_id);
	formatter->uri_base = g_strdup (mail_uri);

        /* Don't free the mail_uri!! */
	g_hash_table_insert (formatters, mail_uri, formatter);

	em_format_parse_async (
		formatter, msg, NULL, NULL,
		message_parsed_cb, preview);

	g_object_unref (mail_session);
}
