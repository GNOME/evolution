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
  XXX TODO: recover the words between #'s or ?'s after the path */

#include <config.h>
#include "url-util.h"

/* general item finder */
/* it begins the search at position @position in @url,
   returns true when the item is found, amd set position after the item */
typedef gboolean find_item_func(const gchar *url, gchar **item, guint *position, gboolean *error);

/* used to find one item (protocol, then user .... */
typedef struct {
	char *item_name;           /* item name : for debug only */
	gchar **item_value;      /* where the item value will go */
	find_item_func *find_func; /* item finder */
} FindStepStruct;

static gboolean _find_protocol (const gchar *url, gchar **item, guint *position, gboolean *error);
static gboolean _find_user (const gchar *url, gchar **item, guint *position, gboolean *error);
static gboolean _find_passwd (const gchar *url, gchar **item, guint *position, gboolean *error);
static gboolean _find_host (const gchar *url, gchar **item, guint *position, gboolean *error);
static gboolean _find_port (const gchar *url, gchar **item, guint *position, gboolean *error);
static gboolean _find_path (const gchar *url, gchar **item, guint *position, gboolean *error);



/**
 * g_url_new: create an Gurl object from a string
 *
 * @url_string: The string containing the URL to scan
 * 
 * This routine takes a gchar and parses it as an
 * URL of the form:
 * protocol://user:password@host:port/path
 * there is no test on the values. For example,
 * "port" can be a string, not only a number !
 * The Gurl structure fields ar filled with
 * the scan results. When a member of the 
 * general URL can not be found, the corresponding
 * Gurl member is NULL  
 * Fields filled in the Gurl structure are allocated
 * and url_string is not modified. 
 * 
 * Return value: a Gurl structure containing the URL items.
 **/
Gurl *g_url_new (const gchar* url_string)
{
	Gurl *g_url;
	
	gchar *protocol;
	gchar *user;
	gchar *passwd;
	gchar *host;
	gchar *port;
	gchar *path;
	
	guint position = 0;
	gboolean error;
	gboolean found;
	guint i;
	
	g_url = g_new (Gurl,1);
	
#define NB_STEP_URL  6
	{
		FindStepStruct step[NB_STEP_URL] = {
			{ "protocol", &(g_url->protocol), _find_protocol},
			{ "user", &(g_url->user), _find_user},
			{ "password", &(g_url->passwd), _find_passwd},
			{ "host", &(g_url->host), _find_host},
			{ "port", &(g_url->port), _find_port},
			{ "path", &(g_url->path), _find_path}
		};
		
		for (i = 0; i < NB_STEP_URL; i++) {
			found = step[i].find_func (url_string, 
						   step[i].item_value, 
						   &position, 
						   &error);
		}
	}
	
	return g_url;
}



void
g_url_free (Gurl *url)
{
	g_assert (url);

	if (url->protocol) g_free (url->protocol);
	if (url->user) g_free (url->user);
	if (url->passwd) g_free (url->passwd);
	if (url->host) g_free (url->host);
	if (url->port) g_free (url->port);
	if (url->path) g_free (url->path);

	g_free (url);
	
}

/**** PARSING FUNCTIONS ****/

/** So, yes, I must admit there would have been more elegant
    ways to do this, but it works, and quite well :)  */


static gboolean 
_find_protocol (const gchar *url, gchar **item, guint *position, gboolean *error)
{

	guint i;
	gint len_url;

	g_assert (url);
	g_assert (item);
	g_assert (position);

	len_url = strlen (url);
	
	*item = NULL;
	*error = FALSE;
	i = *position;
	
	/* find a ':' */
	while ((i < len_url) && (url[i] != ':')) i++;
	
	if (i == len_url) return FALSE;
	i++;

	/* check if it is followed by a "//" */
	if  ((i < len_url) && (url[i++] == '/'))
		if ((i < len_url) && (url[i++] == '/'))
		{
			*item = g_strndup (url, i-3);
			*position = i;
			return TRUE;
		}
	
	return FALSE;
}




static gboolean
_find_user (const gchar *url, gchar **item, guint *position, gboolean *error)
{
	guint i;
	guint at_pos;
	gint len_url;

	g_assert (url);
	g_assert (item);
	g_assert (position);

	len_url = strlen (url);	
	*item = NULL;
	i = *position;
	
	/* find a '@' */
	while ((i < len_url) && (url[i] != '@')) i++;
	
	if (i == len_url) return FALSE;
	at_pos = i;
	i = *position;

	/* find a ':' */
	while ((i < at_pos) && (url[i] != ':')) i++;

	/* now if i has not been incremented at all, there is no user */
	if (i == *position) {
		(*position)++;
		return FALSE;
	}
	
	*item = g_strndup (url+ *position, i - *position);
	if (i < at_pos) *position = i + 1; /* there was a ':', skip it */
	else *position = i;
	
	return TRUE;	
}

