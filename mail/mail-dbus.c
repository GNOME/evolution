/*  
 * Authors: Srinivasa Ragavan <sragavan@novell.com>
 *
 * */

#include <stdio.h>
#include "mail-dbus.h"
#include "mail-session-remote-impl.h"

#define d(x) x
#define DBUS_BUS_NAME "org.gnome.evolution.camel"

static DBusConnection *dbus = NULL;
GMainContext *gm_ctx = NULL;
GMainLoop *main_loop = NULL;
gboolean inited = FALSE;

/* FIXME: This is from dbus internal file */
#define DBUS_SESSION_BUS_DEFAULT_ADDRESS	"autolaunch:"

static const char *
get_session_address ()
{
	const char *address = g_getenv ("DBUS_SESSION_BUS_ADDRESS");

	if (!address)
		address = DBUS_SESSION_BUS_DEFAULT_ADDRESS;

	return address;
}

static DBusConnection *
e_dbus_connection_get ()
{
	DBusError error;

	if (dbus)
		return dbus;

	dbus_error_init (&error);

//	if ((dbus = dbus_bus_get (DBUS_BUS_SESSION, &error))) {
	if ((dbus = dbus_connection_open_private (get_session_address(), &error))) {
		  dbus_bus_register(dbus, &error);
	          dbus_connection_setup_with_g_main (dbus, gm_ctx);
		  dbus_connection_set_exit_on_disconnect (dbus, FALSE);
	} else {
		g_warning ("Failed to open connection to bus: %s\n", error.message);
		dbus_error_free (&error);
		return NULL;
	}
	printf("MAIL DBUS %p\n", dbus);
	dbus_bus_request_name (dbus,
				 DBUS_BUS_NAME,
				 0,
				 &error);

	if (dbus_error_is_set (&error)) {
		g_warning ("dbus_bus_request_name error: %s\n", error.message);
	
		dbus_error_free (&error);

		return NULL;
	}

	dbus_error_free (&error);

	return dbus;
}

void
e_dbus_connection_close ()
{
	if (dbus)
		dbus_connection_unref (dbus);
	dbus = NULL;
}

int 
e_dbus_setup_handlers ()
{
	if (dbus)
		return 0;

	if (!(dbus = e_dbus_connection_get ())) {
		g_warning("Error setting up dbus handlers\n");
		return -1;
	}

	d(printf("DBUS Handlers setup successfully\n"));

	return 0;
}

int 
e_dbus_register_handler (const char *object_path, DBusObjectPathMessageFunction reg, DBusObjectPathUnregisterFunction unreg)
{
	DBusObjectPathVTable *dbus_listener_vtable;

	if (!dbus)
		e_dbus_setup_handlers ();

	if (!dbus)
		return -1;

	dbus_listener_vtable = g_new0 (DBusObjectPathVTable, 1);
	dbus_listener_vtable->message_function = reg;
	dbus_listener_vtable->unregister_function = unreg;

	if (!dbus_connection_register_object_path (dbus,
						   object_path,
						   dbus_listener_vtable,
						   NULL)) {
		g_warning (("Out of memory registering object path '%s'"), object_path);
		return -1;
	}

	d(printf("successfully inited handler for %s\n", object_path));
	return 0;
}

char *
e_dbus_get_store_hash (const char *store_url)
{
	GChecksum *checksum;
	guint8 *digest;
	gsize length;
	int state = 0, save = 0;
	char *buffer = g_malloc0(sizeof(char)*9);
	int i;

	length = g_checksum_type_get_length (G_CHECKSUM_MD5);
	digest = g_alloca (length);

	checksum = g_checksum_new (G_CHECKSUM_MD5);
	g_checksum_update (checksum, (guchar *) store_url, -1);
	g_checksum_get_digest (checksum, digest, &length);
	g_checksum_free (checksum);

	g_base64_encode_step (digest, 6, FALSE, buffer, &state, &save);
	g_base64_encode_close (FALSE, buffer, &state, &save);

	for (i=0;i<8;i++) {
		if (buffer[i] == '+')
			buffer[i] = '.';
		if (buffer[i] == '/')
			buffer[i] = '_';
	}

	return buffer;
}

char *
e_dbus_get_folder_hash (const char *store_url, const char *folder_name)
{
	GChecksum *checksum;
	guint8 *digest;
	gsize length;
	int state = 0, save = 0;
	char *buffer = g_malloc0(sizeof(char)*9);
	int i;

	length = g_checksum_type_get_length (G_CHECKSUM_MD5);
	digest = g_alloca (length);

	checksum = g_checksum_new (G_CHECKSUM_MD5);
	g_checksum_update (checksum, (guchar *) store_url, -1);
	g_checksum_update (checksum, (guchar *) folder_name, -1);
	g_checksum_get_digest (checksum, digest, &length);
	g_checksum_free (checksum);

	g_base64_encode_step (digest, 6, FALSE, buffer, &state, &save);
	g_base64_encode_close (FALSE, buffer, &state, &save);

	for (i=0;i<8;i++) {
		if (buffer[i] == '+')
			buffer[i] = '.';
		if (buffer[i] == '/')
			buffer[i] = '_';
	}

	/* End by NULL */
	buffer[8] = 0;

	return buffer;
}

static gboolean
idle_cb(gpointer data)
{
	e_dbus_setup_handlers ();

	/* Initialize Mail Session */
	mail_session_remote_impl_init ();
	return FALSE;
}

static gpointer 
mail_dbus_run ()
{

	main_loop = g_main_loop_new (gm_ctx, FALSE);
	inited = TRUE;
	g_main_loop_run (main_loop);

	return NULL;
}

int
mail_dbus_init ()
{
	if (gm_ctx)
		return 0;

	gm_ctx = g_main_context_new ();
	idle_cb(NULL);
	g_thread_create (mail_dbus_run, NULL, FALSE, NULL);
	/* FIXME: This is a hack and should be fixed well. It maynot work always */
	while (!inited)
		g_main_context_iteration (NULL, TRUE);

	return 0;
}
