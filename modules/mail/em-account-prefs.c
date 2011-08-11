/*
 * em-account-prefs.c
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

/* XXX EAccountManager handles all the user interface stuff.  This subclass
 *     applies policies using mailer resources that EAccountManager does not
 *     have access to.  The desire is to someday move account management
 *     completely out of the mailer, perhaps to evolution-data-server. */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "em-account-prefs.h"

#include <glib/gi18n.h>

#include "e-util/e-alert-dialog.h"
#include "e-util/e-account-utils.h"

#include "e-mail-backend.h"
#include "e-mail-local.h"
#include "e-mail-store.h"
#include "em-config.h"
#include "em-account-editor.h"
#include "shell/e-shell.h"
#include "capplet/settings/mail-capplet-shell.h"

struct _EMAccountPrefsPrivate {
	EMailBackend *backend;
	gpointer assistant; /* weak pointer */
	gpointer editor;    /* weak pointer */
};

enum {
	PROP_0,
	PROP_BACKEND
};

G_DEFINE_TYPE (
	EMAccountPrefs,
	em_account_prefs,
	E_TYPE_ACCOUNT_MANAGER)

static void
account_prefs_enable_account_cb (EAccountTreeView *tree_view,
                                 EMAccountPrefs *prefs)
{
	EAccount *account;
	EMailSession *session;

	account = e_account_tree_view_get_selected (tree_view);
	g_return_if_fail (account != NULL);

	session = e_mail_backend_get_session (prefs->priv->backend);
	e_mail_store_add_by_account (session, account);
}

static void
account_prefs_disable_account_cb (EAccountTreeView *tree_view,
                                  EMAccountPrefs *prefs)
{
	EAccountList *account_list;
	EMailSession *session;
	EAccount *account;
	gpointer parent;
	gint response;

	account = e_account_tree_view_get_selected (tree_view);
	g_return_if_fail (account != NULL);

	account_list = e_account_tree_view_get_account_list (tree_view);
	g_return_if_fail (account_list != NULL);

	session = e_mail_backend_get_session (prefs->priv->backend);

	if (!e_account_list_account_has_proxies (account_list, account)) {
		e_mail_store_remove_by_account (session, account);
		return;
	}

	parent = gtk_widget_get_toplevel (GTK_WIDGET (tree_view));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	response = e_alert_run_dialog_for_args (
		parent, "mail:ask-delete-proxy-accounts", NULL);

	if (response != GTK_RESPONSE_YES) {
		g_signal_stop_emission_by_name (tree_view, "disable-account");
		return;
	}

	e_account_list_remove_account_proxies (account_list, account);

	e_mail_store_remove_by_account (session, account);
}

static void
account_prefs_set_backend (EMAccountPrefs *prefs,
                           EMailBackend *backend)
{
	g_return_if_fail (E_IS_MAIL_BACKEND (backend));
	g_return_if_fail (prefs->priv->backend == NULL);

	prefs->priv->backend = g_object_ref (backend);
}

