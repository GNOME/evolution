/*
 * e-cell-float.c - Float item for e-table.
 * Copyright 2001, CodeFactory AB
 * Copyright 2001, Mikael Hallendal <micke@codefactory.se>
 *
 * Derived from e-cell-number by Chris Lahey <clahey@ximian.com>
 * ECellFloat - Float item for e-table.
 *
 * Author:
 *  Mikael Hallendal <micke@codefactory.se>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include <sys/time.h>
#include <unistd.h>

#include "e-util/e-i18n.h"
#include "e-util/e-util.h"

#include "e-cell-float.h"

#define PARENT_TYPE e_cell_text_get_type ()

static ECellTextClass *parent_class;

static char *
ecf_get_text(ECellText *cell, ETableModel *model, int col, int row)
{
	gfloat   *fvalue;
	
	fvalue = e_table_model_value_at (model, col, row);
	
	return e_format_number_float (*fvalue);
}

static void
ecf_free_text(ECellText *cell, char *text)
{
	g_free(text);
}

static void
e_cell_float_class_init (GtkObjectClass *object_class)
{
	ECellTextClass *ectc = (ECellTextClass *) object_class;

	parent_class = g_type_class_ref (PARENT_TYPE);

	ectc->get_text  = ecf_get_text;
	ectc->free_text = ecf_free_text;
}

static void
e_cell_float_init (GtkObject *object)
{
}

/**
 * e_cell_float_new:
 * @fontname: font to be used to render on the screen
 * @justify: Justification of the string in the cell.
 *
 * Creates a new ECell renderer that can be used to render floats that
 * that come from the model.  The value returned from the model is
 * interpreted as being an int.
 *
 * See ECellText for other features.
 * 
 * Returns: an ECell object that can be used to render floats.
 */
ECell *
e_cell_float_new (const char *fontname, GtkJustification justify)
{
	ECellFloat *ecn = g_object_new (E_CELL_FLOAT_TYPE, NULL);

	e_cell_text_construct(E_CELL_TEXT(ecn), fontname, justify);
      
	return (ECell *) ecn;
}

E_MAKE_TYPE(e_cell_float, "ECellFloat", ECellFloat, e_cell_float_class_init, e_cell_float_init, PARENT_TYPE)
