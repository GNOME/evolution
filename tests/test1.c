/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#include "camel-mime-message.h"
#include "stdio.h"
#include "camel-stream.h"
#include "camel-stream-fs.h"
#include "camel-log.h"

void
main (int argc, char**argv)
{
	CamelMimeMessage *message;
	/*  FILE *output_file; */
	CamelStream *stream;

	gtk_init (&argc, &argv);
	message = camel_mime_message_new_with_session( (CamelSession *)NULL);
	camel_mime_part_set_description (CAMEL_MIME_PART (message), g_string_new ("a test"));
	camel_mime_part_add_header (CAMEL_MIME_PART (message), g_string_new ("X-test1"), g_string_new ("the value of a test"));
	camel_mime_part_add_header (CAMEL_MIME_PART (message), g_string_new ("X-test2"), g_string_new ("the value of another test"));
	/*camel_mime_part_add_content_language (CAMEL_MIME_PART (message), g_string_new ("es-ca"));*/

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


	stream = camel_stream_fs_new_with_name (g_string_new ("mail1.test"), CAMEL_STREAM_FS_WRITE );
	if (!stream)  {
		CAMEL_LOG(WARNING, "could not open output file");
		exit(2);
	}
		       
	camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (message), stream);
	camel_stream_close (stream);
}
