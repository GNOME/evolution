/*
 * E-table-view.c: A graphical view of a Table.
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * Copyright 1999, Helix Code, Inc
 */
#include <config.h>
#include <libgnomeui/gnome-canvas.h>
#include "e-table.h"
#include "e-util.h"

#define PARENT_OBJECT_TYPE gnome_canvas_get_type ()

E_MAKE_TYPE(e_table, "ETable", ETable, e_table_class_init, NULL, PARENT_TYPE);

ETable *
e_table_new (ETableHeader *eth, ETableModel *etm)
{
}

