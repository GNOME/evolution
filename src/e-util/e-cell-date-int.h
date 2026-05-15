/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Chris Lahey <clahey@ximian.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_CELL_DATE_INT_H
#define E_CELL_DATE_INT_H

#include <e-util/e-cell-date.h>

/* Standard GObject macros */
#define E_TYPE_CELL_DATE_INT \
	(e_cell_date_int_get_type ())
#define E_CELL_DATE_INT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CELL_DATE_INT, ECellDateInt))
#define E_CELL_DATE_INT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CELL_DATE_INT, ECellDateIntClass))
#define E_IS_CELL_DATE_INT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CELL_DATE_INT))
#define E_IS_CELL_DATE_INT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CELL_DATE_INT))
#define E_CELL_DATE_INT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CELL_DATE_INT, ECellDateIntClass))

G_BEGIN_DECLS

typedef struct _ECellDateInt ECellDateInt;
typedef struct _ECellDateIntClass ECellDateIntClass;
typedef struct _ECellDateIntPrivate ECellDateIntPrivate;

struct _ECellDateInt {
	ECellDate parent;

	ECellDateIntPrivate *priv;
};

struct _ECellDateIntClass {
	ECellDateClass parent_class;
};

GType		e_cell_date_int_get_type	(void) G_GNUC_CONST;
ECell *		e_cell_date_int_new		(const gchar *fontname,
						 GtkJustification justify);
G_END_DECLS

#endif /* E_CELL_DATE_INT_H */
