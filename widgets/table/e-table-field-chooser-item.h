/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-field-chooser-item.h
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

#ifndef _E_TABLE_FIELD_CHOOSER_ITEM_H_
#define _E_TABLE_FIELD_CHOOSER_ITEM_H_

#include <libgnomecanvas/gnome-canvas.h>
#include <libxml/tree.h>
#include <table/e-table-header.h>

G_BEGIN_DECLS

#define E_TABLE_FIELD_CHOOSER_ITEM_TYPE        (e_table_field_chooser_item_get_type ())
#define E_TABLE_FIELD_CHOOSER_ITEM(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TABLE_FIELD_CHOOSER_ITEM_TYPE, ETableFieldChooserItem))
#define E_TABLE_FIELD_CHOOSER_ITEM_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TABLE_FIELD_CHOOSER_ITEM_TYPE, ETableFieldChooserItemClass))
#define E_IS_TABLE_FIELD_CHOOSER_ITEM(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TABLE_FIELD_CHOOSER_ITEM_TYPE))
#define E_IS_TABLE_FIELD_CHOOSER_ITEM_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TABLE_FIELD_CHOOSER_ITEM_TYPE))

typedef struct {
	GnomeCanvasItem  parent;
	ETableHeader    *full_header;
	ETableHeader    *header;
	ETableHeader    *combined_header;

	double           height, width;

	GdkFont         *font;

	/*
	 * Ids
	 */
	int full_header_structure_change_id, full_header_dimension_change_id;
	int table_header_structure_change_id, table_header_dimension_change_id;

	gchar           *dnd_code;

	/*
	 * For dragging columns
	 */
	guint            maybe_drag:1;
	int              click_x, click_y;
	int              drag_col;
	guint            drag_data_get_id;
        guint            drag_end_id;
} ETableFieldChooserItem;

typedef struct {
	GnomeCanvasItemClass parent_class;
} ETableFieldChooserItemClass;

GType      e_table_field_chooser_item_get_type (void);

G_END_DECLS

#endif /* _E_TABLE_FIELD_CHOOSER_ITEM_H_ */
