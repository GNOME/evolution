/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_CELL_H_
#define _E_CELL_H_

#include <gdk/gdktypes.h>
#include <libgnomeprint/gnome-print.h>
#include "e-table-model.h"

#define E_CELL_TYPE        (e_cell_get_type ())
#define E_CELL(o)          (GTK_CHECK_CAST ((o), E_CELL_TYPE, ECell))
#define E_CELL_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_CELL_TYPE, ECellClass))
#define E_IS_CELL(o)       (GTK_CHECK_TYPE ((o), E_CELL_TYPE))
#define E_IS_CELL_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_CELL_TYPE))

typedef struct _ECell ECell;
typedef struct _ECellView ECellView;

struct _ECell {
	GtkObject       object;
};

struct _ECellView {
	ECell *ecell;
	ETableModel *e_table_model;
	void        *e_table_item_view;
	
	gint   focus_x1, focus_y1, focus_x2, focus_y2;
	gint   focus_col, focus_row;
};

#define E_CELL_IS_FOCUSED(ecell_view) (ecell_view->focus_x1 != -1)

typedef struct {
	GtkObjectClass parent_class;
	
	ECellView *(*new_view)     (ECell *ecell, ETableModel *table_model, void *e_table_item_view);
	void       (*kill_view)    (ECellView *ecell_view);
				   
	void       (*realize)      (ECellView *ecell_view);
	void       (*unrealize)    (ECellView *ecell_view);
				   
	void   	   (*draw)         (ECellView *ecell_view, GdkDrawable *drawable,
	       			    int model_col, int view_col, int row,
				    gboolean selected, int x1, int y1, int x2, int y2);
	gint   	   (*event)        (ECellView *ecell_view, GdkEvent *event, int model_col, int view_col, int row);
	void   	   (*focus)        (ECellView *ecell_view, int model_col, int view_col,
				    int row, int x1, int y1, int x2, int y2);
	void   	   (*unfocus)      (ECellView *ecell_view);
	int        (*height)       (ECellView *ecell_view, int model_col, int view_col, int row);
				   
	void      *(*enter_edit)   (ECellView *ecell_view, int model_col, int view_col, int row);
	void       (*leave_edit)   (ECellView *ecell_view, int model_col, int view_col, int row, void *context);
	void       (*print)        (ECellView *ecell_view, GnomePrintContext *context,
				    int model_col, int view_col, int row,
				    gdouble width, gdouble height);
	gdouble    (*print_height) (ECellView *ecell_view, GnomePrintContext *context,
				    int model_col, int view_col, int row, gdouble width);
} ECellClass;

GtkType    e_cell_get_type  (void);
ECellView *e_cell_new_view  (ECell *ecell, ETableModel *table_model, void *e_table_item_view);
void       e_cell_kill_view (ECellView *ecell_view);

void       e_cell_event     (ECellView *ecell_view, GdkEvent *event, int model_col, int view_col, int row);

void       e_cell_realize   (ECellView *ecell_view);
void       e_cell_unrealize (ECellView *ecell_view);

void       e_cell_draw      (ECellView *ecell_view, GdkDrawable *dr, 
			     int model_col, int view_col, int row, gboolean selected,
			     int x1, int y1, int x2, int y2);
void       e_cell_print      (ECellView *ecell_view, GnomePrintContext *context, 
			      int model_col, int view_col, int row,
			      double width, double height);
gdouble    e_cell_print_height (ECellView *ecell_view, GnomePrintContext *context,
				int model_col, int view_col, int row, gdouble width);
void       e_cell_focus     (ECellView *ecell_view, int model_col, int view_col, int row,
			     int x1, int y1, int x2, int y2);
void       e_cell_unfocus   (ECellView *ecell_view);
int        e_cell_height    (ECellView *ecell_view, int model_col, int view_col, int row);

void      *e_cell_enter_edit (ECellView *ecell_view, int model_col, int view_col, int row);
void       e_cell_leave_edit (ECellView *ecell_view, int model_col, int view_col, int row, void *edit_context);

#endif /* _E_CELL_H_ */
