/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* tests mime message file parsing */
#include "gmime-utils.h"
#include "stdio.h"

void print_header_line (gpointer data, gpointer user_data)
{
	GString *header_line = (GString *)data;
	printf("\n--------- New Header ----------\n");
	if  ((header_line) && (header_line->str))
		printf("%s\n", header_line->str);
	printf("--------- End -----------------\n"); 
}

void
main (int argc, char**argv)
{
	FILE *input_file;
	GList *header_lines;

	
	gtk_init (&argc, &argv);
	
	input_file = fopen ("mail.test", "r");
	if (!input_file) {
		perror("could not open input file");
		exit(2);
	}
	
	header_lines = get_header_lines_from_file (input_file);
	if (header_lines) g_list_foreach (header_lines, print_header_line, NULL);
	else printf("header is empty, no header line present\n");
	fclose (input_file);
	

}
