/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_MAIL_CONFIG_SENDMAIL_BACKEND_H
#define E_MAIL_CONFIG_SENDMAIL_BACKEND_H

#include <mail/e-mail-config-service-backend.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_CONFIG_SENDMAIL_BACKEND \
	(e_mail_config_sendmail_backend_get_type ())
#define E_MAIL_CONFIG_SENDMAIL_BACKEND(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CONFIG_SENDMAIL_BACKEND, EMailConfigSendmailBackend))
#define E_MAIL_CONFIG_SENDMAIL_BACKEND_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_CONFIG_SENDMAIL_BACKEND, EMailConfigSendmailBackendClass))
#define E_IS_MAIL_CONFIG_SENDMAIL_BACKEND(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CONFIG_SENDMAIL_BACKEND))
#define E_IS_MAIL_CONFIG_SENDMAIL_BACKEND_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_CONFIG_SENDMAIL_BACKEND))
#define E_MAIL_CONFIG_SENDMAIL_BACKEND_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_CONFIG_SENDMAIL_BACKEND, EMailConfigSendmailBackendClass))

G_BEGIN_DECLS

typedef struct _EMailConfigSendmailBackend EMailConfigSendmailBackend;
typedef struct _EMailConfigSendmailBackendClass EMailConfigSendmailBackendClass;
typedef struct _EMailConfigSendmailBackendPrivate EMailConfigSendmailBackendPrivate;

struct _EMailConfigSendmailBackend {
	EMailConfigServiceBackend parent;
	EMailConfigSendmailBackendPrivate *priv;
};

struct _EMailConfigSendmailBackendClass {
	EMailConfigServiceBackendClass parent_class;
};

GType		e_mail_config_sendmail_backend_get_type
						(void) G_GNUC_CONST;
void		e_mail_config_sendmail_backend_type_register
						(GTypeModule *type_module);

G_END_DECLS

#endif /* E_MAIL_CONFIG_SENDMAIL_BACKEND_H */

