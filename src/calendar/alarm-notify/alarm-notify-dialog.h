/*
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
 * Authors:
 *		Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef ALARM_NOTIFY_DIALOG_H
#define ALARM_NOTIFY_DIALOG_H

#include <time.h>
#include <gtk/gtk.h>
#include <libecal/libecal.h>

typedef enum {
	ALARM_NOTIFY_CLOSE,
	ALARM_NOTIFY_SNOOZE,
	ALARM_NOTIFY_EDIT,
	ALARM_NOTIFY_PRINT,
	ALARM_NOTIFY_DISMISS
} AlarmNotifyResult;

typedef struct _AlarmNotificationsDialog AlarmNotificationsDialog;

struct _AlarmNotificationsDialog {
	GtkWidget *dialog;
	GtkWidget *treeview;
};

typedef void	(*AlarmNotifyFunc)		(AlarmNotifyResult result,
						 gint snooze_mins,
						 gpointer data);

AlarmNotificationsDialog *
		notified_alarms_dialog_new	(void);
GtkTreeIter	add_alarm_to_notified_alarms_dialog
						(AlarmNotificationsDialog *na,
						 time_t trigger,
						 time_t occur_start,
						 time_t occur_end,
						 ECalComponent *comp,
						 const gchar *summary,
						 const gchar *description,
						 const gchar *location,
						 AlarmNotifyFunc func,
						 gpointer func_data);

#endif /* ALARM_NOTIFY_DIALOG_H */
