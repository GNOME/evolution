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
#include <errno.h>
#include <ctype.h>

#include <bonobo.h>
#include <bonobo-conf/bonobo-config-database.h>


struct _storeinfo {
	char *base_url;
	char *namespace;
	char dir_sep;
	GByteArray *junk;
};


static void *
e_memmem (const void *haystack, size_t haystacklen, const void *needle, size_t needlelen)
{
	register unsigned char *h, *n, *hc, *nc;
	unsigned char *he, *ne;
	
	if (haystacklen < needlelen) {
		return NULL;
	} else if (needlelen == 0) {
		return (void *) haystack;
	}
	
	h = (unsigned char *) haystack;
	he = (unsigned char *) haystack + haystacklen - needlelen;
	n = (unsigned char *) needle;
	ne = (unsigned char *) needle + needlelen;
	
	while (h < he) {
		if (*h == *n) {
			for (hc = h + 1, nc = n + 1; nc < ne; hc++, nc++)
				if (*hc != *nc)
					break;
			
			if (nc == ne)
				return (void *) h;
		}
		
		h++;
	}
	
	return NULL;
}


static char
find_dir_sep (GByteArray *buf)
{
	unsigned char *p;
	
	if (!(p = e_memmem (buf->data, buf->len, "* LSUB (", 8)))
		return '\0';
	
	p += 9;
	while (*p != ')')
		p++;
	
	p++;
	while (isspace ((int) *p))
		p++;
	
	if (*p == '\"')
		p++;
	
	return *p;
}

