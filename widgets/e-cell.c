/*
 * e-cell.c: base class for cell renderers in e-table
 *
 * Author:
 *   Miguel de Icaza (miguel@kernel.org)
 *
 * (C) 1999 Helix Code, Inc
 */
#include <config.h>
#include "e-cell.h"
#include "e-util.h"

#define PARENT_TYPE gtk_object_get_type()

static void
ec_realize (ECell *e_cell, GnomeCanvas *canvas)
{
}

static void
ec_unrealize (ECell *e_cell)
{
}

static void
ec_draw (ECell *ecell, int x1, int y1, int x2, int y2)
{
}

static gint
ec_event (ECell *ecell, GdkEvent *event)
{
}

static void
e_cell_class_init (GtkObjectClass *object_class)
{
	ECellClass *ecc = (ECellClass *) object_class;

	ecc->realize = ec_realize;
	ecc->unrealize = ec_unrealize;
	ecc->draw = ec_draw;
	ecc->event = ec_event;
}

E_MAKE_TYPE(e_cell, "ECell", ECell, e_cell_class_init, NULL, PARENT_TYPE);

	
