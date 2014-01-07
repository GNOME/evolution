/*
 *
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

#ifndef _E_TABLE_GROUP_CONTAINER_H_
#define _E_TABLE_GROUP_CONTAINER_H_

#include <libgnomecanvas/libgnomecanvas.h>

#include <e-util/e-table-group.h>
#include <e-util/e-table-header.h>
#include <e-util/e-table-item.h>
#include <e-util/e-table-model.h>

/* Standard GObject macros */
#define E_TYPE_TABLE_GROUP_CONTAINER \
	(e_table_group_container_get_type ())
#define E_TABLE_GROUP_CONTAINER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TABLE_GROUP_CONTAINER, ETableGroupContainer))
#define E_TABLE_GROUP_CONTAINER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TABLE_GROUP_CONTAINER, ETableGroupContainerClass))
#define E_IS_TABLE_GROUP_CONTAINER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TABLE_GROUP_CONTAINER))
#define E_IS_TABLE_GROUP_CONTAINER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TABLE_GROUP_CONTAINER))
#define E_TABLE_GROUP_CONTAINER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TABLE_GROUP_CONTAINER, ETableGroupContainerClass))

G_BEGIN_DECLS

typedef struct _ETableGroupContainer ETableGroupContainer;
typedef struct _ETableGroupContainerClass ETableGroupContainerClass;

typedef struct _ETableGroupContainerChildNode ETableGroupContainerChildNode;

struct _ETableGroupContainer {
	ETableGroup group;

	/*
	 * The ETableCol used to group this set
	 */
	ETableCol *ecol;
	gint ascending;

	/*
	 * List of ETableGroups we stack
	 */
	GList *children;

	/*
	 * The canvas rectangle that contains the children
	 */
	GnomeCanvasItem *rect;

	PangoFontDescription *font_desc;

	gdouble width, height, minimum_width;

	ETableSortInfo *sort_info;
	gint n;
	gint length_threshold;

	ESelectionModel *selection_model;

	guint alternating_row_colors : 1;
	guint horizontal_draw_grid : 1;
	guint vertical_draw_grid : 1;
	guint draw_focus : 1;
	guint uniform_row_height : 1;
	ECursorMode cursor_mode;

	/*
	 * State: the ETableGroup is open or closed
	 */
	guint open : 1;
};

struct _ETableGroupContainerClass {
	ETableGroupClass parent_class;
};

struct _ETableGroupContainerChildNode {
	ETableGroup *child;
	gpointer key;
	gchar *string;
	GnomeCanvasItem *text;
	GnomeCanvasItem *rect;
	gint count;
};

GType		e_table_group_container_get_type
						(void) G_GNUC_CONST;
ETableGroup *	e_table_group_container_new	(GnomeCanvasGroup *parent,
						 ETableHeader *full_header,
						 ETableHeader *header,
						 ETableModel *model,
						 ETableSortInfo *sort_info,
						 gint n);
void		e_table_group_container_construct
						(GnomeCanvasGroup *parent,
						 ETableGroupContainer *etgc,
						 ETableHeader *full_header,
						 ETableHeader *header,
						 ETableModel *model,
						 ETableSortInfo *sort_info,
						 gint n);
gboolean	e_table_group_container_is_editing
						(ETableGroupContainer *etgc);

G_END_DECLS

#endif /* _E_TABLE_GROUP_CONTAINER_H_ */
