#ifndef _E_CELL_H_
#define _E_CELL_H_

#include <libgnomeui/gnome-canvas.h>

#define E_CELL_TYPE        (e_cell_get_type ())
#define E_CELL(o)          (GTK_CHECK_CAST ((o), E_CELL_TYPE, ECell))
#define E_CELL_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_CELL_TYPE, ECellClass))
#define E_IS_CELL(o)       (GTK_CHECK_TYPE ((o), E_CELL_TYPE))
#define E_IS_CELL_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_CELL_TYPE))

typedef struct {
	GtkObject       object;
} ECell;

typedef struct {
	GtkObjectClass parent_class;

	void (*realize)   (ECell *, GnomeCanvas *canvas);
	void (*unrealize) (ECell *);
	void (*draw)      (ECell *ecell, int x1, int y1, int x2, int y2);
	gint (*event)     (ECell *ecell, GdkEvent *event);
} ECellClass;

GtkType    e_cell_get_type (void);

#endif /* _E_CELL_H_ */
