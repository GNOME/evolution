/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-mbox-parser.c : mbox folder parser */

/* 
 *
 * Copyright (C) 1999 Bertrand Guiheneuf <bertrand@helixcode.com> .
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */


#include <config.h> 
#include "camel-mbox-parser.h"
#include "camel-log.h"
#include "camel-exception.h"
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>



#define MBOX_PARSER_BUF_SIZE 1000
 
#define MBOX_PARSER_FROM_KW "from:"               
#define MBOX_PARSER_FROM_KW_SZ 5

#define MBOX_PARSER_DATE_KW "date:"
#define MBOX_PARSER_DATE_KW_SZ 5

#define MBOX_PARSER_X_EVOLUTION_KW "x-evolution:"
#define MBOX_PARSER_X_EVOLUTION_KW_SZ 12

/* the maximum lentgh of all the previous keywords */
#define MBOX_PARSER_MAX_KW_SIZE 12


#define MBOX_PARSER_SUMMARY_SIZE 100






typedef struct {
	
	int fd;                          /* file descriptor of the mbox file */
	guint real_position;             /* real position in the file */

	
	gchar *message_delimiter;        /* message delimiter string */
	guint message_delimiter_length;

	guint message_summary_size;      /* how many characters from the begining of the 
					   mail to put into the message summary */
	
	GArray *preparsed_messages;      /* array of MessagePreParsingInfo */
	CamelMboxParserMessageInfo current_message_info;  /* used to store curent info */
	gboolean is_pending_message;     /* is there some message information pending ? */

	/* buffer info */
	gchar *buffer;                   /* temporary buffer */
	guint left_chunk_size;           /* size of the left chunk in the temp buffer */
	guint last_position;             /* last position that can be compared to a keyword */
	guint current_position;          /* current position in the temp buffer */
	gboolean eof;                    /* did we read the entire file */

	/* other */
	GString *tmp_string;             /* temporary string to fill the headers in */

	
	
} CamelMboxPreParser;


/* clear a preparsing info structure */
static void
clear_message_info (CamelMboxParserMessageInfo *preparsing_info)
{
	preparsing_info->message_position = 0;
	preparsing_info->from = NULL;
	preparsing_info->date = NULL;
	preparsing_info->subject = NULL;
	preparsing_info->status = NULL;
	preparsing_info->priority = NULL;
	preparsing_info->references = NULL;
}



static CamelMboxPreParser *
new_parser (int fd,
	    const gchar *message_delimiter) 
{
	
	CamelMboxPreParser *parser;

	parser = g_new0 (CamelMboxPreParser, 1);
	
	parser->fd = fd;
	parser->buffer = g_new (gchar, MBOX_PARSER_BUF_SIZE);
	parser->current_position = 0;
	parser->message_delimiter = g_strdup (message_delimiter);
	parser->message_delimiter_length = strlen (message_delimiter);
	parser->real_position = 0;	
	parser->preparsed_messages = g_array_new (FALSE, FALSE, sizeof (CamelMboxParserMessageInfo));
	parser->message_summary_size = MBOX_PARSER_SUMMARY_SIZE;
	
	parser->left_chunk_size = MAX (parser->message_delimiter_length, MBOX_PARSER_MAX_KW_SIZE);
	parser->eof = FALSE;
	
	parser->tmp_string = g_string_sized_new (1000);

	return parser;
}



/* ** handle exceptions here */
/* read the first chunk of data in the buffer */
static void 
initialize_buffer (CamelMboxPreParser *parser,
		   guint first_position)
{
	gint seek_res;
	gint buf_nb_read;

	g_assert (parser);

	/* set the search start position */
	seek_res = lseek (parser->fd, first_position, SEEK_SET);
	//if (seek_res == (off_t)-1) goto io_error;
	
	
	/* the first part of the buffer is filled with newlines, 
	   but the next time a chunk of buffer is read, it will
	   be filled with the last bytes of the previous chunk. 
	   This allows simple g_strcasecmp to test for the presence of 
	   the keyword */
	memset (parser->buffer, '\n', parser->left_chunk_size);
	do {
		buf_nb_read = read (parser->fd, parser->buffer + parser->left_chunk_size, 
				    MBOX_PARSER_BUF_SIZE - parser->left_chunk_size);
	} while ((buf_nb_read == -1) && (errno == EINTR));
	/* ** check for an error here */

	parser->last_position = buf_nb_read - parser->left_chunk_size;
	if (buf_nb_read < (MBOX_PARSER_BUF_SIZE - parser->left_chunk_size))
		parser->eof =TRUE;

	parser->current_position = 0;
}




