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
/*
  XXX TODO: recover  the words between #'s or ?'s after the path */

#include <config.h>
#include "url-util.h"

/* general item finder */
/* it begins the search at position @position in @url,
   returns true when the item is found, amd set position after the item */
typedef gboolean find_item_func(GString *url, GString **item, guint *position, gboolean *error);

/* used to find one item (protocol, then user .... */
typedef struct {
	char *item_name;           /* item name : for debug only */
	GString **item_value;      /* where the item value will go */
	find_item_func *find_func; /* item finder */
} FindStepStruct;

static gboolean find_protocol(GString *url, GString **item, guint *position, gboolean *error);
static gboolean find_user(GString *url, GString **item, guint *position, gboolean *error);
static gboolean find_passwd(GString *url, GString **item, guint *position, gboolean *error);
static gboolean find_host(GString *url, GString **item, guint *position, gboolean *error);
static gboolean find_port(GString *url, GString **item, guint *position, gboolean *error);
static gboolean find_path(GString *url, GString **item, guint *position, gboolean *error);



/**
 * new_g_url: create an Gurl object from a string
 * @url_string: The string containing the URL to scan
 * 
 * This routine takes a GString and parses it as an
 * URL of the form:
 * protocol://user:password@host:port/path
 * there is no test on the values. For example,
 * "port" can be a string, not only a number !
 * The Gurl structure fields ar filled with
 * the scan results. When a member of the 
 * general URL can not be found, the corresponding
 * Gurl member is NULL  
 * 
 * Return value: a Gurl structure containng the URL items.
 **/
Gurl *g_url_new(GString* url_string)
{
	Gurl *g_url;
	
	GString *protocol;
	GString *user;
	GString *passwd;
	GString *host;
	GString *port;
	GString *path;
	
	guint position=0;
	gboolean error;
	gboolean found;
	guint i;
	
	g_url = g_new(Gurl,1);

#define NB_STEP_URL  6
	{
		FindStepStruct step[NB_STEP_URL] = {
			{ "protocol", &(g_url->protocol), find_protocol},
			{ "user", &(g_url->user), find_user},
			{ "password", &(g_url->passwd), find_passwd},
			{ "host", &(g_url->host), find_host},
			{ "port", &(g_url->port), find_port},
			{ "path", &(g_url->path), find_path}
		};
		
		for (i=0; i<NB_STEP_URL; i++) {
			found = step[i].find_func(url_string, 
						  step[i].item_value, 
						  &position, 
						  &error);
		}
	}
	
	return g_url;
}





/** So, yes, I must admit there would have been more elegant
    ways to do this, but it works, and quite well :)  */


static gboolean 
find_protocol(GString *url, GString **item, guint *position, gboolean *error)
{

	guint i;
	gchar *str_url;
	gint len_url;
	gchar *str_protocol;
	
	str_url = url->str;
	len_url = url->len;
	
	*item = NULL;
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
			*item = g_string_new(str_protocol);
			*position=i;
			return TRUE;
		}
	
	return FALSE;
}




static gboolean
find_user(GString *url, GString **item, guint *position, gboolean *error)
{
	guint i;
	guint at_pos;
	
	gchar *str_url;
	gint len_url;
	gchar *str_user;
	
	str_url = url->str;
	len_url = url->len;
	
	*item = NULL;
	i=*position;
	

	/* find a '@' */
	while ((i<len_url) && (str_url[i] != '@')) i++;
	
	if (i==len_url) return FALSE;
	at_pos = i;
	i = *position;

	/* find a ':' */
	while ( (i<at_pos) && (str_url[i] != ':') ) i++;

	/* now if i has not been incremented at all, there is no user */
	if (i == *position) {
		(*position)++;
		return FALSE;
	}
	
	str_user = g_strndup(str_url+ *position, i - *position);
	*item = g_string_new(str_user);
	if (i<at_pos) *position=i+1; /* there was a ':', skip it */
	else *position=i;
	
	return TRUE;	
}

