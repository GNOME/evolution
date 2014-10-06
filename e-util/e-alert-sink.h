/*
 * e-alert-sink.h
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

GType		e_alert_sink_get_type		(void) G_GNUC_CONST;
void		e_alert_sink_submit_alert	(EAlertSink *alert_sink,
						 EAlert *alert);

struct _EActivity;

struct _EAlertSinkThreadJobData;
typedef struct _EAlertSinkThreadJobData EAlertSinkThreadJobData;

typedef void	(* EAlertSinkThreadJobFunc)	(EAlertSinkThreadJobData *job_data,
						 gpointer user_data,
						 GCancellable *cancellable,
						 GError **error);

struct _EActivity *
		e_alert_sink_submit_thread_job	(EAlertSink *alert_sink,
						 const gchar *description,
						 const gchar *alert_ident,
						 const gchar *alert_arg_0,
						 EAlertSinkThreadJobFunc func,
						 gpointer user_data,
						 GDestroyNotify free_user_data);
void		e_alert_sink_thread_job_set_alert_ident
						(EAlertSinkThreadJobData *job_data,
						 const gchar *alert_ident);
void		e_alert_sink_thread_job_set_alert_arg_0
						(EAlertSinkThreadJobData *job_data,
						 const gchar *alert_arg_0);

G_END_DECLS

#endif /* E_ALERT_SINK_H */
