/*
 *
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

#ifndef _E_TABLE_GROUP_H_
#define _E_TABLE_GROUP_H_

#include <libgnomecanvas/gnome-canvas.h>
#include <table/e-table-model.h>
#include <table/e-table-header.h>
#include <table/e-table-sort-info.h>
#include <table/e-table-defines.h>
#include <e-util/e-util.h>
#include <misc/e-printable.h>

G_BEGIN_DECLS

#define E_TABLE_GROUP_TYPE        (e_table_group_get_type ())
#define E_TABLE_GROUP(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TABLE_GROUP_TYPE, ETableGroup))
#define E_TABLE_GROUP_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TABLE_GROUP_TYPE, ETableGroupClass))
#define E_IS_TABLE_GROUP(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TABLE_GROUP_TYPE))
#define E_IS_TABLE_GROUP_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TABLE_GROUP_TYPE))

typedef struct {
	GnomeCanvasGroup group;

	/*
	 * The full header.
	 */
	ETableHeader *full_header;
	ETableHeader *header;

	/*
	 * The model we pull data from.
	 */
	ETableModel *model;

	/*
	 * Whether we should add indentation and open/close markers,
	 * or if we just act as containers of subtables.
	 */
	guint transparent : 1;

	guint has_focus : 1;

	guint frozen : 1;
} ETableGroup;

typedef struct {
	GnomeCanvasGroupClass parent_class;

	/* Signals */
	void        (*cursor_change)         (ETableGroup *etg, gint row);
	void        (*cursor_activated)      (ETableGroup *etg, gint row);
	void        (*double_click)          (ETableGroup *etg, gint row, gint col, GdkEvent *event);
	gint        (*right_click)           (ETableGroup *etg, gint row, gint col, GdkEvent *event);
	gint        (*click)                 (ETableGroup *etg, gint row, gint col, GdkEvent *event);
	gint        (*key_press)             (ETableGroup *etg, gint row, gint col, GdkEvent *event);
	gint        (*start_drag)            (ETableGroup *etg, gint row, gint col, GdkEvent *event);

	/* Virtual functions. */
	void        (*add)                   (ETableGroup *etg, gint row);
	void        (*add_array)             (ETableGroup *etg, const gint *array, gint count);
	void        (*add_all)               (ETableGroup *etg);
	gboolean    (*remove)                (ETableGroup *etg, gint row);
	gint        (*row_count)             (ETableGroup *etg);
	void        (*increment)             (ETableGroup *etg, gint position, gint amount);
	void        (*decrement)             (ETableGroup *etg, gint position, gint amount);
	void        (*set_focus)             (ETableGroup *etg, EFocus direction, gint view_col);
	gboolean    (*get_focus)             (ETableGroup *etg);
	gint        (*get_focus_column)      (ETableGroup *etg);
	EPrintable *(*get_printable)         (ETableGroup *etg);
	void        (*compute_location)      (ETableGroup *etg, gint *x, gint *y, gint *row, gint *col);
	void        (*get_mouse_over)        (ETableGroup *etg, gint *row, gint *col);
	void        (*get_cell_geometry)     (ETableGroup *etg, gint *row, gint *col, gint *x, gint *y, gint *width, gint *height);

} ETableGroupClass;

/* Virtual functions */
void          e_table_group_add               (ETableGroup       *etg,
					       gint               row);
void          e_table_group_add_array         (ETableGroup       *etg,
					       const gint         *array,
					       gint                count);
void          e_table_group_add_all           (ETableGroup       *etg);
gboolean      e_table_group_remove            (ETableGroup       *etg,
					       gint               row);
void          e_table_group_increment         (ETableGroup       *etg,
					       gint               position,
					       gint               amount);
void          e_table_group_decrement         (ETableGroup       *etg,
					       gint               position,
					       gint               amount);
gint          e_table_group_row_count         (ETableGroup       *etg);
void          e_table_group_set_focus         (ETableGroup       *etg,
					       EFocus             direction,
					       gint               view_col);
gboolean      e_table_group_get_focus         (ETableGroup       *etg);
gint          e_table_group_get_focus_column  (ETableGroup       *etg);
ETableHeader *e_table_group_get_header        (ETableGroup       *etg);
EPrintable   *e_table_group_get_printable     (ETableGroup       *etg);
void          e_table_group_compute_location  (ETableGroup       *etg,
					       gint               *x,
					       gint               *y,
					       gint               *row,
					       gint               *col);
void          e_table_group_get_mouse_over(ETableGroup       *etg,
					       gint               *row,
					       gint               *col);
void          e_table_group_get_cell_geometry (ETableGroup       *etg,
					       gint               *row,
					       gint               *col,
					       gint               *x,
					       gint               *y,
					       gint               *width,
					       gint               *height);
ETableGroup  *e_table_group_new               (GnomeCanvasGroup  *parent,
					       ETableHeader      *full_header,
					       ETableHeader      *header,
					       ETableModel       *model,
					       ETableSortInfo    *sort_info,
					       gint                n);
void          e_table_group_construct         (GnomeCanvasGroup  *parent,
					       ETableGroup       *etg,
					       ETableHeader      *full_header,
					       ETableHeader      *header,
					       ETableModel       *model);

/* For emitting the signals */
void          e_table_group_cursor_change     (ETableGroup       *etg,
					       gint               row);
void          e_table_group_cursor_activated  (ETableGroup       *etg,
					       gint               row);
void          e_table_group_double_click      (ETableGroup       *etg,
					       gint               row,
					       gint               col,
					       GdkEvent          *event);
gint          e_table_group_right_click       (ETableGroup       *etg,
					       gint               row,
					       gint               col,
					       GdkEvent          *event);
gint          e_table_group_click             (ETableGroup       *etg,
					       gint               row,
					       gint               col,
					       GdkEvent          *event);
gint          e_table_group_key_press         (ETableGroup       *etg,
					       gint               row,
					       gint               col,
					       GdkEvent          *event);
gint          e_table_group_start_drag        (ETableGroup       *etg,
					       gint               row,
					       gint               col,
					       GdkEvent          *event);
GType         e_table_group_get_type          (void);

typedef void (*ETableGroupLeafFn) (gpointer e_table_item, gpointer closure);
void          e_table_group_apply_to_leafs    (ETableGroup       *etg,
					       ETableGroupLeafFn  fn,
					       void              *closure);

G_END_DECLS

#endif /* _E_TABLE_GROUP_H_ */
