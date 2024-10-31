/*
 * Copyright (C) 2015 Red Hat Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of version 2.1. of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef E_MAIL_REMOTE_CONTENT_H
#define E_MAIL_REMOTE_CONTENT_H

#include <glib-object.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_REMOTE_CONTENT \
	(e_mail_remote_content_get_type ())
#define E_MAIL_REMOTE_CONTENT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_REMOTE_CONTENT, EMailRemoteContent))
#define E_MAIL_REMOTE_CONTENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_REMOTE_CONTENT, EMailRemoteContentClass))
#define E_IS_MAIL_REMOTE_CONTENT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_REMOTE_CONTENT))
#define E_IS_MAIL_REMOTE_CONTENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_REMOTE_CONTENT))
#define E_MAIL_REMOTE_CONTENT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_REMOTE_CONTENT, EMailRemoteContentClass))

G_BEGIN_DECLS

typedef struct _EMailRemoteContent EMailRemoteContent;
typedef struct _EMailRemoteContentClass EMailRemoteContentClass;
typedef struct _EMailRemoteContentPrivate EMailRemoteContentPrivate;

struct _EMailRemoteContent {
	GObject parent;
	EMailRemoteContentPrivate *priv;
};

struct _EMailRemoteContentClass {
	GObjectClass parent_class;
};

GType		e_mail_remote_content_get_type	(void) G_GNUC_CONST;
EMailRemoteContent *
		e_mail_remote_content_new	(const gchar *config_filename);
void		e_mail_remote_content_add_site	(EMailRemoteContent *content,
						 const gchar *site);
void		e_mail_remote_content_remove_site
						(EMailRemoteContent *content,
						 const gchar *site);
gboolean	e_mail_remote_content_has_site	(EMailRemoteContent *content,
						 const gchar *site);
GSList *	e_mail_remote_content_get_sites	(EMailRemoteContent *content);
void		e_mail_remote_content_add_mail	(EMailRemoteContent *content,
						 const gchar *mail);
void		e_mail_remote_content_remove_mail
						(EMailRemoteContent *content,
						 const gchar *mail);
gboolean	e_mail_remote_content_has_mail	(EMailRemoteContent *content,
						 const gchar *mail);
GSList *	e_mail_remote_content_get_mails	(EMailRemoteContent *content);

G_END_DECLS

#endif /* E_MAIL_REMOTE_CONTENT_H */
