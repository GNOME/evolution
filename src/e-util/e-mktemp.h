/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Jeffrey Stedfast <fejj@ximian.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef __E_MKTEMP_H__
#define __E_MKTEMP_H__

gchar *e_mktemp  (const gchar *template);

gint   e_mkstemp (const gchar *template);

gchar *e_mkdtemp (const gchar *template);

#endif /* __E_MKTEMP_H__ */
