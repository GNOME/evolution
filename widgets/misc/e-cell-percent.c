/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Author :
 *  Damon Chaplin <damon@ximian.com>
 *
 * Copyright 2001, Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

/*
 * ECellPercent - a subclass of ECellText used to show an integer percentage
 * in an ETable.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>

#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <libgnomeui/gnome-messagebox.h>
#include <libgnome/gnome-i18n.h>

#include "e-cell-percent.h"

G_DEFINE_TYPE (ECellPercent, e_cell_percent, E_CELL_TEXT_TYPE);


static char *
ecp_get_text (ECellText *cell, ETableModel *model, int col, int row)
{
	int percent;
	static char buffer[8];

	percent = GPOINTER_TO_INT (e_table_model_value_at (model, col, row));

	/* A -ve value means the property is not set. */
	if (percent < 0) {
		buffer[0] = '\0';
	} else {
		g_snprintf (buffer, sizeof (buffer), "%i%%", percent);
	}

	return buffer;
}

static void
ecp_free_text(ECellText *cell, char *text)
{
	/* Do Nothing. */
}

/* FIXME: We need to set the "transient_for" property for the dialog. */
static void
show_percent_warning (void)
{
	GtkWidget *dialog;

	dialog = gnome_message_box_new (_("The percent value must be between 0 and 100, inclusive"),
					GNOME_MESSAGE_BOX_ERROR,
					GNOME_STOCK_BUTTON_OK, NULL);
	gtk_widget_show (dialog);
}

static void
ecp_set_value (ECellText *cell, ETableModel *model, int col, int row,
	       const char *text)
{
	int matched, percent;
	gboolean empty = TRUE;
	const char *p;

	if (text) {
		p = text;
		while (*p) {
			if (!isspace ((unsigned char) *p)) {
				empty = FALSE;
				break;
			}
			p++;
		}
	}

	if (empty) {
		percent = -1;
	} else {
		matched = sscanf (text, "%i", &percent);

		if (matched != 1 || percent < 0 || percent > 100) {
			show_percent_warning ();
			return;
		}
	}

	e_table_model_set_value_at (model, col, row,
				    GINT_TO_POINTER (percent));
}

static void
e_cell_percent_class_init (ECellPercentClass *ecpc) 
{
	ECellTextClass *ectc = (ECellTextClass *) ecpc;

	ectc->get_text  = ecp_get_text;
	ectc->free_text = ecp_free_text;
	ectc->set_value = ecp_set_value;
}

static void
e_cell_percent_init (ECellPercent *ecp)
{
}

/**
 * e_cell_percent_new:
 * @fontname: font to be used to render on the screen
 * @justify: Justification of the string in the cell.
 *
 * Creates a new ECell renderer that can be used to render an integer
 * percentage that comes from the model.  The value returned from the model is
 * interpreted as being an int.
 *
 * See ECellText for other features.
 * 
 * Returns: an ECell object that can be used to render numbers.
 */
ECell *
e_cell_percent_new (const char *fontname, GtkJustification justify)
{
	ECellPercent *ecn = g_object_new (E_CELL_PERCENT_TYPE, NULL);

	e_cell_text_construct (E_CELL_TEXT(ecn), fontname, justify);
      
	return (ECell *) ecn;
}
