/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_MAIL_CONFIG_ASSISTANT_H
#define E_MAIL_CONFIG_ASSISTANT_H

#include <gtk/gtk.h>
#include <libemail-engine/libemail-engine.h>

#include <mail/e-mail-config-page.h>
#include <mail/e-mail-config-service-backend.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_CONFIG_ASSISTANT \
	(e_mail_config_assistant_get_type ())
#define E_MAIL_CONFIG_ASSISTANT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CONFIG_ASSISTANT, EMailConfigAssistant))
#define E_MAIL_CONFIG_ASSISTANT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_CONFIG_ASSISTANT, EMailConfigAssistantClass))
#define E_IS_MAIL_CONFIG_ASSISTANT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CONFIG_ASSISTANT))
#define E_IS_MAIL_CONFIG_ASSISTANT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_CONFIG_ASSISTANT))
#define E_MAIL_CONFIG_ASSISTANT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_CONFIG_ASSISTANT, EMailConfigAssistantClass))

G_BEGIN_DECLS

typedef struct _EMailConfigAssistant EMailConfigAssistant;
typedef struct _EMailConfigAssistantClass EMailConfigAssistantClass;
typedef struct _EMailConfigAssistantPrivate EMailConfigAssistantPrivate;

struct _EMailConfigAssistant {
	GtkAssistant parent;
	EMailConfigAssistantPrivate *priv;
};

struct _EMailConfigAssistantClass {
	GtkAssistantClass parent_class;

	/* Signals */
	void		(* new_source)	(EMailConfigAssistant *assistant,
					 const gchar *uid);
};

GType		e_mail_config_assistant_get_type
					(void) G_GNUC_CONST;
GtkWidget *	e_mail_config_assistant_new
					(EMailSession *session);
EMailSession *	e_mail_config_assistant_get_session
					(EMailConfigAssistant *assistant);
EMailConfigServiceBackend *
		e_mail_config_assistant_get_account_backend
					(EMailConfigAssistant *assistant);
ESource *	e_mail_config_assistant_get_account_source
					(EMailConfigAssistant *assistant);
ESource *	e_mail_config_assistant_get_identity_source
					(EMailConfigAssistant *assistant);
EMailConfigServiceBackend *
		e_mail_config_assistant_get_transport_backend
					(EMailConfigAssistant *assistant);
ESource *	e_mail_config_assistant_get_transport_source
					(EMailConfigAssistant *assistant);
void		e_mail_config_assistant_add_page
					(EMailConfigAssistant *assistant,
					 EMailConfigPage *page);
void		e_mail_config_assistant_commit
					(EMailConfigAssistant *assistant,
					 GCancellable *cancellable,
					 GAsyncReadyCallback callback,
					 gpointer user_data);
gboolean	e_mail_config_assistant_commit_finish
					(EMailConfigAssistant *assistant,
					 GAsyncResult *result,
					 GError **error);

G_END_DECLS

#endif /* E_MAIL_CONFIG_ASSISTANT_H */

