/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <camel.h>
#include <e-util/e-setup.h>

static char *
auth_callback (char *prompt, gboolean secret, CamelService *service,
	       char *item, CamelException *ex)
{
	char buf[80];

	printf ("%s\n", prompt);
	if (secret)
		printf ("(Warning: your input will be displayed)\n");
	if (fgets (buf, sizeof (buf), stdin) == NULL) {
		camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL,
				     "User cancelled input.");
		return NULL;
	}
	return g_strdup (buf);
}

extern char *evolution_folders_dir;

int main (int argc, char **argv)
{
	CamelSession *session;
	CamelException *ex;
	CamelStore *store, *outstore;
	CamelFolder *folder, *outfolder;
	int nmsgs, i;
	CamelMimeMessage *msg;
	char *url;
	gboolean delete = FALSE;

	gtk_init (&argc, &argv);
	camel_init ();

	if (argc == 3) {
		if (!strcmp (argv[1], "--delete") ||
		    !strcmp (argv[1], "-d")) {
			delete = TRUE;
			argc--;
			argv++;
		}
	}
	if (argc != 2) {
		fprintf (stderr, "Usage: test-movemail [--delete] url\n");
		exit (1);
	}
	e_setup_base_dir ();
	camel_provider_scan ();
	session = camel_session_new (auth_callback);

	ex = camel_exception_new ();
	store = camel_session_get_store (session, argv[1], ex);
	if (!store) {
		fprintf(stderr, "Could not open store %s:\n%s\n", argv[1],
			camel_exception_get_description (ex));
		exit (1);
	}
	camel_service_connect_with_url (CAMEL_SERVICE (store), argv[1], ex);
	if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
		printf ("Couldn't connect to %s:\n%s\n", argv[1],
			camel_exception_get_description (ex));
		exit (1);
	}

	folder = camel_store_get_folder (store, "inbox", ex);
	if (!folder) {
		fprintf(stderr, "Could not get inbox:\n%s\n",
			camel_exception_get_description (ex));
		exit (1);
	}
	camel_folder_open (folder, FOLDER_OPEN_READ, ex);
	if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
		printf ("Couldn't open folder: %s\n",
			camel_exception_get_description (ex));
		exit (1);
	}

	nmsgs = camel_folder_get_message_count (folder, ex);
	if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
		printf ("Couldn't get message count: %s\n",
			camel_exception_get_description (ex));
		exit (1);
	}
	printf ("Inbox contains %d messages.\n", nmsgs);

#ifdef DISPLAY_ONLY
	stdout_stream = camel_stream_fs_new_with_fd (1);
#else
	url = g_strdup_printf ("mbox://%s", evolution_folders_dir);
	outstore = camel_session_get_store (session, url, ex);
	g_free (url);
	if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
		printf ("Couldn't open output store: %s\n",
			camel_exception_get_description (ex));
		exit (1);
	}
	outfolder = camel_store_get_folder (outstore, "inbox", ex);
	if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
		printf ("Couldn't make output folder: %s\n",
			camel_exception_get_description (ex));
		exit (1);
	}
	camel_folder_open (outfolder, FOLDER_OPEN_WRITE, ex);
	if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
		printf ("Couldn't open output folder: %s\n",
			camel_exception_get_description (ex));
		exit (1);
	}
#endif

	for (i = 1; i <= nmsgs; i++) {
		msg = camel_folder_get_message_by_number (folder, i, ex);
		if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
			printf ("Couldn't get message: %s\n",
				camel_exception_get_description (ex));
			exit (1);
		}

#ifdef DISPLAY_ONLY
		camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (msg),
						    stdout_stream);
#else
		camel_folder_append_message (outfolder, msg, ex);
		if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
			printf ("Couldn't write message: %s\n",
				camel_exception_get_description (ex));
			exit (1);
		}

		if (delete) {
			camel_folder_delete_message_by_number (folder, i, ex);
			if (camel_exception_get_id (ex) !=
			    CAMEL_EXCEPTION_NONE) {
				printf ("Couldn't delete message: %s\n",
					camel_exception_get_description (ex));
				exit (1);
			}
		}
#endif
	}

#ifndef DISPLAY_ONLY
	camel_folder_close (outfolder, FALSE, ex);
#endif
	camel_folder_close (folder, TRUE, ex);

	camel_service_disconnect (CAMEL_SERVICE (store), ex);
	if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
		printf ("Couldn't disconnect: %s\n",
			camel_exception_get_description (ex));
		exit (1);
	}

	return 0;
}

void
gratuitous_dependency_generator()
{
	xmlSetProp();
}
