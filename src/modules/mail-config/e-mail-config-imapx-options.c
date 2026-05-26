/*
 * SPDX-FileCopyrightText: (C) 2016 Red Hat, Inc. (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>
#include <libebackend/libebackend.h>
#include <libedataserver/libedataserver.h>

#include "e-util/e-util.h"
#include "shell/e-shell.h"
#include "mail/e-mail-backend.h"
#include "mail/e-mail-config-provider-page.h"
#include "mail/em-subscription-editor.h"

#include "e-mail-config-imapx-options.h"

/* Standard GObject macros */
#define E_TYPE_MAIL_CONFIG_IMAPX_OPTIONS \
	(e_mail_config_imapx_options_get_type ())
#define E_MAIL_CONFIG_IMAPX_OPTIONS(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CONFIG_IMAPX_OPTIONS, EMailConfigIMAPxOptions))
#define E_MAIL_CONFIG_IMAPX_OPTIONS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_CONFIG_IMAPX_OPTIONS, EMailConfigIMAPxOptionsClass))
#define E_IS_MAIL_CONFIG_IMAPX_OPTIONS(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CONFIG_IMAPX_OPTIONS))
#define E_IS_MAIL_CONFIG_IMAPX_OPTIONS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_CONFIG_IMAPX_OPTIONS))
#define E_MAIL_CONFIG_IMAPX_OPTIONS_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_CONFIG_IMAPX_OPTIONS, EMailConfigIMAPxOptionsClass))

typedef struct _EMailConfigIMAPxOptions EMailConfigIMAPxOptions;
typedef struct _EMailConfigIMAPxOptionsClass EMailConfigIMAPxOptionsClass;

struct _EMailConfigIMAPxOptions {
	EExtension parent;
};

struct _EMailConfigIMAPxOptionsClass {
	EExtensionClass parent_class;
};

GType		e_mail_config_imapx_options_get_type
						(void) G_GNUC_CONST;

G_DEFINE_DYNAMIC_TYPE (EMailConfigIMAPxOptions, e_mail_config_imapx_options, E_TYPE_EXTENSION)

static void
manage_subscriptions_clicked_cb (GtkButton *button,
                                 gpointer user_data)
{
	CamelStore *store = CAMEL_STORE (user_data);
	EShell *shell;
	EShellBackend *shell_backend;
	EMailSession *session;
	GtkWidget *dialog;
	GtkWidget *toplevel;

	shell = e_shell_get_default ();
	shell_backend = e_shell_get_backend_by_name (shell, "mail");
	session = e_mail_backend_get_session (E_MAIL_BACKEND (shell_backend));

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (button));

	dialog = em_subscription_editor_new (GTK_WINDOW (toplevel), session, store);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static void
mail_config_imapx_options_constructed (GObject *object)
{
	EMailConfigProviderPage *provider_page;
	EMailConfigServiceBackend *backend;
	CamelProvider *provider;
	CamelSettings *settings;
	GtkBox *placeholder;
	GtkWidget *hbox;
	GtkWidget *button;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_config_imapx_options_parent_class)->constructed (object);

	provider_page = E_MAIL_CONFIG_PROVIDER_PAGE (e_extension_get_extensible (E_EXTENSION (object)));
	backend = e_mail_config_provider_page_get_backend (provider_page);
	provider = e_mail_config_service_backend_get_provider (backend);
	settings = e_mail_config_service_backend_get_settings (backend);

	if (e_mail_config_provider_page_is_empty (provider_page) ||
	    !provider || g_strcmp0 (provider->protocol, "imapx") != 0)
		return;

	g_return_if_fail (CAMEL_IS_OFFLINE_SETTINGS (settings));

	placeholder = e_mail_config_provider_page_get_placeholder (provider_page, "imapx-subscriptions-placeholder");
	if (placeholder != NULL) {
		ESource *source = e_mail_config_service_backend_get_source (backend);
		EShell *shell;
		EShellBackend *shell_backend;
		EMailSession *session;
		CamelService *service;

		shell = e_shell_get_default ();
		shell_backend = e_shell_get_backend_by_name (shell, "mail");
		session = e_mail_backend_get_session (E_MAIL_BACKEND (shell_backend));

		service = camel_session_ref_service (CAMEL_SESSION (session), e_source_get_uid (source));

		button = gtk_button_new_with_mnemonic (_("Manage _Subscriptions…"));
		g_object_set (G_OBJECT (button),
			"margin-start", 20,
			"sensitive", service != NULL && CAMEL_IS_STORE (service),
			"visible", TRUE,
			NULL);
		gtk_box_pack_start (placeholder, button, FALSE, FALSE, 0);

		if (service != NULL && CAMEL_IS_STORE (service)) {
			g_signal_connect_data (button, "clicked",
				G_CALLBACK (manage_subscriptions_clicked_cb),
				g_steal_pointer (&service),
				(GClosureNotify) g_object_unref, 0);
		}

		g_clear_object (&service);
	}

	placeholder = e_mail_config_provider_page_get_placeholder (provider_page, "imapx-limit-by-age-placeholder");
	g_return_if_fail (placeholder != NULL);

	hbox = e_dialog_offline_settings_new_limit_box (CAMEL_OFFLINE_SETTINGS (settings));
	gtk_box_pack_start (placeholder, hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);
}

static void
e_mail_config_imapx_options_class_init (EMailConfigIMAPxOptionsClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = mail_config_imapx_options_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_MAIL_CONFIG_PROVIDER_PAGE;
}

static void
e_mail_config_imapx_options_class_finalize (EMailConfigIMAPxOptionsClass *class)
{
}

static void
e_mail_config_imapx_options_init (EMailConfigIMAPxOptions *extension)
{
}

void
e_mail_config_imapx_options_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_mail_config_imapx_options_register_type (type_module);
}
