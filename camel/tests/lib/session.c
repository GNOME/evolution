#include "session.h"

static guint
register_timeout (CamelSession *session, guint32 interval, CamelTimeoutCallback callback, gpointer user_data)
{
	return 1;
}

static gboolean
unregister_timeout (CamelSession *session, guint handle)
{
	return TRUE;
}


static void
class_init (CamelTestSessionClass *camel_test_session_class)
{
	CamelSessionClass *camel_session_class =
		CAMEL_SESSION_CLASS (camel_test_session_class);
	
	/* virtual method override */
	camel_session_class->register_timeout = register_timeout;
	camel_session_class->remove_timeout = unregister_timeout;
}

CamelType
camel_test_session_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (
			camel_session_get_type (),
			"CamelTestSession",
			sizeof (CamelTestSession),
			sizeof (CamelTestSessionClass),
			(CamelObjectClassInitFunc) class_init,
			NULL,
			NULL,
			NULL);
	}
	
	return type;
}

CamelSession *
camel_test_session_new (const char *path)
{
	CamelSession *session;
	
	session = CAMEL_SESSION (camel_object_new (CAMEL_TEST_SESSION_TYPE));
	
	camel_session_construct (session, path);
	
	return session;
}


