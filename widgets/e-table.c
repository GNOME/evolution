/*
 * E-table-view.c: A graphical view of a Table.
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * Copyright 1999, International GNOME Support
 */
#include <config.h>
#include <libgnomeui/gnome-canvas.h>
#include "e-table.h"

#define PARENT_OBJECT_TYPE gnome_canvas_get_type ()

GtkType
e_table_get_type (void)
{
	static GtkType type = 0;

	if (!type){
		GtkTypeInfo info = {
			"ETable",
			sizeof (ETable),
			sizeof (ETableClass),
			(GtkClassInitFunc) e_table_class_init,
			(GtkObjectInitFunc) NULL,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (PARENT_OBJECT_TYPE, &info);
	}

	return type;
}