static gboolean
find_passwd(GString *url, GString **item, guint *position, gboolean *error)
{
	guint i;
	
	gchar *str_url;
	gint len_url;
	gchar *str_passwd;
	
	str_url = url->str;
	len_url = url->len;
	
	*item = NULL;
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
	*item = g_string_new(str_passwd);
	*position=i+1; /* skip it the '@' */
	
	return TRUE;
}



static gboolean
find_host(GString *url, GString **item, guint *position, gboolean *error)
{
	guint i;
	guint slash_pos;
	
	gchar *str_url;
	gint len_url;
	gchar *str_host;
	
	str_url = url->str;
	len_url = url->len;
	
	*item = NULL;
	i=*position;
	

	/* find a '/' */
	while ((i<len_url) && (str_url[i] != '/')) i++;
	
	slash_pos = i;
	i = *position;

	/* find a ':' */
	while ( (i<slash_pos) && (str_url[i] != ':') ) i++;

	/* at this point if i has not been incremented at all, 
	   there is no host */
	if (i == *position) {
		(*position)++;
		return FALSE;
	}
	
	str_host = g_strndup(str_url+ *position, i - *position);
	*item = g_string_new(str_host);
	if (i<slash_pos) *position=i+1; /* there was a ':', skip it */
	else *position=i;
	
	return TRUE;
}


static gboolean
find_port(GString *url, GString **item, guint *position, gboolean *error)
{
	guint i;
	guint slash_pos;
	
	gchar *str_url;
	gint len_url;
	gchar *str_port;
	
	str_url = url->str;
	len_url = url->len;
	
	*item = NULL;
	i=*position;
	

	/* find a '/' */
	while ((i<len_url) && (str_url[i] != '/')) i++;
	
	slash_pos = i;
	i = *position;

	/* find a ':' */
	while ( (i<slash_pos) && (str_url[i] != ':') ) i++;

	/* at this point if i has not been incremented at all, 
	   there is no port */
	if (i == *position) return FALSE;

	str_port = g_strndup(str_url+ *position, i - *position);
	*item = g_string_new(str_port);
	*position = i;
	return TRUE;
}


static gboolean
find_path(GString *url, GString **item, guint *position, gboolean *error)
{
	guint i;
	
	gchar *str_url;
	gint len_url;
	gchar *str_path;
	
	str_url = url->str;
	len_url = url->len;
	
	*item = NULL;
	i=*position;
	

	/* find a '#' */
	while ((i<len_url) && (str_url[i] != '#') && (str_url[i] != '?')) i++;
	
	/*i has not been incremented at all, there is no path */
	if (i == *position) return FALSE;
	
	str_path = g_strndup(str_url+ *position, i - *position);
	*item = g_string_new(str_path);
	*position=i;
	
	
	return TRUE;
}


/* to tests this file :
   gcc -o test_url_util `glib-config --cflags`  -DTEST_URL_UTIL url-util.c `glib-config --libs`
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
	GString *host;
	GString *port;
	GString *path;
	guint position=0;
	gboolean error;
	gboolean found;
	guint i;
	guint i_pos;

#define NB_STEP_TEST  6
	FindStepStruct test_step[NB_STEP_TEST] = {
		{ "protocol", &protocol, find_protocol},
		{ "user", &user, find_user},
		{ "password", &passwd, find_passwd},
		{ "host", &host, find_host},
		{ "port", &port, find_port},
		{ "path", &path, find_path}
	};
	url = g_string_new(argv[1]);
	printf("URL to test : %s\n\n", url->str);
	for (i=0; i<NB_STEP_TEST; i++) {
		found = test_step[i].find_func(url, 
					  test_step[i].item_value, 
					  &position, 
					  &error);
		if (found) {
			printf("\t\t\t\t** %s found : %s\n",
			       test_step[i].item_name,
			       (*test_step[i].item_value)->str);
		} else printf("** %s not found in URL\n", test_step[i].item_name);
		printf("next item position:\n");
		printf("%s\n", url->str);
		for(i_pos=0; i_pos<position; i_pos++) printf(" ");
		printf("^\n");
		
	}
	
}

#endif /* TEST_URL_UTIL */
