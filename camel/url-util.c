/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* url-util.c : utility functions to parse URLs */

/* 
 * This code is adapted form gzillaurl.c (http://www.gzilla.com)
 * Copyright (C) Raph Levien <raph@acm.org>
 *
 * Modifications by Bertrand Guiheneuf <Bertrand.Guiheneuf@inria.fr>
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






#include <ctype.h> /* for isalpha */
#include <stdlib.h> /* for atoi */

#include "url-util.h"



/**
 * g_url_is_absolute:
 * @url: 
 * 
 * 
 * 
 * Return value: 
 **/
gboolean 
g_url_is_absolute (const char *url) 
{
	gint i;
	
	for (i = 0; url[i] != '\0'; i++) {
		if (url[i] == ':')
			return TRUE;
		else if (!isalpha (url[i])) 
			return FALSE;
	}
	return FALSE;
}



/**
 * g_url_match_method:
 * @url: 
 * @method: 
 * 
 * 
 * 
 * Return value: TRUE if the method matches
 **/
gboolean 
g_url_match_method (const char *url, const char *method) 
{
	gint i;
	
	for (i = 0; method[i] != '\0'; i++)
		if (url[i] != method[i]) return FALSE;
	return (url[i] == ':');
}




/**
 * g_url_add_slash:
 * @url: 
 * @size_url: 
 * 
 * Add the trailing slash if necessary. Return FALSE if there isn't room
 * 
 * Return value: 
 **/
gboolean 
g_url_add_slash (char *url, gint size_url) 
{
	char hostname[256];
	gint port;
	char *tail;
	
	if (g_url_match_method (url, "http") ||
	    g_url_match_method (url, "ftp")) {
		tail = g_url_parse (url, hostname, sizeof(hostname), &port);
		if (tail == NULL)
			return TRUE;
		if (tail[0] == '\0') {
			if (strlen (url) + 1 == size_url)
				return FALSE;
			tail[0] = '/';
			tail[1] = '\0';
		}
	}
	return TRUE;
}




/**
 * g_url_relative:
 * @base_url: 
 * @relative_url: 
 * @new_url: 
 * @size_new_url: 
 * 
 * 
 * 
 * Return value: 
 **/
gboolean 
g_url_relative (const char *base_url,
		    const char *relative_url,
		    char *new_url,
		    gint size_new_url) 
{
	gint i, j, k;
	gint num_dotdot;
	
	if (base_url == NULL || g_url_is_absolute (relative_url)) {
		if (strlen (relative_url) >= size_new_url)
			return FALSE;
		strcpy (new_url, relative_url);
		return g_url_add_slash (new_url, size_new_url);
	}
	
	/* Assure that we have enough room for at least the base URL. */
	if (strlen (base_url) >= size_new_url)
		return FALSE;
	
	/* Copy http://hostname:port/ from base_url to new_url */
		i = 0;
		if (g_url_match_method (base_url, "http") ||
		    g_url_match_method (base_url, "ftp")) {
			while (base_url[i] != '\0' && base_url[i] != ':')
				new_url[i] = base_url[i++];
			if (base_url[i] != '\0')
				new_url[i] = base_url[i++];
			if (base_url[i] != '\0')
				new_url[i] = base_url[i++];
			if (base_url[i] != '\0')
				new_url[i] = base_url[i++];
			while (base_url[i] != '\0' && base_url[i] != '/')
				new_url[i] = base_url[i++];
		} else {
			while (base_url[i] != '\0' && base_url[i] != ':')
				new_url[i] = base_url[i++];
			if (base_url[i] != '\0')
				new_url[i] = base_url[i++];
		}
		
		if (relative_url[0] == '/') {
			if (i + strlen (relative_url) >= size_new_url)
				return FALSE;
			strcpy (new_url + i, relative_url);
			return g_url_add_slash (new_url, size_new_url);
		}
		
		/* At this point, i points to the first slash following the hostname
		   (and port) in base_url. */
		
		/* Now, figure how many ..'s to follow. */
		num_dotdot = 0;
		j = 0;
		while (relative_url[j] != '\0') {
			if (relative_url[j] == '.' &&
			    relative_url[j + 1] == '/') {
				j += 2;
			} else if (relative_url[j] == '.' &&
				   relative_url[j + 1] == '.' &&
				   relative_url[j + 2] == '/') {
				j += 3;
				num_dotdot++;
			} else {
				break;
			}
		}
		
		/* Find num_dotdot+1 slashes back from the end, point k there. */
		
		for (k = strlen (base_url); k > i && num_dotdot >= 0; k--)
			if (base_url[k - 1] == '/')
				num_dotdot--;
		
		if (k + 1 + strlen (relative_url) - j >= size_new_url)
			return FALSE;
		
		while (i < k)
			new_url[i] = base_url[i++];
		if (relative_url[0] == '#')
			while (base_url[i] != '\0')
				new_url[i] = base_url[i++];
		else if (base_url[i] == '/' || base_url[i] == '\0')
			new_url[i++] = '/';
		strcpy (new_url + i, relative_url + j);
		return g_url_add_slash (new_url, size_new_url);
}





