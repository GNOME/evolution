/*  
 * Authors: Srinivasa Ragavan <sragavan@novell.com>
 *
 * */

#include <mail-dbus.h>
#include "camel-session-remote.h"
#include "camel-store-remote.h"
#include "camel/camel-types.h"

#define CAMEL_SESSION_INTERFACE	"org.gnome.evolution.camel.session.mail"
#define CAMEL_SESSION_OBJECT_PATH "/org/gnome/evolution/camel/session"
#define CAMEL_DBUS_NAME "org.gnome.evolution.camel"


void
camel_session_remote_construct	(CamelSessionRemote *session,
			const char *storage_path)
{
	gboolean ret;
	DBusError error;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_SESSION_OBJECT_PATH,
			CAMEL_SESSION_INTERFACE,
			"camel_session_construct",
			&error, 
			"ss", session->object_id, storage_path); /* Just string of base dir */

	if (!ret) {
		g_warning ("Error: Constructing camel session: %s\n", error.message);
		return;
	}

	d(printf("Camel session constructed remotely\n"));

}

char *
camel_session_remote_get_password (CamelSessionRemote *session,
			CamelStoreRemote *service,
			const char *domain,
			const char *prompt,
			const char *item,
			guint32 flags,
			CamelException *ex)
{
	gboolean ret;
	DBusError error;
	const char *passwd;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_SESSION_OBJECT_PATH,
			CAMEL_SESSION_INTERFACE,
			"camel_session_get_password",
			&error, 
			"sssssu=>ss", session->object_id, service->object_id, domain, prompt, item, flags, &passwd, &ex); /* Just string of base dir */

	if (!ret) {
		g_warning ("Error: Camel session fetching password: %s\n", error.message);
		return NULL;
	}

	d(printf("Camel session get password remotely\n"));

	return passwd;
}


