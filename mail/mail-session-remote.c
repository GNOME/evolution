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

gboolean
mail_session_remote_get_interactive (void)
{
	gboolean ret;
	DBusError error;
	gboolean ret_val;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			MAIL_SESSION_OBJECT_PATH,
			MAIL_SESSION_INTERFACE,
			"mail_session_get_interactive",
			&error, 
			"=>b", &ret_val);

	if (!ret) {
		g_warning ("Error: mail_session_remote_get_interactive: %s\n", error.message);
		return FALSE;
	}

	d(printf("Mail session get interactive : %d\n", ret_val));
	return ret_val;
}

void
mail_session_remote_set_interactive (gboolean interactive)
{
	gboolean ret;
	DBusError error;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			MAIL_SESSION_OBJECT_PATH,
			MAIL_SESSION_INTERFACE,
			"mail_session_set_interactive",
			&error, 
			"b", interactive); 

	if (!ret) {
		g_warning ("Error:mail_session_remote_set_interactive : %s\n", error.message);
		return ;
	}

	d(printf("Mail session set interactive : %d\n", interactive));
	return;
}

char *
mail_session_remote_get_password (const char *url_string)
{
	gboolean ret;
	DBusError error;
	char *password = NULL;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			MAIL_SESSION_OBJECT_PATH,
			MAIL_SESSION_INTERFACE,
			"mail_session_get_password",
			&error, 
			"s=>s", url_string, &password); 

	if (!ret) {
		g_warning ("Error:mail_session_remote_get_password: %s\n", error.message);
		return NULL;
	}

	d(printf("mail_session_get_password : %s\n", password ? "*****" : "NULL"));
	return password;
}

void
mail_session_remote_add_password (const char *url, const char *passwd)
{
	gboolean ret;
	DBusError error;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			MAIL_SESSION_OBJECT_PATH,
			MAIL_SESSION_INTERFACE,
			"mail_session_add_password",
			&error, 
			"ss", url, passwd); 

	if (!ret) {
		g_warning ("Error:mail_session_remote_add_password: %s\n", error.message);
		return;
	}

	d(printf("mail_session_add_password : %s\n", url));
	return;
}

void
mail_session_remote_remember_password (const char *url_string)
{
	gboolean ret;
	DBusError error;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			MAIL_SESSION_OBJECT_PATH,
			MAIL_SESSION_INTERFACE,
			"mail_session_remember_password",
			&error, 
			"s", url_string); 

	if (!ret) {
		g_warning ("Error:mail_session_remote_remember_password: %s\n", error.message);
		return ;
	}

	d(printf("mail_session_remember_password : %s\n", url_string ? "*****" : "NULL"));
	return;
}

void
mail_session_remote_forget_password (const char *key)
{
	gboolean ret;
	DBusError error;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			MAIL_SESSION_OBJECT_PATH,
			MAIL_SESSION_INTERFACE,
			"mail_session_forget_password",
			&error, 
			"s", key); 

	if (!ret) {
		g_warning ("Error:mail_session_remote_forget_password: %s\n", error.message);
		return ;
	}

	d(printf("mail_session_forget_password : %s\n", key));
	return;
}

void 
mail_session_remote_flush_filter_log ()
{
	gboolean ret;
	DBusError error;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			MAIL_SESSION_OBJECT_PATH,
			MAIL_SESSION_INTERFACE,
			"mail_session_flush_filter_log",
			&error, 
			""); 

	if (!ret) {
		g_warning ("Error:mail_session_remote_flush_filter_log: %s\n", error.message);
		return ;
	}

	d(printf("mail_session_flush_filter_log: Success \n"));
	return;
}

void
mail_session_remote_shutdown (void)
{
	gboolean ret;
	DBusError error;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			MAIL_SESSION_OBJECT_PATH,
			MAIL_SESSION_INTERFACE,
			"mail_session_shutdown",
			&error, 
			""); 

	if (!ret) {
		g_warning ("Error:mail_session_remote_shutdown : %s\n", error.message);
		return ;
	}

	d(printf("mail_session_shutdown: Success \n"));
	return;
}


