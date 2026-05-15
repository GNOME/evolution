/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Michael Zucchi <notzed@ximian.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

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
