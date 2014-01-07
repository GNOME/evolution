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

#ifndef _E_TABLE_FIELD_CHOOSER_ITEM_H_
#define _E_TABLE_FIELD_CHOOSER_ITEM_H_

#include <libgnomecanvas/libgnomecanvas.h>
#include <libxml/tree.h>

#include <e-util/e-table-header.h>

/* Standard GObject macros */
#define E_TYPE_TABLE_FIELD_CHOOSER_ITEM \
	(e_table_field_chooser_item_get_type ())
#define E_TABLE_FIELD_CHOOSER_ITEM(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TABLE_FIELD_CHOOSER_ITEM, ETableFieldChooserItem))
#define E_TABLE_FIELD_CHOOSER_ITEM_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TABLE_FIELD_CHOOSER_ITEM, ETableFieldChooserItemClass))
#define E_IS_TABLE_FIELD_CHOOSER_ITEM(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TABLE_FIELD_CHOOSER_ITEM))
#define E_IS_TABLE_FIELD_CHOOSER_ITEM_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TABLE_FIELD_CHOOSER_ITEM))
#define E_TABLE_FIELD_CHOOSER_ITEM_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TABLE_FIELD_CHOOSER_ITEM, ETableFieldChooserItemClass))

G_BEGIN_DECLS

typedef struct _ETableFieldChooserItem ETableFieldChooserItem;
typedef struct _ETableFieldChooserItemClass ETableFieldChooserItemClass;

struct _ETableFieldChooserItem {
	GnomeCanvasItem parent;

	ETableHeader *full_header;
	ETableHeader *header;
	ETableHeader *combined_header;

	gdouble height, width;

	PangoFontDescription *font_desc;

	/*
	 * Ids
	 */
	gint full_header_structure_change_id;
	gint full_header_dimension_change_id;
	gint table_header_structure_change_id;
	gint table_header_dimension_change_id;

	gchar *dnd_code;

	/*
	 * For dragging columns
	 */
	guint maybe_drag : 1;
	gint click_x, click_y;
	gint drag_col;
	guint drag_data_get_id;
	guint drag_end_id;
};

struct _ETableFieldChooserItemClass {
	GnomeCanvasItemClass parent_class;
};

GType		e_table_field_chooser_item_get_type	(void) G_GNUC_CONST;

G_END_DECLS

#endif /* _E_TABLE_FIELD_CHOOSER_ITEM_H_ */
