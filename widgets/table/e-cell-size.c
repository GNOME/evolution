/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* ECellSize - Size item for e-table.
 * Copyright (C) 2001 Ximian, Inc.
 * Author: Chris Lahey <clahey@ximian.com>
 */

#include <config.h>
#include "e-cell-size.h"
#include <gnome.h>
#include <sys/time.h>
#include <unistd.h>
#include <gal/util/e-util.h>

#define PARENT_TYPE e_cell_text_get_type ()

static ECellTextClass *parent_class;

static char *
ecd_get_text(ECellText *cell, ETableModel *model, int col, int row)
{
	gint size = GPOINTER_TO_INT(e_table_model_value_at(model, col, row));
	gfloat fsize;
	
	if (size < 1024) {
		return g_strdup_printf ("%d", size);
	} else {
		fsize = ((gfloat) size) / 1024.0;
		if (fsize < 1024.0) {
			return g_strdup_printf ("%.2f K", fsize);
		} else {
			fsize /= 1024.0;
			return g_strdup_printf ("%.2f M", fsize);
		}
	}
}

static void
ecd_free_text(ECellText *cell, char *text)
{
	g_free(text);
}

static void
e_cell_size_class_init (GtkObjectClass *object_class)
{
	ECellTextClass *ectc = (ECellTextClass *) object_class;

	parent_class = gtk_type_class (PARENT_TYPE);

	ectc->get_text  = ecd_get_text;
	ectc->free_text = ecd_free_text;
}

static void
e_cell_size_init (GtkObject *object)
{
}

/**
 * e_cell_size_new:
 * @fontname: font to be used to render on the screen
 * @justify: Justification of the string in the cell.
 *
 * Creates a new ECell renderer that can be used to render file sizes
 * that that come from the model.  The value returned from the model
 * is interpreted as being a time_t.
 *
 * The ECellSize object support a large set of properties that can be
 * configured through the Gtk argument system and allows the user to have
 * a finer control of the way the string is displayed.  The arguments supported
 * allow the control of strikeout, bold, color and a size filter.
 *
 * The arguments "strikeout_column", "bold_column" and "color_column" set
 * and return an integer that points to a column in the model that controls
 * these settings.  So controlling the way things are rendered is achieved
 * by having special columns in the model that will be used to flag whether
 * the size should be rendered with strikeout, or bolded.   In the case of
 * the "color_column" argument, the column in the model is expected to have
 * a string that can be parsed by gdk_color_parse().
 * 
 * Returns: an ECell object that can be used to render file sizes.  */
ECell *
e_cell_size_new (const char *fontname, GtkJustification justify)
{
	ECellSize *ecd = gtk_type_new (e_cell_size_get_type ());

	e_cell_text_construct(E_CELL_TEXT(ecd), fontname, justify);
      
	return (ECell *) ecd;
}

E_MAKE_TYPE(e_cell_size, "ECellSize", ECellSize, e_cell_size_class_init, e_cell_size_init, PARENT_TYPE);
