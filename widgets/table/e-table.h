/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_H_
#define _E_TABLE_H_

#include <libgnomeui/gnome-canvas.h>
#include <gtk/gtktable.h>
#include <gnome-xml/tree.h>
#include "e-table-model.h"
#include "e-table-header.h"
#include "e-table-group.h"
#include "e-table-sort-info.h"
#include "e-table-item.h"

BEGIN_GNOME_DECLS

#define E_TABLE_TYPE        (e_table_get_type ())
#define E_TABLE(o)          (GTK_CHECK_CAST ((o), E_TABLE_TYPE, ETable))
#define E_TABLE_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_TYPE, ETableClass))
#define E_IS_TABLE(o)       (GTK_CHECK_TYPE ((o), E_TABLE_TYPE))
#define E_IS_TABLE_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_TYPE))

typedef struct {
	GtkTable parent;

	ETableModel *model;

	ETableHeader *full_header, *header;

	ETableGroup  *group;

	ETableSortInfo *sort_info;

	int table_model_change_id;
	int table_row_change_id;
	int table_cell_change_id;
	int table_row_inserted_id;
	int table_row_deleted_id;

	int group_info_change_id;

	GnomeCanvas *header_canvas, *table_canvas;

	GnomeCanvasItem *header_item, *root;

	gint length_threshold;

	gint rebuild_idle_id;
	guint need_rebuild:1;

	/*
	 * Configuration settings
	 */
	guint draw_grid : 1;
	guint draw_focus : 1;

	ETableCursorMode cursor_mode;
} ETable;

typedef struct {
	GtkTableClass parent_class;

	void        (*row_selection)      (ETable *et, int row, gboolean selected);
	void        (*cursor_change)      (ETable *et, int row);
	void        (*double_click)       (ETable *et, int row);
	gint        (*key_press)          (ETable *et, int row, int col, GdkEvent *event);
} ETableClass;

GtkType    e_table_get_type   		    (void);

ETable    *e_table_construct  		    (ETable *e_table, ETableHeader *full_header, ETableModel *etm,
			      		     const char *spec);
GtkWidget *e_table_new        		    (ETableHeader *full_header, ETableModel *etm,
			      		     const char *spec);

ETable    *e_table_construct_from_spec_file (ETable *e_table,
					     ETableHeader *full_header,
					     ETableModel *etm,
					     const char *filename);
GtkWidget *e_table_new_from_spec_file       (ETableHeader *full_header,
					     ETableModel *etm,
					     const char *filename);

gchar     *e_table_get_specification        (ETable *e_table);
void       e_table_save_specification       (ETable *e_table, gchar *filename);

void       e_table_select_row               (ETable *e_table,
					     int row);
/* -1 means no selection. */
int        e_table_get_selected_view_row    (ETable *e_table);

END_GNOME_DECLS

#endif /* _E_TABLE_H_ */
