/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <ctype.h>

#include <bonobo.h>
#include <bonobo-conf/bonobo-config-database.h>
#include <gal/util/e-xml-utils.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <camel/camel-file-utils.h>

struct _storeinfo {
	char *base_url;
	char *namespace;
	char *encoded_namespace;
	char dir_sep;
	GPtrArray *folders;
};



static char
find_dir_sep (const char *lsub_response)
{
	register const unsigned char *inptr;
	const unsigned char *inend;
	
	inptr = (const unsigned char *) lsub_response;
	inend = inptr + strlen (inptr);
	
	if (strncmp (inptr, "* LSUB (", 8))
		return '\0';
	
	inptr += 8;
	while (inptr < inend && *inptr != ')')
		inptr++;
	
	if (inptr >= inend)
		return '\0';
	
	inptr++;
	while (inptr < inend && isspace ((int) *inptr))
		inptr++;
	
	if (inptr >= inend)
		return '\0';
	
	if (*inptr == '\"')
		inptr++;
	
	return inptr < inend ? *inptr : '\0';
}

static void
si_free (struct _storeinfo *si)
{
	int i;
	
	g_free (si->base_url);
	g_free (si->namespace);
	g_free (si->encoded_namespace);
	if (si->folders) {
		for (i = 0; i < si->folders->len; i++)
			g_free (si->folders->pdata[i]);
		g_ptr_array_free (si->folders, TRUE);
	}
	g_free (si);
}

