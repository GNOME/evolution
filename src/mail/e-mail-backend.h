/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

/* This is an abstract EShellBackend subclass that integrates
 * with libevolution-mail.  It serves as a common base class
 * for Evolution's mail module and Anjal. */

#ifndef E_MAIL_BACKEND_H
#define E_MAIL_BACKEND_H

#include <shell/e-shell-backend.h>
#include <libemail-engine/libemail-engine.h>
#include <mail/e-mail-composer-mode-override.h>
#include <mail/e-mail-remote-content.h>
#include <mail/e-mail-send-account-override.h>
#include <mail/e-mail-properties.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_BACKEND \
	(e_mail_backend_get_type ())
#define E_MAIL_BACKEND(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_BACKEND, EMailBackend))
#define E_MAIL_BACKEND_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_BACKEND, EMailBackendClass))
#define E_IS_MAIL_BACKEND(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_BACKEND))
#define E_IS_MAIL_BACKEND_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_BACKEND))
#define E_MAIL_BACKEND_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_BACKEND, EMailBackendClass))

G_BEGIN_DECLS

typedef struct _EMailBackend EMailBackend;
typedef struct _EMailBackendClass EMailBackendClass;
typedef struct _EMailBackendPrivate EMailBackendPrivate;

struct _EMailBackend {
	EShellBackend parent;
	EMailBackendPrivate *priv;
};

struct _EMailBackendClass {
	EShellBackendClass parent_class;

	/* Methods */
	gboolean	(*delete_junk_policy_decision)
						(EMailBackend *backend);
	gboolean	(*empty_trash_policy_decision)
						(EMailBackend *backend);
};

GType		e_mail_backend_get_type		(void);
EMailSession *	e_mail_backend_get_session	(EMailBackend *backend);
EAlertSink *	e_mail_backend_get_alert_sink	(EMailBackend *backend);
gboolean	e_mail_backend_delete_junk_policy_decision
						(EMailBackend *backend);
gboolean	e_mail_backend_empty_trash_policy_decision
						(EMailBackend *backend);
EMailComposerModeOverride *
		e_mail_backend_get_composer_mode_override
						(EMailBackend *backend);
EMailSendAccountOverride *
		e_mail_backend_get_send_account_override
						(EMailBackend *backend);
EMailRemoteContent *
		e_mail_backend_get_remote_content
						(EMailBackend *backend);
EMailProperties *
		e_mail_backend_get_mail_properties
						(EMailBackend *backend);

G_END_DECLS

#endif /* E_MAIL_BACKEND_H */
