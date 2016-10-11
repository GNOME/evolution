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

#include <libgnomecanvas/libgnomecanvas.h>

/* Standard GObject macros */
#define E_TYPE_WEEKDAY_CHOOSER \
	(e_weekday_chooser_get_type ())
#define E_WEEKDAY_CHOOSER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_WEEKDAY_CHOOSER, EWeekdayChooser))
#define E_WEEKDAY_CHOOSER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_WEEKDAY_CHOOSER, EWeekdayChooserClass))
#define E_IS_WEEKDAY_CHOOSER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_WEEKDAY_CHOOSER))
#define E_IS_WEEKDAY_CHOOSER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_WEEKDAY_CHOOSER))
#define E_WEEKDAY_CHOOSER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_WEEKDAY_CHOOSER, EWeekdayChooser))

G_BEGIN_DECLS

typedef struct _EWeekdayChooser EWeekdayChooser;
typedef struct _EWeekdayChooserClass EWeekdayChooserClass;
typedef struct _EWeekdayChooserPrivate EWeekdayChooserPrivate;

struct _EWeekdayChooser {
	GnomeCanvas canvas;
	EWeekdayChooserPrivate *priv;
};

struct _EWeekdayChooserClass {
	GnomeCanvasClass parent_class;

	void		(*changed)		(EWeekdayChooser *chooser);
};

GType		e_weekday_chooser_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_weekday_chooser_new		(void);
gboolean	e_weekday_chooser_get_selected	(EWeekdayChooser *chooser,
						 GDateWeekday weekday);
void		e_weekday_chooser_set_selected	(EWeekdayChooser *chooser,
						 GDateWeekday weekday,
						 gboolean selected);
gboolean	e_weekday_chooser_get_blocked	(EWeekdayChooser *chooser,
						 GDateWeekday weekday);
void		e_weekday_chooser_set_blocked	(EWeekdayChooser *chooser,
						 GDateWeekday weekday,
						 gboolean blocked);
GDateWeekday	e_weekday_chooser_get_week_start_day
						(EWeekdayChooser *chooser);
void		e_weekday_chooser_set_week_start_day
						(EWeekdayChooser *chooser,
						 GDateWeekday week_start_day);

G_END_DECLS

#endif /* E_WEEKDAY_CHOOSER_H */

