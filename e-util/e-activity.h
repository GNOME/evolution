/*
 * e-activity.h
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_ACTIVITY_H
#define E_ACTIVITY_H

#include <gtk/gtk.h>

#include <e-util/e-alert-sink.h>
#include <e-util/e-util-enums.h>

/* Standard GObject macros */
#define E_TYPE_ACTIVITY \
	(e_activity_get_type ())
#define E_ACTIVITY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ACTIVITY, EActivity))
#define E_ACTIVITY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ACTIVITY, EActivityClass))
#define E_IS_ACTIVITY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ACTIVITY))
#define E_IS_ACTIVITY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ACTIVITY))
#define E_ACTIVITY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ACTIVITY, EActivityClass))

G_BEGIN_DECLS

typedef struct _EActivity EActivity;
typedef struct _EActivityClass EActivityClass;
typedef struct _EActivityPrivate EActivityPrivate;

struct _EActivity {
	GObject parent;
	EActivityPrivate *priv;
};

struct _EActivityClass {
	GObjectClass parent_class;

	/* Methods */
	gchar *		(*describe)		(EActivity *activity);
};

GType		e_activity_get_type		(void) G_GNUC_CONST;
EActivity *	e_activity_new			(void);
void		e_activity_cancel		(EActivity *activity);
gchar *		e_activity_describe		(EActivity *activity);
EAlertSink *	e_activity_get_alert_sink	(EActivity *activity);
void		e_activity_set_alert_sink	(EActivity *activity,
						 EAlertSink *alert_sink);
GCancellable *	e_activity_get_cancellable	(EActivity *activity);
void		e_activity_set_cancellable	(EActivity *activity,
						 GCancellable *cancellable);
const gchar *	e_activity_get_icon_name	(EActivity *activity);
void		e_activity_set_icon_name	(EActivity *activity,
						 const gchar *icon_name);
gdouble		e_activity_get_percent		(EActivity *activity);
void		e_activity_set_percent		(EActivity *activity,
						 gdouble percent);
EActivityState	e_activity_get_state		(EActivity *activity);
void		e_activity_set_state		(EActivity *activity,
						 EActivityState state);
const gchar *	e_activity_get_text		(EActivity *activity);
void		e_activity_set_text		(EActivity *activity,
						 const gchar *text);
const gchar *	e_activity_get_last_known_text	(EActivity *activity);
gboolean	e_activity_handle_cancellation	(EActivity *activity,
						 const GError *error);

G_END_DECLS

#endif /* E_ACTIVITY_H */
