
#include <libbonobo.h>

#include "Evolution-DataServer-Mail.h"

#include "evolution-mail-listener.h"

static EvolutionMailListener *listener;

static GNOME_Evolution_Mail_Session
get_session(void)
{
	char *path, *ior;
	GNOME_Evolution_Mail_Session sess = NULL;
	CORBA_Environment ev = { 0 };

	/* The new-improved bonobo-activation ... */

	path = g_build_filename(g_get_home_dir(), ".evolution-mail-remote.ior", NULL);
	if (g_file_get_contents(path, &ior, NULL, NULL)) {
		sess = CORBA_ORB_string_to_object(bonobo_orb(), ior, &ev);
		g_free(ior);
	}

	if (sess != CORBA_OBJECT_NIL) {
		listener = evolution_mail_listener_new();
		GNOME_Evolution_Mail_Session_addListener(sess, bonobo_object_corba_objref((BonoboObject *)listener), &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			printf("AddListener failed: %s\n", ev._id);
			CORBA_exception_free(&ev);
		}
	}

	return sess;
}

static int domain(void *data)
{
	GNOME_Evolution_Mail_Session sess;
	GNOME_Evolution_Mail_StoreInfos *stores;
	GNOME_Evolution_Mail_FolderInfos *folders;
	CORBA_Environment ev = { 0 };
	int i, j, f;


	sess = get_session();

	stores = GNOME_Evolution_Mail_Session_getStores(sess, "", &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		printf("getStores failed\n");
		return 1;
	}

	printf("Got %d stores\n", stores->_length);
	for (i=0;i<stores->_length;i++) {
		GNOME_Evolution_Mail_PropertyName namesarray[] = {
			"name", "uid"
		};
		GNOME_Evolution_Mail_PropertyNames names = {
			2, 2,
			namesarray,
			FALSE,
		};
		GNOME_Evolution_Mail_Properties *props;
		GNOME_Evolution_Mail_Store store = stores->_buffer[i].store;
		
		printf("store %p '%s' uid '%s'\n", store, stores->_buffer[i].name, stores->_buffer[i].uid);
			
		GNOME_Evolution_Mail_Store_getProperties(store, &names, &props, &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			printf("getProperties failed\n");
			return 1;
		}

		for (j=0;j<props->_length;j++) {
			printf(" %s = (%s)", props->_buffer[j].name, ORBit_tk_to_name(props->_buffer[j].value._type->kind));
			if (props->_buffer[j].value._type == TC_CORBA_string) {
				printf(" '%s'\n", props->_buffer[j].value._value);
			} else {
				printf(" '%s' ", BONOBO_ARG_GET_STRING(&props->_buffer[j].value));
				printf(" <unknonw type>\n");
			}
		}

		CORBA_free(props);
#if 0
		printf("attempt send mail to store\n");
		GNOME_Evolution_Mail_Store_sendMessage(store, NULL, "notzed@ximian.com", "notzed@novell.com, user@host", &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			printf("sendmessage failed\n");
			/* FIXME:L leaks ex data? */
			CORBA_exception_init(&ev);
		}
#endif

		folders = GNOME_Evolution_Mail_Store_getFolders(store, "", &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			printf("getfolders failed\n");
			/* FIXME: leaks ex data? */
			CORBA_exception_free(&ev);
		} else {
			for (f = 0; f<folders->_length;f++) {
				printf("folder %p full:'%s' name:'%s'\n", folders->_buffer[f].folder, folders->_buffer[f].full_name, folders->_buffer[f].name);
			}
		}
	}

	CORBA_free(stores);

	return 0;
}

int main(int argc, char **argv)
{
	bonobo_init(&argc, argv);

	g_idle_add(domain, NULL);

	bonobo_main();

	return 0;
}
