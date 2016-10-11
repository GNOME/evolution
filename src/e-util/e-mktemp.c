/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <glib/gstdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>

#include <libedataserver/libedataserver.h>

#include "e-mktemp.h"

#define d(x)

/* define to put temporary files in ~/evolution/cache/tmp */
#define TEMP_HOME (1)

/* how old things need to be to expire */
#define TEMP_EXPIRE (60*60*2)
/* dont scan more often than this */
#define TEMP_SCAN (60)

static gint
expire_dir_rec (const gchar *base,
                time_t now)
{
	GDir *dir;
	const gchar *d;
	GString *path;
	gsize len;
	struct stat st;
	gint count = 0;

	d (printf ("expire dir '%s'\n", base));

	dir = g_dir_open (base, 0, NULL);
	if (dir == NULL)
		return 0;

	path = g_string_new (base);
	len = path->len;

	while ((d = g_dir_read_name (dir))) {
		g_string_truncate (path, len);
		g_string_append_printf (path, "/%s", d);
		d (printf ("Checking '%s' for expiry\n", path->str));

		if (g_stat (path->str, &st) == 0
		    && st.st_atime + TEMP_EXPIRE < now) {
			if (S_ISDIR (st.st_mode)) {
				if (expire_dir_rec (path->str, now) == 0) {
					d (printf ("Removing dir '%s'\n", path->str));
					g_rmdir (path->str);
				} else {
					count++;
				}
			} else if (g_unlink (path->str) == -1) {
				d (printf ("expiry failed: %s\n", g_strerror (errno)));
				count++;
			} else {
				d (printf ("expired %s\n", path->str));
			}
		} else {
			count++;
		}
	}
	g_string_free (path, TRUE);
	g_dir_close (dir);

	d (printf ("expire dir '%s' %d remaining files\n", base, count));

	return count;
}

static GString *
get_dir (gboolean make)
{
	GString *path;
	time_t now = time (NULL);
	static time_t last = 0;

#ifdef TEMP_HOME
	const gchar *user_cache_dir;
	gchar *tmpdir;

	user_cache_dir = e_get_user_cache_dir ();
	tmpdir = g_build_filename (user_cache_dir, "tmp", NULL);
	path = g_string_new (tmpdir);
	if (make && g_mkdir_with_parents (tmpdir, 0777) == -1) {
		g_string_free (path, TRUE);
		path = NULL;
	}
	g_free (tmpdir);
#else
	path = g_string_new ("/tmp/evolution-");
	g_string_append_printf (path, "%d", (gint) getuid ());
	if (make) {
		gint ret;

		/* shoot now, ask questions later */
		ret = g_mkdir (path->str, S_IRWXU);
		if (ret == -1) {
			if (errno == EEXIST) {
				struct stat st;

				if (g_stat (path->str, &st) == -1) {
					/* reset errno */
					errno = EEXIST;
					g_string_free (path, TRUE);
					return NULL;
				}

				/* make sure this is a directory and
				 * belongs to us... */
				if (!S_ISDIR (st.st_mode) || st.st_uid != getuid ()) {
					/* eek! this is bad... */
					g_string_free (path, TRUE);
					return NULL;
				}
			} else {
				/* some other error...do not pass go,
				 * do not collect $200 */
				g_string_free (path, TRUE);
				return NULL;
			}
		}
	}
#endif

	d (printf ("temp dir '%s'\n", path ? path->str : "(null)"));

	/* fire off an expiry attempt no more often than TEMP_SCAN seconds */
	if (path && (last + TEMP_SCAN) < now) {
		last = now;
		expire_dir_rec (path->str, now);
	}

	return path;
}

gchar *
e_mktemp (const gchar *template)
{
	GString *path;
	gint fd;

	path = get_dir (TRUE);
	if (!path)
		return NULL;

	g_string_append_c (path, '/');
	if (template)
		g_string_append (path, template);
	else
		g_string_append (path, "unknown-XXXXXX");

	fd = g_mkstemp (path->str);

	if (fd != -1) {
		close (fd);
		g_unlink (path->str);
	}

	return g_string_free (path, fd == -1);
}

gint
e_mkstemp (const gchar *template)
{
	GString *path;
	gint fd;

	path = get_dir (TRUE);
	if (!path)
		return -1;

	g_string_append_c (path, '/');
	if (template)
		g_string_append (path, template);
	else
		g_string_append (path, "unknown-XXXXXX");

	fd = g_mkstemp (path->str);
	g_string_free (path, TRUE);

	return fd;
}

gchar *
e_mkdtemp (const gchar *template)
{
	GString *path;
	gchar *tmpdir;

	path = get_dir (TRUE);
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
	{
		gint fd = g_mkstemp (path->str);
		if (fd == -1) {
			tmpdir = NULL;
		} else {
			close (fd);
			tmpdir = path->str;
			g_unlink (tmpdir);
		}
	}

	if (tmpdir) {
		if (g_mkdir (tmpdir, S_IRWXU) == -1)
			tmpdir = NULL;
	}
#endif
	g_string_free (path, tmpdir == NULL);

	return tmpdir;
}
