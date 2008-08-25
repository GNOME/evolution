/*  
 * Authors: Srinivasa Ragavan <sragavan@novell.com>
 *
 * */

#include <mail-dbus.h>

#define MAIL_SESSION_INTERFACE	"org.gnome.evolution.camel.session.mail"
#define MAIL_SESSION_OBJECT_PATH "/org/gnome/evolution/camel/session/mail"
#define CAMEL_DBUS_NAME "org.gnome.evolution.camel"

#define d(x) x

void
mail_session_remote_init (const char *base_dir)
{
	gboolean ret;
	DBusError error;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			MAIL_SESSION_OBJECT_PATH,
			MAIL_SESSION_INTERFACE,
			"mail_session_init",
			&error, 
			"s", base_dir); /* Just string of base dir */

	if (!ret) {
		g_warning ("Error: Initializing mail session: %s\n", error.message);
		return;
	}

	d(printf("Mail session initated remotely\n"));
}
