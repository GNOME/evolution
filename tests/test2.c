/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* tests mime message file parsing */
#include "gmime-utils.h"
#include "stdio.h"
#include "camel-log.h"
#include "camel-mime-message.h"
#include "camel-mime-part.h"
#include "camel-stream.h"
#include "camel-stream-fs.h"
#include "camel.h"

void
main (int argc, char**argv)
{
	GHashTable *header_table;
	CamelMimeMessage *message;
	CamelStream *input_stream, *output_stream;
	
	gtk_init (&argc, &argv);
	camel_init ();
	camel_debug_level = CAMEL_LOG_LEVEL_FULL_DEBUG;
	message = camel_mime_message_new_with_session( (CamelSession *)NULL);

	
	input_stream = camel_stream_fs_new_with_name ("mail.test", CAMEL_STREAM_FS_READ);
	if (!input_stream) {
		perror ("could not open input file");
		printf ("You must create the file mail.test before running this test");
		exit(2);
	}
	
	camel_data_wrapper_construct_from_stream ( CAMEL_DATA_WRAPPER (message), input_stream);
					     
	camel_stream_close (input_stream);

	output_stream = camel_stream_fs_new_with_name ("mail2.test", CAMEL_STREAM_FS_WRITE);
	camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (message), output_stream);
	camel_stream_close (output_stream);

	

}
