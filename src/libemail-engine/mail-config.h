/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Jeffrey Stedfast <fejj@ximian.com>
 */

#if !defined (__LIBEMAIL_ENGINE_H_INSIDE__) && !defined (LIBEMAIL_ENGINE_COMPILATION)
#error "Only <libemail-engine/libemail-engine.h> should be included directly."
#endif

#ifndef MAIL_CONFIG_H
#define MAIL_CONFIG_H

#include <libemail-engine/e-mail-session.h>

G_BEGIN_DECLS

/* Configuration */
void		mail_config_init		(EMailSession *session);

/* General Accessor functions */

gint		mail_config_get_address_count	(void);
gboolean	mail_config_get_show_mails_in_preview
						(void);

/* static utility functions */
gchar *		mail_config_folder_to_cachename	(CamelFolder *folder,
						 const gchar *prefix);
gchar *		mail_config_folder_uri_to_cachename
						(const gchar *folder_uri,
						 const gchar *prefix);
gchar *		mail_config_folder_uri_to_view_id
						(const gchar *folder_uri);
gint		mail_config_get_sync_timeout	(void);

void		mail_config_reload_junk_headers	(EMailSession *session);
gboolean	mail_config_get_lookup_book	(void);
gboolean	mail_config_get_lookup_book_local_only (void);
gchar *		mail_config_dup_local_archive_folder (void);

G_END_DECLS

#endif /* MAIL_CONFIG_H */
