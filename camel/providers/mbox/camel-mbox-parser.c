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

#define MBOX_PARSER_SUBJECT_KW "subject:"
#define MBOX_PARSER_SUBJECT_KW_SZ 8

#define MBOX_PARSER_X_EVOLUTION_KW "x-evolution:"
#define MBOX_PARSER_X_EVOLUTION_KW_SZ 12

/* the maximum lentgh of all the previous keywords */
#define MBOX_PARSER_MAX_KW_SIZE 12


#define MBOX_PARSER_SUMMARY_SIZE 150






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



void 
parser_free (CamelMboxPreParser *parser)
{
	
	g_free (parser->buffer);
	g_free (parser->message_delimiter);
	g_string_free (parser->tmp_string, TRUE);
	g_free (parser);
	
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

	parser->last_position = buf_nb_read;
	if (buf_nb_read < (MBOX_PARSER_BUF_SIZE - parser->left_chunk_size))
		parser->eof =TRUE;

	parser->current_position = parser->left_chunk_size;
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


	parser->last_position = buf_nb_read;
	if (buf_nb_read < (MBOX_PARSER_BUF_SIZE - parser->left_chunk_size))
		parser->eof =TRUE;

	parser->current_position = 0;
	
}



/* read next char in the buffer */
static void 
goto_next_char (CamelMboxPreParser *parser) 
{	
	if (parser->current_position < parser->last_position - 1)
			parser->current_position++;
	else 
		read_next_buffer_chunk (parser);

	parser->real_position++;
}






/* advance n_chars in the buffer */
static void
advance_n_chars (CamelMboxPreParser *parser, guint n) 
{	
	
	gint position_to_the_end;

	position_to_the_end = parser->last_position - parser->current_position;

	if (n < position_to_the_end)
			parser->current_position += n;
	else {
		printf ("Advance %d chars\n", n);
		printf ("Last position = %d\n", parser->last_position);
		printf ("Current position = %d\n", parser->current_position);
		read_next_buffer_chunk (parser);
		parser->current_position = n - position_to_the_end;
		printf ("New position = %d\n", parser->current_position);
	}
	
	parser->real_position += n;
}






/* called when the buffer has detected the begining of 
   a new message. This routine is supposed to simply 
   store the previous message information and 
   clean the temporary structure used to store 
   the informations */
static void 
new_message_detected (CamelMboxPreParser *parser)
{
	
	gchar c;

	/* if we were filling a message information 
	   save it in the message information array */ 

	if (parser->is_pending_message) {
		parser->current_message_info.size = 
			parser->real_position - parser->current_message_info.message_position;
		g_array_append_vals (parser->preparsed_messages, (gchar *)parser + 
				    G_STRUCT_OFFSET (CamelMboxPreParser, current_message_info), 1);
	}
	
	clear_message_info ( &(parser->current_message_info));

	/* go to the end of the line */
	do {
		c = parser->buffer[parser->current_position];
		goto_next_char (parser);	
		//printf ("%c", c);
	} while (c != '\n');
	
	//printf ("\n");
	(parser->current_message_info).message_position = parser->real_position;

	parser->is_pending_message = TRUE;
		
}






/* read a header value and put it in the string pointer
   to by header_content. This routine stops on a 
   the first character after the last newline of the 
   header content. 
*/
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
   summary field. If we the search ended on a newline, returns 
   %TRUE, else returns %FALSE   
*/
static gboolean
read_message_begining (CamelMboxPreParser *parser, gchar **message_summary)
{
	guint nb_read = 0;
	gchar *buffer;
	gboolean new_message = FALSE;
	guint nb_line = 0;
	g_assert (parser);
	
	/* reset the header buffer string */
	parser->tmp_string = g_string_truncate (parser->tmp_string, 0);
	
	buffer = parser->buffer; 
	/* the message should not be filled character by
	   character but there is no g_string_n_append 
	   function, so for the moment, this is a lazy 
	   implementation */
	while (! (parser->eof) && (nb_line <2) && (nb_read<parser->message_summary_size) && (!new_message)) {
		
		
		/* test if we are not at the end of the message */
		if (buffer[parser->current_position] == '\n') {
			
			nb_line++;
			goto_next_char (parser);
			if ((parser->eof) || (g_strncasecmp (parser->buffer + parser->current_position, 
					   parser->message_delimiter, 
					   parser->message_delimiter_length) == 0)) {				
				new_message = TRUE;
				continue;
			} else {
				/* we're not at the end, so let's just add the cr to the summary */
				parser->tmp_string = g_string_append_c (parser->tmp_string, 
							'\n');
				nb_read++;
				continue;				
			}
				
				
		}
		
		parser->tmp_string = g_string_append_c (parser->tmp_string, 
							buffer[parser->current_position]);
		nb_read++;
		goto_next_char (parser);
	}
	
	*message_summary = g_strndup (parser->tmp_string->str, parser->tmp_string->len);
	
	return new_message;
}










