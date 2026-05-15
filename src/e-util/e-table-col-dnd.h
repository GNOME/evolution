/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Chris Lahey <clahey@ximian.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef _E_TABLE_COL_DND_H_
#define _E_TABLE_COL_DND_H_

#include <glib.h>

G_BEGIN_DECLS

#define TARGET_ETABLE_COL_TYPE "application/x-etable-column-header"

enum {
	TARGET_ETABLE_COL_HEADER
};

G_END_DECLS

#endif /* _E_TABLE_COL_DND_H_ */
