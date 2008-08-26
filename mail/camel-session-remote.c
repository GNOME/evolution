/*  
 * Authors: Srinivasa Ragavan <sragavan@novell.com>
 *
 * */

#include <mail-dbus.h>
#include "camel-session-remote.h"
#include "camel-store-remote.h"

#define CAMEL_SESSION_INTERFACE	"org.gnome.evolution.camel.session.mail"
#define CAMEL_SESSION_OBJECT_PATH "/org/gnome/evolution/camel/session"
#define CAMEL_DBUS_NAME "org.gnome.evolution.camel"


typedef enum {
	CAMEL_PROVIDER_STORE,
	CAMEL_PROVIDER_TRANSPORT,
	CAMEL_NUM_PROVIDER_TYPES
} CamelProviderType;

typedef enum {
	CAMEL_SESSION_ALERT_INFO,
	CAMEL_SESSION_ALERT_WARNING,
	CAMEL_SESSION_ALERT_ERROR
} CamelSessionAlertType;

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
			guint32 flags)
{
	gboolean ret;
	DBusError error;
	char *passwd, *ex;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_SESSION_OBJECT_PATH,
			CAMEL_SESSION_INTERFACE,
			"camel_session_get_password",
			&error, 
			"sssssu=>ss", session->object_id, service->object_id, domain, prompt, item, flags, &passwd, &ex); 

	if (!ret) {
		g_warning ("Error: Camel session fetching password: %s\n", error.message);
		return NULL;
	}

	d(printf("Camel session get password remotely\n"));

	return passwd;
}

char *
camel_session_remote_get_storage_path (CamelSessionRemote *session, CamelStoreRemote *service)
{
	gboolean ret;
	DBusError error;
	char *storage_path, *ex;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_SESSION_OBJECT_PATH,
			CAMEL_SESSION_INTERFACE,
			"camel_session_get_storage_path",
			&error, 
			"ss=>ss", session->object_id, service->object_id, &storage_path, &ex);

	if (!ret) {
		g_warning ("Error: Camel session fetching storage path: %s\n", error.message);
		return NULL;
	}

	d(printf("Camel session get storage path remotely\n"));

	return storage_path;
}

void
camel_session_remote_forget_password (CamelSessionRemote *session, 
				CamelStoreRemote *service,
				const char *domain,
				const char *item)
{
	gboolean ret;
	DBusError error;
	char *ex;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_SESSION_OBJECT_PATH,
			CAMEL_SESSION_INTERFACE,
			"camel_session_forget_password",
			&error, 
			"ssss=>s", session->object_id, service->object_id, domain, item, &ex); 

	if (!ret) {
		g_warning ("Error: Camel session forget password: %s\n", error.message);
		return;
	}

	d(printf("Camel session forget password remotely\n"));

	return;
}

CamelStoreRemote *
camel_session_remote_get_service (CamelSessionRemote *session, const char *url_string,
			   CamelProviderType type)
{
	gboolean ret;
	DBusError error;
	char *service, *ex;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_SESSION_OBJECT_PATH,
			CAMEL_SESSION_INTERFACE,
			"camel_session_get_service",
			&error, 
			"ssi=>ss", session->object_id, url_string, type, &service, &ex);

	if (!ret) {
		g_warning ("Error: Camel session get service: %s\n", error.message);
		return;
	}

	d(printf("Camel session get service remotely\n"));

	return;
}

gboolean  
camel_session_remote_alert_user (CamelSessionRemote *session, 
					CamelSessionAlertType type,
					const char *prompt,
					gboolean cancel)
{
	gboolean ret, success;
	DBusError error;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_SESSION_OBJECT_PATH,
			CAMEL_SESSION_INTERFACE,
			"camel_session_alert_user",
			&error, 
			"sis(int)b=>(int)b", session->object_id, type, prompt, cancel, &success);

	if (!ret) {
		g_warning ("Error: Camel session alerting user: %s\n", error.message);
		return 0;
	}

	d(printf("Camel session alert user remotely\n"));

	return success;
}

char *
camel_session_remote_build_password_prompt (const char *type,
				     const char *user,
				     const char *host)
{
	gboolean ret;
	DBusError error;
	char *prompt;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_SESSION_OBJECT_PATH,
			CAMEL_SESSION_INTERFACE,
			"camel_session_build_password_prompt",
			&error, 
			"sss=>s", type, user, host, &prompt);

	if (!ret) {
		g_warning ("Error: Camel session building password prompt: %s\n", error.message);
		return NULL;
	}

	d(printf("Camel session build password prompt remotely\n"));

	return prompt;
}

gboolean           
camel_session_remote_is_online (CamelSessionRemote *session)
{
	gboolean ret, is_online;
	DBusError error;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_SESSION_OBJECT_PATH,
			CAMEL_SESSION_INTERFACE,
			"camel_session_is_online",
			&error, 
			"s=>(int)b", session->object_id, &is_online);

	if (!ret) {
		g_warning ("Error: Camel session check for online: %s\n", error.message);
		return 0;
	}

	d(printf("Camel session check for online remotely\n"));

	return is_online;

}

void 
camel_session_remote_set_online  (CamelSessionRemote *session,
				gboolean online)
{
	gboolean ret;
	DBusError error;

	dbus_error_init (&error);
	/* Invoke the appropriate dbind call to MailSessionRemoteImpl */
	ret = dbind_context_method_call (evolution_dbus_peek_context(), 
			CAMEL_DBUS_NAME,
			CAMEL_SESSION_OBJECT_PATH,
			CAMEL_SESSION_INTERFACE,
			"camel_session_set_online",
			&error, 
			"ss", session->object_id, online);

	if (!ret) {
		g_warning ("Error: Camel session set online: %s\n", error.message);
		return;
	}

	d(printf("Camel session set online remotely\n"));

	return;
}


