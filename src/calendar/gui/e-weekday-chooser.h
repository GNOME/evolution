/*
 * e-weekday-chooser.h
 *
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
 */

#ifndef E_WEEKDAY_CHOOSER_H
#define E_WEEKDAY_CHOOSER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Standard GObject macros */
#define E_TYPE_WEEKDAY_CHOOSER e_weekday_chooser_get_type ()
G_DECLARE_FINAL_TYPE (EWeekdayChooser, e_weekday_chooser, E, WEEKDAY_CHOOSER, GtkBox)

GtkWidget *	e_weekday_chooser_new		(void);
gboolean	e_weekday_chooser_get_selected	(EWeekdayChooser *chooser,
						 GDateWeekday weekday);
void		e_weekday_chooser_set_selected	(EWeekdayChooser *chooser,
						 GDateWeekday weekday,
						 gboolean selected);
GDateWeekday	e_weekday_chooser_get_week_start_day
						(EWeekdayChooser *chooser);
void		e_weekday_chooser_set_week_start_day
						(EWeekdayChooser *chooser,
						 GDateWeekday week_start_day);

G_END_DECLS

#endif /* E_WEEKDAY_CHOOSER_H */

