/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-group.h
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

#ifndef _E_TABLE_GROUP_H_
#define _E_TABLE_GROUP_H_

#include <libgnomecanvas/gnome-canvas.h>
#include <table/e-table-model.h>
#include <table/e-table-header.h>
#include <table/e-table-sort-info.h>
#include <table/e-table-defines.h>
#include <e-util/e-util.h>
#include <widgets/misc/e-printable.h>

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
	void        (*cursor_change)         (ETableGroup *etg, int row);
	void        (*cursor_activated)      (ETableGroup *etg, int row);
	void        (*double_click)          (ETableGroup *etg, int row, int col, GdkEvent *event);
	gint        (*right_click)           (ETableGroup *etg, int row, int col, GdkEvent *event);
	gint        (*click)                 (ETableGroup *etg, int row, int col, GdkEvent *event);
	gint        (*key_press)             (ETableGroup *etg, int row, int col, GdkEvent *event);
	gint        (*start_drag)            (ETableGroup *etg, int row, int col, GdkEvent *event);

	/* Virtual functions. */
	void        (*add)                   (ETableGroup *etg, gint row);
	void        (*add_array)             (ETableGroup *etg, const int *array, int count);
	void        (*add_all)               (ETableGroup *etg);
	gboolean    (*remove)                (ETableGroup *etg, gint row);
	gint        (*row_count)             (ETableGroup *etg);
	void        (*increment)             (ETableGroup *etg, gint position, gint amount);
	void        (*decrement)             (ETableGroup *etg, gint position, gint amount);
	void        (*set_focus)             (ETableGroup *etg, EFocus direction, gint view_col);
	gboolean    (*get_focus)             (ETableGroup *etg);
	gint        (*get_focus_column)      (ETableGroup *etg);
	EPrintable *(*get_printable)         (ETableGroup *etg);
	void        (*compute_location)      (ETableGroup *etg, int *x, int *y, int *row, int *col);
	void        (*get_cell_geometry)     (ETableGroup *etg, int *row, int *col, int *x, int *y, int *width, int *height);

} ETableGroupClass;

/* Virtual functions */
void          e_table_group_add               (ETableGroup       *etg,
					       gint               row);
void          e_table_group_add_array         (ETableGroup       *etg,
					       const int         *array,
					       int                count);
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
					       int               *x,
					       int               *y,
					       int               *row,
					       int               *col);
void          e_table_group_get_cell_geometry (ETableGroup       *etg,
					       int               *row,
					       int               *col,
					       int               *x,
					       int               *y,
					       int               *width,
					       int               *height);
ETableGroup  *e_table_group_new               (GnomeCanvasGroup  *parent,
					       ETableHeader      *full_header,
					       ETableHeader      *header,
					       ETableModel       *model,
					       ETableSortInfo    *sort_info,
					       int                n);
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

typedef void (*ETableGroupLeafFn) (void *e_table_item, void *closure);
void          e_table_group_apply_to_leafs    (ETableGroup       *etg,
					       ETableGroupLeafFn  fn,
					       void              *closure);

G_END_DECLS

#endif /* _E_TABLE_GROUP_H_ */
