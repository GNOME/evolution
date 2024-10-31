/*
 * SPDX-FileCopyrightText: (C) 2021 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <libecal/libecal.h>

#include "e-cell-estimated-duration.h"

G_DEFINE_TYPE (ECellEstimatedDuration, e_cell_estimated_duration, E_TYPE_CELL_TEXT)

static gchar *
eced_get_text (ECellText *cell,
	       ETableModel *model,
	       gint col,
	       gint row)
{
	gint64 *pvalue = e_table_model_value_at (model, col, row);
	gchar *res;

	if (!pvalue || *pvalue == 0) {
		e_table_model_free_value (model, col, pvalue);
		return g_strdup ("");
	}

	res = e_cal_util_seconds_to_string (*pvalue);

	e_table_model_free_value (model, col, pvalue);

	return res;
}

static void
eced_free_text (ECellText *cell,
		ETableModel *model,
		gint col,
		gchar *text)
{
	g_free (text);
}

static void
e_cell_estimated_duration_class_init (ECellEstimatedDurationClass *klass)
{
	ECellTextClass *ectc = E_CELL_TEXT_CLASS (klass);

	ectc->get_text = eced_get_text;
	ectc->free_text = eced_free_text;
}

static void
e_cell_estimated_duration_init (ECellEstimatedDuration *self)
{
	g_object_set (self, "use-tabular-numbers", TRUE, NULL);
}

/**
 * e_cell_estimated_duration_new:
 * @fontname: (nullable): font to be used to render on the screen
 * @justify: Justification of the string in the cell.
 *
 * Creates a new ECell renderer that can be used to render estimated duration
 *
 * Returns: an ECell object that can be used to render estimated duration.
 *
 * Since: 3.44
 */
ECell *
e_cell_estimated_duration_new (const gchar *fontname,
			       GtkJustification justify)
{
	ECellEstimatedDuration *self = g_object_new (E_TYPE_CELL_ESTIMATED_DURATION, NULL);

	e_cell_text_construct (E_CELL_TEXT (self), fontname, justify);

	return E_CELL (self);
}
