/*
 * e-alert-activity.h
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_ALERT_ACTIVITY_H
#define E_ALERT_ACTIVITY_H

#include <e-util/e-timeout-activity.h>

/* Standard GObject macros */
#define E_TYPE_ALERT_ACTIVITY \
	(e_alert_activity_get_type ())
#define E_ALERT_ACTIVITY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ALERT_ACTIVITY, EAlertActivity))
#define E_ALERT_ACTIVITY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ALERT_ACTIVITY, EAlertActivityClass))
#define E_IS_ALERT_ACTIVITY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ALERT_ACTIVITY))
#define E_IS_ALERT_ACTIVITY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ALERT_ACTIVITY))
#define E_ALERT_ACTIVITY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ALERT_ACTIVITY, EAlertActivityClass))

G_BEGIN_DECLS

typedef struct _EAlertActivity EAlertActivity;
typedef struct _EAlertActivityClass EAlertActivityClass;
typedef struct _EAlertActivityPrivate EAlertActivityPrivate;

struct _EAlertActivity {
	ETimeoutActivity parent;
	EAlertActivityPrivate *priv;
};

struct _EAlertActivityClass {
	ETimeoutActivityClass parent_class;
};

GType		e_alert_activity_get_type	(void);
EActivity *	e_alert_activity_new_info	(GtkWidget *message_dialog);
EActivity *	e_alert_activity_new_error	(GtkWidget *message_dialog);
EActivity *	e_alert_activity_new_warning	(GtkWidget *message_dialog);
GtkWidget *	e_alert_activity_get_message_dialog
						(EAlertActivity *alert_activity);

G_END_DECLS

#endif /* E_ALERT_ACTIVITY_H */
