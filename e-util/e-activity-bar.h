/*
 * e-activity-bar.h
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

#ifndef E_ACTIVITY_BAR_H
#define E_ACTIVITY_BAR_H

#include <gtk/gtk.h>
#include <e-util/e-activity.h>

/* Standard GObject macros */
#define E_TYPE_ACTIVITY_BAR \
	(e_activity_bar_get_type ())
#define E_ACTIVITY_BAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ACTIVITY_BAR, EActivityBar))
#define E_ACTIVITY_BAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ACTIVITY_BAR, EActivityBarClass))
#define E_IS_ACTIVITY_BAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ACTIVITY_BAR))
#define E_IS_ACTIVITY_BAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ACTIVITY_BAR))
#define E_ACTIVITY_BAR_GET_CLASS(obj) \
	(G_TYPE_CHECK_INSTANCE_GET_TYPE \
	((obj), E_TYPE_ACTIVITY_BAR, EActivityBarClass))

G_BEGIN_DECLS

typedef struct _EActivityBar EActivityBar;
typedef struct _EActivityBarClass EActivityBarClass;
typedef struct _EActivityBarPrivate EActivityBarPrivate;

struct _EActivityBar {
	GtkInfoBar parent;
	EActivityBarPrivate *priv;
};

struct _EActivityBarClass {
	GtkInfoBarClass parent_class;
};

GType		e_activity_bar_get_type		(void) G_GNUC_CONST;
GtkWidget *	e_activity_bar_new		(void);
EActivity *	e_activity_bar_get_activity	(EActivityBar *bar);
void		e_activity_bar_set_activity	(EActivityBar *bar,
						 EActivity *activity);

G_END_DECLS

#endif /* E_ACTIVITY_BAR_H */
