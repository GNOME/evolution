/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-FileCopyrightText: (C) 2008 - Diego Escalante Urrelo
 * SPDX-FileCopyrightText: (C) 2018 Red Hat, Inc. (www.redhat.com)
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileContributor: Diego Escalante Urrelo <diegoe@gnome.org>
 * SPDX-FileContributor: Bharath Acharya <abharath@novell.com>
 */

#ifndef E_MAIL_TEMPLATES_H
#define E_MAIL_TEMPLATES_H

#include <camel/camel.h>

G_BEGIN_DECLS

CamelMimeMessage *
		e_mail_templates_apply_sync	(CamelMimeMessage *source_message,
						 CamelFolder *source_folder,
						 const gchar *source_message_uid,
						 CamelFolder *templates_folder,
						 const gchar *templates_message_uid,
						 GCancellable *cancellable,
						 GError **error);
void		e_mail_templates_apply		(CamelMimeMessage *source_message,
						 CamelFolder *source_folder,
						 const gchar *source_message_uid,
						 CamelFolder *templates_folder,
						 const gchar *templates_message_uid,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
CamelMimeMessage *
		e_mail_templates_apply_finish	(GObject *source_object,
						 GAsyncResult *result,
						 GError **error);

G_END_DECLS

#endif /* E_MAIL_TEMPLATES_H */
