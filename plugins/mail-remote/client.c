
#include <stdio.h>
#include <string.h>

#include <libbonobo.h>

#include "Evolution-DataServer-Mail.h"

#include "evolution-mail-sessionlistener.h"
#include "evolution-mail-storelistener.h"
#include "evolution-mail-folderlistener.h"
#include "evolution-mail-messagestream.h"

#include <camel/camel-folder.h>

static EvolutionMailSessionListener *listener_sess;
static EvolutionMailStoreListener *listener_store;
static EvolutionMailFolderListener *listener_folder;

#if 0
static char *
em_map_mail_ex(CORBA_Environment *ev, void *data)
{
	Evolution_Mail_MailException *x = CORBA_exception_value(ev);

	switch (x->id) {
	case Evolution_Mail_SYSTEM_ERROR:
		return g_strdup_printf(_("System error: %s"), x->desc);
	case Evolution_Mail_CAMEL_ERROR:
		return g_strdup_printf(_("Camel error: %s"), x->desc);
	default:
		return g_strdup(x->desc);
	}
}
#endif

static void e_mail_exception_dump(CORBA_Environment *ev, char *what)
{
#if 0
	static int init = 0;
	char *d;

	/* *shrug* this doesn't work */
	if (!init) {
		bonobo_exception_add_handler_fn(ex_Evolution_Mail_MailException, em_map_mail_ex, NULL, NULL);
		init = 1;
	}

	d = bonobo_exception_get_text(ev);

	if (d) {
		printf("Failed %s: %s\n", what, d);
		g_free(d);
	}
	CORBA_exception_free(ev);
#else
	const char *id = CORBA_exception_id(ev);

	switch (ev->_major) {
	case CORBA_USER_EXCEPTION:
		if (!strcmp(id, ex_Evolution_Mail_MailException)) {
			Evolution_Mail_MailException *x = CORBA_exception_value(ev);

			switch (x->id) {
			case Evolution_Mail_SYSTEM_ERROR:
				printf("Failed %s: System error %s\n", what, x->desc);
				break;
			case Evolution_Mail_CAMEL_ERROR:
				printf("Failed %s: Camel error %s\n", what, x->desc);
				break;
			default:
				printf("Failed %s: %s\n", what, x->desc);
				break;
			}
			break;
		}
	default:
		printf("Failed %s: %s\n", what, id);
		break;
	}

	CORBA_exception_free(ev);
#endif
}

static Evolution_Mail_Session
get_session(void)
{
	char *path, *ior;
	Evolution_Mail_Session sess = NULL;
	CORBA_Environment ev = { 0 };

	/* The new-improved bonobo-activation ... */

	path = g_build_filename(g_get_home_dir(), ".evolution-mail-remote.ior", NULL);
	if (g_file_get_contents(path, &ior, NULL, NULL)) {
		sess = CORBA_ORB_string_to_object(bonobo_orb(), ior, &ev);
		g_free(ior);
	}

	if (sess != CORBA_OBJECT_NIL) {
		listener_sess = evolution_mail_sessionlistener_new();
		listener_store = evolution_mail_storelistener_new();
		listener_folder = evolution_mail_folderlistener_new();
		Evolution_Mail_Session_addListener(sess, bonobo_object_corba_objref((BonoboObject *)listener_sess), &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			e_mail_exception_dump(&ev, "adding store listener");
		}
	}

	return sess;
}

