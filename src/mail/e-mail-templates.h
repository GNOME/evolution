/*
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 2008 - Diego Escalante Urrelo
 * Copyright (C) 2018 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *		Diego Escalante Urrelo <diegoe@gnome.org>
 *		Bharath Acharya <abharath@novell.com>
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
