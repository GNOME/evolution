#ifndef _E_CELL_TOGGLE_H_
#define _E_CELL_TOGGLE_H_

#include <libgnomeui/gnome-canvas.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "e-cell.h"

#define E_CELL_TOGGLE_TYPE        (e_cell_toggle_get_type ())
#define E_CELL_TOGGLE(o)          (GTK_CHECK_CAST ((o), E_CELL_TOGGLE_TYPE, ECellToggle))
#define E_CELL_TOGGLE_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_CELL_TOGGLE_TYPE, ECellToggleClass))
#define E_IS_CELL_TOGGLE(o)       (GTK_CHECK_TYPE ((o), E_CELL_TOGGLE_TYPE))
#define E_IS_CELL_TOGGLE_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_CELL_TOGGLE_TYPE))

typedef struct {
	ECell parent;

	int        border;
	int        n_states;
	GdkPixbuf **images;

	int        height;
} ECellToggle;

typedef struct {
	ECellClass parent_class;
} ECellToggleClass;

GtkType    e_cell_toggle_get_type  (void);
ECell     *e_cell_toggle_new       (int border, int n_states, GdkPixbuf **images);
void       e_cell_toggle_construct (ECellToggle *etog, int border,
				    int n_states, GdkPixbuf **images);

#endif /* _E_CELL_TOGGLE_H_ */