/**
 * camel_mbox_parse_file: read an mbox file and parse it. 
 * @fd: file descriptor opened on the mbox file.
 * @message_delimiter: character string delimiting the beginig of a new message 
 * @start_position: poition in the file where to start the parsing. 
 * @get_message_summary: should the parser retrieve the begining of the messages
 * @status_callback: function to call peridically to indicate the progress of the parser
 * @status_interval: floating value between 0 and 1 indicate how often to call @status_callback. 
 * @user_data: user data that will be passed to the callback function
 * 
 * This routine parses an mbox file and retreives both the message starting positions and 
 * some of the informations contained in the message. Those informations are mainly 
 * some RFC822 headers values but also (optionally) the first characters of the mail 
 * body. The @get_message_summary parameter allows to enable or disable this option.
 * 
 * 
 * Return value: 
 **/
GArray *
camel_mbox_parse_file (int fd, 
		       const gchar *message_delimiter,
		       guint start_position,
		       gboolean get_message_summary,
		       camel_mbox_preparser_status_callback *status_callback,
		       double status_interval,
		       gpointer user_data)
{
	CamelMboxPreParser *parser;
	gboolean is_parsing_a_message = FALSE;
	gchar c;
	struct stat stat_buf;
	gint fstat_result;
	guint total_file_size;
	int last_status = 0;
	int real_interval;
	gboolean newline;
	GArray *return_value;

	/* get file size */
	fstat_result = fstat (fd, &stat_buf);
	if (fstat_result == -1) {
		g_warning ("Manage exception here \n");
	}
		
	total_file_size = stat_buf.st_size;
	real_interval = status_interval * total_file_size;
	
	
	/* create the parser */
	parser = new_parser (fd, message_delimiter);
	
	/* initialize the temporary char buffer */
	initialize_buffer (parser, start_position);
	
	/* the first line is indeed at the begining of a new line ... */
	newline = TRUE;

	while (!parser->eof) {

		

		
		/* read the current character */
		if (!newline) {
			c = parser->buffer[parser->current_position];
			newline = (c == '\n');
			goto_next_char (parser);
		}
			
		if (newline) {
			
			/* check if we reached a status milestone */
			if ( status_callback && ((parser->real_position - last_status) > real_interval)) {
				last_status += real_interval;
				status_callback ((double)last_status / (double)total_file_size, 
						 user_data);
			}
			
			/* is the next part a message delimiter ? */
			if (g_strncasecmp (parser->buffer + parser->current_position, 
					   parser->message_delimiter, 
					   parser->message_delimiter_length) == 0) {
				
				is_parsing_a_message = TRUE;
				new_message_detected (parser);
				newline = TRUE;
				continue;
			}
			
			
			if (is_parsing_a_message) {
				/* we could find the headers in a clever way, like
				   storing them in a list of pair 
				   [keyword, offset_in_CamelMboxParserMessageInfo]
				   I am too busy for now. Contribution welcome */
				   
				/* is the next part a "from" header ? */
				if (g_strncasecmp (parser->buffer + parser->current_position, 
						  MBOX_PARSER_FROM_KW, 
						  MBOX_PARSER_FROM_KW_SZ) == 0) {
					
					advance_n_chars (parser, MBOX_PARSER_FROM_KW_SZ);
					read_header (parser, (gchar **) ((gchar *)parser +
						     G_STRUCT_OFFSET (CamelMboxPreParser, current_message_info) + 
						     G_STRUCT_OFFSET (CamelMboxParserMessageInfo, from)));
					
					newline = TRUE;
					continue;
				}

				/* is the next part a "Date" header ? */
				if (g_strncasecmp (parser->buffer + parser->current_position, 
						  MBOX_PARSER_DATE_KW, 
						  MBOX_PARSER_DATE_KW_SZ) == 0) {
					
					advance_n_chars (parser, MBOX_PARSER_DATE_KW_SZ);
					read_header (parser, (gchar **) ((gchar *)parser +
						     G_STRUCT_OFFSET (CamelMboxPreParser, current_message_info) + 
						     G_STRUCT_OFFSET (CamelMboxParserMessageInfo, date)));
					
					newline = TRUE;
					continue;
				}


				/* is the next part a "Subject" header ? */
				if (g_strncasecmp (parser->buffer + parser->current_position, 
						  MBOX_PARSER_SUBJECT_KW, 
						  MBOX_PARSER_SUBJECT_KW_SZ) == 0) {

					advance_n_chars (parser, MBOX_PARSER_SUBJECT_KW_SZ);
					read_header (parser, (gchar **) ((gchar *)parser +
						     G_STRUCT_OFFSET (CamelMboxPreParser, current_message_info) + 
						     G_STRUCT_OFFSET (CamelMboxParserMessageInfo, subject)));

					newline = TRUE;
					continue;
				}


				/* is the next part a "X-evolution" header ? */
				if (g_strncasecmp (parser->buffer + parser->current_position, 
						  MBOX_PARSER_X_EVOLUTION_KW, 
						  MBOX_PARSER_X_EVOLUTION_KW_SZ) == 0) {

					advance_n_chars (parser, MBOX_PARSER_X_EVOLUTION_KW_SZ);
					read_header (parser, (gchar **) ((gchar *)parser +
						     G_STRUCT_OFFSET (CamelMboxPreParser, current_message_info) + 
						     G_STRUCT_OFFSET (CamelMboxParserMessageInfo, x_evolution)));

					newline = TRUE;
					continue;
				}


				

				/* is it an empty line ? */
				if (parser->buffer[parser->current_position] == '\n') {
					
					goto_next_char (parser);
					if (get_message_summary)
						newline = read_message_begining (parser, (gchar **) ((gchar *)parser +
						           G_STRUCT_OFFSET (CamelMboxPreParser, current_message_info) + 
						           G_STRUCT_OFFSET (CamelMboxParserMessageInfo, body_summary)));
					
					is_parsing_a_message = FALSE;
					continue;
				}
			}
			newline = FALSE;
		}
		
	}
	
	/* if there is a pending message information put it in the array */
	if (parser->is_pending_message) {
		g_array_append_vals (parser->preparsed_messages, (gchar *)parser + 
				     G_STRUCT_OFFSET (CamelMboxPreParser, current_message_info), 1);	
	}



	return_value = parser->preparsed_messages;
	/* free the parser */
	parser_free (parser);

	return return_value;
}










