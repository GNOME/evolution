/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* url-util.c : utility functions to parse URLs */


/* 
 * Copyright (C) 1999 Bertrand Guiheneuf <Bertrand.Guiheneuf@inria.fr>
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



/* 
   Here we deal with URL following the general scheme:
   protocol://user:password@host:port/name
   where name is a path-like string (ie dir1/dir2/....)
   See rfc1738 for the complete description of 
   Uniform Ressource Locators 
   
     Bertrand. */


#include <ctype.h> /* for isalpha */
#include <stdlib.h> /* for atoi */

#include "url-util.h"






static gboolean 
find_protocol(GString *url, GString **protocol, guint *position, gboolean *error)
{

	guint i;
	gchar *str_url;
	gint len_url;
	gchar *str_protocol;
	
	str_url = url->str;
	len_url = url->len;
	
	*protocol = NULL;
	*error = FALSE;
	i=*position;
	
	/* find a ':' */
	while ( (i<len_url) && (str_url[i] != ':') ) i++;
	
	if (i==len_url) return FALSE;
	i++;

	/* check if it is followed by a "//" */
	if  ((i<len_url) && (str_url[i++] == '/'))
		if ((i<len_url) && (str_url[i++] == '/'))
		{
			
			str_protocol = g_strndup(str_url, i-3);
			*protocol = g_string_new(str_protocol);
			*position=i;
			return TRUE;
		}
	
	return FALSE;
}




static gboolean
find_user(GString *url, GString **user, guint *position, gboolean *error)
{
	guint i;
	guint at_pos;
	
	gchar *str_url;
	gint len_url;
	gchar *str_user;
	
	str_url = url->str;
	len_url = url->len;
	
	*user = NULL;
	i=*position;
	

	/* find a '@' */
	while ((i<len_url) && (str_url[i] != '@')) i++;
	
	if (i==len_url) return FALSE;
	at_pos = i;
	i = *position;

	/* find a ':' */
	while ( (i<at_pos) && (str_url[i] != ':') ) i++;

	/* now if i has not been incremented at all, there is no user */
	if (i == *position) return FALSE;
	
	str_user = g_strndup(str_url+ *position, i - *position);
	*user = g_string_new(str_user);
	if (i<at_pos) *position=i+1; /* there was a ':', skip it */
	else *position=i;
	
	return TRUE;

	
	
}

static gboolean
find_passwd(GString *url, GString **passwd, guint *position, gboolean *error)
{
	guint i;
	
	gchar *str_url;
	gint len_url;
	gchar *str_passwd;
	
	str_url = url->str;
	len_url = url->len;
	
	*passwd = NULL;
	i=*position;
	

	/* find a '@' */
	while ((i<len_url) && (str_url[i] != '@')) i++;
	
	if (i==len_url) return FALSE;
	/*i has not been incremented at all, there is no passwd */
	if (i == *position) {
		*position = i+1;
		return FALSE;
	}
	
	str_passwd = g_strndup(str_url+ *position, i - *position);
	*passwd = g_string_new(str_passwd);
	*position=i+1; /* skip it the '@' */
	
	return TRUE;

	
	
}




/* to tests this file :
   gcc -o test_url_util `glib-config --cflags`  -DTEST_URL_UTIL url-util.c `glib-config --libs
   ./test_url_util URL
*/
#ifdef TEST_URL_UTIL

int 
main (int argc, char **argv)
{

	GString *url;
	GString *protocol;
	GString *user;
	GString *passwd;
	guint position=0;
	gboolean error;
	gboolean found;
	guint i;

	url = g_string_new(argv[1]);
	printf("URL to test : %s\n\n", url->str);
	
	/* Try to find the protocol */
	found = find_protocol(url, &protocol, &position, &error);
	if (found) {
		printf("protocol found : %s\n", protocol->str);
	} else printf("protocol not found in URL\n\n");
	printf("posistion of the next item:\n");
	printf("%s\n", url->str);
	for(i=0; i<position; i++) printf(" ");
	printf("^\n");
		
	/* Try to find the user name */
	found = find_user(url, &user, &position, &error);
	if (found) {
		printf("name found : %s\n", user->str);
	} else printf("user name not found in URL\n");
	printf("posistion of the next item:\n");
	printf("%s\n", url->str);
	for(i=0; i<position; i++) printf(" ");
	printf("^\n");
	
	/* Try to find the password */
	found = find_passwd(url, &passwd, &position, &error);
	if (found) {
		printf("passwd found : %s\n", passwd->str);
		printf("\n");
	} else printf("passwd not found in URL\n");
	printf("posistion of the next item:\n");
	printf("%s\n", url->str);
	for(i=0; i<position; i++) printf(" ");
	printf("^\n");
	
	
	return 0;
}

#endif /* TEST_URL_UTIL */
