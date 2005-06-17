/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-group-container.h
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef _E_TABLE_GROUP_CONTAINER_H_
#define _E_TABLE_GROUP_CONTAINER_H_

#include <libgnomecanvas/gnome-canvas.h>
#include <table/e-table-model.h>
#include <table/e-table-header.h>
#include <table/e-table-group.h>
#include <table/e-table-item.h>

G_BEGIN_DECLS

#define E_TABLE_GROUP_CONTAINER_TYPE        (e_table_group_container_get_type ())
#define E_TABLE_GROUP_CONTAINER(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TABLE_GROUP_CONTAINER_TYPE, ETableGroupContainer))
#define E_TABLE_GROUP_CONTAINER_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TABLE_GROUP_CONTAINER_TYPE, ETableGroupContainerClass))
#define E_IS_TABLE_GROUP_CONTAINER(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TABLE_GROUP_CONTAINER_TYPE))
#define E_IS_TABLE_GROUP_CONTAINER_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TABLE_GROUP_CONTAINER_TYPE))

typedef struct {
	ETableGroup group;

	/*
	 * The ETableCol used to group this set
	 */
	ETableCol    *ecol;
	gint          ascending;

	/*
	 * List of ETableGroups we stack
	 */
	GList *children;

	/*
	 * The canvas rectangle that contains the children
	 */
	GnomeCanvasItem *rect;

	GdkFont *font;

	gdouble width, height, minimum_width;

	ETableSortInfo *sort_info;
	int n;
	int length_threshold;

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
	guint open:1;
} ETableGroupContainer;

typedef struct {
	ETableGroupClass parent_class;
} ETableGroupContainerClass;

typedef struct {
        ETableGroup *child;
        void *key;
        char *string;
        GnomeCanvasItem *text;
        GnomeCanvasItem *rect;
        gint count;
} ETableGroupContainerChildNode;


ETableGroup *e_table_group_container_new       (GnomeCanvasGroup *parent, ETableHeader *full_header, ETableHeader     *header,
						ETableModel *model, ETableSortInfo *sort_info, int n);
void         e_table_group_container_construct (GnomeCanvasGroup *parent, ETableGroupContainer *etgc,
						ETableHeader *full_header,
						ETableHeader     *header,
						ETableModel *model, ETableSortInfo *sort_info, int n);

GType        e_table_group_container_get_type  (void);

G_END_DECLS

#endif /* _E_TABLE_GROUP_CONTAINER_H_ */
