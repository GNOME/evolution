/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_H_
#define _E_TABLE_H_

#include <libgnomeui/gnome-canvas.h>
#include <gtk/gtktable.h>
#include <gnome-xml/tree.h>
#include "e-table-model.h"
#include "e-table-header.h"
#include "e-table-group.h"

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

	GnomeCanvas *header_canvas, *table_canvas;

	GnomeCanvasItem *header_item, *root;

	xmlDoc *specification;
	
	guint draw_grid:1;
	guint draw_focus:1;
	guint spreadsheet:1;
} ETable;

typedef struct {
	GtkTableClass parent_class;
} ETableClass;

GtkType    e_table_get_type  (void);
void       e_table_construct (ETable *e_table, ETableHeader *full_header, ETableModel *etm,
			      const char *spec);
GtkWidget *e_table_new       (ETableHeader *full_header, ETableModel *etm,
			      const char *spec);


END_GNOME_DECLS

 #endif /* _E_TABLE_H_ */
