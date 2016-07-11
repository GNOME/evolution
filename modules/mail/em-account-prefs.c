/*
 * em-account-prefs.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/* XXX EMailAccountManager handles all the user interface stuff.
 *     This subclass applies policies using mailer resources that
 *     EMailAccountManager does not have access to.  The desire is
 *     to someday move account management completely out of the mailer,
 *     perhaps to evolution-data-server. */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "em-account-prefs.h"
#include "e-mail-shell-backend.h"

#include <glib/gi18n.h>

#include <shell/e-shell.h>

#include <mail/e-mail-backend.h>
#include <mail/e-mail-ui-session.h>
#include <mail/em-config.h>
#include <mail/em-utils.h>
#include <mail/mail-vfolder-ui.h>

#define EM_ACCOUNT_PREFS_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), EM_TYPE_ACCOUNT_PREFS, EMAccountPrefsPrivate))

#define EM_ACCOUNT_PREFS_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), EM_TYPE_ACCOUNT_PREFS, EMAccountPrefsPrivate))

struct _EMAccountPrefsPrivate {
	EMailBackend *backend;
};

enum {
	PROP_0,
	PROP_BACKEND
};

G_DEFINE_DYNAMIC_TYPE (
	EMAccountPrefs,
	em_account_prefs,
	E_TYPE_MAIL_ACCOUNT_MANAGER)

static void
account_prefs_service_enabled_cb (EMailAccountStore *store,
                                  CamelService *service,
                                  EMAccountPrefs *prefs)
{
	EMailBackend *backend;
	const gchar *uid;
	EMailSession *session;

	uid = camel_service_get_uid (service);
	backend = em_account_prefs_get_backend (prefs);
	session = e_mail_backend_get_session (backend);

	/* FIXME Kind of a gross hack.  EMailSession doesn't have
	 *       access to EMailBackend so it can't do this itself. */
	if (g_strcmp0 (uid, E_MAIL_SESSION_VFOLDER_UID) == 0)
		vfolder_load_storage (session);
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

	priv = EM_ACCOUNT_PREFS_GET_PRIVATE (object);

	if (priv->backend != NULL) {
		g_object_unref (priv->backend);
		priv->backend = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (em_account_prefs_parent_class)->dispose (object);
}

static void
account_prefs_constructed (GObject *object)
{
	EMailAccountManager *manager;
	EMailAccountStore *store;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (em_account_prefs_parent_class)->constructed (object);

	manager = E_MAIL_ACCOUNT_MANAGER (object);
	store = e_mail_account_manager_get_store (manager);

	g_signal_connect (
		store, "service-enabled",
		G_CALLBACK (account_prefs_service_enabled_cb), manager);
}

static void
account_prefs_add_account (EMailAccountManager *manager)
{
	EMAccountPrefsPrivate *priv;
	gpointer parent;

	priv = EM_ACCOUNT_PREFS_GET_PRIVATE (manager);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (manager));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	e_mail_shell_backend_new_account (
		E_MAIL_SHELL_BACKEND (priv->backend), parent);
}

static void
account_prefs_edit_account (EMailAccountManager *manager,
                            ESource *source)
{
	EMAccountPrefsPrivate *priv;
	gpointer parent;

	priv = EM_ACCOUNT_PREFS_GET_PRIVATE (manager);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (manager));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	e_mail_shell_backend_edit_account (
		E_MAIL_SHELL_BACKEND (priv->backend), parent, source);
}

static void
em_account_prefs_class_init (EMAccountPrefsClass *class)
{
	GObjectClass *object_class;
	EMailAccountManagerClass *account_manager_class;

	g_type_class_add_private (class, sizeof (EMAccountPrefsPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = account_prefs_set_property;
	object_class->get_property = account_prefs_get_property;
	object_class->dispose = account_prefs_dispose;
	object_class->constructed = account_prefs_constructed;

	account_manager_class = E_MAIL_ACCOUNT_MANAGER_CLASS (class);
	account_manager_class->add_account = account_prefs_add_account;
	account_manager_class->edit_account = account_prefs_edit_account;

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
em_account_prefs_class_finalize (EMAccountPrefsClass *class)
{
}

static void
em_account_prefs_init (EMAccountPrefs *prefs)
{
	prefs->priv = EM_ACCOUNT_PREFS_GET_PRIVATE (prefs);
}

void
em_account_prefs_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	em_account_prefs_register_type (type_module);
}

GtkWidget *
em_account_prefs_new (EPreferencesWindow *window)
{
	EShell *shell;
	EShellBackend *shell_backend;
	EMailAccountStore *account_store;
	EMailBackend *backend;
	EMailSession *session;

	/* XXX Figure out a better way to get the mail backend. */
	shell = e_preferences_window_get_shell (window);
	shell_backend = e_shell_get_backend_by_name (shell, "mail");

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);
	account_store = e_mail_ui_session_get_account_store (
		E_MAIL_UI_SESSION (session));

	return g_object_new (
		EM_TYPE_ACCOUNT_PREFS,
		"store", account_store,
		"backend", backend, NULL);
}

EMailBackend *
em_account_prefs_get_backend (EMAccountPrefs *prefs)
{
	g_return_val_if_fail (EM_IS_ACCOUNT_PREFS (prefs), NULL);

	return prefs->priv->backend;
}
