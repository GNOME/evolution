/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Chris Lahey <clahey@ximian.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_CELL_NUMBER_H
#define E_CELL_NUMBER_H

#include <e-util/e-cell-text.h>

/* Standard GObject macros */
#define E_TYPE_CELL_NUMBER \
	(e_cell_number_get_type ())
#define E_CELL_NUMBER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CELL_NUMBER, ECellNumber))
#define E_CELL_NUMBER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CELL_NUMBER, ECellNumberClass))
#define E_IS_CELL_NUMBER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CELL_NUMBER))
#define E_IS_CELL_NUMBER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CELL_NUMBER))
#define E_CELL_NUMBER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CELL_NUMBER, ECellNumberClass))

G_BEGIN_DECLS

typedef struct _ECellNumber ECellNumber;
typedef struct _ECellNumberClass ECellNumberClass;

struct _ECellNumber {
	ECellText parent;
};

struct _ECellNumberClass {
	ECellTextClass parent_class;
};

GType		e_cell_number_get_type		(void) G_GNUC_CONST;
ECell *		e_cell_number_new		(const gchar *fontname,
						 GtkJustification justify);

G_END_DECLS

#endif /* E_CELL_NUMBER_H */
