/* --------------------------------------------------

 An address parser, yielding fields as per RFC 2426.

 Author:
   Jesse Pavel (jpavel@helixcode.com)

 Copyright 2000, Helix Code, Inc.
   -------------------------------------------------- 
*/

#include <ctype.h>
#include <string.h>
#include <glib.h>

#ifdef E_ADDRESS_WESTERN_TEST 

#include "e-address-western.h"

#else

#include <ename/e-address-western.h>

#endif

static gboolean
e_address_western_is_line_blank (gchar *line)
{
	gboolean blank = TRUE;
	gint cntr;

	/* A blank line consists of whitespace only, or a NULL line. */
	for (cntr = 0; line[cntr] != '\0'; cntr++ ) {
		if (!isspace(line[cntr])) {
			blank = FALSE;
			break;
		}
	}

	return blank;
}



/* In the array of lines, `lines', we will erase the line at line_num, and
 shift the remaining lines, up to line number num_lines, up one position. */

void
e_address_western_shift_line (gchar *lines[], gint line_num, gint num_lines)
{
	gint cntr;

	if (line_num >= (num_lines - 1)) {
		/* It is the last line, so simply shift in a NULL. */
		lines[line_num] = NULL;
	}
	else {
		for (cntr = line_num; cntr < num_lines; cntr++)
			lines[cntr] = lines[cntr + 1];
	}
}


static void 
e_address_western_remove_blank_lines (gchar *lines[], gint *linecntr)
{
	gint cntr;

	for (cntr = 0; cntr < *linecntr; cntr++) {
		if (e_address_western_is_line_blank (lines[cntr])) {
			/* Delete the blank line, and shift all subsequent lines up
			   one spot to fill its old spot. */
			e_address_western_shift_line (lines, cntr, *linecntr);

			/* Since we must check the newly shifted line, let's 
			  not advance the counter on this next pass. */
			cntr--;

			/* There is now one less line, total. */
			*linecntr -= 1;
		}
	}
}
 

gboolean
e_address_western_is_po_box (gchar *line)
{
	gboolean retval;

	/* In which phase of processing are we? */
	enum State { FIRSTCHAR, SECONDCHAR, WHITESPACE } state;
	
	
	/* If the first two letters of the line are `p' and `o', and these
	 are in turn followed by whitespace before another letter, then I
	 will deem the line a representation of a PO Box address. */
	
	gint cntr;

	state = FIRSTCHAR;
	for (cntr = 0; line[cntr] != '\0'; cntr++) {
		if (state == FIRSTCHAR)	{
			if (isalnum(line[cntr])) {
				if (tolower(line[cntr]) == 'p')
					state = SECONDCHAR;
				else {
					retval = FALSE;
					break;
				}
			}
		}
		else if (state == SECONDCHAR) {
			if (isalnum (line[cntr])) {
				if (tolower(line[cntr]) == 'o')
					state = WHITESPACE;
				else {
					retval = FALSE;
					break;
				}
			}
		}
		else if (state == WHITESPACE) {
			if (isspace (line[cntr])) {
				retval = TRUE;
				break;
			}
			else if (isalnum (line[cntr])) {
				retval = FALSE;
				break;
			}
		}
	}

	return retval;
}

/* A line that contains a comma followed eventually by a number is
  deemed to be the line in the form of <town, region postal-code>. */

gboolean
e_address_western_is_postal (gchar *line)
{
	gboolean retval;
	int cntr;
	
	if (strchr (line, ',') == NULL)
		retval = FALSE;  /* No comma. */
	else {
		int index;
		
		/* Ensure that the first character after the comma is
		 a letter. */
		index = strcspn (line, ",");
		index++;
		while (isspace(line[index]))
			index++;
		
		if (!isalpha(line[index]))
			return FALSE;   /* FIXME: ugly control flow. */

		cntr = strlen(line) - 1;

		/* Go to the character immediately following the last
		  whitespace character. */
		while (cntr >= 0 && isspace(line[cntr]))
			cntr--;	
		
		while (cntr >= 0 && !isspace(line[cntr]))
			cntr--;

		if (cntr == 0)
			retval = FALSE;
		else {
			if (isdigit (line[cntr+1]))
				retval = TRUE;
			else
				retval = FALSE;
		}
	}	

	return retval;
}

gchar *
e_address_western_extract_po_box (gchar *line)
{
	return g_strdup (line);
}

gchar *
e_address_western_extract_locality (gchar *line)
{
	gint index;

	/* Everything before the comma is the locality. */
	index = strcspn(line, ",");

	if (index == 0)
		return NULL;
	else
		return g_strndup (line, index);
}


/* Whatever resides between the comma and the start of the
  postal code is deemed to be the region. */

