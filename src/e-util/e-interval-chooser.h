/*
 * e-interval-chooser.h
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

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_INTERVAL_CHOOSER_H
#define E_INTERVAL_CHOOSER_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_INTERVAL_CHOOSER \
	(e_interval_chooser_get_type ())
#define E_INTERVAL_CHOOSER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_INTERVAL_CHOOSER, EIntervalChooser))
#define E_INTERVAL_CHOOSER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_INTERVAL_CHOOSER, EIntervalChooserClass))
#define E_IS_INTERVAL_CHOOSER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_INTERVAL_CHOOSER))
#define E_IS_INTERVAL_CHOOSER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_INTERVAL_CHOOSER))
#define E_INTERVAL_CHOOSER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_INTERVAL_CHOOSER, EIntervalChooserClass))

G_BEGIN_DECLS

typedef struct _EIntervalChooser EIntervalChooser;
typedef struct _EIntervalChooserClass EIntervalChooserClass;
typedef struct _EIntervalChooserPrivate EIntervalChooserPrivate;

struct _EIntervalChooser {
	GtkBox parent;
	EIntervalChooserPrivate *priv;
};

struct _EIntervalChooserClass {
	GtkBoxClass parent_class;
};

GType		e_interval_chooser_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_interval_chooser_new		(void);
guint		e_interval_chooser_get_interval_minutes
						(EIntervalChooser *refresh);
void		e_interval_chooser_set_interval_minutes
						(EIntervalChooser *refresh,
						 guint interval_minutes);

G_END_DECLS

#endif /* E_INTERVAL_CHOOSER_H */
