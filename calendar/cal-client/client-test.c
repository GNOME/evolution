#include <config.h>
#include <libgnorba/gnorba.h>
#include <bonobo.h>
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
			char *calobj;

			uid = l->data;
			calobj = cal_client_get_object (client, uid);

			printf ("------------------------------\n%s", calobj);
			printf ("------------------------------\n");

			cal_client_update_object (client, uid, calobj);

			g_free (calobj);
		}
	}

	cal_obj_uid_list_free (uids);

/*  	gtk_object_unref (GTK_OBJECT (client)); */

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

/* Creates a calendar client and tries to load the specified URI into it */
static CalClient *
create_client (const char *uri, gboolean load)
{
	CalClient *client;
	gboolean result;

	client = cal_client_new ();
	if (!client) {
		g_message ("create_client(): could not create the client");
		exit (1);
	}

	gtk_signal_connect (GTK_OBJECT (client), "cal_loaded",
			    GTK_SIGNAL_FUNC (cal_loaded),
			    NULL);
	gtk_signal_connect (GTK_OBJECT (client), "obj_updated",
			    GTK_SIGNAL_FUNC (obj_updated),
			    NULL);

	printf ("Calendar loading `%s'...\n", uri);

	if (load)
		result = cal_client_load_calendar (client, uri);
	else
		result = cal_client_load_calendar (client, uri);

	if (!result) {
		g_message ("create_client(): failure when issuing calendar load/create request `%s'",
			   uri);
		exit (1);
	}

	return client;
}

int
main (int argc, char **argv)
{
	CORBA_Environment ev;

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);

	CORBA_exception_init (&ev);
	gnome_CORBA_init ("tl-test", VERSION, &argc, argv, 0, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("main(): could not initialize the ORB");
		CORBA_exception_free (&ev);
		exit (1);
	}
	CORBA_exception_free (&ev);

	if (!bonobo_init (CORBA_OBJECT_NIL, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL)) {
		g_message ("main(): could not initialize Bonobo");
		exit (1);
	}

	client1 = create_client ("/cvs/evolution/calendar/test2.vcf", TRUE);
	client2 = create_client ("/cvs/evolution/calendar/test2.vcf", FALSE);

	bonobo_main ();

	return 0;
}
