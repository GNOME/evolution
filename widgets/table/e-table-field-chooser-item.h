/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_FIELD_CHOOSER_ITEM_H_
#define _E_TABLE_FIELD_CHOOSER_ITEM_H_

#include <libgnomeui/gnome-canvas.h>
#include <gnome-xml/tree.h>
#include "e-table-header.h"

#define E_TABLE_FIELD_CHOOSER_ITEM_TYPE        (e_table_field_chooser_item_get_type ())
#define E_TABLE_FIELD_CHOOSER_ITEM(o)          (GTK_CHECK_CAST ((o), E_TABLE_FIELD_CHOOSER_ITEM_TYPE, ETableFieldChooserItem))
#define E_TABLE_FIELD_CHOOSER_ITEM_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_FIELD_CHOOSER_ITEM_TYPE, ETableFieldChooserItemClass))
#define E_IS_TABLE_FIELD_CHOOSER_ITEM(o)       (GTK_CHECK_TYPE ((o), E_TABLE_FIELD_CHOOSER_ITEM_TYPE))
#define E_IS_TABLE_FIELD_CHOOSER_ITEM_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_FIELD_CHOOSER_ITEM_TYPE))

typedef struct {
	GnomeCanvasItem  parent;
	ETableHeader    *full_header;

	double           height, width;
	double           button_height;

	GdkFont         *font;

	/*
	 * Ids
	 */
	int structure_change_id, dimension_change_id;

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

GtkType    e_table_field_chooser_item_get_type (void);

#endif /* _E_TABLE_FIELD_CHOOSER_ITEM_H_ */
