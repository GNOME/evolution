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

/* define to put temporary files in ~/.cache/evolution/tmp/ */
/* Keep this for Flatpak builds, until glib allows access to in-sandbox /tmp
   See https://gitlab.gnome.org/GNOME/glib/-/issues/2235 for more information */
/* #define TEMP_HOME */

/* how old things need to be to expire */
#define TEMP_EXPIRE (60*60*2)
/* don't scan more often than this */
#define TEMP_SCAN (60)

#ifdef TEMP_HOME

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

#endif /* TEMP_HOME */

static GString *
get_dir (const gchar *tmpl)
{
	GString *path;
	gchar *tmpdir;
#ifdef TEMP_HOME
	time_t now = time (NULL);
	static time_t last = 0;
	const gchar *user_cache_dir;
#else
	GError *error = NULL;
#endif

	if (!tmpl || !*tmpl)
		tmpl = "evolution-XXXXXX";

#ifdef TEMP_HOME
	user_cache_dir = e_get_user_cache_dir ();
	tmpdir = g_build_filename (user_cache_dir, "tmp", NULL);
	path = g_string_new (tmpdir);
	if (g_mkdir_with_parents (tmpdir, 0777) == -1) {
		g_string_free (path, TRUE);
		path = NULL;
	}

	/* fire off an expiry attempt no more often than TEMP_SCAN seconds */
	if (path && (last + TEMP_SCAN) < now) {
		last = now;
		expire_dir_rec (path->str, now);
	}
#else
	tmpdir = g_dir_make_tmp (tmpl, &error);
	if (!tmpdir) {
		g_debug ("Failed to create tmp directory: %s", error ? error->message : "Unknown error");
		g_clear_error (&error);

		return NULL;
	}

	path = g_string_new (tmpdir);
#endif
	g_free (tmpdir);

	d (printf ("temp dir '%s'\n", path ? path->str : "(null)"));

	return path;
}

static gint
e_mkstemp_impl (const gchar *template,
		gchar **out_path)
{
	GString *path;
	gint fd;

	path = get_dir (NULL);
	if (!path)
		return -1;

	g_string_append_c (path, G_DIR_SEPARATOR);
	g_string_append (path, template && *template ? template : "unknown-XXXXXX");

	fd = g_mkstemp (path->str);

	if (out_path)
		*out_path = g_string_free (path, fd == -1);
	else
		g_string_free (path, TRUE);

	return fd;
}

gchar *
e_mktemp (const gchar *template)
{
	gchar *path = NULL;
	gint fd;

	fd = e_mkstemp_impl (template, &path);

	if (fd != -1) {
		close (fd);
		g_unlink (path);
	}

	return path;
}

gint
e_mkstemp (const gchar *template)
{
	return e_mkstemp_impl (template, NULL);
}

gchar *
e_mkdtemp (const gchar *template)
{
	GString *path;

	path = get_dir (template);
	if (!path)
		return NULL;

	return g_string_free (path, FALSE);
}
