/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/*
 * ECellPercent - a subclass of ECellText used to show an integer percentage
 * in an ETable.
 */

#include "evolution-config.h"

#include <ctype.h>

#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <glib/gi18n.h>

#include "e-cell-percent.h"

G_DEFINE_TYPE (ECellPercent, e_cell_percent, E_TYPE_CELL_TEXT)

static gchar *
ecp_get_text (ECellText *cell,
              ETableModel *model,
              gint col,
              gint row)
{
	gint percent;
	static gchar buffer[8];

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
ecp_free_text (ECellText *cell,
	       ETableModel *model,
	       gint col,
               gchar *text)
{
	/* Do Nothing. */
}

/* FIXME: We need to set the "transient_for" property for the dialog. */
static void
show_percent_warning (void)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (
		NULL, 0,
		GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
		"%s", _("The percent value must be between 0 and 100, inclusive"));
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static void
ecp_set_value (ECellText *cell,
               ETableModel *model,
               gint col,
               gint row,
               const gchar *text)
{
	gint matched, percent;
	gboolean empty = TRUE;
	const gchar *p;

	if (text) {
		p = text;
		while (*p) {
			if (!isspace ((guchar) *p)) {
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

	e_table_model_set_value_at (
		model, col, row,
		GINT_TO_POINTER (percent));
}

static void
e_cell_percent_class_init (ECellPercentClass *ecpc)
{
	ECellTextClass *ectc = (ECellTextClass *) ecpc;

	ectc->get_text = ecp_get_text;
	ectc->free_text = ecp_free_text;
	ectc->set_value = ecp_set_value;
}

static void
e_cell_percent_init (ECellPercent *ecp)
{
	g_object_set (ecp, "use-tabular-numbers", TRUE, NULL);
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
e_cell_percent_new (const gchar *fontname,
                    GtkJustification justify)
{
	ECellPercent *ecn = g_object_new (E_TYPE_CELL_PERCENT, NULL);

	e_cell_text_construct (E_CELL_TEXT (ecn), fontname, justify);

	return (ECell *) ecn;
}
