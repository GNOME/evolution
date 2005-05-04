
#include "evolution-mail-session.h"
#include <bonobo/bonobo-main.h>

#define MAIL_SESSION_ID  "OAFIID:GNOME_Evolution_Mail_Session:" BASE_VERSION

init()
{
	static EvolutionMailSession *sess;
	/* placeholder for EvolutionMailSession registration, this must use a different poa */
	if (sess == NULL) {
		static PortableServer_POA poa = NULL;
		int res;
		CORBA_Object existing;

		if (poa == NULL)
			poa = bonobo_poa_get_threaded (ORBIT_THREAD_HINT_PER_REQUEST, NULL);

		sess = g_object_new(evolution_mail_session_get_type(), NULL); //"poa", poa, NULL);

		if ((res = bonobo_activation_register_active_server_ext(MAIL_SESSION_ID, bonobo_object_corba_objref((BonoboObject *)sess), NULL,
									Bonobo_REGISTRATION_FLAG_NO_SERVERINFO, &existing, NULL)) != Bonobo_ACTIVATION_REG_SUCCESS) {
			g_warning("Could not register Mail EDS Interface: %d", res);
			g_object_unref(sess);
			sess = NULL;
		}

		if (existing != CORBA_OBJECT_NIL)
			CORBA_Object_release(existing, NULL);
	}
}
