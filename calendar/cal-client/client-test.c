/* Evolution calendar client - test program
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Author: Federico Mena-Quintero <federico@helixcode.com>
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

#include <config.h>
#include <bonobo.h>
#include <liboaf/liboaf.h>
#include <gnome.h>
#include <cal-client/cal-client.h>

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

/* Callback used when a calendar is loaded */
static void
cal_loaded (CalClient *client, CalClientLoadStatus status, gpointer data)
{
	cl_printf (client, "Load/create %s\n",
		   ((status == CAL_CLIENT_LOAD_SUCCESS) ? "success" :
		    (status == CAL_CLIENT_LOAD_ERROR) ? "error" :
		    (status == CAL_CLIENT_LOAD_IN_USE) ? "in use" :
		    "unknown status value"));

	if (status == CAL_CLIENT_LOAD_SUCCESS)
		g_idle_add (list_uids, client);
	else
		gtk_object_unref (GTK_OBJECT (client));
}

/* Callback used when an object is updated */
static void
obj_updated (CalClient *client, const char *uid, gpointer data)
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
create_client (CalClient **client, const char *uri, gboolean load)
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

	gtk_signal_connect (GTK_OBJECT (*client), "cal_loaded",
			    GTK_SIGNAL_FUNC (cal_loaded),
			    NULL);
	gtk_signal_connect (GTK_OBJECT (*client), "obj_updated",
			    GTK_SIGNAL_FUNC (obj_updated),
			    NULL);

	printf ("Calendar loading `%s'...\n", uri);

	if (load)
		result = cal_client_load_calendar (*client, uri);
	else
		result = cal_client_create_calendar (*client, uri);

	if (!result) {
		g_message ("create_client(): failure when issuing calendar %s request `%s'",
			   load ? "load" : "create",
			   uri);
		exit (1);
	}
}

int
main (int argc, char **argv)
{
	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);

	gnome_init ("tl-test", VERSION, argc, argv);
	oaf_init (argc, argv);

	if (!bonobo_init (CORBA_OBJECT_NIL, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL)) {
		g_message ("main(): could not initialize Bonobo");
		exit (1);
	}

	create_client (&client1, "/cvs/evolution/calendar/cal-client/test.ics", TRUE);
	create_client (&client2, "/cvs/evolution/calendar/cal-client/test.ics", FALSE);

	bonobo_main ();
	return 0;
}
