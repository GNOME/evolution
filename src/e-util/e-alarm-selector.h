/*
 * e-alarm-selector.h
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

#ifndef E_ALARM_SELECTOR_H
#define E_ALARM_SELECTOR_H

#include <e-util/e-source-selector.h>

/* Standard GObject macros */
#define E_TYPE_ALARM_SELECTOR \
	(e_alarm_selector_get_type ())
#define E_ALARM_SELECTOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ALARM_SELECTOR, EAlarmSelector))
#define E_ALARM_SELECTOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ALARM_SELECTOR, EAlarmSelectorClass))
#define E_IS_ALARM_SELECTOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ALARM_SELECTOR))
#define E_IS_ALARM_SELECTOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ALARM_SELECTOR))
#define E_ALARM_SELECTOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ALARM_SELECTOR, EAlarmSelectorClass))

G_BEGIN_DECLS

typedef struct _EAlarmSelector EAlarmSelector;
typedef struct _EAlarmSelectorClass EAlarmSelectorClass;
typedef struct _EAlarmSelectorPrivate EAlarmSelectorPrivate;

struct _EAlarmSelector {
	ESourceSelector parent;
	EAlarmSelectorPrivate *priv;
};

struct _EAlarmSelectorClass {
	ESourceSelectorClass parent_class;
};

GType		e_alarm_selector_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_alarm_selector_new		(ESourceRegistry *registry,
						 const gchar *extension_name);

G_END_DECLS

#endif /* E_ALARM_SELECTOR_H */