static void
list_folder(Evolution_Mail_Folder folder)
{
	CORBA_Environment ev = { 0 };
	Evolution_Mail_MessageIterator iter;
	int more, total = 0;

	iter = Evolution_Mail_Folder_getMessages(folder, "", &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		e_mail_exception_dump(&ev, "getting mssages");
		return;
	}

	do {
		Evolution_Mail_MessageInfos *msgs;
		int i;

		msgs = Evolution_Mail_MessageIterator_next(iter, 50, &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			e_mail_exception_dump(&ev, "getting next messages");
			break;
		}

		/* NB: set the first 50 messages in private to unseen */
		if (total == 0) {
			Evolution_Mail_MessageInfoSets *changes;
			int j;

			changes = Evolution_Mail_MessageInfoSets__alloc();
			changes->_length = msgs->_length;
			changes->_maximum = msgs->_maximum;
			changes->_buffer = Evolution_Mail_MessageInfoSets_allocbuf(changes->_maximum);
			for (j=0;j<msgs->_length;j++) {
				changes->_buffer[j].uid = CORBA_string_dup(msgs->_buffer[j].uid);
				changes->_buffer[j].flagSet = 0;
				changes->_buffer[j].flagMask = CAMEL_MESSAGE_SEEN;
			}
			Evolution_Mail_Folder_changeMessages(folder, changes, &ev);
			if (ev._major != CORBA_NO_EXCEPTION)
				e_mail_exception_dump(&ev, "changing messages");
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

	printf("calling dispose\n");
	Evolution_Mail_MessageIterator_dispose(iter, &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		e_mail_exception_dump(&ev, "disposing messageiterator");

	CORBA_Object_release(iter, &ev);

	printf("Got %d messages total\n", total);
}

static void
add_message(Evolution_Mail_Folder folder, const char *msg)
{
	BonoboObject *mem;
	CORBA_Environment ev = { 0 };
	Evolution_Mail_MessageInfoSet mis = { 0 };

	mis.uid = "";
	mis.flagSet = CAMEL_MESSAGE_SEEN;
	mis.flagMask = CAMEL_MESSAGE_SEEN;

	mem = (BonoboObject *)evolution_mail_messagestream_new_buffer(msg, strlen(msg));
			
	printf("attempt send mail to store\n");
	Evolution_Mail_Folder_appendMessage(folder, &mis, bonobo_object_corba_objref(mem), &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		printf("appendmessage failed: %s\n", ev._id);
		CORBA_exception_free(&ev);
		CORBA_exception_init(&ev);
	}
}

static int domain(void *data)
{
	Evolution_Mail_Session sess;
	Evolution_Mail_StoreInfos *stores;
	Evolution_Mail_FolderInfos *folders;
	CORBA_Environment ev = { 0 };
	int i, j, f;

	sess = get_session();

	stores = Evolution_Mail_Session_getStores(sess, "", bonobo_object_corba_objref((BonoboObject *)listener_store), &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		e_mail_exception_dump(&ev, "getting stores");
		_exit(1);
		return 0;
	}

	printf("Got %d stores\n", stores->_length);
	for (i=0;i<stores->_length;i++) {
#if 0
		Evolution_Mail_PropertyName namesarray[] = {
			"name", "uid"
		};
		Evolution_Mail_PropertyNames names = {
			2, 2,
			namesarray,
			FALSE,
		};
		Evolution_Mail_Properties *props;
#endif
		Evolution_Mail_Store store = stores->_buffer[i].store;
		
		printf("store %p '%s' uid '%s'\n", store, stores->_buffer[i].name, stores->_buffer[i].uid);
			
#if 0
		Evolution_Mail_Store_getProperties(store, &names, &props, &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			e_mail_exception_dump(&ev, "getting store properties");
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

			mem = (BonoboObject *)evolution_mail_messagestream_new_buffer(msg, strlen(msg));
			
			printf("attempt send mail to store\n");
			Evolution_Mail_Store_sendMessage(store, bonobo_object_corba_objref(mem), &ev);
			if (ev._major != CORBA_NO_EXCEPTION)
				e_mail_exception_dump(&ev, "sending message to store");
			/* If we get a system exception, do we have to dispose it ourselves?? */
		}
#endif

		folders = Evolution_Mail_Store_getFolders(store, "", bonobo_object_corba_objref((BonoboObject *)listener_folder), &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			e_mail_exception_dump(&ev, "getting folders");
		} else {
			for (f = 0; f<folders->_length;f++) {
				printf("folder %p full:'%s' name:'%s'\n", folders->_buffer[f].folder, folders->_buffer[f].full_name, folders->_buffer[f].name);
			}

			for (f = 0; f<folders->_length;f++) {
				if (!strcmp(folders->_buffer[f].full_name, "Private")) {
					const char *msg = "To: notzed@novell.com\r\n"
						"Subject: This is a test append from client\r\n"
						"\r\n" 
						"Blah blah, test appended message!\r\n";

					list_folder(folders->_buffer[f].folder);
					add_message(folders->_buffer[f].folder, msg);
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
