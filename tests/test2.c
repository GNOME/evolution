/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* tests mime message file parsing */
#include "gmime-utils.h"
#include "stdio.h"
#include "camel-mime-message.h"
#include "camel-mime-part.h"
#include "camel-stream.h"
#include "camel-stream-fs.h"
#include "camel.h"

int
main (int argc, char**argv)
{
	GHashTable *header_table;
	CamelMimeMessage *message;
	CamelStream *input_stream, *output_stream;
	
	gtk_init (&argc, &argv);
	camel_init ();
		
	message = camel_mime_message_new ();

	
	input_stream = camel_stream_fs_new_with_name ("mail.test", CAMEL_STREAM_FS_READ);
	if (!input_stream) {
		perror ("could not open input file\n");
		printf ("You must create the file mail.test before running this test\n");
		exit(2);
	}


	camel_data_wrapper_construct_from_stream ( CAMEL_DATA_WRAPPER (message), input_stream);

	camel_medium_get_content_object (CAMEL_MEDIUM (message));

#if 0
	camel_stream_close (input_stream);
	gtk_object_unref (GTK_OBJECT (input_stream));

	output_stream = camel_stream_fs_new_with_name ("mail2.test", CAMEL_STREAM_FS_WRITE);
	camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (message), output_stream);
	camel_stream_close (output_stream);
	gtk_object_unref (GTK_OBJECT (output_stream));
	
	//gtk_object_unref (GTK_OBJECT (message));
#endif 
	return 0;

}
