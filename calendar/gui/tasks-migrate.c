/* Evolution calendar - Migrate tasks from the calendar folder to the tasks folder
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnome.h>
#include <cal-client/cal-client.h>
#include "component-factory.h"
#include "tasks-migrate.h"



/* Client for the calendar folder */
static CalClient *calendar_client = NULL;

/* Client for the tasks folder */
static CalClient *tasks_client = NULL;

/* Whether we have done the migration yet */
static gboolean migrated = FALSE;



/* Performs the actual migration process */
static void
migrate (void)
{
	GList *uids;
	GList *l;
	gboolean success;
	gboolean at_least_one;

	g_assert (calendar_client != NULL);
	g_assert (tasks_client != NULL);
	g_assert (cal_client_get_load_state (calendar_client) == CAL_CLIENT_LOAD_LOADED);
	g_assert (cal_client_get_load_state (tasks_client) == CAL_CLIENT_LOAD_LOADED);

	uids = cal_client_get_uids (calendar_client, CALOBJ_TYPE_TODO);

	success = TRUE;
	at_least_one = FALSE;

	for (l = uids; l; l = l->next) {
		const char *uid;
		CalComponent *comp;
		CalClientGetStatus status;

		at_least_one = TRUE;

		uid = l->data;
		status = cal_client_get_object (calendar_client, uid, &comp);

		switch (status) {
		case CAL_CLIENT_GET_SUCCESS:
			if (cal_client_update_object (tasks_client, comp))
				cal_client_remove_object (calendar_client, uid);
			else
				success = FALSE;

			gtk_object_unref (GTK_OBJECT (comp));
			break;

		case CAL_CLIENT_GET_NOT_FOUND:
			/* This is OK; the object may have disappeared from the server */
			break;

		case CAL_CLIENT_GET_SYNTAX_ERROR:
			success = FALSE;
			break;

		default:
			g_assert_not_reached ();
		}
	}

	cal_obj_uid_list_free (uids);

	if (!at_least_one)
		return;

	if (success)
		gnome_ok_dialog (_("Evolution has taken the tasks that were in your calendar folder "
				   "and automatically migrated them to the new tasks folder."));
	else
		gnome_ok_dialog (_("Evolution has tried to take the tasks that were in your "
				   "calendar folder and migrate them to the new tasks folder.\n"
				   "Some of the tasks could not be migrated, so "
				   "this process may be attempted again in the future."));
}

/* Displays an error to indicate that a calendar could not be opened */
static void
open_error (const char *uri)
{
	char *msg;

	msg = g_strdup_printf (_("Could not open `%s'; no items from the calendar folder "
				 "will be migrated to the tasks folder."),
				 uri);
	gnome_error_dialog (msg);
	g_free (msg);
}

/* Displays an error to indicate that a URI method is not supported */
static void
method_error (const char *uri)
{
	char *msg;

	msg = g_strdup_printf (_("The method required to load `%s' is not supported; "
				 "no items from the calendar folder will be migrated "
				 "to the tasks folder."),
			       uri);
	gnome_error_dialog (msg);
	g_free (msg);
}

/* Callback used when the tasks client is finished loading */
static void
tasks_opened_cb (CalClient *client, CalClientOpenStatus status, gpointer data)
{
	g_assert (calendar_client != NULL);
	g_assert (cal_client_get_load_state (calendar_client) == CAL_CLIENT_LOAD_LOADED);

	switch (status) {
	case CAL_CLIENT_OPEN_SUCCESS:
		migrate ();
		break;

	case CAL_CLIENT_OPEN_ERROR:
		open_error (cal_client_get_uri (client));
		migrated = FALSE;
		break;

	case CAL_CLIENT_OPEN_NOT_FOUND:
		/* This can't happen because we did not specify only_if_exists when
		 * issuing the open request.
		 */
		g_assert_not_reached ();
		break;

	case CAL_CLIENT_OPEN_METHOD_NOT_SUPPORTED:
		method_error (cal_client_get_uri (client));
		migrated = FALSE;
		break;

	default:
		g_assert_not_reached ();
	}

	gtk_object_unref (GTK_OBJECT (calendar_client));
	calendar_client = NULL;

	gtk_object_unref (GTK_OBJECT (tasks_client));
	tasks_client = NULL;
}

/* Initiates the loading process for the tasks client */
static gboolean
load_tasks_client (void)
{
	char *uri;
	gboolean success;

	g_assert (calendar_client != NULL);
	g_assert (cal_client_get_load_state (calendar_client) == CAL_CLIENT_LOAD_LOADED);

	tasks_client = cal_client_new ();
	if (!tasks_client)
		goto error;

	gtk_signal_connect (GTK_OBJECT (tasks_client), "cal_opened",
			    GTK_SIGNAL_FUNC (tasks_opened_cb),
			    NULL);

	uri = g_strdup_printf ("%s/local/Tasks/tasks.ics", evolution_dir);
	success = cal_client_open_calendar (tasks_client, uri, FALSE);
	g_free (uri);

	if (success)
		return TRUE;

 error:
	g_message ("load_tasks_client(): could not issue open request for the tasks client");

	if (tasks_client) {
		gtk_object_unref (GTK_OBJECT (tasks_client));
		tasks_client = NULL;
	}

	return FALSE;
}

/* Callback used when the calendar client finishes loading */
static void
calendar_opened_cb (CalClient *client, CalClientOpenStatus status, gpointer data)
{
	switch (status) {
	case CAL_CLIENT_OPEN_SUCCESS:
		if (!load_tasks_client ()) {
			migrated = FALSE;
			break;
		}

		return;

	case CAL_CLIENT_OPEN_ERROR:
		open_error (cal_client_get_uri (client));
		migrated = FALSE;
		break;

	case CAL_CLIENT_OPEN_NOT_FOUND:
		/* This is OK; the calendar folder did not exist in the first
		 * place so there is nothing to migrate.
		 */
		break;

	case CAL_CLIENT_OPEN_METHOD_NOT_SUPPORTED:
		method_error (cal_client_get_uri (client));
		migrated = FALSE;
		break;

	default:
		g_assert_not_reached ();
	}

	gtk_object_unref (GTK_OBJECT (calendar_client));
	calendar_client = NULL;
}

/* Initiates the loading process for the calendar client */
static gboolean
load_calendar_client (void)
{
	char *uri;
	gboolean success;

	/* First we load the calendar client; the tasks client will be loaded
	 * later only if the former one succeeds.
	 */

	calendar_client = cal_client_new ();
	if (!calendar_client)
		goto error;

	gtk_signal_connect (GTK_OBJECT (calendar_client), "cal_opened",
			    GTK_SIGNAL_FUNC (calendar_opened_cb),
			    NULL);

	uri = g_strdup_printf ("%s/local/Calendar/calendar.ics", evolution_dir);
	success = cal_client_open_calendar (calendar_client, uri, TRUE);
	g_free (uri);

	if (success)
		return TRUE;

 error:
	g_message ("load_calendar_client(): could not issue open request for the calendar client");

	if (calendar_client) {
		gtk_object_unref (GTK_OBJECT (calendar_client));
		calendar_client = NULL;
	}

	return FALSE;
}

/**
 * tasks_migrate:
 *
 * Initiates the asynchronous process that migrates the tasks from the default
 * user calendar folder to the default tasks folder.  This is because Evolution
 * used to store tasks in the same folder as the calendar by default, but they
 * are separate folders now.
 **/
void
tasks_migrate (void)
{
	g_assert (!migrated);
	migrated = TRUE;

	if (!load_calendar_client ())
		migrated = FALSE;
}
