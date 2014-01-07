/*
 *
 * Evolution calendar - Alarm page of the calendar component dialogs
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
 * Authors:
 *		Federico Mena-Quintero <federico@ximian.com>
 *      Miguel de Icaza <miguel@ximian.com>
 *      Seth Alves <alves@hungry.com>
 *      JP Rosevear <jpr@ximian.com>
 *      Hans Petter Jansson <hpj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef ALARM_LIST_DIALOG_H
#define ALARM_LIST_DIALOG_H

#include <libecal/libecal.h>

#include <e-util/e-util.h>

#include "../e-alarm-list.h"

G_BEGIN_DECLS

gboolean	alarm_list_dialog_run		(GtkWidget *parent,
						 EClientCache *client_cache,
						 ECalClient *cal_client,
						 EAlarmList *list_store);
GtkWidget *	alarm_list_dialog_peek		(EClientCache *client_cache,
						 ECalClient *cal_client,
						 EAlarmList *list_store);
void		alarm_list_dialog_set_client	(GtkWidget *dlg_box,
						 ECalClient *cal_client);

G_END_DECLS

#endif
