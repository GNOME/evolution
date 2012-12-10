/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
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

#ifndef _E_TABLE_STATE_H_
#define _E_TABLE_STATE_H_

#include <libxml/tree.h>

#include <e-util/e-table-sort-info.h>

/* Standard GObject macros */
#define E_TYPE_TABLE_STATE \
	(e_table_state_get_type ())
#define E_TABLE_STATE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TABLE_STATE, ETableState))
#define E_TABLE_STATE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TABLE_STATE, ETableStateClass))
#define E_IS_TABLE_STATE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TABLE_STATE))
#define E_IS_TABLE_STATE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TABLE_STATE))
#define E_TABLE_STATE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TABLE_STATE, ETableStateClass))

G_BEGIN_DECLS

typedef struct _ETableState ETableState;
typedef struct _ETableStateClass ETableStateClass;

struct _ETableState {
	GObject parent;

	ETableSortInfo *sort_info;
	gint col_count;
	gint *columns;
	gdouble *expansions;
};

struct _ETableStateClass {
	GObjectClass parent_class;
};

GType		e_table_state_get_type		(void) G_GNUC_CONST;
ETableState *	e_table_state_new		(void);
ETableState *	e_table_state_vanilla		(gint col_count);
gboolean	e_table_state_load_from_file	(ETableState *state,
						 const gchar *filename);
void		e_table_state_load_from_string	(ETableState *state,
						 const gchar *xml);
void		e_table_state_load_from_node	(ETableState *state,
						 const xmlNode *node);
void		e_table_state_save_to_file	(ETableState *state,
						 const gchar *filename);
gchar *		e_table_state_save_to_string	(ETableState *state);
xmlNode *	e_table_state_save_to_node	(ETableState *state,
						 xmlNode *parent);
ETableState *	e_table_state_duplicate		(ETableState *state);

G_END_DECLS

#endif /* _E_TABLE_STATE_H_ */
