/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Chris Lahey <clahey@ximian.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef _E_TABLE_COLUMN_SPECIFICATION_H_
#define _E_TABLE_COLUMN_SPECIFICATION_H_

#include <glib-object.h>

/* Standard GObject macros */
#define E_TYPE_TABLE_COLUMN_SPECIFICATION \
	(e_table_column_specification_get_type ())
#define E_TABLE_COLUMN_SPECIFICATION(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TABLE_COLUMN_SPECIFICATION, ETableColumnSpecification))
#define E_TABLE_COLUMN_SPECIFICATION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TABLE_COLUMN_SPECIFICATION, ETableColumnSpecificationClass))
#define E_IS_TABLE_COLUMN_SPECIFICATION(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TABLE_COLUMN_SPECIFICATION))
#define E_IS_TABLE_COLUMN_SPECIFICATION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TABLE_COLUMN_SPECIFICATION))
#define E_TABLE_COLUMN_SPECIFICATION_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TABLE_COLUMN_SPECIFICATION, ETableColumnSpecificationClass))

G_BEGIN_DECLS

typedef struct _ETableColumnSpecification ETableColumnSpecification;
typedef struct _ETableColumnSpecificationClass ETableColumnSpecificationClass;

struct _ETableColumnSpecification {
	GObject parent;

	gint model_col;
	gint compare_col;
	gchar *title;
	gchar *pixbuf;

	gdouble expansion;
	gint minimum_width;
	gboolean resizable;
	gboolean disabled;
	gboolean sortable;

	gchar *cell;
	gchar *compare;
	gchar *search;
	gint priority;
};

struct _ETableColumnSpecificationClass {
	GObjectClass parent_class;
};

GType		e_table_column_specification_get_type	(void) G_GNUC_CONST;
ETableColumnSpecification *
		e_table_column_specification_new	(void);
gboolean	e_table_column_specification_equal
					(ETableColumnSpecification *spec_a,
					 ETableColumnSpecification *spec_b);

G_END_DECLS

#endif /* _E_TABLE_COLUMN_SPECIFICATION_H_ */
