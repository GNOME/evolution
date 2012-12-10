/*
 * e-alert-sink.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 */

#ifndef E_ALERT_SINK_H
#define E_ALERT_SINK_H

#include <gtk/gtk.h>

#include <e-util/e-alert.h>

/* Standard GObject macros */
#define E_TYPE_ALERT_SINK \
	(e_alert_sink_get_type ())
#define E_ALERT_SINK(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ALERT_SINK, EAlertSink))
#define E_ALERT_SINK_INTERFACE(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ALERT_SINK, EAlertSinkInterface))
#define E_IS_ALERT_SINK(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ALERT_SINK))
#define E_IS_ALERT_SINK_INTERFACE(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ALERT_SINK))
#define E_ALERT_SINK_GET_INTERFACE(obj) \
	(G_TYPE_INSTANCE_GET_INTERFACE \
	((obj), E_TYPE_ALERT_SINK, EAlertSinkInterface))

G_BEGIN_DECLS

typedef struct _EAlertSink EAlertSink;
typedef struct _EAlertSinkInterface EAlertSinkInterface;

struct _EAlertSinkInterface {
	GTypeInterface parent_interface;

	void		(*submit_alert)		(EAlertSink *alert_sink,
						 EAlert *alert);
};

GType		e_alert_sink_get_type		(void);
void		e_alert_sink_submit_alert	(EAlertSink *alert_sink,
						 EAlert *alert);

G_END_DECLS

#endif /* E_ALERT_SINK_H */
