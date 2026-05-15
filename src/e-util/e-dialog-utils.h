/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Ettore Perazzoli <ettore@ximian.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_DIALOG_UTILS_H
#define E_DIALOG_UTILS_H

#include <gtk/gtk.h>

void		e_notice			(gpointer parent,
						 GtkMessageType type,
						 const gchar *format,
						 ...) G_GNUC_PRINTF (3, 4);

#endif /* E_DIALOG_UTILS_H */