static unsigned char tohex[16] = {
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

static char *
hex_encode (const char *in, size_t len)
{
	const unsigned char *inend = in + len;
	unsigned char *inptr, *outptr;
	char *outbuf;
	
	outptr = outbuf = g_malloc ((len * 3) + 1);
	
	inptr = (unsigned char *) in;
	while (inptr < inend) {
		if (*inptr > 127 || isspace ((int) *inptr)) {
			*outptr++ = '%';
			*outptr++ = tohex[(*inptr >> 4) & 0xf];
			*outptr++ = tohex[*inptr & 0xf];
			inptr++;
		} else
			*outptr++ = *inptr++;
	}
	
	*outptr = '\0';
	
	return outbuf;
}

#define HEXVAL(c) (isdigit (c) ? (c) - '0' : tolower (c) - 'a' + 10)

static char *
hex_decode (const char *in, size_t len)
{
	const unsigned char *inend = in + len;
	unsigned char *inptr, *outptr;
	char *outbuf;
	
	outptr = outbuf = g_malloc (len + 1);
	
	inptr = (unsigned char *) in;
	while (inptr < inend) {
		if (*inptr == '%') {
			if (isxdigit ((int) inptr[1]) && isxdigit ((int) inptr[2])) {
				*outptr++ = HEXVAL (inptr[1]) * 16 + HEXVAL (inptr[2]);
				inptr += 3;
			} else
				*outptr++ = *inptr++;
		} else
			*outptr++ = *inptr++;
	}
	
	*outptr = '\0';
	
	return outbuf;
}

static char *
parse_lsub (const char *lsub, char *dir_sep)
{
	const unsigned char *inptr = (const unsigned char *) lsub;
	const unsigned char *inend;
	int inlen, quoted = 0;
	
	inend = inptr + strlen (inptr);
	if (strncmp (inptr, "* LSUB (", 8))
		return NULL;
	
	inptr += 8;
	while (inptr < inend && *inptr != ')')
		inptr++;
	
	if (inptr >= inend)
		return NULL;
	
	inptr++;
	while (inptr < inend && isspace ((int) *inptr))
		inptr++;
	
	if (inptr >= inend)
		return NULL;
	
	/* skip over the dir sep */
	if (*inptr == '\"')
		inptr++;
	
	*dir_sep = (char) *inptr++;
	if (*inptr == '\"')
		inptr++;
	
	if (inptr >= inend)
		return NULL;
	
	while (inptr < inend && isspace ((int) *inptr))
		inptr++;
	
	if (inptr >= inend)
		return NULL;
	
	if (*inptr == '\"') {
		inptr++;
		quoted = 1;
	} else
		quoted = 0;
	
	inlen = strlen (inptr) - quoted;
	
	return g_strndup (inptr, inlen);
}

static void
cache_upgrade (struct _storeinfo *si, const char *folder_name)
{
	const char *old_folder_name = folder_name;
	char *oldpath, *newpath, *p;
	struct dirent *dent;
	DIR *dir = NULL;
	
	if (si->namespace && strcmp ("INBOX", folder_name)) {
		if (!strncmp (old_folder_name, si->namespace, strlen (si->namespace))) {
			old_folder_name += strlen (si->namespace);
			if (*old_folder_name == si->dir_sep)
				old_folder_name++;
		}
	}
	
	oldpath = g_strdup_printf ("%s/evolution/mail/imap/%s/%s", getenv ("HOME"),
				   si->base_url + 7, old_folder_name);
	
	newpath = g_strdup_printf ("%s/evolution/mail/imap/%s/folders/%s",
				   getenv ("HOME"), si->base_url + 7, folder_name);
	
	if (!strcmp (folder_name, "folders"))
		goto special_case_folders;
	
	if (si->dir_sep != '/') {
		p = newpath + strlen (newpath) - strlen (folder_name) - 1;
		while (*p) {
			if (*p == si->dir_sep)
				*p = '/';
			p++;
		}
	}
	
	/* make sure all parent directories exist */
	if ((p = strrchr (newpath, '/'))) {
		*p = '\0';
		camel_mkdir (newpath, 0755);
		*p = '/';
	}
	
	if (rename (oldpath, newpath) == -1) {
		fprintf (stderr, "Failed to upgrade cache for imap folder %s/%s: %s\n",
			 si->base_url, folder_name, g_strerror (errno));
	}
	
	g_free (oldpath);
	g_free (newpath);
	
	return;
	
 special_case_folders:
	
	/* the user had a toplevel folder named "folders" */
	if (camel_mkdir (newpath, 0755) == -1) {
		/* we don't bother to check EEXIST because well, if
                   folders/folders exists then we're pretty much
                   fucked */
		goto exception;
	}
	
	if (!(dir = opendir (oldpath)))
		goto exception;
	
	while ((dent = readdir (dir))) {
		char *old_path, *new_path;
		
		if (!strcmp (dent->d_name, ".") || !strcmp (dent->d_name, ".."))
			continue;
		
		old_path = g_strdup_printf ("%s/%s", oldpath, dent->d_name);
		new_path = g_strdup_printf ("%s/%s", newpath, dent->d_name);
		
		/* make sure all parent directories exist */
		if ((p = strrchr (new_path, '/'))) {
			*p = '\0';
			camel_mkdir (new_path, 0755);
			*p = '/';
		}
		
		if (rename (old_path, new_path) == -1) {
			g_free (old_path);
			g_free (new_path);
			goto exception;
		}
		
		g_free (old_path);
		g_free (new_path);
	}
	
	closedir (dir);
	
	g_free (oldpath);
	g_free (newpath);
	
	return;
	
 exception:
	
	fprintf (stderr, "Failed to upgrade cache for imap folder %s/%s: %s\n",
		 si->base_url, folder_name, g_strerror (errno));
	
	if (dir)
		closedir (dir);
	
	g_free (oldpath);
	g_free (newpath);
}

static int
foldercmp (const void *f1, const void *f2)
{
	const char **folder1 = (const char **) f1;
	const char **folder2 = (const char **) f2;
	
	return strcmp (*folder1, *folder2);
}

static void
cache_upgrade_and_free (gpointer key, gpointer val, gpointer user_data)
{
	struct _storeinfo *si = val;
	GPtrArray *folders;
	char *path = NULL;
	char dir_sep;
	int i;
	
	if (si->folders) {
		path = g_strdup_printf ("%s/evolution/mail/imap/%s/folders",
					getenv ("HOME"), si->base_url + 7);
		
		if (mkdir (path, 0755) == -1 && errno != EEXIST) {
			fprintf (stderr, "Failed to create directory %s: %s", path, g_strerror (errno));
			goto exception;
		}
		
		g_free (path);
		folders = g_ptr_array_new ();
		for (i = 0; i < si->folders->len; i++) {
			if ((path = parse_lsub (si->folders->pdata[i], &dir_sep))) {
				g_ptr_array_add (folders, path);
			}
		}
		
		/* sort the folders so that parents get created before
                   their children */
		qsort (folders->pdata, folders->len, sizeof (void *), foldercmp);
		
		for (i = 0; i < folders->len; i++) {
			cache_upgrade (si, folders->pdata[i]);
			g_free (folders->pdata[i]);
		}
	}
	
	si_free (si);
	
	return;
	
 exception:
	
	fprintf (stderr, "Could not upgrade imap cache for %s: %s\n",
		 si->base_url + 7, g_strerror (errno));
	
	g_free (path);
	
	si_free (si);
}

static char *
get_base_url (const char *protocol, const char *uri)
{
	unsigned char *base_url, *p;
	
	p = (unsigned char *) uri + strlen (protocol) + 1;
	if (!strncmp (p, "//", 2))
		p += 2;
	
	base_url = p;
	p = strchr (p, '/');
	base_url = g_strdup_printf ("%s://%.*s", protocol, p ? (int) (p - base_url) : (int) strlen (base_url), base_url);
	
	return base_url;
}

static char *
imap_namespace (const char *uri)
{
	unsigned char *name, *p;
	
	if ((name = strstr (uri, ";namespace=\"")) == NULL)
		return NULL;
	
	name += strlen (";namespace=\"");
	p = name;
	while (*p && *p != '\"')
		p++;
	
	return g_strndup (name, p - name);
}

static char *
find_folder (GPtrArray *folders, const char *folder, char *dir_sep)
{
	const unsigned char *inptr, *inend;
	int inlen, len, diff, i;
	int quoted;
	
	len = strlen (folder);
	
	for (i = 0; i < folders->len; i++) {
		inptr = folders->pdata[i];
		inend = inptr + strlen (inptr);
		if (strncmp (inptr, "* LSUB (", 8))
			continue;
		
		inptr += 8;
		while (inptr < inend && *inptr != ')')
			inptr++;
		
		if (inptr >= inend)
			continue;
		
		inptr++;
		while (inptr < inend && isspace ((int) *inptr))
			inptr++;
		
		if (inptr >= inend)
			continue;
		
		/* skip over the dir sep */
		if (*inptr == '\"')
			inptr++;
		
		*dir_sep = *inptr++;
		if (*inptr == '\"')
			inptr++;
		
		if (inptr >= inend)
			continue;
		
		while (inptr < inend && isspace ((int) *inptr))
			inptr++;
		
		if (inptr >= inend)
			continue;
		
		if (*inptr == '\"') {
			inptr++;
			quoted = 1;
		} else
			quoted = 0;
		
		inlen = strlen (inptr) - quoted;
		if (len > inlen)
			continue;
		
		diff = inlen - len;
		if (!strncmp (inptr + diff, folder, len))
			return hex_encode (inptr, inlen);
	}
	
	*dir_sep = '\0';
	
	return NULL;
}

static char *
imap_url_upgrade (GHashTable *imap_sources, const char *uri)
{
	struct _storeinfo *si;
	unsigned char *base_url, *folder, *p, *new = NULL;
	char dir_sep;
	
	base_url = get_base_url ("imap", uri);
	
	fprintf (stderr, "checking for %s... ", base_url);
	if (!(si = g_hash_table_lookup (imap_sources, base_url))) {
		fprintf (stderr, "not found.\n");
		g_warning ("Unknown imap account: %s", base_url);
		g_free (base_url);
		return NULL;
	}
	
	fprintf (stderr, "found.\n");
	p = (unsigned char *) uri + strlen (base_url) + 1;
	if (!strcmp (p, "INBOX")) {
		new = g_strdup_printf ("%s/INBOX", base_url);
		g_free (base_url);
		return new;
	}
	
	p = hex_decode (p, strlen (p));
	
	fprintf (stderr, "checking for folder %s on %s... ", p, base_url);
	folder = si->folders ? find_folder (si->folders, p, &dir_sep) : NULL;
	if (folder == NULL) {
		fprintf (stderr, "not found.\n");
		folder = p;
		if (si->namespace) {
			if (!si->dir_sep) {
				fprintf (stderr, "checking for directory separator in namespace param... ");
				if (*si->namespace == '/') {
					dir_sep = '/';
				} else {
					p = si->namespace;
					while (*p && !ispunct ((int) *p))
						p++;
					
					dir_sep = (char) *p;
				}
			} else {
				dir_sep = si->dir_sep;
			}
			
			if (dir_sep) {
				fprintf (stderr, "found: '%c'\n", dir_sep);
				p = folder;
				folder = hex_encode (folder, strlen (folder));
				new = g_strdup_printf ("%s/%s%c%s", base_url, si->encoded_namespace, dir_sep, folder);
				g_free (folder);
				folder = p;
				
				p = new + strlen (base_url) + 1;
				while (*p) {
					if (*p == dir_sep)
						*p = '/';
					p++;
				}
			} else {
				fprintf (stderr, "not found.");
				g_warning ("Cannot update settings for imap folder %s: unknown directory separator", uri);
			}
		} else {
			g_warning ("Cannot update settings for imap folder %s: unknown namespace", uri);
		}
		
		g_free (base_url);
		g_free (folder);
		
		return new;
	} else
		g_free (p);
	
	fprintf (stderr, "found.\n");
	new = g_strdup_printf ("%s/%s", base_url, folder);
	g_free (folder);
	
	if (!si->dir_sep)
		si->dir_sep = dir_sep;
	
	if (dir_sep) {
		p = new + strlen (base_url) + 1;
		while (*p) {
			if (*p == dir_sep)
				*p = '/';
			p++;
		}
	}
	
	g_free (base_url);
	
	return new;
}

static char *
exchange_url_upgrade (const char *uri)
{
	unsigned char *base_url, *folder;
	char *url;
	
	base_url = get_base_url ("exchange", uri);
	folder = (unsigned char *) uri + strlen (base_url) + 1;
	
	if (strncmp (folder, "exchange/", 9))
		return g_strdup (uri);
	
	folder += 9;
	while (*folder && *folder != '/')
		folder++;
	if (*folder == '/')
		folder++;
	
	folder = hex_decode (folder, strlen (folder));
	url = g_strdup_printf ("%s/personal/%s", base_url, folder);
	g_free (base_url);
	g_free (folder);
	
	return url;
}

static int
mailer_upgrade_account_info (Bonobo_ConfigDatabase db, const char *key, int num, GHashTable *imap_sources)
{
	char *path, *uri, *new;
	int i;
	
	for (i = 0;  i < num; i++) {
		path = g_strdup_printf ("/Mail/Accounts/account_%s_folder_uri_%d", key, i);
		uri = bonobo_config_get_string (db, path, NULL);
		if (uri) {
			if (!strncmp (uri, "imap:", 5)) {
				new = imap_url_upgrade (imap_sources, uri);
				if (new) {
					bonobo_config_set_string (db, path, new, NULL);
					g_free (new);
				}
			} else if (!strncmp (uri, "exchange:", 9)) {
				new = exchange_url_upgrade (uri);
				bonobo_config_set_string (db, path, new, NULL);
				g_free (new);
			}
		}
		
		g_free (uri);
		g_free (path);
	}
	
	return 0;
}

static int
mailer_upgrade_xml_file (GHashTable *imap_sources, const char *filename)
{
	unsigned char *buffer, *inptr, *start, *uri, *new;
	ssize_t nread = 0, nwritten, n;
	gboolean url_need_upgrade;
	struct stat st;
	size_t len;
	char *bak;
	int fd;
	
	bak = g_strdup_printf ("%s.bak-1.0", filename);
	if (stat (bak, &st) != -1) {
		/* seems we have already converted this file? */
		fprintf (stderr, "\n%s already exists, assuming %s has already been upgraded\n", bak, filename);
		g_free (bak);
		return 0;
	}
	
	if (stat (filename, &st) == -1 || (fd = open (filename, O_RDONLY)) == -1) {
		/* file doesn't exist? I guess nothing to upgrade here */
		fprintf (stderr, "\nCould not open %s: %s\n", filename, strerror (errno));
		g_free (bak);
		return 0;
	}
	
	start = buffer = g_malloc (st.st_size + 1);
	do {
		do {
			n = read (fd, buffer + nread, st.st_size - nread);
		} while (n == -1 && errno == EINTR);
		
		if (n > 0)
			nread += n;
	} while (n != -1 && nread < st.st_size);
	buffer[nread] = '\0';
	
	if (nread < st.st_size) {
		/* failed to load the entire file? */
		fprintf (stderr, "\nFailed to load %s: %s\n", filename, strerror (errno));
		g_free (buffer);
		g_free (bak);
		close (fd);
		return -1;
	}
	
	close (fd);
	
	inptr = buffer;
	url_need_upgrade = FALSE;
	do {
		inptr = strstr (inptr, "uri=\"");
		if (inptr) {
			inptr += 5;
			url_need_upgrade = !strncmp (inptr, "imap:", 5) || !strncmp (inptr, "exchange:", 9);
		}
	} while (inptr && !url_need_upgrade);
	
	if (inptr == NULL) {
		/* no imap urls in this xml file, so no need to "upgrade" it */
		fprintf (stdout, "\nNo updates required for %s\n", filename);
		g_free (buffer);
		g_free (bak);
		return 0;
	}
	
	if (rename (filename, bak) == -1) {
		/* failed to backup xml file */
		fprintf (stderr, "\nFailed to create backup file %s: %s\n", bak, strerror (errno));
		g_free (buffer);
		g_free (bak);
		return -1;
	}
	
	if ((fd = open (filename, O_WRONLY | O_CREAT | O_TRUNC, 0644)) == -1) {
		/* failed to create new xml file */
		fprintf (stderr, "\nFailed to create new %s: %s\n", filename, strerror (errno));
		rename (bak, filename);
		g_free (buffer);
		g_free (bak);
		return -1;
	}
	
	while (inptr != NULL) {
		len = inptr - start;
		nwritten = 0;
		do {
			do {
				n = write (fd, start + nwritten, len - nwritten);
			} while (n == -1 && errno == EINTR);
			
			if (n > 0)
				nwritten += n;
		} while (n != -1 && nwritten < len);
		
		if (nwritten < len)
			goto exception;
		
		start = inptr;
		while (*start && *start != '"')
			start++;
		
		uri = g_strndup (inptr, start - inptr);
		if (!strncmp (uri, "imap:", 5)) {
			if ((new = imap_url_upgrade (imap_sources, uri)) == NULL) {
				new = uri;
				uri = NULL;
			}
		} else if (!strncmp (uri, "exchange:", 9)) {
			new = exchange_url_upgrade (uri);
		} else {
			new = uri;
			uri = NULL;
		}
		g_free (uri);
		
		nwritten = 0;
		len = strlen (new);
		do {
			do {
				n = write (fd, new + nwritten, len - nwritten);
			} while (n == -1 && errno == EINTR);
			
			if (n > 0)
				nwritten += n;
		} while (n != -1 && nwritten < len);
		
		g_free (new);
		
		if (nwritten < len)
			goto exception;
		
		inptr = start;
		url_need_upgrade = FALSE;
		do {
			inptr = strstr (inptr, "uri=\"");
			if (inptr) {
				inptr += 5;
				url_need_upgrade = !strncmp (inptr, "imap:", 5) || !strncmp (inptr, "exchange:", 9);
			}
		} while (inptr && !url_need_upgrade);
	}
	
	nwritten = 0;
	len = strlen (start);
	do {
		do {
			n = write (fd, start + nwritten, len - nwritten);
		} while (n == -1 && errno == EINTR);
		
		if (n > 0)
			nwritten += n;
	} while (n != -1 && nwritten < len);
	
	if (nwritten < len)
		goto exception;
	
	if (fsync (fd) == -1)
		goto exception;
	
	close (fd);
	g_free (buffer);
	
	fprintf (stdout, "\nSuccessfully upgraded %s\nPrevious settings saved in %s\n\n", filename, bak);
	
	g_free (bak);
	
	return 0;
	
 exception:
	
	fprintf (stderr, "\nFailed to save updated settings to %s: %s\n\n", filename, strerror (errno));
	
	close (fd);
	g_free (buffer);
	unlink (filename);
	rename (bak, filename);
	g_free (bak);
	
	return -1;
}

static char *
shortcuts_upgrade_uri (GHashTable *accounts, GHashTable *imap_sources, const char *account, const char *folder)
{
	char *url, *name, *decoded, *new = NULL;
	struct _storeinfo *si;
	int type;
	
	type = GPOINTER_TO_INT ((si = g_hash_table_lookup (accounts, account)));
	if (type == 1) {
		/* exchange */
		decoded = hex_decode (folder, strlen (folder));
		name = g_strdup_printf ("personal/%s", decoded);
		g_free (decoded);
		
		return name;
	} else {
		/* imap */
		url = g_strdup_printf ("%s/%s", si->base_url, folder);
		new = imap_url_upgrade (imap_sources, url);
		g_free (url);
		
		if (new) {
			name = new + strlen (si->base_url) + 1;
			name = hex_decode (name, strlen (name));
			g_free (new);
			
			return name;
		}
	}
	
	return NULL;
}

static int
shortcuts_upgrade_xml_file (GHashTable *accounts, GHashTable *imap_sources, const char *filename)
{
	char *bak, *uri, *account, *folder, *new, *new_uri, *type;
	struct stat st;
	xmlDoc *doc;
	xmlNode *group, *item;
	int account_len;
	gboolean changed = FALSE;
	
	bak = g_strdup_printf ("%s.bak-1.0", filename);
	if (stat (bak, &st) != -1) {
		/* seems we have already converted this file? */
		fprintf (stderr, "\n%s already exists, assuming %s has already been upgraded\n", bak, filename);
		g_free (bak);
		return 0;
	}
	
	if (stat (filename, &st) == -1) {
		/* file doesn't exist? I guess nothing to upgrade here */
		fprintf (stderr, "\nCould not open %s: %s\n", filename, strerror (errno));
		g_free (bak);
		return 0;
	}
	
	doc = xmlParseFile (filename);
	if (!doc || !doc->xmlRootNode) {
		/* failed to load/parse the file? */
		fprintf (stderr, "\nFailed to load %s\n", filename);
		g_free (bak);
		return -1;
	}
	
	for (group = doc->xmlRootNode->xmlChildrenNode; group; group = group->next) {
		for (item = group->xmlChildrenNode; item; item = item->next) {
			/* Fix IMAP/Exchange URIs */
			uri = xmlNodeGetContent (item);
			if (!strncmp (uri, "evolution:/", 11)) {
				if (!strcmp (uri, "evolution:/local/Inbox")) {
					xmlNodeSetContent (item, "default:mail");
					changed = TRUE;
				} else if (!strcmp (uri, "evolution:/local/Calendar")) {
					xmlNodeSetContent (item, "default:calendar");
					changed = TRUE;
				} else if (!strcmp (uri, "evolution:/local/Contacts")) {
					xmlNodeSetContent (item, "default:contacts");
					changed = TRUE;
				} else if (!strcmp (uri, "evolution:/local/Tasks")) {
					xmlNodeSetContent (item, "default:tasks");
					changed = TRUE;
				} else {
					account_len = strcspn (uri + 11, "/");
					account = g_strndup (uri + 11, account_len);
					if (g_hash_table_lookup (accounts, account)) {
						folder = uri + 11 + account_len;
						if (*folder)
							folder++;
						new = shortcuts_upgrade_uri (accounts, imap_sources, account, folder);
						new_uri = g_strdup_printf ("evolution:/%s/%s", account, new);
						xmlNodeSetContent (item, new_uri);
						changed = TRUE;
						g_free (new_uri);
					}
					g_free (account);
				}
			}
			xmlFree (uri);
			
			/* Fix LDAP shortcuts */
			type = xmlGetProp (item, "type");
			if (type) {
				if (!strcmp (type, "ldap-contacts")) {
					xmlSetProp (item, "type", "contacts/ldap");
					changed = TRUE;
				}
				xmlFree (type);
			}
		}
	}
	
	if (!changed) {
		fprintf (stdout, "\nNo updates required for %s\n", filename);
		xmlFreeDoc (doc);
		g_free (bak);
		return 0;
 	}
	
	if (rename (filename, bak) == -1) {
		/* failed to backup xml file */
		fprintf (stderr, "\nFailed to create backup file %s: %s\n", bak, strerror (errno));
		xmlFreeDoc (doc);
		g_free (bak);
		return -1;
	}
	
	if (e_xml_save_file (filename, doc) == -1) {
		fprintf (stderr, "\nFailed to save updated settings to %s: %s\n\n", filename, strerror (errno));
		xmlFreeDoc (doc);
		unlink (filename);
		rename (bak, filename);
		g_free (bak);
		return -1;
	}
	
	fprintf (stdout, "\nSuccessfully upgraded %s\nPrevious settings saved in %s\n\n", filename, bak);
	
	xmlFreeDoc (doc);
	g_free (bak);
	
	return 0;
}


static int
mailer_upgrade (Bonobo_ConfigDatabase db)
{
	GHashTable *imap_sources, *accounts;
	char *path, *uri;
	char *account, *transport;
	int num, i;
	
	if ((num = bonobo_config_get_long_with_default (db, "/Mail/Accounts/num", 0, NULL)) == 0) {
		/* nothing to upgrade */
		return 0;
	}
	
	accounts = g_hash_table_new (g_str_hash, g_str_equal);
	imap_sources = g_hash_table_new (g_str_hash, g_str_equal);
	for (i = 0; i < num; i++) {
		struct _storeinfo *si;
		struct stat st;
		char *string;
		guint32 tmp;
		FILE *fp;
		int j;
		
		path = g_strdup_printf ("/Mail/Accounts/source_url_%d", i);
		uri = bonobo_config_get_string (db, path, NULL);
		g_free (path);
		if (uri && !strncmp (uri, "imap:", 5)) {
			path = g_strdup_printf ("/Mail/Accounts/account_name_%d", i);
			account = bonobo_config_get_string (db, path, NULL);
			g_free (path);
			
			si = g_new (struct _storeinfo, 1);
			si->base_url = get_base_url ("imap", uri);
			si->namespace = imap_namespace (uri);
			si->encoded_namespace = NULL;
			si->dir_sep = '\0';
			si->folders = NULL;
			
			path = si->base_url + 7;
			
			path = g_strdup_printf ("%s/evolution/mail/imap/%s/storeinfo", getenv ("HOME"), path);
			if (stat (path, &st) != -1 && (fp = fopen (path, "r")) != NULL) {
				camel_file_util_decode_uint32 (fp, &tmp);
				camel_file_util_decode_uint32 (fp, &tmp);
				
				j = 0;
				si->folders = g_ptr_array_new ();
				while (camel_file_util_decode_string (fp, &string) != -1) {
					if (j++ > 0) {
						g_ptr_array_add (si->folders, string);
					} else {
						if (!si->namespace)
							si->namespace = string;
						else
							g_free (string);
						
						camel_file_util_decode_uint32 (fp, &tmp);
						si->dir_sep = (char) tmp & 0xff;
					}
				}
				
				fclose (fp);
			}
			g_free (path);
			
			if (si->folders && si->folders->len > 0)
				si->dir_sep = find_dir_sep (si->folders->pdata[0]);
			
			if (si->namespace) {
				/* strip trailing dir_sep from namespace if it's there */
				j = strlen (si->namespace) - 1;
				if (si->namespace[j] == si->dir_sep)
					si->namespace[j] = '\0';
				
				/* set the encoded version of the namespace */
				si->encoded_namespace = g_strdup (si->namespace);
				for (j = 0; j < strlen (si->encoded_namespace); j++) {
					if (si->encoded_namespace[j] == '/')
						si->encoded_namespace[j] = '.';
				}
			}
			
			g_hash_table_insert (imap_sources, si->base_url, si);
			
			if (account)
				g_hash_table_insert (accounts, account, si);
		} else if (uri && !strncmp (uri, "exchange:", 9)) {
			/* Upgrade transport uri */
			path = g_strdup_printf ("/Mail/Accounts/transport_url_%d", i);
			transport = bonobo_config_get_string (db, path, NULL);
			if (transport && !strncmp (transport, "exchanget:", 10))
				bonobo_config_set_string (db, path, uri, NULL);
			g_free (transport);
			g_free (path);
			
			path = g_strdup_printf ("/Mail/Accounts/account_name_%d", i);
			account = bonobo_config_get_string (db, path, NULL);
			g_free (path);
			
			if (account)
				g_hash_table_insert (accounts, account, GINT_TO_POINTER (1));
		}
		
		g_free (uri);
	}
	
	if (g_hash_table_size (accounts) == 0) {
		/* user doesn't have any imap/exchange accounts - nothing to upgrade */
		g_hash_table_destroy (imap_sources);
		return 0;
	}
	
	/* upgrade user's account info (bug #29135) */
	mailer_upgrade_account_info (db, "drafts", num, imap_sources);
	mailer_upgrade_account_info (db, "sent", num, imap_sources);
	
	/* upgrade user's filters/vfolders (bug #24451) */
	path = g_strdup_printf ("%s/evolution/filters.xml", getenv ("HOME"));
	mailer_upgrade_xml_file (imap_sources, path);
	g_free (path);
	
	path = g_strdup_printf ("%s/evolution/vfolders.xml", getenv ("HOME"));
	mailer_upgrade_xml_file (imap_sources, path);
	g_free (path);
	
	/* upgrade user's shortcuts (there's no bug # for this one) */
	path = g_strdup_printf ("%s/evolution/shortcuts.xml", getenv ("HOME"));
	shortcuts_upgrade_xml_file (accounts, imap_sources, path);
	g_free (path);
	
	g_hash_table_foreach (imap_sources, cache_upgrade_and_free, NULL);
	g_hash_table_destroy (imap_sources);
#if 0
	path = g_strdup_printf ("%s/evolution/mail/imap", getenv ("HOME"));
	bak = g_strdup_printf ("%s.bak-1.0", path);
	
	if (rename (path, bak) == -1)
		fprintf (stderr, "\nFailed to backup Evolution 1.0's IMAP cache: %s\n", strerror (errno));
	
	g_free (path);
	g_free (bak);
#endif
	
	return 0;
}

static Bonobo_ConfigDatabase
get_config_db (void)
{
	Bonobo_ConfigDatabase db;
	CORBA_Environment ev;
	
	CORBA_exception_init (&ev);
	
	db = bonobo_get_object ("wombat:", "Bonobo/ConfigDatabase", &ev);
	if (BONOBO_EX (&ev) || db == CORBA_OBJECT_NIL) {
		fprintf (stderr, "get_config_db(): Could not get the config database object '%s'",
			 bonobo_exception_get_text (&ev));
		db = CORBA_OBJECT_NIL;
	}
	
	CORBA_exception_free (&ev);
	
	return db;
}

static int
upgrade (void)
{
	Bonobo_ConfigDatabase db;
	CORBA_Environment ev;
	
	if ((db = get_config_db ()) == CORBA_OBJECT_NIL)
		g_error ("Could not get config db");
	
	mailer_upgrade (db);
	
	CORBA_exception_init (&ev);
	Bonobo_ConfigDatabase_sync (db, &ev);
	
	gtk_main_quit ();
	
	return FALSE;
}

int main (int argc, char **argv)
{
	CORBA_ORB orb;
	
	gnome_init ("evolution-upgrade", "1.0", argc, argv);
	
	if ((orb = oaf_init (argc, argv)) == NULL)
		g_error ("Cannot init oaf");
	
	if (bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE)
		g_error ("Cannot init bonobo");
	
	gtk_idle_add ((GtkFunction) upgrade, NULL);
	
	bonobo_main ();
	
	return 0;
}
