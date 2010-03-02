/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_FILE_UTILS_H
#define E_FILE_UTILS_H

#include <gio/gio.h>
#include <e-util/e-activity.h>

G_BEGIN_DECLS

EActivity *	e_file_replace_contents_async	(GFile *file,
						 const gchar *contents,
						 gsize length,
						 const gchar *etag,
						 gboolean make_backup,
						 GFileCreateFlags flags,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	e_file_replace_contents_finish	(GFile *file,
						 GAsyncResult *result,
						 gchar **new_etag,
						 GError **error);

G_END_DECLS

#endif /* E_FILE_UTILS_H */
