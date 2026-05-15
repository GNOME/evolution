/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Damon Chaplin <damon@ximian.com>
 */

/*
 * ECellPercent - a subclass of ECellText used to show an integer percentage
 * in an ETable.
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_CELL_PERCENT_H
#define E_CELL_PERCENT_H

#include <e-util/e-cell-text.h>

/* Standard GObject macros */
#define E_TYPE_CELL_PERCENT \
	(e_cell_percent_get_type ())
#define E_CELL_PERCENT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CELL_PERCENT, ECellPercent))
#define E_CELL_PERCENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CELL_PERCENT, ECellPercentClass))
#define E_IS_CELL_PERCENT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CELL_PERCENT))
#define E_IS_CELL_PERCENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CELL_PERCENT))
#define E_CELL_PERCENT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CELL_PERCENT, ECellPercentClass))

G_BEGIN_DECLS

typedef struct _ECellPercent ECellPercent;
typedef struct _ECellPercentClass ECellPercentClass;

struct _ECellPercent {
	ECellText parent;
};

struct _ECellPercentClass {
	ECellTextClass parent_class;
};

GType		e_cell_percent_get_type		(void) G_GNUC_CONST;
ECell *		e_cell_percent_new		(const gchar *fontname,
						 GtkJustification justify);

G_END_DECLS

#endif /* E_CELL_PERCENT_H */
