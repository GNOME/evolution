
#include <stdio.h>
#include <unistd.h>

#include "evolution-mail-session.h"
#include <bonobo/bonobo-main.h>

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
		CORBA_string ior;
		CORBA_Environment ev = { 0 };

		if (sess != NULL)
			return 0;

		if (poa == NULL)
			poa = bonobo_poa_get_threaded (ORBIT_THREAD_HINT_PER_REQUEST, NULL);

		sess = g_object_new(evolution_mail_session_get_type(), "poa", poa, NULL);
		ior = CORBA_ORB_object_to_string(bonobo_orb(), bonobo_object_corba_objref((BonoboObject *)sess), &ev);

		path = g_build_filename(g_get_home_dir(), ".evolution-mail-remote.ior", NULL);
		fp = fopen(path, "w");
		fprintf(fp, "%s", ior);
		fclose(fp);
		g_free(path);

		printf("Enable mail-remote: IOR=%s\n", ior);
	} else {
		if (sess == NULL)
			return 0;

		path = g_build_filename(g_get_home_dir(), ".evolution-mail-remote.ior", NULL);
		unlink(path);
		g_free(path);

		g_object_unref(sess);
		sess = NULL;
	}

	return 0;
}
