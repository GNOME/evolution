/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Christopher James Lahey <clahey@ximian.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef __GAL_A11Y_UTIL_H__
#define __GAL_A11Y_UTIL_H__

#include <glib-object.h>

GType  gal_a11y_type_register_static_with_private  (GType        parent_type,
						    const gchar *type_name,
						    GTypeInfo   *info,
						    GTypeFlags   flags,
						    gint          priv_size,
						    gint        *priv_offset);

#endif /* __GAL_A11Y_UTIL_H__ */
