/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-movemail.c: mbox copying function */

/*
 * Author:
 *  Dan Winship <danw@helixcode.com>
 *
 * Copyright 2000 Helix Code, Inc. (http://www.helixcode.com)
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

#include <config.h>

#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "camel-movemail.h"
#include "camel-exception.h"

/**
 * camel_movemail: Copy an mbox file from a shared spool directory to a
 * new folder in a Camel store
 * @source: source file
 * @dest: destination file
 * @ex: a CamelException
 *
 * This copies an mbox file from a shared directory with multiple
 * readers and writers into a private (presumably Camel-controlled)
 * directory. Dot locking is used on the source file (but not the
 * destination).
 *
 * Return value: 1 if mail was copied, 0 if the source file contained
 * no mail, -1 if an error occurred.
 **/
int
camel_movemail (const char *source, const char *dest, CamelException *ex)
{
	gboolean locked, error;
	int sfd, dfd, tmpfd;
	char *locktmpfile, *lockfile;
	struct stat st;
	time_t now, timeout;
	int nread, nwrote;
	char buf[BUFSIZ];

	camel_exception_clear (ex);

	/* Stat and then open the spool file. If it doesn't exist or
	 * is empty, the user has no mail. (There's technically a race
	 * condition here in that an MDA might have just now locked it
	 * to deliver a message, but we don't care. In that case,
	 * assuming it's unlocked is equivalent to pretending we were
	 * called a fraction earlier.)
	 */
	if (stat (source, &st) == -1) {
		if (errno == ENOENT)
			return 0;

		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "Could not check mail file %s: %s",
				      source, g_strerror (errno));
		return -1;
	}
	if (st.st_size == 0)
		return 0;

	sfd = open (source, O_RDWR);
	if (sfd == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "Could not open mail file %s: %s",
				      source, g_strerror (errno));
		return -1;
	}

	dfd = open (dest, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
	if (dfd == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "Could not open temporary mail "
				      "file %s: %s", dest, g_strerror (errno));
		close (sfd);
		return -1;
	}

	/* Create the unique lock file. */
	locktmpfile = g_strdup_printf ("%s.lock.XXXXXX", source);
#ifdef HAVE_MKSTEMP
	tmpfd = mkstemp (locktmpfile);
#else
	if (mktemp (locktmpfile)) {
		tmpfd = open (locktmpfile, O_RDWR | O_CREAT | O_EXCL,
			      S_IRUSR | S_IWUSR);
	} else
		tmpfd = -1;
#endif
	if (tmpfd == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "Could not create lock file "
				      "for %s: %s", source, g_strerror (errno));
		close (sfd);
		close (dfd);
		unlink (dest);
		return -1;
	}
	close (tmpfd);

	lockfile = g_strdup_printf ("%s.lock", source);
	locked = FALSE;
	time (&timeout);
	timeout += 30;

	/* Loop trying to lock the file for 30 seconds. */
	while (time (&now) < timeout) {
		/* Try to make the lock. */
		if (symlink (locktmpfile, lockfile) == 0) {
			locked = TRUE;
			break;
		}

		/* If we fail for a reason other than that someone
		 * else has the lock, then abort.
		 */
		if (errno != EEXIST) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      "Could not create lock "
					      "file for %s: %s", source,
					      g_strerror (errno));
			break;
		}

		/* Check the modtime on the lock file. */
		if (stat (lockfile, &st) == -1) {
			/* If the lockfile disappeared, try again. */
			if (errno == ENOENT)
				continue;

			/* Some other error. Abort. */
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      "Could not test lock "
					      "file for %s: %s", source,
					      g_strerror (errno));
			break;
		}

		/* If the lock file is stale, remove it and try again. */
		if (st.st_mtime < now - 60) {
			unlink (lockfile);
			continue;
		}

		/* Otherwise, sleep and try again. */
		sleep (5);
	}

	if (!locked) {
		/* Something has gone awry. */
		if (now >= timeout) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      "Timed out trying to get "
					      "lock file on %s. Try again "
					      "later.", source);
		}
		g_free (lockfile);
		unlink (locktmpfile);
		g_free (locktmpfile);
		close (sfd);
		close (dfd);
		unlink (dest);
		return -1;
	}

	/* OK. We have the file locked now. */

	/* FIXME: Set a timer to keep the file locked. */

	error = FALSE;
	while (1) {
		int written = 0;

		nread = read (sfd, buf, sizeof (buf));
		if (nread == 0)
			break;
		else if (nread == -1) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      "Error reading mail file: %s",
					      g_strerror (errno));
			error = TRUE;
			break;
		}

		while (nread) {
			nwrote = write (dfd, buf + written, nread);
			if (nwrote == -1) {
				camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
						      "Error writing "
						      "mail temp file: %s",
						      g_strerror (errno));
				error = TRUE;
				break;
			}
			written += nwrote;
			nread -= nwrote;
		}
	}

	/* If no errors occurred copying the data, and we successfully
	 * close the destination file, then truncate the source file.
	 * If there is some sort of error, delete the destination file.
	 */
	if (!error) {
		if (close (dfd) == 0)
			ftruncate (sfd, 0);
		else {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      "Failed to store mail in "
					      "temp file %s: %s", dest,
					      g_strerror (errno));
			unlink (dest);
			error = TRUE;
		}
	} else {
		close (dfd);
		unlink (dest);
	}
	close (sfd);

	/* Clean up lock files. */
	unlink (lockfile);
	g_free (lockfile);
	unlink (locktmpfile);
	g_free (locktmpfile);

	return error ? -1 : 1;
}