/* read next data in the mbox file */
static void 
read_next_buffer_chunk (CamelMboxPreParser *parser)
{
	gint buf_nb_read;


	g_assert (parser);
	
	/* read the next chunk of data in the folder file  : */
	/*  -   first, copy the last bytes from the previous 
	    chunk at the begining of the new one. */
	memcpy (parser->buffer, 
		parser->buffer + MBOX_PARSER_BUF_SIZE - parser->left_chunk_size, 
		parser->left_chunk_size);

	/*  -   then read the next chunk on disk */
	do {
		buf_nb_read = read (parser->fd, 
				    parser->buffer + parser->left_chunk_size, 
				    MBOX_PARSER_BUF_SIZE - parser->left_chunk_size);	
	} while ((buf_nb_read == -1) && (errno == EINTR));
	/* ** check for an error here */


	parser->last_position = buf_nb_read - parser->left_chunk_size;
	if (buf_nb_read < (MBOX_PARSER_BUF_SIZE - parser->left_chunk_size))
		parser->eof =TRUE;

	parser->current_position = 0;
	
}



/* read next char in the buffer */
static void 
goto_next_char (CamelMboxPreParser *parser) 
{	
	if (parser->current_position < parser->last_position)
			parser->current_position++;
	else 
		read_next_buffer_chunk (parser);

	parser->real_position++;
}




static void 
new_message_detected (CamelMboxPreParser *parser)
{
	/* if we were filling a message information 
	   save it in the message information array */ 

	if (parser->is_pending_message) {
		g_array_append_vals (parser->preparsed_messages, (gchar *)parser + 
				    G_STRUCT_OFFSET (CamelMboxPreParser, current_message_info), 1);
}
	
	clear_message_info ( &(parser->current_message_info));

	(parser->current_message_info).message_position = parser->real_position;

	parser->is_pending_message = TRUE;
		
}




/* read a header value and put it in the string pointer
   to by header_content */
static void 
read_header (CamelMboxPreParser *parser, gchar **header_content)
{
	gboolean space = FALSE;
	gboolean newline = FALSE;
	gboolean header_end = FALSE;
	gchar *buffer;
	gchar c;
	

	g_assert (parser);

	/* reset the header buffer string */
	parser->tmp_string = g_string_truncate (parser->tmp_string, 0);

	buffer = parser->buffer;

	while (! (parser->eof || header_end) ) {
		
		/* read the current character */
		c = buffer[parser->current_position];
		
		if (space) {
			if (c == ' ' && c == '\t')
				goto next_char;
			else
				space = FALSE;
		}

		if (newline) {
			if (c == ' ' && c == '\t') {

				space = TRUE;
				newline = FALSE;
				goto next_char;
			} else {

				header_end = TRUE;
				continue;
			}
		}

		if (c == '\n') {
			newline = TRUE;
			goto next_char;
		}

		/* feed the header content */
		parser->tmp_string = g_string_append_c (parser->tmp_string, c);

	next_char: /* read next char in the buffer */
		goto_next_char (parser);
	}

	
	/* copy the buffer in the preparsing information structure */
	*header_content = g_strndup (parser->tmp_string->str, parser->tmp_string->len);	
}


