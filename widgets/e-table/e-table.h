#ifndef _E_TABLE_H_
#define _E_TABLE_H_

#include <libgnomeui/gnome-canvas.h>
#include <gtk/gtktable.h>
#include "e-table-model.h"
#include "e-table-header.h"

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

	GnomeCanvas *header_canvas, *table_canvas;

	GnomeCanvasItem *header_item, *root;
	
	guint draw_grid:1;
	guint draw_focus:1;
	guint spreadsheet:1;

	char *group_spec;

	/*
	 * Used during table generation
	 */
	int gen_header_width;
} ETable;

typedef struct {
	GtkTableClass parent_class;
} ETableClass;

GtkType    e_table_get_type  (void);
void       e_table_construct (ETable *e_table, ETableHeader *full_header, ETableModel *etm,
			      const char *cols_spec, const char *group_spec);
GtkWidget *e_table_new       (ETableHeader *full_header, ETableModel *etm,
			      const char *cols_spec, const char *group_spec);


END_GNOME_DECLS

#endif /* _E_TABLE_H_ */
