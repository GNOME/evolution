#ifndef _E_CELL_H_
#define _E_CELL_H_

#include <libgnomeui/gnome-canvas.h>
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
	ETableModel    *table_model;
};

struct _ECellView {
	ECell *ecell;
	gint   focus_x1, focus_y1, focus_x2, focus_y2;
	gint   focus_col, focus_row;
};

#define E_CELL_IS_FOCUSED(ecell_view) (ecell_view->focus_x1 != -1)

typedef struct {
	GtkObjectClass parent_class;

	ECellView *(*realize)   (ECell *, GnomeCanvas *canvas);
	void       (*unrealize) (ECellView *);
	void   	   (*draw)      (ECellView *ecell_view, GdkDrawable *drawable,
	       			 int col, int row, int x1, int y1, int x2, int y2);
	gint   	   (*event)     (ECellView *ecell_view, GdkEvent *event, int col, int row);
	void   	   (*focus)     (ECellView *ecell, int col, int row, int x1, int y1, int x2, int y2);
	void   	   (*unfocus)   (ECellView *ecell);
} ECellClass;

GtkType    e_cell_get_type  (void);
void       e_cell_event     (ECellView *ecell_view, GdkEvent *event, int col, int row);
ECellView *e_cell_realize   (ECell *ecell, GnomeCanvas *canvas);
void       e_cell_unrealize (ECellView *ecell);
void       e_cell_draw      (ECellView *ecell, GdkDrawable *dr,
			     int col, int row, int x1, int y1, int x2, int y2);
void       e_cell_focus     (ECellView *ecell, int col, int row, int x1, int y1, int x2, int y2);
void       e_cell_unfocus   (ECellView *ecell);

#endif /* _E_CELL_H_ */
