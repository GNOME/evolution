
#include <libbonobo.h>

#include "Evolution-DataServer-Mail.h"

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

	return sess;
}

int main(int argc, char **argv)
{
	GNOME_Evolution_Mail_Session sess;
	GNOME_Evolution_Mail_Stores *stores;
	CORBA_Environment ev = { 0 };

	bonobo_init(&argc, argv);

	sess = get_session();

	stores = GNOME_Evolution_Mail_Session_getStores(sess, "", &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		printf("getStores failed\n");
		return 1;
	}

	printf("Got %d stores\n", stores->_length);
	{
		GNOME_Evolution_Mail_PropertyName namesarray[] = {
			"name", "uid"
		};
		GNOME_Evolution_Mail_PropertyNames names = {
			1, 1,
			namesarray,
			FALSE,
		};
		GNOME_Evolution_Mail_Properties *props;
		int i, j;

		for (i=0;i<stores->_length;i++) {
			GNOME_Evolution_Mail_Store store = stores->_buffer[i];

			printf("store %p\n", store);

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

			printf("attempt send mail to store\n");
			GNOME_Evolution_Mail_Store_sendMessage(store, NULL, "notzed@ximian.com", "notzed@novell.com, user@host", &ev);
			if (ev._major != CORBA_NO_EXCEPTION) {
				printf("sendmessage failed\n");
				/* FIXME:L leaks ex data? */
				CORBA_exception_init(&ev);
			}
		}
	}

	CORBA_free(stores);
}
