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
	camel_mime_message_set_received_date (message, g_string_new ("a date"));


	output_file = fopen ("mail.test", "w");
	if (!output_file) {
		perror("could not open output file");
		exit(2);
	}
	camel_data_wrapper_write_to_file (CAMEL_DATA_WRAPPER (message), output_file);
	fclose (output_file);
	
				       
	
	gtk_main();
}
