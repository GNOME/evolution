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
#include "e-table-selection-model.h"
#include "e-util/e-printable.h"

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

	GnomeCanvasItem *canvas_vbox;
	ETableGroup  *group;

	ETableSortInfo *sort_info;

	ETableSelectionModel *selection;

	int table_model_change_id;
	int table_row_change_id;
	int table_cell_change_id;
	int table_row_inserted_id;
	int table_row_deleted_id;

	int group_info_change_id;

	int reflow_idle_id;

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
	guint row_selection_active : 1;
	
	char *click_to_add_message;
	GnomeCanvasItem *click_to_add;
	gboolean use_click_to_add;

	ETableCursorMode cursor_mode;

	int drag_get_data_row;
	int drag_get_data_col;

	int drag_row;
	int drag_col;

	int drop_row;
	int drop_col;
} ETable;

typedef struct {
	GtkTableClass parent_class;

	void        (*row_selection)      (ETable *et, int row, gboolean selected);
	void        (*cursor_change)      (ETable *et, int row);
	void        (*double_click)       (ETable *et, int row);
	gint        (*right_click)        (ETable *et, int row, int col, GdkEvent *event);
	gint        (*key_press)          (ETable *et, int row, int col, GdkEvent *event);

	void  (*set_scroll_adjustments)   (ETable	 *table,
					   GtkAdjustment *hadjustment,
					   GtkAdjustment *vadjustment);

	/* Source side drag signals */
	void (* table_drag_begin)	           (ETable	       *table,
						    int                 row,
						    int                 col,
						    GdkDragContext     *context);
	void (* table_drag_end)	           (ETable	       *table,
					    int                 row,
					    int                 col,
					    GdkDragContext     *context);
	void (* table_drag_data_get)             (ETable             *table,
						  int                 row,
						  int                 col,
						  GdkDragContext     *context,
						  GtkSelectionData   *selection_data,
						  guint               info,
						  guint               time);
	void (* table_drag_data_delete)          (ETable	       *table,
						  int                 row,
						  int                 col,
						  GdkDragContext     *context);
	
	/* Target side drag signals */	   
	void (* table_drag_leave)	           (ETable	       *table,
						    int                 row,
						    int                 col,
						    GdkDragContext     *context,
						    guint               time);
	gboolean (* table_drag_motion)           (ETable	       *table,
						  int                 row,
						  int                 col,
						  GdkDragContext     *context,
						  gint                x,
						  gint                y,
						  guint               time);
	gboolean (* table_drag_drop)             (ETable	       *table,
						  int                 row,
						  int                 col,
						  GdkDragContext     *context,
						  gint                x,
						  gint                y,
						  guint               time);
	void (* table_drag_data_received)        (ETable             *table,
						  int                 row,
						  int                 col,
						  GdkDragContext     *context,
						  gint                x,
						  gint                y,
						  GtkSelectionData   *selection_data,
						  guint               info,
						  guint               time);
} ETableClass;

GtkType     e_table_get_type   		    (void);
	   
ETable     *e_table_construct  		    (ETable *e_table, ETableHeader *full_header, ETableModel *etm,
	   				      const char *spec);
GtkWidget  *e_table_new        		    (ETableHeader *full_header, ETableModel *etm,
	   				      const char *spec);
	   
ETable     *e_table_construct_from_spec_file (ETable *e_table,
	   				      ETableHeader *full_header,
	   				      ETableModel *etm,
	   				      const char *filename);
GtkWidget  *e_table_new_from_spec_file       (ETableHeader *full_header,
	   				      ETableModel *etm,
	   				      const char *filename);
	   
gchar      *e_table_get_specification        (ETable *e_table);
void        e_table_save_specification       (ETable *e_table, gchar *filename);
	   
void        e_table_set_cursor_row           (ETable *e_table,
					      int row);
/* -1 means we don't have the cursor. */
int         e_table_get_cursor_row           (ETable *e_table);
void        e_table_selected_row_foreach     (ETable *e_table,
					      ETableForeachFunc callback,
					      gpointer closure);
EPrintable *e_table_get_printable            (ETable *e_table);


/* Drag & drop stuff. */
/* Target */
void e_table_drag_get_data (ETable         *table,
			    int             row,
			    int             col,
			    GdkDragContext *context,
			    GdkAtom         target,
			    guint32         time);

void e_table_drag_highlight   (ETable     *table,
			       int         row,
			       int         col); /* col == -1 to highlight entire row. */
void e_table_drag_unhighlight (ETable     *table);

void e_table_drag_dest_set   (ETable               *table,
			      GtkDestDefaults       flags,
			      const GtkTargetEntry *targets,
			      gint                  n_targets,
			      GdkDragAction         actions);

void e_table_drag_dest_set_proxy (ETable         *table,
				  GdkWindow      *proxy_window,
				  GdkDragProtocol protocol,
				  gboolean        use_coordinates);

/* There probably should be functions for setting the targets
 * as a GtkTargetList
 */

void e_table_drag_dest_unset (GtkWidget          *widget);

/* Source side */

void e_table_drag_source_set  (ETable               *table,
			       GdkModifierType       start_button_mask,
			       const GtkTargetEntry *targets,
			       gint                  n_targets,
			       GdkDragAction         actions);

void e_table_drag_source_unset (ETable        *table);

/* There probably should be functions for setting the targets
 * as a GtkTargetList
 */

GdkDragContext *e_table_drag_begin (ETable            *table,
				    int row,
				    int col,
				    GtkTargetList     *targets,
				    GdkDragAction      actions,
				    gint               button,
				    GdkEvent          *event);

END_GNOME_DECLS

#endif /* _E_TABLE_H_ */

