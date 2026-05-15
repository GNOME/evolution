/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Jeffrey Stedfast <fejj@ximian.com>
 * SPDX-FileContributor: Rodney Dawes <dobey@novell.com>
 */

#ifndef EM_FOLDER_UTILS_H
#define EM_FOLDER_UTILS_H

#include <gtk/gtk.h>
#include <camel/camel.h>
#include <libemail-engine/libemail-engine.h>
#include <mail/em-folder-tree.h>

G_BEGIN_DECLS

gint		em_folder_utils_copy_folders	(CamelStore *fromstore,
						 const gchar *frombase,
						 CamelStore *tostore,
						 const gchar *tobase,
						 gint delete);

/* FIXME These API's are really busted.  There is no consistency and
 *       most rely on the wrong data. */

void		em_folder_utils_copy_folder	(GtkWindow *parent,
						 EMailSession *session,
						 EAlertSink *alert_sink,
						 const gchar *folder_uri,
						 gboolean delete);

const gchar *	em_folder_utils_get_icon_name	(guint32 flags);

G_END_DECLS

#endif /* EM_FOLDER_UTILS_H */
