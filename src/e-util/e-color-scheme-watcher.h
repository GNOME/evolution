/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_COLOR_SCHEME_WATCHER_H
#define E_COLOR_SCHEME_WATCHER_H

#include <glib.h>

G_BEGIN_DECLS

#define E_TYPE_COLOR_SCHEME_WATCHER e_color_scheme_watcher_get_type ()

G_DECLARE_FINAL_TYPE (EColorSchemeWatcher, e_color_scheme_watcher, E, COLOR_SCHEME_WATCHER, GObject)

EColorSchemeWatcher *
		e_color_scheme_watcher_new	(void);

G_END_DECLS

#endif /* E_COLOR_SCHEME_WATCHER_H */
