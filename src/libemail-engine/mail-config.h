/*
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
 * Authors:
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
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
gint		mail_config_get_sync_timeout	(void);

void		mail_config_reload_junk_headers	(EMailSession *session);
gboolean	mail_config_get_lookup_book	(void);
gboolean	mail_config_get_lookup_book_local_only (void);
gchar *		mail_config_dup_local_archive_folder (void);

G_END_DECLS

#endif /* MAIL_CONFIG_H */
