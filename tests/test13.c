/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* tests mime message file parsing */
#include "stdio.h"
#include "camel-mime-message.h"
#include "camel-mime-part.h"
#include "camel-stream.h"
#include "camel-stream-fs.h"
#include "camel.h"


static void
dump_message_content(CamelDataWrapper *object)
{
	CamelDataWrapper *containee;
	CamelStream *stream;
	int parts, i;
	int len;
	int left;
	char buffer[128];

	printf("Dumping message ...");

	containee = camel_medium_get_content_object(CAMEL_MEDIUM(object));

	if (containee) {
		char *type = gmime_content_field_get_mime_type(containee->mime_type);

		printf("type = %s\n", type);
		
		if (CAMEL_IS_MULTIPART(containee)) {
			parts = camel_multipart_get_number (CAMEL_MULTIPART(containee));
			printf("multipart message, scanning contents  %d parts ...\n", parts);
			for (i=0;i<parts;i++) {
				dump_message_content(CAMEL_DATA_WRAPPER (camel_multipart_get_part(CAMEL_MULTIPART(containee), i)));
			}
		} else if (CAMEL_IS_MIME_MESSAGE(containee)) {
			dump_message_content((CamelDataWrapper *)containee);
		} else {
			stream = camel_data_wrapper_get_output_stream(containee);
			left = 0;

			if (stream) {
				while ( (len = camel_stream_read(stream, buffer+left, sizeof(buffer)-left)) > 0) {
					fwrite(buffer, len, 1, stdout);
				}
				printf("\n");
			} else {
				g_warning("cannot get stream for message?");
			}
		}

		g_free(type);
	} else {
		printf("no containee?\n");
	}
}

int
main (int argc, char**argv)
{
	CamelMimeMessage *message;
	CamelStream *input_stream, *output_stream;
	CamelMimeParser *parser;

	gtk_init (&argc, &argv);
	camel_init ();

/* should have another program to test all this internationalisation/header parsing stuff */
#if 0
	{
		char *s, *o;
		s = "This is a test, simple ascii text";
		o = header_encode_string(s);
		printf("%s -> %s\n", s, o);
		s = "To: Markus \"DÅ√Öhr\" <doehrm@aubi.de>";
		o = header_encode_string(s);
		printf("%s -> %s\n", s, o);

		s = "From: =?iso-8859-1?Q?Kenneth_ll=E9phaane_Christiansen?= <kenneth@ripen.dk>";
		o = header_encode_string(s);
		printf("%s -> %s\n", s, o);

		printf("decoding ... \n");
		s = "From: =?iso-8859-1?Q?Kenneth_ll=E9phaane_Christiansen?= <kenneth@ripen.dk>";
		o = header_decode_string(s);
		printf("%s -> %s\n", s, o);

		printf("reencoded\n");
		s = header_encode_string(o);
		printf("%s -> %s\n", o, s);
		return 0;
	}
#endif
		
	message = camel_mime_message_new ();

	
	input_stream = camel_stream_fs_new_with_name ("mail.test", O_RDONLY, 0);
	if (!input_stream) {
		perror ("could not open input file\n");
		printf ("You must create the file mail.test before running this test\n");
		exit(2);
	}

	printf("creating parser to create message\n");
	parser = camel_mime_parser_new();
	camel_mime_parser_init_with_stream(parser, input_stream);
	camel_mime_part_construct_from_parser(CAMEL_MIME_PART (message),
					      parser);

	dump_message_content(CAMEL_DATA_WRAPPER (message));

	camel_stream_close (input_stream);
	gtk_object_unref (GTK_OBJECT (input_stream));

	output_stream = camel_stream_fs_new_with_name ("mail2.test", O_WRONLY|O_CREAT|O_TRUNC, 0600);
	camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (message), output_stream);
	camel_stream_close (output_stream);
	gtk_object_unref (GTK_OBJECT (output_stream));
	
	//gtk_object_unref (GTK_OBJECT (message));
	return 0;

}
