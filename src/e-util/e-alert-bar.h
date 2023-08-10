/*
 * e-alert-bar.h
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

#ifndef E_ALERT_BAR_H
#define E_ALERT_BAR_H

#include <gtk/gtk.h>

#include <e-util/e-alert.h>

/* Standard GObject macros */
#define E_TYPE_ALERT_BAR \
	(e_alert_bar_get_type ())
#define E_ALERT_BAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ALERT_BAR, EAlertBar))
#define E_ALERT_BAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ALERT_BAR, EAlertBarClass))
#define E_IS_ALERT_BAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ALERT_BAR))
#define E_IS_ALERT_BAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ALERT_BAR))
#define E_ALERT_BAR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ALERT_BAR, EAlertBarClass))

G_BEGIN_DECLS

typedef struct _EAlertBar EAlertBar;
typedef struct _EAlertBarClass EAlertBarClass;
typedef struct _EAlertBarPrivate EAlertBarPrivate;

struct _EAlertBar {
	GtkInfoBar parent;
	EAlertBarPrivate *priv;
};

struct _EAlertBarClass {
	GtkInfoBarClass parent_class;
};

GType		e_alert_bar_get_type		(void) G_GNUC_CONST;
GtkWidget *	e_alert_bar_new			(void);
void		e_alert_bar_clear		(EAlertBar *alert_bar);
void		e_alert_bar_add_alert		(EAlertBar *alert_bar,
						 EAlert *alert);
gboolean	e_alert_bar_remove_alert_by_tag	(EAlertBar *alert_bar,
						 const gchar *tag);
gboolean	e_alert_bar_close_alert		(EAlertBar *alert_bar);
void		e_alert_bar_submit_alert	(EAlertBar *alert_bar,
						 EAlert *alert);

G_END_DECLS

#endif /* E_ALERT_BAR_H */
