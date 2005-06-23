/* 
 * e-cell-date.c - Date item for e-table.
 * Copyright 2001, Ximian, Inc.
 *
 * Author:
 *  Chris Lahey <clahey@ximian.com>
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
#include <time.h>
#include <unistd.h>
#include <string.h>

#include "e-util/e-i18n.h"
#include "e-util/e-util.h"
#include "misc/e-unicode.h"

#include "e-cell-date.h"

#define PARENT_TYPE e_cell_text_get_type ()

#ifdef G_OS_WIN32
/* The localtime() in Microsoft's C library *is* thread-safe */
#define localtime_r(timep, result)  (localtime (timep) ? memcpy ((result), localtime (timep), sizeof (*(result))) : 0)
#endif

static ECellTextClass *parent_class;

static char *
ecd_get_text(ECellText *cell, ETableModel *model, int col, int row)
{
	time_t date = GPOINTER_TO_INT (e_table_model_value_at(model, col, row));
	time_t nowdate = time(NULL);
	time_t yesdate;
	struct tm then, now, yesterday;
	char buf[100];
	char *temp;
	gboolean done = FALSE;

	if (date == 0) {
		return g_strdup (_("?"));
	}

	localtime_r (&date, &then);
	localtime_r (&nowdate, &now);

	if (nowdate - date < 60 * 60 * 8 && nowdate > date) {
		e_utf8_strftime_fix_am_pm (buf, 100, _("%l:%M %p"), &then);
		done = TRUE;
	}

	if (!done) {
		if (then.tm_mday == now.tm_mday &&
		    then.tm_mon == now.tm_mon &&
		    then.tm_year == now.tm_year) {
			e_utf8_strftime_fix_am_pm (buf, 100, _("Today %l:%M %p"), &then);
			done = TRUE;
		}
	}
	if (!done) {
		yesdate = nowdate - 60 * 60 * 24;
		localtime_r (&yesdate, &yesterday);
		if (then.tm_mday == yesterday.tm_mday &&
		    then.tm_mon == yesterday.tm_mon &&
		    then.tm_year == yesterday.tm_year) {
			e_utf8_strftime_fix_am_pm (buf, 100, _("Yesterday %l:%M %p"), &then);
			done = TRUE;
		}
	}
	if (!done) {
		int i;
		for (i = 2; i < 7; i++) {
			yesdate = nowdate - 60 * 60 * 24 * i;
			localtime_r (&yesdate, &yesterday);
			if (then.tm_mday == yesterday.tm_mday &&
			    then.tm_mon == yesterday.tm_mon &&
			    then.tm_year == yesterday.tm_year) {
				e_utf8_strftime_fix_am_pm (buf, 100, _("%a %l:%M %p"), &then);
				done = TRUE;
				break;
			}
		}
	}
	if (!done) {
		if (then.tm_year == now.tm_year) {
			e_utf8_strftime_fix_am_pm (buf, 100, _("%b %d %l:%M %p"), &then);
		} else {
			e_utf8_strftime_fix_am_pm (buf, 100, _("%b %d %Y"), &then);
		}
	}
	temp = buf;
	while ((temp = strstr (temp, "  "))) {
		memmove (temp, temp + 1, strlen (temp));
	}
	temp = e_strdup_strip (buf);
	return temp;
}

static void
ecd_free_text(ECellText *cell, char *text)
{
	g_free(text);
}

static void
e_cell_date_class_init (GtkObjectClass *object_class)
{
	ECellTextClass *ectc = (ECellTextClass *) object_class;

	parent_class = g_type_class_ref (PARENT_TYPE);

	ectc->get_text  = ecd_get_text;
	ectc->free_text = ecd_free_text;
}

static void
e_cell_date_init (GtkObject *object)
{
}

/**
 * e_cell_date_new:
 * @fontname: font to be used to render on the screen
 * @justify: Justification of the string in the cell.
 *
 * Creates a new ECell renderer that can be used to render dates that
 * that come from the model.  The value returned from the model is
 * interpreted as being a time_t.
 *
 * The ECellDate object support a large set of properties that can be
 * configured through the Gtk argument system and allows the user to have
 * a finer control of the way the string is displayed.  The arguments supported
 * allow the control of strikeout, bold, color and a date filter.
 *
 * The arguments "strikeout_column", "underline_column", "bold_column"
 * and "color_column" set and return an integer that points to a
 * column in the model that controls these settings.  So controlling
 * the way things are rendered is achieved by having special columns
 * in the model that will be used to flag whether the date should be
 * rendered with strikeout, underline, or bolded.  In the case of the
 * "color_column" argument, the column in the model is expected to
 * have a string that can be parsed by gdk_color_parse().
 * 
 * Returns: an ECell object that can be used to render dates.
 */
ECell *
e_cell_date_new (const char *fontname, GtkJustification justify)
{
	ECellDate *ecd = g_object_new (E_CELL_DATE_TYPE, NULL);

	e_cell_text_construct(E_CELL_TEXT(ecd), fontname, justify);
      
	return (ECell *) ecd;
}

E_MAKE_TYPE(e_cell_date, "ECellDate", ECellDate, e_cell_date_class_init, e_cell_date_init, PARENT_TYPE)