/* Parse the url, packing the hostname and port into the arguments, and
   returning the suffix. Return NULL in case of failure. */

/**
 * g_url_parse:
 * @url: 
 * @hostname: 
 * @hostname_size: 
 * @port: 
 * 
 * 
 * 
 * Return value: 
 **/
char *
g_url_parse (char *url,
		 char *hostname,
		 gint hostname_size,
		 int *port) 
{
	gint i, j;
	
	for (i = 0; url[i] != '\0' && url[i] != ':'; i++);
	if (url[i] != ':' || url[i + 1] != '/' || url[i + 2] != '/') return NULL;
	i += 3;
	for (j = i; url[j] != '\0' && url[j] != ':' && url[j] != '/'; j++);
	if (j - i >= hostname_size) return NULL;
	memcpy (hostname, url + i, j - i);
	hostname[j - i] = '\0';
	if (url[j] == ':') {
		*port = atoi (url + j + 1);
		for (j++; url[j] != '\0' && url[j] != '/'; j++);
	}
	return url + j;
}




#ifndef UNIT_TEST
/* Parse "http://a/b#c" into "http://a/b" and "#c" (storing both as
   newly allocated strings into *p_head and *p_tail, respectively.
   
   Note: this routine allocates new strings for the subcomponents, so
   that there's no arbitrary restriction on sizes. That's the way I want
   all the URL functions to work eventually.
*/
void
g_url_parse_hash (char **p_head, char **p_tail, const char *url)
{
	gint i;
	
	/* todo: I haven't checked this for standards compliance. What's it
	   supposed to do when there are two hashes? */
	
	for (i = 0; url[i] != '\0' && url[i] != '#'; i++);
	*p_tail = g_strdup (url + i);
	*p_head = g_new (char, i + 1);
	memcpy (*p_head, url, i);
	(*p_head)[i] = '\0';
}
#endif





#ifdef UNIT_TEST
/* Unit test as follows:
   
   gcc -g -I/usr/local/include/gtk -DUNIT_TEST camelurl.c -o camelurl
   ./camelurl base_url relative_url
   
*/

int 
main (int argc, char **argv) 
{
	char buf[80];
	char hostname[80];
	char *tail;
	int port;
	
	if (argc == 3) {
		if (g_url_relative (argv[1], argv[2], buf, sizeof(buf))) {
			printf ("%s\n", buf);
			port = 80;
			tail = g_url_parse (buf, hostname, sizeof (hostname), &port);
			if (tail != NULL) {
				printf ("hostname = %s, port = %d, tail = %s\n", hostname, port, tail);
			}
		} else {
			printf ("buffer overflow!\n");
		}
	} else {
		printf ("Usage: %s base_url relative_url\n", argv[0]);
	}
	return 0;
}
#endif