static gboolean
_find_passwd (const gchar *url, gchar **item, guint *position, gboolean *error)
{
	guint i;	
	gint len_url;
	gchar *str_passwd;

	g_assert (url);
	g_assert (item);
	g_assert (position);

	len_url = strlen (url);
	*item = NULL;
	i = *position;
	
	/* find a '@' */
	while ((i < len_url) && (url[i] != '@')) i++;
	
	if (i == len_url) return FALSE;
	/*i has not been incremented at all, there is no passwd */
	if (i == *position) {
		*position = i + 1;
		return FALSE;
	}
	
	*item = g_strndup (url + *position, i - *position);
	*position = i + 1; /* skip it the '@' */
	
	return TRUE;
}



static gboolean
_find_host (const gchar *url, gchar **item, guint *position, gboolean *error)
{
	guint i;
	guint slash_pos;
	gint len_url;
	
	g_assert (url);
	g_assert (item);
	g_assert (position);

	len_url = strlen (url);	
	*item = NULL;
	i = *position;
	
	/* find a '/' */
	while ((i < len_url) && (url[i] != '/')) i++;
	
	slash_pos = i;
	i = *position;

	/* find a ':' */
	while ( (i < slash_pos) && (url[i] != ':') ) i++;

	/* at this point if i has not been incremented at all, 
	   there is no host */
	if (i == *position) {
		/* if we have not met / or \0, we have : and must skip it */
		if (i < slash_pos) (*position)++;
		return FALSE;
	}
	
	*item = g_strndup (url + *position, i - *position);
	if (i < slash_pos) *position = i + 1; /* there was a ':', skip it */
	else *position=i;
	
	return TRUE;
}


static gboolean
_find_port (const gchar *url, gchar **item, guint *position, gboolean *error)
{
	guint i;
	guint slash_pos;
	gint len_url;
	
	g_assert (url);
	g_assert (item);
	g_assert (position);

	len_url = strlen (url);	
	*item = NULL;
	i=*position;
	
	/* find a '/' */
	while ((i < len_url) && (url[i] != '/')) i++;
	
	slash_pos = i;
	i = *position;

	/* find a ':' */
	while ((i < slash_pos) && (url[i] != ':')) i++;

	/* at this point if i has not been incremented at all, */
	/*   there is no port */
	if (i == *position) return FALSE;

	*item = g_strndup (url+ *position, i - *position);
	*position = i;
	return TRUE;
}


static gboolean
_find_path (const gchar *url, gchar **item, guint *position, gboolean *error)
{
	guint i;
	gint len_url;
	
	g_assert (url);
	g_assert (item);
	g_assert (position);

	len_url = strlen (url);
	*item = NULL;
	i = *position;
	

	/* find a '#' */
	while ((i < len_url) && (url[i] != '#') && (url[i] != '?')) i++;
	
	/*i has not been incremented at all, there is no path */
	if (i == *position) return FALSE;
	
	*item = g_strndup (url + *position, i - *position);
	*position=i;
	
	return TRUE;
}





/**** TEST ROUTINE - NOT COMPILED BY DEFAULT ****/

/* to tests this file :
   gcc -o test_url_util `glib-config --cflags`  -I.. -DTEST_URL_UTIL url-util.c `glib-config --libs`
   ./test_url_util URL
*/
#ifdef TEST_URL_UTIL



int 
main (int argc, char **argv)
{

	gchar *url;
	gchar *protocol;
	gchar *user;
	gchar *passwd;
	gchar *host;
	gchar *port;
	gchar *path;
	guint position=0;
	gboolean error;
	gboolean found;
	guint i;
	guint i_pos;

#define NB_STEP_TEST  6
	FindStepStruct test_step[NB_STEP_TEST] = {
		{ "protocol", &protocol, _find_protocol},
		{ "user", &user, _find_user},
		{ "password", &passwd, _find_passwd},
		{ "host", &host, _find_host},
		{ "port", &port, _find_port},
		{ "path", &path, _find_path}
	};
	url = argv[1];
	printf("URL to test : %s\n\n", url);
	for (i=0; i<NB_STEP_TEST; i++) {
		found = test_step[i].find_func (url, 
						test_step[i].item_value, 
						&position, 
						&error);
		if (found) {
			printf ("\t\t\t\t** %s found : %s\n",
				test_step[i].item_name,
				*(test_step[i].item_value));
		} else printf ("** %s not found in URL\n", test_step[i].item_name);
		printf ("next item position:\n");
		printf ("%s\n", url);
		for (i_pos = 0; i_pos < position; i_pos++) printf (" ");
		printf ("^\n");
		
	}
	 
}

#endif /* TEST_URL_UTIL */
