#ifndef _E_CELL_CHECKBOX_H_
#define _E_CELL_CHECKBOX_H_

#include <libgnomeui/gnome-canvas.h>
#include "e-cell.h"

#define E_CELL_CHECKBOX_TYPE        (e_cell_checkbox_get_type ())
#define E_CELL_CHECKBOX(o)          (GTK_CHECK_CAST ((o), E_CELL_CHECKBOX_TYPE, ECellCheckbox))
#define E_CELL_CHECKBOX_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_CELL_CHECKBOX_TYPE, ECellCheckboxClass))
#define E_IS_CELL_CHECKBOX(o)       (GTK_CHECK_TYPE ((o), E_CELL_CHECKBOX_TYPE))
#define E_IS_CELL_CHECKBOX_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_CELL_CHECKBOX_TYPE))

typedef struct {
	ECell parent;
} ECellCheckbox;

typedef struct {
	ECellClass parent_class;
} ECellCheckboxClass;

GtkType    e_cell_checkbox_get_type (void);
ECell     *e_cell_checkbox_new      (ETableModel *model);

#endif /* _E_CELL_CHECKBOX_H_ */

