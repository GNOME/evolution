/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#ifdef ENABLE_THREADS
#include <pthread.h>
#endif

#include "e-mktemp.h"


static gboolean initialized = FALSE;
static GSList *temp_files = NULL;
static GSList *temp_dirs = NULL;
#ifdef ENABLE_THREADS
static pthread_mutex_t temp_files_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t temp_dirs_lock = PTHREAD_MUTEX_INITIALIZER;
#define TEMP_FILES_LOCK() pthread_mutex_lock (&temp_files_lock)
#define TEMP_FILES_UNLOCK() pthread_mutex_unlock (&temp_files_lock)
#define TEMP_DIRS_LOCK() pthread_mutex_lock (&temp_dirs_lock)
#define TEMP_DIRS_UNLOCK() pthread_mutex_unlock (&temp_dirs_lock)
#else
#define TEMP_FILES_LOCK()
#define TEMP_FILES_UNLOCK()
#define TEMP_DIRS_LOCK()
#define TEMP_DIRS_UNLOCK()
#endif /* ENABLE_THREADS */


static GString *
get_path (gboolean make)
{
	GString *path;
	
	path = g_string_new ("/tmp/evolution-");
	g_string_append_printf (path, "%d-%d", (int) getuid (), (int) getpid ());
	
	if (make) {
		int ret;
		
		/* shoot now, ask questions later */
		ret = mkdir (path->str, S_IRWXU);
		if (ret == -1) {
			if (errno == EEXIST) {
				struct stat st;
				
				if (stat (path->str, &st) == -1) {
					/* reset errno */
					errno = EEXIST;
					g_string_free (path, TRUE);
					return NULL;
				}
				
				/* make sure this is a directory and belongs to us... */
				if (!S_ISDIR (st.st_mode) || st.st_uid != getuid ()) {
					/* eek! this is bad... */
					g_string_free (path, TRUE);
					return NULL;
				}
			} else {
				/* some other error...do not pass go, do not collect $200 */
				g_string_free (path, TRUE);
				return NULL;
			}
		}
	}
	
	return path;
}

static void
e_mktemp_cleanup (void)
{
	GString *path;
	GSList *node;
	
	TEMP_FILES_LOCK ();
	if (temp_files) {
		node = temp_files;
		while (node) {
			unlink (node->data);
			g_free (node->data);
			node = node->next;
		}
		g_slist_free (temp_files);
		temp_files = NULL;
	}
	TEMP_FILES_UNLOCK ();
	
	TEMP_DIRS_LOCK ();
	if (temp_dirs) {
		node = temp_dirs;
		while (node) {
			/* perform the equivalent of a rm -rf */
			struct dirent *dent;
			DIR *dir;
			
			/* first empty out this directory of it's files... */
			dir = opendir (node->data);
			if (dir) {
				while ((dent = readdir (dir)) != NULL) {
					char *full_path;
					
					if (!strcmp (dent->d_name, ".") || !strcmp (dent->d_name, ".."))
						continue;
					
					full_path = g_strdup_printf ("%s/%s", node->data, dent->d_name);
					unlink (full_path);
					g_free (full_path);
				}
				closedir (dir);
			}
			
			/* ...then rmdir the directory */
			rmdir (node->data);
			g_free (node->data);
			node = node->next;
		}
		g_slist_free (temp_dirs);
		temp_dirs = NULL;
	}
	TEMP_DIRS_UNLOCK ();
	
	path = get_path (FALSE);
	rmdir (path->str);
	
	g_string_free (path, TRUE);
}


const char *
e_mktemp (const char *template)
{
	GString *path;
	char *ret;
	
	path = get_path (TRUE);
	if (!path)
		return NULL;
	
	g_string_append_c (path, '/');
	if (template)
		g_string_append (path, template);
	else
		g_string_append (path, "unknown-XXXXXX");
	
	ret = mktemp (path->str);
	if (ret) {
		TEMP_FILES_LOCK ();
		if (!initialized) {
			g_atexit (e_mktemp_cleanup);
			initialized = TRUE;
		}
		temp_files = g_slist_prepend (temp_files, ret);
		g_string_free (path, FALSE);
		TEMP_FILES_UNLOCK ();
	} else {
		g_string_free (path, TRUE);
	}
	
	return ret;
}


int
e_mkstemp (const char *template)
{
	GString *path;
	int fd;
	
	path = get_path (TRUE);
	if (!path)
		return -1;
	
	g_string_append_c (path, '/');
	if (template)
		g_string_append (path, template);
	else
		g_string_append (path, "unknown-XXXXXX");
	
	fd = mkstemp (path->str);
	if (fd != -1) {
		TEMP_FILES_LOCK ();
		if (!initialized) {
			g_atexit (e_mktemp_cleanup);
			initialized = TRUE;
		}
		temp_files = g_slist_prepend (temp_files, path->str);
		g_string_free (path, FALSE);
		TEMP_FILES_UNLOCK ();
	} else {
		g_string_free (path, TRUE);
	}
	
	return fd;
}


const char *
e_mkdtemp (const char *template)
{
	GString *path;
	char *tmpdir;
	
	path = get_path (TRUE);
	if (!path)
		return NULL;
	
	g_string_append_c (path, '/');
	if (template)
		g_string_append (path, template);
	else
		g_string_append (path, "unknown-XXXXXX");
	
#ifdef HAVE_MKDTEMP
	tmpdir = mkdtemp (path->str);
#else
	tmpdir = mktemp (path->str);
	if (tmpdir) {
		if (mkdir (tmpdir, S_IRWXU) == -1)
			tmpdir = NULL;
	}
#endif
	
	if (tmpdir) {
		TEMP_DIRS_LOCK ();
		if (!initialized) {
			g_atexit (e_mktemp_cleanup);
			initialized = TRUE;
		}
		temp_dirs = g_slist_prepend (temp_dirs, tmpdir);
		g_string_free (path, FALSE);
		TEMP_DIRS_UNLOCK ();
	} else {
		g_string_free (path, TRUE);
	}
	
	return tmpdir;
}