static void
account_prefs_set_property (GObject *object,
                            guint property_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_BACKEND:
			account_prefs_set_backend (
				EM_ACCOUNT_PREFS (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
account_prefs_get_property (GObject *object,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_BACKEND:
			g_value_set_object (
				value,
				em_account_prefs_get_backend (
				EM_ACCOUNT_PREFS (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
account_prefs_dispose (GObject *object)
{
	EMAccountPrefsPrivate *priv;

	priv = EM_ACCOUNT_PREFS (object)->priv;

	if (priv->backend != NULL) {
		g_object_unref (priv->backend);
		priv->backend = NULL;
	}

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
	G_OBJECT_CLASS (em_account_prefs_parent_class)->dispose (object);
}

static void
account_prefs_add_account (EAccountManager *manager)
{
	EMAccountPrefsPrivate *priv;
	EMAccountEditor *emae;
	gpointer parent;

	priv = EM_ACCOUNT_PREFS (manager)->priv;

	if (priv->assistant != NULL) {
		gtk_window_present (GTK_WINDOW (priv->assistant));
		return;
	}

	parent = gtk_widget_get_toplevel (GTK_WIDGET (manager));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	if (!e_shell_get_express_mode (e_shell_get_default ())) {
		/** @HookPoint-EMConfig: New Mail Account Assistant
		 * @Id: org.gnome.evolution.mail.config.accountAssistant
		 * @Type: E_CONFIG_ASSISTANT
		 * @Class: org.gnome.evolution.mail.config:1.0
		 * @Target: EMConfigTargetAccount
		 *
		 * The new mail account assistant.
		 */
		emae = em_account_editor_new (
			NULL, EMAE_ASSISTANT, priv->backend,
			"org.gnome.evolution.mail.config.accountAssistant");
		e_config_create_window (
			E_CONFIG (emae->config), NULL,
			_("Evolution Account Assistant"));
		priv->assistant = E_CONFIG (emae->config)->window;
		g_object_set_data_full (
			G_OBJECT (priv->assistant), "AccountEditor",
			emae, (GDestroyNotify) g_object_unref);
	} else {
		priv->assistant = mail_capplet_shell_new (0, TRUE, FALSE);
	}

	g_object_add_weak_pointer (G_OBJECT (priv->assistant), &priv->assistant);
	gtk_window_set_transient_for (GTK_WINDOW (priv->assistant), parent);
	gtk_widget_show (priv->assistant);
}

static void
account_prefs_edit_account (EAccountManager *manager)
{
	EMAccountPrefsPrivate *priv;
	EMAccountEditor *emae;
	EAccountTreeView *tree_view;
	EAccount *account;
	gpointer parent;

	priv = EM_ACCOUNT_PREFS (manager)->priv;

	if (priv->editor != NULL) {
		gtk_window_present (GTK_WINDOW (priv->editor));
		return;
	}

	tree_view = e_account_manager_get_tree_view (manager);
	account = e_account_tree_view_get_selected (tree_view);
	g_return_if_fail (account != NULL);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (manager));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	/** @HookPoint-EMConfig: Mail Account Editor
	 * @Id: org.gnome.evolution.mail.config.accountEditor
	 * @Type: E_CONFIG_BOOK
	 * @Class: org.gnome.evolution.mail.config:1.0
	 * @Target: EMConfigTargetAccount
	 *
	 * The account editor window.
	 */
	emae = em_account_editor_new (
		account, EMAE_NOTEBOOK, priv->backend,
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

static void
account_prefs_delete_account (EAccountManager *manager)
{
	EMAccountPrefsPrivate *priv;
	EAccountTreeView *tree_view;
	EAccountList *account_list;
	EMailSession *session;
	EAccount *account;
	gboolean has_proxies;
	gpointer parent;
	gint response;

	priv = EM_ACCOUNT_PREFS (manager)->priv;
	session = e_mail_backend_get_session (priv->backend);

	account_list = e_account_manager_get_account_list (manager);
	tree_view = e_account_manager_get_tree_view (manager);
	account = e_account_tree_view_get_selected (tree_view);
	g_return_if_fail (account != NULL);

	/* Make sure we aren't editing anything... */
	if (priv->editor != NULL)
		return;

	parent = gtk_widget_get_toplevel (GTK_WIDGET (manager));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	has_proxies =
		e_account_list_account_has_proxies (account_list, account);

	response = e_alert_run_dialog_for_args (
		parent, has_proxies ?
		"mail:ask-delete-account-with-proxies" :
		"mail:ask-delete-account", NULL);

	if (response != GTK_RESPONSE_YES) {
		g_signal_stop_emission_by_name (manager, "delete-account");
		return;
	}

	/* Remove the account from the folder tree. */
	if (account->enabled)
		e_mail_store_remove_by_account (session, account);

	/* Remove all the proxies the account has created. */
	if (has_proxies)
		e_account_list_remove_account_proxies (account_list, account);

	/* Remove it from the config file. */
	e_account_list_remove (account_list, account);

	e_account_list_save (account_list);
}

static void
em_account_prefs_class_init (EMAccountPrefsClass *class)
{
	GObjectClass *object_class;
	EAccountManagerClass *account_manager_class;

	g_type_class_add_private (class, sizeof (EMAccountPrefsPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = account_prefs_set_property;
	object_class->get_property = account_prefs_get_property;
	object_class->dispose = account_prefs_dispose;

	account_manager_class = E_ACCOUNT_MANAGER_CLASS (class);
	account_manager_class->add_account = account_prefs_add_account;
	account_manager_class->edit_account = account_prefs_edit_account;
	account_manager_class->delete_account = account_prefs_delete_account;

	g_object_class_install_property (
		object_class,
		PROP_BACKEND,
		g_param_spec_object (
			"backend",
			NULL,
			NULL,
			E_TYPE_MAIL_BACKEND,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
em_account_prefs_init (EMAccountPrefs *prefs)
{
	EAccountManager *manager;
	EAccountTreeView *tree_view;

	prefs->priv = G_TYPE_INSTANCE_GET_PRIVATE (
		prefs, EM_TYPE_ACCOUNT_PREFS, EMAccountPrefsPrivate);

	manager = E_ACCOUNT_MANAGER (prefs);
	tree_view = e_account_manager_get_tree_view (manager);

	g_signal_connect (
		tree_view, "enable-account",
		G_CALLBACK (account_prefs_enable_account_cb), prefs);

	g_signal_connect (
		tree_view, "disable-account",
		G_CALLBACK (account_prefs_disable_account_cb), prefs);
}

GtkWidget *
em_account_prefs_new (EPreferencesWindow *window)
{
	EShell *shell;
	EShellBackend *shell_backend;
	EAccountList *account_list;

	account_list = e_get_account_list ();
	g_return_val_if_fail (E_IS_ACCOUNT_LIST (account_list), NULL);

	/* XXX Figure out a better way to get the mail backend. */
	shell = e_preferences_window_get_shell (window);
	shell_backend = e_shell_get_backend_by_name (shell, "mail");

	/* make sure the e-mail-local is initialized */
	e_mail_local_init (e_mail_backend_get_session (E_MAIL_BACKEND (shell_backend)), e_shell_backend_get_data_dir (shell_backend));

	return g_object_new (
		EM_TYPE_ACCOUNT_PREFS,
		"account-list", account_list,
		"backend", shell_backend, NULL);
}

EMailBackend *
em_account_prefs_get_backend (EMAccountPrefs *prefs)
{
	g_return_val_if_fail (EM_IS_ACCOUNT_PREFS (prefs), NULL);

	return prefs->priv->backend;
}
