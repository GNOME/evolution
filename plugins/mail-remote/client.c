
#include <stdio.h>
#include <string.h>

#include <libbonobo.h>

#include "Evolution-DataServer-Mail.h"

#include "evolution-mail-listener.h"

#include <camel/camel-folder.h>

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
		GNOME_Evolution_Mail_Session_addListener(sess, bonobo_object_corba_objref((BonoboObject *)listener), 0, &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			printf("AddListener failed: %s\n", ev._id);
			CORBA_exception_free(&ev);
		}
	}

	return sess;
}

static void
list_folder(GNOME_Evolution_Mail_Folder folder)
{
	CORBA_Environment ev = { 0 };
	GNOME_Evolution_Mail_MessageIterator iter;
	int more, total = 0;

	iter = GNOME_Evolution_Mail_Folder_getMessages(folder, "", &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		printf("getmessages failed: %s\n", ev._id);
		CORBA_exception_free(&ev);
		return;
	}

	do {
		GNOME_Evolution_Mail_MessageInfos *msgs;
		int i;

		msgs = GNOME_Evolution_Mail_MessageIterator_next(iter, 50, &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			printf("msgs.next(): %s\n", ev._id);
			CORBA_exception_free(&ev);
			break;
		}

		/* NB: set the first 50 messages in private to unseen */
		if (total == 0) {
			GNOME_Evolution_Mail_MessageInfoSets *changes;
			int j;

			changes = GNOME_Evolution_Mail_MessageInfoSets__alloc();
			changes->_length = msgs->_length;
			changes->_maximum = msgs->_maximum;
			changes->_buffer = GNOME_Evolution_Mail_MessageInfoSets_allocbuf(changes->_maximum);
			for (j=0;j<msgs->_length;j++) {
				changes->_buffer[j].uid = CORBA_string_dup(msgs->_buffer[j].uid);
				changes->_buffer[j].flagSet = 0;
				changes->_buffer[j].flagMask = CAMEL_MESSAGE_SEEN;
			}
			GNOME_Evolution_Mail_Folder_changeMessages(folder, changes, &ev);
			if (ev._major != CORBA_NO_EXCEPTION) {
				printf("changemessages failed: %s\n", ev._id);
				CORBA_exception_free(&ev);
				memset(&ev, 0, sizeof(ev));
			}
		}

		total += msgs->_length;
		more = msgs->_length == 50;
#if 0
		for (i=0;i<msgs->_length;i++) {
			printf("uid: %s  '%s'\n", msgs->_buffer[i].uid, msgs->_buffer[i].subject);
		}
#endif
		CORBA_free(msgs);
	} while (more);

	CORBA_Object_release(iter, &ev);

	printf("Got %d messages total\n", total);
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
		printf("getStores failed: %s\n", ev._id);
		CORBA_exception_free(&ev);
		_exit(1);
		return 0;
	}

	printf("Got %d stores\n", stores->_length);
	for (i=0;i<stores->_length;i++) {
#if 0
		GNOME_Evolution_Mail_PropertyName namesarray[] = {
			"name", "uid"
		};
		GNOME_Evolution_Mail_PropertyNames names = {
			2, 2,
			namesarray,
			FALSE,
		};
		GNOME_Evolution_Mail_Properties *props;
#endif
		GNOME_Evolution_Mail_Store store = stores->_buffer[i].store;
		
		printf("store %p '%s' uid '%s'\n", store, stores->_buffer[i].name, stores->_buffer[i].uid);
			
#if 0
		GNOME_Evolution_Mail_Store_getProperties(store, &names, &props, &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			printf("getProperties failed\n");
			return 1;
		}

		for (j=0;j<props->_length;j++) {
			printf(" %s = (%s)", props->_buffer[j].name, (char *)ORBit_tk_to_name(props->_buffer[j].value._type->kind));
			if (props->_buffer[j].value._type == TC_CORBA_string) {
				printf(" '%s'\n", (char *)props->_buffer[j].value._value);
			} else {
				printf(" '%s' ", BONOBO_ARG_GET_STRING(&props->_buffer[j].value));
				printf(" <unknonw type>\n");
			}
		}

		CORBA_free(props);
#endif

#if 1
		{
			char *msg = "To: notzed@novell.com\r\n"
				"Subject: This is a test from auto-send\r\n"
				"\r\n" 
				"Blah blah, test message!\r\n";
			BonoboObject *mem;

			mem = bonobo_stream_mem_create(msg, strlen(msg), TRUE, FALSE);
			
			printf("attempt send mail to store\n");
			GNOME_Evolution_Mail_Store_sendMessage(store, bonobo_object_corba_objref(mem), &ev);
			if (ev._major != CORBA_NO_EXCEPTION) {
				printf("sendmessage failed: %s\n", ev._id);
				CORBA_exception_free(&ev);
				CORBA_exception_init(&ev);
			}

			g_object_unref(mem);
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

			for (f = 0; f<folders->_length;f++) {
				if (!strcmp(folders->_buffer[f].full_name, "Private")) {
					list_folder(folders->_buffer[f].folder);
				}
			}

		}
		CORBA_free(folders);
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