static void
si_free (gpointer key, gpointer val, gpointer user_data)
{
	struct _storeinfo *si = val;
	
	g_free (si->base_url);
	g_free (si->namespace);
	g_byte_array_free (si->junk, TRUE);
	g_free (si);
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
	base_url = g_strdup_printf ("%s://%.*s", protocol, p ? p - base_url : strlen (base_url), base_url);
	
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
imap_url_upgrade (GHashTable *imap_sources, const char *uri)
{
	struct _storeinfo *si;
	unsigned char *base_url, *folder, *p, *new = NULL;
	unsigned char dir_sep;
	int len;
	
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
	
	folder = g_strdup_printf ("%s\"", p);
	len = strlen (folder);
	
	fprintf (stderr, "checking for folder %s on %s... ", p, base_url);
	p = e_memmem (si->junk->data, si->junk->len, folder, len);
	g_free (folder);
	if (p == NULL) {
		fprintf (stderr, "not found.\n");
		if (si->namespace) {
			if (!si->dir_sep) {
				fprintf (stderr, "checking for directory separator in namespace param... ");
				folder = p;
				if (*si->namespace == '/') {
					dir_sep = '/';
				} else {
					p = si->namespace;
					while (*p && !ispunct ((int) *p))
						p++;
					
					dir_sep = *p;
				}
			} else
				dir_sep = si->dir_sep;
			
			if (dir_sep) {
				fprintf (stderr, "found: '%c'\n", dir_sep);
				if (si->namespace[strlen (si->namespace) - 1] == dir_sep)
					new = g_strdup_printf ("%s/%s%s", base_url, si->namespace, folder);
				else
					new = g_strdup_printf ("%s/%s%c%s", base_url, si->namespace, dir_sep, folder);
				
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
		return new;
	}
	
	fprintf (stderr, "found.\n");
	len--;
	folder = p;
	while (*folder != '\"') {
		folder--;
		len++;
	}
	
	new = g_strdup_printf ("%s/%.*s", base_url, len - 1, folder + 1);
	if (si->dir_sep) {
		p = new + strlen (base_url) + 1;
		while (*p) {
			if (*p == si->dir_sep)
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
	
	if (!strncmp (folder, "exchange/", 9)) {
		folder += 9;
		while (*folder && *folder != '/')
			folder++;
		if (*folder == '/')
			folder++;
	}
	
	url = g_strdup_printf ("%s/personal/%s", base_url, folder);
	g_free (base_url);
	
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
		inptr = strstr (inptr, "<folder uri=\"");
		if (inptr) {
			inptr += 13;
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
			inptr = strstr (inptr, "<folder uri=\"");
			if (inptr) {
				inptr += 13;
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
	
	fprintf (stdout, "\nSuccessfully upgraded %s\nPrevious settings saved in %s\n", filename, bak);
	
	g_free (bak);
	
	return 0;
	
 exception:
	
	fprintf (stderr, "\nFailed to save updated settings to %s: %s\n", filename, strerror (errno));
	
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
	struct _storeinfo *si;
	char *url, *new;
	int type;
	
	type = GPOINTER_TO_INT ((si = g_hash_table_lookup (accounts, account)));
	if (type == 1) {
		/* exchange */
		return g_strdup_printf ("personal/%s", folder);
	} else {
		/* imap */
		url = g_strdup_printf ("%s/%s", si->base_url, folder);
		new = imap_url_upgrade (imap_sources, url);
		g_free (url);
		
		if (new) {
			url = g_strdup (new + strlen (si->base_url) + 1);
			g_free (new);
			
			return url;
		}
	}
	
	return NULL;
}

static int
shortcuts_upgrade_xml_file (GHashTable *accounts, GHashTable *imap_sources, const char *filename)
{
	unsigned char *buffer, *inptr, *start, *folder, *new, *account = NULL;
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
		g_free (account);
		inptr = strstr (inptr, ">evolution:");
		if (inptr) {
			inptr += 11;
			account = inptr;
			while (*inptr && *inptr != '/')
				inptr++;
			
			account = g_strndup (account, inptr - account);
			inptr++;
			
			url_need_upgrade = GPOINTER_TO_INT (g_hash_table_lookup (accounts, account));
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
		
		folder = g_strndup (inptr, start - inptr);
		new = shortcuts_upgrade_uri (accounts, imap_sources, account, folder);
		g_free (account);
		account = NULL;
		g_free (folder);
		
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
			g_free (account);
			inptr = strstr (inptr, ">evolution:");
			if (inptr) {
				inptr += 11;
				account = inptr;
				while (*inptr && *inptr != '/')
					inptr++;
				
				account = g_strndup (account, inptr - account);
				inptr++;
				
				url_need_upgrade = GPOINTER_TO_INT (g_hash_table_lookup (accounts, account));
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
	
	fprintf (stdout, "\nSuccessfully upgraded %s\nPrevious settings saved in %s\n", filename, bak);
	
	g_free (bak);
	
	return 0;
	
 exception:
	
	fprintf (stderr, "\nFailed to save updated settings to %s: %s\n", filename, strerror (errno));
	
	close (fd);
	g_free (buffer);
	unlink (filename);
	rename (bak, filename);
	g_free (bak);
	
	return -1;
}

static int
mailer_upgrade (Bonobo_ConfigDatabase db)
{
	GHashTable *imap_sources, *accounts;
	char *path, *uri, *bak;
	char *account;
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
		int fd;
		
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
			si->dir_sep = '\0';
			si->junk = NULL;
			
			path = si->base_url + 7;
			
			path = g_strdup_printf ("%s/evolution/mail/imap/%s/storeinfo", getenv ("HOME"), path);
			if (stat (path, &st) != -1 && (fd = open (path, O_RDONLY)) != -1) {
				ssize_t nread = 0, n;
				
				si->junk = g_byte_array_new ();
				g_byte_array_set_size (si->junk, st.st_size);
				do {
					do {
						n = read (fd, si->junk->data + nread, st.st_size - nread);
					} while (n == -1 && errno == EINTR);
					
					if (n > 0)
						nread += n;
				} while (n != -1 && nread < st.st_size);
				
				close (fd);
			}
			g_free (path);
			
			si->dir_sep = find_dir_sep (si->junk);
			
			g_hash_table_insert (imap_sources, si->base_url, si);
			
			if (account)
				g_hash_table_insert (accounts, account, si);
		} else if (uri && !strncmp (uri, "exchange:", 9)) {
			path = g_strdup_printf ("/Mail/Accounts/account_name_%d", i);
			account = bonobo_config_get_string (db, path, NULL);
			g_free (path);
			
			if (account)
				g_hash_table_insert (accounts, account, GINT_TO_POINTER (1));
		}
		
		g_free (uri);
	}
	
	if (g_hash_table_size (imap_sources) == 0) {
		/* user doesn't have any imap accounts - nothing to upgrade */
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
	
	g_hash_table_foreach (imap_sources, si_free, NULL);
	g_hash_table_destroy (imap_sources);
	
	path = g_strdup_printf ("%s/evolution/mail/imap", getenv ("HOME"));
	bak = g_strdup_printf ("%s.bak-1.0", path);
	
	if (rename (path, bak) == -1)
		fprintf (stderr, "\nFailed to backup Evolution 1.0's IMAP cache: %s\n", strerror (errno));
	
	g_free (path);
	g_free (bak);
	
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
	
	if ((db = get_config_db ()) == CORBA_OBJECT_NIL)
		g_error ("Could not get config db");
	
	mailer_upgrade (db);
	
	gtk_main_quit ();
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