/* read the begining of the message and put it in the message
   summary field 
   
*/
static void
read_message_begining (CamelMboxPreParser *parser, gchar **message_summary)
{
	guint nb_read = 0;
	gchar *buffer;
	
	g_assert (parser);
	
	/* reset the header buffer string */
	parser->tmp_string = g_string_truncate (parser->tmp_string, 0);
	
	buffer = parser->buffer;
	/* the message should not be filled character by
	   character but there is no g_string_n_append 
	   function, so for the moment, this is a lazy 
	   implementation */
	while (! (parser->eof) && nb_read<parser->message_summary_size) {

		parser->tmp_string = g_string_append_c (parser->tmp_string, 
							buffer[parser->current_position]);
		nb_read++;
		goto_next_char (parser);
	}

	*message_summary = g_strndup (parser->tmp_string->str, parser->tmp_string->len);
}








GArray *
camel_mbox_parse_file (int fd, guint start_position, const gchar *message_delimiter)
{
	CamelMboxPreParser *parser;
	gboolean is_parsing_a_message = FALSE;
	gchar c;
	


	/* create the parser */
	parser = new_parser (fd, message_delimiter);
	
	/* initialize the temporary char buffer */
	initialize_buffer (parser, start_position);
	
	while (!parser->eof) {
		
		/* read the current character */
		c = parser->buffer[parser->current_position];
		goto_next_char (parser);
			
		if (c == '\n') {
			
			/* is the next part a message delimiter ? */
			if (g_strncasecmp (parser->buffer + parser->current_position, 
					   parser->message_delimiter, 
					   parser->message_delimiter_length) == 0) {
				
				is_parsing_a_message = TRUE;
				new_message_detected (parser);
				goto_next_char (parser);
				continue;
			}
			
			
			if (is_parsing_a_message) {
				
				/* is the next part a "from" header ? */
				if (g_strncasecmp (parser->buffer + parser->current_position, 
						  MBOX_PARSER_FROM_KW, 
						  MBOX_PARSER_FROM_KW_SZ) == 0) {

					parser->current_position += MBOX_PARSER_FROM_KW_SZ;
					read_header (parser, (gchar **) ((gchar *)parser +
						     G_STRUCT_OFFSET (CamelMboxPreParser, current_message_info) + 
						     G_STRUCT_OFFSET (CamelMboxParserMessageInfo, from)));
					continue;
				}

				/* is it an empty line ? */
				if (parser->buffer[parser->current_position] == '\n') {
					
					goto_next_char (parser);
					read_message_begining (parser,  (gchar **) ((gchar *)parser +
							       G_STRUCT_OFFSET (CamelMboxPreParser, current_message_info) + 
							       G_STRUCT_OFFSET (CamelMboxParserMessageInfo, body_summary)));
					is_parsing_a_message = FALSE;
				}
					
			}
		}
		
	}
	
	/* if there is a pending message information put it in the array */
	if (parser->is_pending_message) {
		g_array_append_vals (parser->preparsed_messages, (gchar *)parser + 
				     G_STRUCT_OFFSET (CamelMboxPreParser, current_message_info), 1);	
	}
	
	/* free the parser */
	/* ** FIXME : FREE THE PARSER */

	return parser->preparsed_messages;
	
}










#ifdef MBOX_PARSER_TEST
/* to build the test : 
   gcc -o test_parser -DMBOX_PARSER_TEST -I ../.. -I ../../.. \
   -I /usr/lib/glib/include camel-mbox-parser.c \
   -lglib ../../.libs/libcamel.a

   
 */
   
int 
main (int argc, char **argv)
{
	int test_file_fd;
	int i;
	GArray *message_positions; 
	CamelMboxParserMessageInfo *message_info;


	test_file_fd = open (argv[1], O_RDONLY);
	message_positions = camel_mbox_parse_file (test_file_fd, 
						   0,
						   "From ");

	printf ("Found %d messages \n", message_positions->len);
	
#if 0
	for (i=0; i<message_positions->len; i++) {
		//message_info = g_array_index(message_positions, CamelMboxParserMessageInfo, i);
		message_info = ((CamelMboxParserMessageInfo *)(message_positions->data)) + i;
		printf ("\n\n** Message %d : \n", i);
		printf ("\t From: %s\n", message_info->from) ;
		printf ("\t Summary: %s\n", message_info->body_summary) ;
	}
#endif
}




#endif /* MBOX_PARSER_TEST */