#ifdef MBOX_PARSER_TEST
/* to build the test : 
   
   gcc -O3 -I/opt/gnome/lib/glib/include  `glib-config --cflags` -o test_parser -DMBOX_PARSER_TEST -I ../.. -I ../../..  -I /usr/lib/glib/include camel-mbox-parser.c  `glib-config --libs` -lm

   
 */


#include <math.h>

static void 
status (double done, gpointer user_data)
{
	printf ("%d %% done\n", (int)floor (done * 100));
}
int 
main (int argc, char **argv)
{
	int test_file_fd;
	int i;
	GArray *message_positions; 
	CamelMboxParserMessageInfo *message_info;
	gchar tmp_buffer[50];

	tmp_buffer[49] = '\0';

	test_file_fd = open (argv[1], O_RDONLY);
	message_positions = camel_mbox_parse_file (test_file_fd, 
						   "From ", 
						   0,
						   TRUE,
						   status,
						   0.05,
						   NULL);

	printf ("Found %d messages \n", message_positions->len);
	

	for (i=0; i<message_positions->len; i++) {
		
		message_info = ((CamelMboxParserMessageInfo *)(message_positions->data)) + i;
		printf ("\n\n** Message %d : \n", i);
		printf ("Size : %d\n", message_info->size);
		printf ("From: %s\n", message_info->from);
		printf ("Date: %s\n", message_info->date);
		printf ("Subject: %s\n", message_info->subject);
		printf ("Summary: %s\n", message_info->body_summary) ;
		

		lseek (test_file_fd, message_info->message_position, SEEK_SET);
		read (test_file_fd, tmp_buffer, 49);
		printf ("File content at position %d : \n===\n%s\n===\n", message_info->message_position, tmp_buffer);
		
	}



	return 0;
}




#endif /* MBOX_PARSER_TEST */
