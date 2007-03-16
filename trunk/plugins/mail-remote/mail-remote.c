
#include <stdio.h>
#include <unistd.h>

#include "evolution-mail-session.h"
#include <bonobo/bonobo-main.h>

#include "e-corba-utils.h"

struct _EPlugin;
struct _ESEventTargetUpgrade;

void org_gnome_evolution_mail_remote_startup(struct _EPlugin *ep, struct _ESEventTargetUpgrade *target);
int e_plugin_lib_enable(int enable);

void org_gnome_evolution_mail_remote_startup(struct _EPlugin *ep, struct _ESEventTargetUpgrade *target) {
	/* noop */ ;
}

int e_plugin_lib_enable(int enable)
{
	static EvolutionMailSession *sess;
	char *path;
	FILE *fp;

	if (enable) {
		static PortableServer_POA poa = NULL;
		void *component;

		if (sess != NULL)
			return 0;

		component = mail_component_peek();
		if (component == NULL) {
			g_warning("Unable to find mail component, cannot instantiate mail remote api");
			return -1;
		}

		if (poa == NULL)
			poa = bonobo_poa_get_threaded (ORBIT_THREAD_HINT_PER_REQUEST, NULL);

		sess = g_object_new(evolution_mail_session_get_type(), "poa", poa, NULL);

		/*
		  NB: This only works if this is done early enough in the process ...
		  I guess it will be.  But i'm not entirely sure ...

		  If this wrong, then we have to add a mechanism to the mailcomponent directly
		  to retrieve it */

		bonobo_object_add_interface((BonoboObject *)component, (BonoboObject *)sess);
		w(printf(" ** Added mail interface to mail component\n"));
	} else {
		/* can't easily disable this until restart? */
		/* can we just destroy it? */
	}

	return 0;
}
