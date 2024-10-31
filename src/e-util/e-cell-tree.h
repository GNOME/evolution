/*
 * e-cell-tree.h - Tree cell object.
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * Authors:
 *   Chris Toshok <toshok@ximian.com>
 *
 * A majority of code taken from:
 *
 * the ECellText renderer.
 * Copyright 1998, The Free Software Foundation
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * published by the Free Software Foundation; either the version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser  General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef _E_CELL_TREE_H_
#define _E_CELL_TREE_H_

#include <libgnomecanvas/libgnomecanvas.h>

#include <e-util/e-cell.h>

/* Standard GObject macros */
#define E_TYPE_CELL_TREE \
	(e_cell_tree_get_type ())
#define E_CELL_TREE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CELL_TREE, ECellTree))
#define E_CELL_TREE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CELL_TREE, ECellTreeClass))
#define E_IS_CELL_TREE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CELL_TREE))
#define E_IS_CELL_TREE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CELL_TREE))
#define E_CELL_TREE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CELL_TREE, ECellTreeClass))

G_BEGIN_DECLS

typedef struct _ECellTree ECellTree;
typedef struct _ECellTreeClass ECellTreeClass;

struct _ECellTree {
	ECell parent;

	gboolean draw_lines;
	gboolean grouped_view;
	gboolean show_expander;

	ECell *subcell;
};

struct _ECellTreeClass {
	ECellClass parent_class;
};

GType		e_cell_tree_get_type		(void) G_GNUC_CONST;
ECell *		e_cell_tree_new			(gboolean draw_lines,
						 gboolean show_expander,
						 ECell *subcell);
void		e_cell_tree_construct		(ECellTree *ect,
						 gboolean draw_lines,
						 gboolean show_expander,
						 ECell *subcell);
ECellView *	e_cell_tree_view_get_subcell_view
						(ECellView *ect);
gboolean	e_cell_tree_get_grouped_view	(ECellTree *cell_tree);
void		e_cell_tree_set_grouped_view	(ECellTree *cell_tree,
						 gboolean grouped_view);
gboolean	e_cell_tree_get_show_expander	(ECellTree *cell_tree);
void		e_cell_tree_set_show_expander	(ECellTree *cell_tree,
						 gboolean show_expander);

G_END_DECLS

#endif /* _E_CELL_TREE_H_ */

