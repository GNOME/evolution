/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#include "camel-mime-message.h"
#include "stdio.h"

void
main (int argc, char**argv)
{
	CamelMimeMessage *message;
	FILE *output_file;

	gtk_init (&argc, &argv);
	message = camel_mime_message_new_with_session( (CamelSession *)NULL);
	camel_mime_part_set_description (CAMEL_MIME_PART (message), g_string_new ("a test"));
	camel_mime_message_set_received_date (message, g_string_new ("Thu, 20 May 1999, 10:39:14 +0200"));
	camel_mime_message_set_subject (message, g_string_new ("A test message"));
	camel_mime_message_set_reply_to (message, g_string_new ("toto@toto.com"));
	camel_mime_message_set_from (message, g_string_new ("Bertrand.Guiheneuf@inria.fr"));

	camel_mime_message_add_recipient (message, g_string_new (RECIPIENT_TYPE_TO), g_string_new ("franck.dechamps@alseve.fr"));
	camel_mime_message_add_recipient (message, g_string_new (RECIPIENT_TYPE_TO), g_string_new ("mc@alseve.fr"));
	camel_mime_message_add_recipient (message, g_string_new (RECIPIENT_TYPE_TO), g_string_new ("richard.lengagne@inria.fr"));

	camel_mime_message_add_recipient (message, g_string_new (RECIPIENT_TYPE_CC), g_string_new ("Francois.fleuret@inria.fr"));
	camel_mime_message_add_recipient (message, g_string_new (RECIPIENT_TYPE_CC), g_string_new ("maury@justmagic.com"));

 	camel_mime_message_add_recipient (message, g_string_new (RECIPIENT_TYPE_BCC), g_string_new ("guiheneu@aful.org"));
 

	output_file = fopen ("mail.test", "w");
	if (!output_file) {
		perror("could not open output file");
		exit(2);
	}
	camel_data_wrapper_write_to_file (CAMEL_DATA_WRAPPER (message), output_file);
	fclose (output_file);
	
				       
	
	gtk_main();
}
