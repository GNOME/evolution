/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Base64 handlers
 *
 * Author:
 *   Miguel de Icaza (miguel@kernel.org)
 */
#include <config.h>
#include "gmime-base64.h"

#define BSIZE 512

/*
 * 64-based alphabet used by the the Base64 enconding
 */
static char *base64_alphabet =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * gmime_encode_base64:
 * @input: The data source to be encoded in base64 format
 * @output: Where to put the encoded information in.
 *
 * This routine encodes the information pulled from @input using 
 * base64 encoding and stores it on the @output CamelStream object
 */
void
gmime_encode_base64 (CamelStream *input, CamelStream *output)
{
	char buffer [BSIZE];
	char obuf [80];	/* Output is limited to 76 characters, rfc2045 */
	int n, i, j, state;
	int keep = 0;

	state = 0;
	j = 0;
	while ((n = camel_stream_read (input, buffer, sizeof (buffer))) > 0){
		for (i = 0; i < n; i++, state++){
			char c = buffer [i];
			
			switch (state % 3){
			case 0:
				obuf [j++] = base64_alphabet [c >> 2];
				keep = (c & 3) << 4;
				break;
			case 1:
				obuf [j++] = base64_alphabet [keep | (c >> 4)];
				keep = (c & 0xf) << 2;
				break;
			case 2:
				obuf [j++] = base64_alphabet [keep | (c >> 6)];
				obuf [j++] = base64_alphabet [c & 0x3f];
				break;
			}

			if (j == 72){
				obuf [j++] = '\r';
				obuf [j++] = '\n';
				camel_stream_write (output, obuf, j);
				j = 0;
			}
		}
	}

	switch (state % 3){
	case 0:
		/* full ouput, nothing left to do */
		break;

	case 1:
		obuf [j++] = base64_alphabet [keep];
		obuf [j++] = '=';
		obuf [j++] = '=';
		break;

	case 2:
		obuf [j++] = base64_alphabet [keep];
		obuf [j++] = '=';
		break;
	}
	camel_stream_write (output, obuf, j);
	camel_stream_flush (output);
}


/**
 * gmime_decode_base64:
 * @input: A buffer in base64 format.
 * @output: Destination where the decoded information is stored.
 *
 * This routine decodes the base64 information pulled from @input
 * and stores it on the @output CamelStream object.
 */
void
gmime_decode_base64 (CamelStream *input, CamelStream *output)
{
}




