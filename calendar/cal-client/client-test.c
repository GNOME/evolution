/* Evolution calendar client - test program
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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

#include <config.h>
#include <stdlib.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-init.h>
#include <bonobo-activation/bonobo-activation.h>
#include <bonobo/bonobo-main.h>
#include "cal-client.h"
#include "cal-util/cal-component.h"

static CalClient *client1;
static CalClient *client2;

/* Prints a message with a client identifier */
static void
cl_printf (CalClient *client, const char *format, ...)
{
	va_list args;

	va_start (args, format);
	printf ("Client %s: ",
		client == client1 ? "1" :
		client == client2 ? "2" :
		"UNKNOWN");
	vprintf (format, args);
	va_end (args);
}

/* Dumps some interesting data from a component */
static void
dump_component (CalComponent *comp)
{
	const char *uid;
	CalComponentText summary;

	cal_component_get_uid (comp, &uid);

	printf ("UID %s\n", uid);

	cal_component_get_summary (comp, &summary);
	if (summary.value)
		printf ("\tSummary: `%s', altrep `%s'\n",
			summary.value,
			summary.altrep ? summary.altrep : "NONE");
	else
		printf ("\tNo summary\n");
}

/* Lists the UIDs of objects in a calendar, called as an idle handler */
static gboolean
list_uids (gpointer data)
{
	CalClient *client;
	GList *uids;
	GList *l;

	client = CAL_CLIENT (data);

	uids = cal_client_get_uids (client, CALOBJ_TYPE_ANY);

	cl_printf (client, "UIDs: ");

	if (!uids)
		printf ("none\n");
	else {
		for (l = uids; l; l = l->next) {
			char *uid;

			uid = l->data;
			printf ("`%s' ", uid);
		}

		printf ("\n");

		for (l = uids; l; l = l->next) {
			char *uid;
			CalComponent *comp;
			CalClientGetStatus status;

			uid = l->data;
			status = cal_client_get_object (client, uid, &comp);

			if (status == CAL_CLIENT_GET_SUCCESS) {
				printf ("------------------------------\n");
				dump_component (comp);
				printf ("------------------------------\n");
				gtk_object_unref (GTK_OBJECT (comp));
			} else {
				printf ("FAILED: %d\n", status);
			}
		}
	}

	cal_obj_uid_list_free (uids);

	gtk_object_unref (GTK_OBJECT (client));

	return FALSE;
}

/* Callback used when a calendar is opened */
static void
cal_opened_cb (CalClient *client, CalClientOpenStatus status, gpointer data)
{
	cl_printf (client, "Load/create %s\n",
		   ((status == CAL_CLIENT_OPEN_SUCCESS) ? "success" :
		    (status == CAL_CLIENT_OPEN_ERROR) ? "error" :
		    (status == CAL_CLIENT_OPEN_NOT_FOUND) ? "not found" :
		    (status == CAL_CLIENT_OPEN_METHOD_NOT_SUPPORTED) ? "method not supported" :
		    "unknown status value"));

	if (status == CAL_CLIENT_OPEN_SUCCESS) {
		GList *comp_list;

		/* get free/busy information */
		comp_list = cal_client_get_free_busy (client, NULL, 0, time (NULL));
		if (comp_list) {
			GList *l;

			for (l = comp_list; l; l = l->next) {
				char *comp_str;

				comp_str = cal_component_get_as_string (CAL_COMPONENT (l->data));
				gtk_object_unref (GTK_OBJECT (l->data));
				cl_printf (client, "Free/Busy -> %s\n", comp_str);
				g_free (comp_str);
			}
			g_list_free (comp_list);
		}

		g_idle_add (list_uids, client);
	}
	else
		gtk_object_unref (GTK_OBJECT (client));
}

/* Callback used when an object is updated */
static void
obj_updated_cb (CalClient *client, const char *uid, gpointer data)
{
	cl_printf (client, "Object updated: %s\n", uid);
}

/* Callback used when a client is destroyed */
static void
client_destroy_cb (GtkObject *object, gpointer data)
{
	if (CAL_CLIENT (object) == client1)
		client1 = NULL;
	else if (CAL_CLIENT (object) == client2)
		client2 = NULL;
	else
		g_assert_not_reached ();

	if (!client1 && !client2)
		gtk_main_quit ();
}

/* Creates a calendar client and tries to load the specified URI into it */
static void
create_client (CalClient **client, const char *uri, gboolean only_if_exists)
{
	gboolean result;

	*client = cal_client_new ();
	if (!*client) {
		g_message ("create_client(): could not create the client");
		exit (1);
	}

	gtk_signal_connect (GTK_OBJECT (*client), "destroy",
			    client_destroy_cb,
			    NULL);

	gtk_signal_connect (GTK_OBJECT (*client), "cal_opened",
			    GTK_SIGNAL_FUNC (cal_opened_cb),
			    NULL);
	gtk_signal_connect (GTK_OBJECT (*client), "obj_updated",
			    GTK_SIGNAL_FUNC (obj_updated_cb),
			    NULL);

	printf ("Calendar loading `%s'...\n", uri);

	result = cal_client_open_calendar (*client, uri, only_if_exists);

	if (!result) {
		g_message ("create_client(): failure when issuing calendar open request `%s'",
			   uri);
		exit (1);
	}
}

int
main (int argc, char **argv)
{
	char *dir;

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);

	gnome_program_init ("tl-test", VERSION, LIBGNOME_MODULE, argc, argv, NULL);
	oaf_init (argc, argv);

	if (!bonobo_init (&argc, argv)) {
		g_message ("main(): could not initialize Bonobo");
		exit (1);
	}

	dir = g_strdup_printf ("%s/evolution/local/Calendar/calendar.ics", g_get_home_dir ());
	create_client (&client1, dir, FALSE);
	g_free (dir);
	create_client (&client2, "/cvs/evolution/calendar/cal-client/test.ics", TRUE);

	bonobo_main ();
	return 0;
}
