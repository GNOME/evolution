/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001 CodeFactory AB
 * Copyright (C) 2001 Mikael Hallendal <micke@codefactory.se>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Derived from e-cell-number by Chris Lahey <clahey@ximian.com>
 * ECellFloat - Float item for e-table.
 *
 * Author: Mikael Hallendal <micke@codefactory.se>
 */

#include <config.h>
#include <sys/time.h>
#include <unistd.h>
#include <gal/util/e-util.h>
#include <gal/util/e-i18n.h>
#include "e-cell-float.h"

#define PARENT_TYPE e_cell_text_get_type ()

static ECellTextClass *parent_class;

static char *
ecn_get_text(ECellText *cell, ETableModel *model, int col, int row)
{
	gfloat   *fvalue;
	
	fvalue = e_table_model_value_at (model, col, row);
	
	return e_format_number_float (*fvalue);
}

static void
ecn_free_text(ECellText *cell, char *text)
{
	g_free(text);
}

static void
e_cell_float_class_init (GtkObjectClass *object_class)
{
	ECellTextClass *ectc = (ECellTextClass *) object_class;

	parent_class = gtk_type_class (PARENT_TYPE);

	ectc->get_text  = ecn_get_text;
	ectc->free_text = ecn_free_text;
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
	ECellFloat *ecn = gtk_type_new (e_cell_float_get_type ());

	e_cell_text_construct(E_CELL_TEXT(ecn), fontname, justify);
      
	return (ECell *) ecn;
}

E_MAKE_TYPE(e_cell_float, "ECellFloat", ECellFloat, e_cell_float_class_init, e_cell_float_init, PARENT_TYPE);
