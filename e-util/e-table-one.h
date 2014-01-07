/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef _E_TABLE_ONE_H_
#define _E_TABLE_ONE_H_

#include <e-util/e-table-model.h>

/* Standard GObject macros */
#define E_TYPE_TABLE_ONE \
	(e_table_one_get_type ())
#define E_TABLE_ONE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TABLE_ONE, ETableOne))
#define E_TABLE_ONE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TABLE_ONE, ETableOneClass))
#define E_IS_TABLE_ONE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TABLE_ONE))
#define E_IS_TABLE_ONE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TABLE_ONE))
#define E_TABLE_ONE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TABLE_ONE, ETableOneClass))

G_BEGIN_DECLS

typedef struct _ETableOne ETableOne;
typedef struct _ETableOneClass ETableOneClass;

struct _ETableOne {
	GObject parent;

	ETableModel  *source;
	gpointer *data;
};

struct _ETableOneClass {
	GObjectClass parent_class;
};

GType		e_table_one_get_type		(void) G_GNUC_CONST;

ETableModel *	e_table_one_new			(ETableModel *source);
void		e_table_one_commit		(ETableOne *one);

G_END_DECLS

#endif /* _E_TABLE_ONE_H_ */

