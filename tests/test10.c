/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */


#include "camel.h"
#include "camel-mbox-folder.h"
#include "camel-mbox-parser.h"
#include "camel-mbox-utils.h"
#include "camel-mbox-summary.h"
#include "camel-log.h"
#include "camel-exception.h"
#include "md5-utils.h"
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <glib.h>


static CamelMimeMessage *
create_sample_mime_message ()
{
	CamelMimeMessage *message;
	CamelMimeBodyPart *body_part;
	CamelMultipart *multipart;


	message = camel_mime_message_new_with_session ((CamelSession *)NULL);

	camel_mime_part_set_description (CAMEL_MIME_PART (message), "a test");

	camel_medium_add_header (CAMEL_MEDIUM (message), "X-test1", "the value of a test");
	camel_medium_add_header (CAMEL_MEDIUM (message), "X-test2", "the value of another test");

	camel_mime_message_set_received_date (message, g_strdup ("Thu, 20 May 1999, 10:39:14 +0200"));
	camel_mime_message_set_subject (message, g_strdup ("A test message"));
	camel_mime_message_set_reply_to (message, g_strdup ("toto@toto.com"));
	camel_mime_message_set_from (message, g_strdup ("Bertrand.Guiheneuf@aful.org"));

	camel_mime_message_add_recipient (message, CAMEL_RECIPIENT_TYPE_TO, 
					  g_strdup ("franck.dechamps@alseve.fr"));
	camel_mime_message_add_recipient (message, CAMEL_RECIPIENT_TYPE_TO, 
					  g_strdup ("mc@alseve.fr"));
	camel_mime_message_add_recipient (message, CAMEL_RECIPIENT_TYPE_TO, 
					  g_strdup ("richard.lengagne@inria.fr"));
	camel_mime_message_add_recipient (message, CAMEL_RECIPIENT_TYPE_CC, 
					  g_strdup ("Francois.fleuret@inria.fr"));
	camel_mime_message_add_recipient (message, CAMEL_RECIPIENT_TYPE_CC, 
					  g_strdup ("maury@justmagic.com"));
 	camel_mime_message_add_recipient (message, CAMEL_RECIPIENT_TYPE_BCC, 
					  g_strdup ("Bertrand.Guiheneuf@aful.org"));

	multipart = camel_multipart_new ();
	body_part = camel_mime_body_part_new ();
	camel_mime_part_set_text (CAMEL_MIME_PART (body_part), "This is a test.\nThis is only a test.\n");
	camel_multipart_add_part (multipart, body_part);
	camel_medium_set_content_object (CAMEL_MEDIUM (message), CAMEL_DATA_WRAPPER (multipart));
	
	return message;
}




int
main (int argc, char**argv)
{
	CamelSession *session;
	CamelException *ex;
	CamelStore *store;
	gchar *store_url = "mbox:///tmp/evmail";
	CamelFolder *folder;
	CamelMimeMessage *message;
	GList *uid_list;
	camel_debug_level = 10;

	gtk_init (&argc, &argv);
	camel_init ();		
	ex = camel_exception_new ();
	camel_provider_register_as_module ("../camel/providers/mbox/.libs/libcamelmbox.so");
	
	session = camel_session_new ();
	store = camel_session_get_store (session, store_url, ex);
	if (camel_exception_get_id (ex)) {
		printf ("Exception caught in camel_session_get_store\n"
			"Full description : %s\n", camel_exception_get_description (ex));
		return -1;
	}

	folder = camel_store_get_folder (store, "Inbox", ex);
	if (camel_exception_get_id (ex)) {
		printf ("Exception caught in camel_store_get_folder\n"
			"Full description : %s\n", camel_exception_get_description (ex));
		return -1;
	}

	camel_folder_open (folder, FOLDER_OPEN_RW, ex);
	if (camel_exception_get_id (ex)) {
		printf ("Exception caught when trying to open the folder\n"
			"Full description : %s\n", camel_exception_get_description (ex));
		return -1;
	}

	message = create_sample_mime_message ();
	camel_folder_append_message (folder, message, ex);
	if (camel_exception_get_id (ex)) {
		printf ("Exception caught when trying to append a message to the folder\n"
			"Full description : %s\n", camel_exception_get_description (ex));
		return -1;
	}
	
	uid_list = camel_folder_get_uid_list (folder, ex);
	
	
	camel_folder_get_message_by_uid (folder, (gchar *)uid_list->data, ex);
	camel_folder_close (folder, FALSE, ex);	
	return 1;
}




