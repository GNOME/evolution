#ifndef _E_CELL_TEXT_H_
#define _E_CELL_TEXT_H_

#include <libgnomeui/gnome-canvas.h>
#include "e-cell.h"

#define E_CELL_TEXT_TYPE        (e_cell_text_get_type ())
#define E_CELL_TEXT(o)          (GTK_CHECK_CAST ((o), E_CELL_TEXT_TYPE, ECellText))
#define E_CELL_TEXT_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_CELL_TEXT_TYPE, ECellTextClass))
#define E_IS_CELL_TEXT(o)       (GTK_CHECK_TYPE ((o), E_CELL_TEXT_TYPE))
#define E_IS_CELL_TEXT_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_CELL_TEXT_TYPE))

typedef struct {
	ECell parent;

	GdkGC            *gc;
	GdkFont          *font;
	GtkJustification  justify;

	char             *font_name;
	GnomeCanvas      *canvas;
} ECellText;

typedef struct {
	ECellClass parent_class;
} ECellTextClass;

GtkType    e_cell_text_get_type (void);
ECell     *e_cell_text_new      (const char *fontname, GtkJustification justify);

#endif /* _E_CELL_TEXT_H_ */
