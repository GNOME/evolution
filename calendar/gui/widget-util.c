/* Evolution calendar - Widget utilities
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Author: Federico Mena-Quintero <federico@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <widgets/misc/e-dateedit.h>
#include "calendar-config.h"
#include "widget-util.h"



/**
 * date_edit_new:
 * @show_date: Whether to show a date picker in the widget.
 * @show_time: Whether to show a time picker in the widget.
 * 
 * Creates a new #EDateEdit widget, configured using the calendar's preferences.
 * 
 * Return value: A newly-created #EDateEdit widget.
 **/
GtkWidget *
date_edit_new (gboolean show_date, gboolean show_time)
{
	EDateEdit *dedit;

	dedit = E_DATE_EDIT (e_date_edit_new ());

	e_date_edit_set_show_date (dedit, show_date);
	e_date_edit_set_show_time (dedit, show_time);
	e_date_edit_set_time_popup_range (dedit,
					  calendar_config_get_day_start_hour (),
					  calendar_config_get_day_end_hour ());
	e_date_edit_set_week_start_day (dedit, (calendar_config_get_week_start_day () + 6) % 7);
	e_date_edit_set_show_week_numbers (dedit, calendar_config_get_dnav_show_week_no ());

	return GTK_WIDGET (dedit);
}
