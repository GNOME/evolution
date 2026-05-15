/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Michael Zucchi <notzed@ximian.com>
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
