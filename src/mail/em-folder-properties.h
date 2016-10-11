/*
 *
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
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __EM_FOLDER_PROPERTIES_H__
#define __EM_FOLDER_PROPERTIES_H__

#include <camel/camel.h>
#include <e-util/e-util.h>
#include <mail/e-mail-backend.h>

G_BEGIN_DECLS

void		em_folder_properties_show	(CamelStore *store,
						 const gchar *folder_name,
						 EAlertSink *alert_sink,
						 GtkWindow *parent_window);

typedef enum {
	E_AUTO_ARCHIVE_CONFIG_UNKNOWN,
	E_AUTO_ARCHIVE_CONFIG_MOVE_TO_ARCHIVE,
	E_AUTO_ARCHIVE_CONFIG_MOVE_TO_CUSTOM,
	E_AUTO_ARCHIVE_CONFIG_DELETE
} EAutoArchiveConfig;

typedef enum {
	E_AUTO_ARCHIVE_UNIT_UNKNOWN,
	E_AUTO_ARCHIVE_UNIT_DAYS,
	E_AUTO_ARCHIVE_UNIT_WEEKS,
	E_AUTO_ARCHIVE_UNIT_MONTHS
} EAutoArchiveUnit;

gboolean	em_folder_properties_autoarchive_get
						(EMailBackend *mail_backend,
						 const gchar *folder_uri,
						 gboolean *enabled,
						 EAutoArchiveConfig *config,
						 gint *n_units,
						 EAutoArchiveUnit *unit,
						 gchar **custom_target_folder_uri);

void		em_folder_properties_autoarchive_set
						(EMailBackend *mail_backend,
						 const gchar *folder_uri,
						 gboolean enabled,
						 EAutoArchiveConfig config,
						 gint n_units,
						 EAutoArchiveUnit unit,
						 const gchar *custom_target_folder_uri);

G_END_DECLS

#endif /* __EM_FOLDER_PROPERTIES_H__ */
