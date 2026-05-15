/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_MAIL_CONFIG_SMTP_BACKEND_H
#define E_MAIL_CONFIG_SMTP_BACKEND_H

#include <mail/e-mail-config-service-backend.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_CONFIG_SMTP_BACKEND \
	(e_mail_config_smtp_backend_get_type ())
#define E_MAIL_CONFIG_SMTP_BACKEND(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CONFIG_SMTP_BACKEND, EMailConfigSmtpBackend))
#define E_MAIL_CONFIG_SMTP_BACKEND_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_CONFIG_SMTP_BACKEND, EMailConfigSmtpBackendClass))
#define E_IS_MAIL_CONFIG_SMTP_BACKEND(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CONFIG_SMTP_BACKEND))
#define E_IS_MAIL_CONFIG_SMTP_BACKEND_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_CONFIG_SMTP_BACKEND))
#define E_MAIL_CONFIG_SMTP_BACKEND_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_CONFIG_SMTP_BACKEND, EMailConfigSmtpBackendClass))

G_BEGIN_DECLS

typedef struct _EMailConfigSmtpBackend EMailConfigSmtpBackend;
typedef struct _EMailConfigSmtpBackendClass EMailConfigSmtpBackendClass;
typedef struct _EMailConfigSmtpBackendPrivate EMailConfigSmtpBackendPrivate;

struct _EMailConfigSmtpBackend {
	EMailConfigServiceBackend parent;
	EMailConfigSmtpBackendPrivate *priv;
};

struct _EMailConfigSmtpBackendClass {
	EMailConfigServiceBackendClass parent_class;
};

GType		e_mail_config_smtp_backend_get_type
						(void) G_GNUC_CONST;
void		e_mail_config_smtp_backend_type_register
						(GTypeModule *type_module);

G_END_DECLS

#endif /* E_MAIL_CONFIG_SMTP_BACKEND_H */

