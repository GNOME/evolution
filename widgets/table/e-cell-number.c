/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* ECellNumber - Number item for e-table.
 * Copyright (C) 2001 Ximian, Inc.
 * Author: Chris Lahey <clahey@ximian.com>
 */

#include <config.h>
#include "gal/util/e-i18n.h"
#include "e-cell-number.h"
#include <gnome.h>
#include <sys/time.h>
#include <unistd.h>
#include <gal/util/e-util.h>

#define PARENT_TYPE e_cell_text_get_type ()

static ECellTextClass *parent_class;

static char *
ecn_get_text(ECellText *cell, ETableModel *model, int col, int row)
{
	return e_format_number(GPOINTER_TO_INT (e_table_model_value_at(model, col, row)));
}

static void
ecn_free_text(ECellText *cell, char *text)
{
	g_free(text);
}

static void
e_cell_number_class_init (GtkObjectClass *object_class)
{
	ECellTextClass *ectc = (ECellTextClass *) object_class;

	parent_class = gtk_type_class (PARENT_TYPE);

	ectc->get_text  = ecn_get_text;
	ectc->free_text = ecn_free_text;
}

static void
e_cell_number_init (GtkObject *object)
{
}

/**
 * e_cell_number_new:
 * @fontname: font to be used to render on the screen
 * @justify: Justification of the string in the cell.
 *
 * Creates a new ECell renderer that can be used to render numbers that
 * that come from the model.  The value returned from the model is
 * interpreted as being an int.
 *
 * See ECellText for other features.
 * 
 * Returns: an ECell object that can be used to render numbers.
 */
ECell *
e_cell_number_new (const char *fontname, GtkJustification justify)
{
	ECellNumber *ecn = gtk_type_new (e_cell_number_get_type ());

	e_cell_text_construct(E_CELL_TEXT(ecn), fontname, justify);
      
	return (ECell *) ecn;
}

E_MAKE_TYPE(e_cell_number, "ECellNumber", ECellNumber, e_cell_number_class_init, e_cell_number_init, PARENT_TYPE);