gchar *
e_address_western_extract_region (gchar *line)
{
	gint start, end;

	start = strcspn (line, ",");
	start++;
	while (isspace(line[start]))
		start++;
	
	end = strlen(line) - 1;
	while (isspace (line[end]))
		end--;

	while (!isspace (line[end]))
		end--;

	while (isspace (line[end]))
		end--;
	end++;

	/* Between start and end lie the string. */
	return g_strndup ( (line+start), end-start);
}

gchar *
e_address_western_extract_postal_code (gchar *line)
{
	int start, end;

	end = strlen (line) - 1;
	while (isspace(line[end]))
		end--;
	
	start = end;
	end++;

	while (!isspace(line[start]))
		start--;
	start++;	

	/* Between start and end lie the string. */
	return g_strndup ( (line+start), end-start);
}

EAddressWestern *
e_address_western_parse (const gchar *in_address)
{
	gchar **lines;
	gint linecntr, lineindex;
	gchar *address;
	gint cntr;
	gboolean found_po_box, found_postal;

	EAddressWestern *eaw;
	gint start, end;  /* To be used to classify address lines. */

	if (in_address == NULL)
		return NULL;
	
	eaw = (EAddressWestern *)g_malloc (sizeof(EAddressWestern));
	eaw->po_box = NULL;
	eaw->extended = NULL;
	eaw->street = NULL;
	eaw->locality = NULL;
	eaw->region = NULL;
	eaw->postal_code = NULL;
	eaw->country = NULL;
	
	address = g_strdup (in_address);

	/* The first thing I'll do is divide the multiline input string
	into lines. */

	/* ... count the lines. */
	linecntr = 1;
	lineindex = 0;
	while (address[lineindex] != '\0') {
		if (address[lineindex] == '\n')
			linecntr++;
			
		lineindex++;
	}

	/* ... tally them. */
	lines = (gchar **)g_malloc (sizeof(gchar *) * (linecntr+3));
	lineindex = 0;
	lines[0] = &address[0];
	linecntr = 1;
	while (address[lineindex] != '\0') {
		if (address[lineindex] == '\n') {
			lines[linecntr] = &address[lineindex + 1];
			linecntr++;
		}

		lineindex++;
	}

	/* Convert the newlines at the end of each line (except the last,
	 because it is already NULL terminated) to NULLs. */
	for (cntr = 0; cntr < (linecntr - 1); cntr++) {
		*(strchr (lines[cntr], '\n')) = '\0';
	}

	e_address_western_remove_blank_lines (lines, &linecntr);

	/* Let's just test these functions. */
	found_po_box = FALSE;
	found_postal = FALSE;

   	for (cntr = 0; cntr < linecntr; cntr++)  {
		if (e_address_western_is_po_box (lines[cntr])) {
			eaw->po_box = e_address_western_extract_po_box (lines[cntr]);
			found_po_box = TRUE;
		}
		else if (e_address_western_is_postal (lines[cntr])) {
			eaw->locality = e_address_western_extract_locality (lines[cntr]);
			eaw->region = e_address_western_extract_region (lines[cntr]);
			eaw->postal_code = e_address_western_extract_postal_code (lines[cntr]);
			found_postal = TRUE;
		}
		else {
			if (found_postal) {
				if (eaw->country == NULL)
					eaw->country = g_strdup (lines[cntr]);
				else {
					gchar *temp;
					temp = g_strconcat (eaw->country, "\n", lines[cntr]);
					g_free (eaw->country);
					eaw->country = temp;
				}
			}
			else {
				if (eaw->street == NULL) {
					eaw->street = g_strdup (lines[cntr]);
				}
				else {
					if (eaw->extended == NULL) {
						eaw->extended = g_strdup (lines[cntr]);
					}
					else {
						gchar *temp;
						temp = g_strconcat (eaw->extended, "\n", lines[cntr]);
						g_free (eaw->extended);
						eaw->extended = temp;
					}
				}
			}
		}
	}			
	
	g_free (lines);
	g_free (address);	
	
	return eaw;
}	


void 
e_address_western_free (EAddressWestern *eaw)
{
	if (eaw == NULL)
		return;
	
	if (eaw->po_box != NULL)
		g_free(eaw->po_box);
	if (eaw->extended != NULL)
		g_free(eaw->extended);
	if (eaw->street != NULL)
		g_free(eaw->street);
	if (eaw->locality != NULL)
		g_free(eaw->locality);
	if (eaw->region != NULL)
		g_free(eaw->region);
	if (eaw->postal_code != NULL)
		g_free(eaw->postal_code);
	if (eaw->country != NULL)
		g_free(eaw->country);
	
	g_free (eaw);
}

