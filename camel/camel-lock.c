/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Author: Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999 Ximian (www.ximian.com/).
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif
#include <time.h>

#ifdef USE_DOT
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

#ifdef USE_FCNTL
#include <unistd.h>
#include <fcntl.h>
#endif

#ifdef USE_FLOCK
#include <sys/file.h>
#endif

#include <glib.h>

#include "camel-lock.h"
#include "camel-i18n.h"

#define d(x) /*(printf("%s(%d): ", __FILE__, __LINE__),(x))*/

/**
 * camel_lock_dot:
 * @path: 
 * @ex: 
 * 
 * Create an exclusive lock using .lock semantics.
 * All locks are equivalent to write locks (exclusive).
 * 
 * Return value: -1 on error, sets @ex appropriately.
 **/
int
camel_lock_dot(const char *path, CamelException *ex)
{
#ifdef USE_DOT
	char *locktmp, *lock;
	int retry = 0;
	int fdtmp;
	struct stat st;

	/* TODO: Is there a reliable way to refresh the lock, if we're still busy with it?
	   Does it matter?  We will normally also use fcntl too ... */

	/* use alloca, save cleaning up afterwards */
	lock = alloca(strlen(path) + strlen(".lock") + 1);
	sprintf(lock, "%s.lock", path);
	locktmp = alloca(strlen(path) + strlen("XXXXXX") + 1);

#ifndef HAVE_MKSTEMP
	sprintf(locktmp, "%sXXXXXX", path);
	if (mktemp(locktmp) == NULL) {
		/* well, this is really only a programatic error */
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not create lock file for %s: %s"),
				      path, g_strerror (errno));
		return -1;
	}
#endif

	while (retry < CAMEL_LOCK_DOT_RETRY) {

		d(printf("trying to lock '%s', attempt %d\n", lock, retry));

		if (retry > 0)
			sleep(CAMEL_LOCK_DOT_DELAY);

#ifdef HAVE_MKSTEMP
		sprintf(locktmp, "%sXXXXXX", path);
		fdtmp = mkstemp(locktmp);
#else
		fdtmp = open(locktmp, O_RDWR|O_CREAT|O_EXCL, 0600);
#endif
		if (fdtmp == -1) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Could not create lock file for %s: %s"),
					      path, g_strerror (errno));
			return -1;
		}
		close(fdtmp);

		/* apparently return code from link can be unreliable for nfs (see link(2)), so we ignore it */
		link(locktmp, lock);

		/* but we check stat instead (again, see link(2)) */
		if (stat(locktmp, &st) == -1) {
			d(printf("Our lock file %s vanished!?\n", locktmp));

			/* well that was unexpected, try cleanup/retry */
			unlink(locktmp);
			unlink(lock);
		} else {
			d(printf("tmp lock created, link count is %d\n", st.st_nlink));

			unlink(locktmp);

			/* if we had 2 links, we have created the .lock, return ok, otherwise we need to keep trying */
			if (st.st_nlink == 2)
				return 0;
		}

		/* check for stale lock, kill it */
		if (stat(lock, &st) == 0) {
			time_t now = time(0);
			(printf("There is an existing lock %ld seconds old\n", now-st.st_ctime));
			if (st.st_ctime < now - CAMEL_LOCK_DOT_STALE) {
				d(printf("Removing it now\n"));
				unlink(lock);
			}
		}

		retry++;
	}

	d(printf("failed to get lock after %d retries\n", retry));

	camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM, _("Timed out trying to get lock file on %s. Try again later."), path);
	return -1;
#else /* ! USE_DOT */
	return 0;
#endif
}

/**
 * camel_unlock_dot:
 * @path: 
 * 
 * Attempt to unlock a .lock lock.
 **/
void
camel_unlock_dot(const char *path)
{
#ifdef USE_DOT
	char *lock;

	lock = alloca(strlen(path) + strlen(".lock") + 1);
	sprintf(lock, "%s.lock", path);
	d(printf("unlocking %s\n", lock));
	(void)unlink(lock);
#endif
}

/**
 * camel_lock_fcntl:
 * @fd: 
 * @type: 
 * @ex: 
 * 
 * Create a lock using fcntl(2).
 *
 * @type is CAMEL_LOCK_WRITE or CAMEL_LOCK_READ,
 * to create exclusive or shared read locks
 * 
 * Return value: -1 on error.
 **/
int
camel_lock_fcntl(int fd, CamelLockType type, CamelException *ex)
{
#ifdef USE_FCNTL
	struct flock lock;

	d(printf("fcntl locking %d\n", fd));

	memset(&lock, 0, sizeof(lock));
	lock.l_type = type==CAMEL_LOCK_READ?F_RDLCK:F_WRLCK;
	if (fcntl(fd, F_SETLK, &lock) == -1) {
		/* If we get a 'locking not vailable' type error,
		   we assume the filesystem doesn't support fcntl() locking */
		/* this is somewhat system-dependent */
		if (errno != EINVAL && errno != ENOLCK) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Failed to get lock using fcntl(2): %s"),
					      g_strerror (errno));
			return -1;
		} else {
			static int failed = 0;

			if (failed == 0)
				fprintf(stderr, "fcntl(2) locking appears not to work on this filesystem");
			failed++;
		}
	}
#endif
	return 0;
}

/**
 * camel_unlock_fcntl:
 * @fd: 
 * 
 * Unlock an fcntl lock.
 **/
void
camel_unlock_fcntl(int fd)
{
#ifdef USE_FCNTL
	struct flock lock;

	d(printf("fcntl unlocking %d\n", fd));

	memset(&lock, 0, sizeof(lock));
	lock.l_type = F_UNLCK;
	fcntl(fd, F_SETLK, &lock);
#endif
}

/**
 * camel_lock_flock:
 * @fd: 
 * @type: 
 * @ex: 
 * 
 * Create a lock using flock(2).
 * 
 * @type is CAMEL_LOCK_WRITE or CAMEL_LOCK_READ,
 * to create exclusive or shared read locks
 *
 * Return value: -1 on error.
 **/
int
camel_lock_flock(int fd, CamelLockType type, CamelException *ex)
{
#ifdef USE_FLOCK
	int op;

	d(printf("flock locking %d\n", fd));

	if (type == CAMEL_LOCK_READ)
		op = LOCK_SH|LOCK_NB;
	else
		op = LOCK_EX|LOCK_NB;

	if (flock(fd, op) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Failed to get lock using flock(2): %s"),
				      g_strerror (errno));
		return -1;
	}
#endif
	return 0;
}

/**
 * camel_unlock_flock:
 * @fd: 
 * 
 * Unlock an flock lock.
 **/
void
camel_unlock_flock(int fd)
{
#ifdef USE_FLOCK
	d(printf("flock unlocking %d\n", fd));

	(void)flock(fd, LOCK_UN);
#endif
}

/**
 * camel_lock_folder:
 * @path: Path to the file to lock (used for .locking only).
 * @fd: Open file descriptor of the right type to lock.
 * @type: Type of lock, CAMEL_LOCK_READ or CAMEL_LOCK_WRITE.
 * @ex: 
 * 
 * Attempt to lock a folder, multiple attempts will be made using all
 * locking strategies available.
 * 
 * Return value: -1 on error, @ex will describe the locking system that failed.
 **/
int
camel_lock_folder(const char *path, int fd, CamelLockType type, CamelException *ex)
{
	int retry = 0;

	while (retry < CAMEL_LOCK_RETRY) {
		if (retry > 0)
			sleep(CAMEL_LOCK_DELAY);

		if (camel_lock_fcntl(fd, type, ex) == 0) {
			if (camel_lock_flock(fd, type, ex) == 0) {
				if (camel_lock_dot(path, ex) == 0)
					return 0;
				camel_unlock_flock(fd);
			}
			camel_unlock_fcntl(fd);
		}
		retry++;
	}

	return -1;
}

/**
 * camel_unlock_folder:
 * @path: Filename of folder.
 * @fd: Open descrptor on which locks were placed.
 * 
 * Free a lock on a folder.
 **/
void
camel_unlock_folder(const char *path, int fd)
{
	camel_unlock_dot(path);
	camel_unlock_flock(fd);
	camel_unlock_fcntl(fd);
}

#if 0
int main(int argc, char **argv)
{
	CamelException *ex;
	int fd1, fd2;

	ex = camel_exception_new();

#if 0
	if (camel_lock_dot("mylock", ex) == 0) {
		if (camel_lock_dot("mylock", ex) == 0) {
			printf("Got lock twice?\n");
		} else {
			printf("failed to get lock 2: %s\n", camel_exception_get_description(ex));
		}
		camel_unlock_dot("mylock");
	} else {
		printf("failed to get lock 1: %s\n", camel_exception_get_description(ex));
	}

	camel_exception_clear(ex);
#endif

	fd1 = open("mylock", O_RDWR);
	fd2 = open("mylock", O_RDWR);

	if (camel_lock_fcntl(fd1, CAMEL_LOCK_WRITE, ex) == 0) {
		printf("got fcntl write lock once\n");
		sleep(5);
		if (camel_lock_fcntl(fd2, CAMEL_LOCK_WRITE, ex) == 0) {
			printf("got fcntl write lock twice!\n");
		} else {
			printf("failed to get write lock: %s\n", camel_exception_get_description(ex));
		}

		camel_exception_clear(ex);

		if (camel_lock_fcntl(fd2, CAMEL_LOCK_READ, ex) == 0) {
			printf("got fcntl read lock as well?\n");
			camel_unlock_fcntl(fd2);
		} else {
			printf("failed to get read lock: %s\n", camel_exception_get_description(ex));
		}

		camel_exception_clear(ex);
		camel_unlock_fcntl(fd1);
	} else {
		printf("failed to get write lock at all: %s\n", camel_exception_get_description(ex));
	}

	if (camel_lock_fcntl(fd1, CAMEL_LOCK_READ, ex) == 0) {
		printf("got fcntl read lock once\n");
		sleep(5);
		if (camel_lock_fcntl(fd2, CAMEL_LOCK_WRITE, ex) == 0) {
			printf("got fcntl write lock too?!\n");
		} else {
			printf("failed to get write lock: %s\n", camel_exception_get_description(ex));
		}

		camel_exception_clear(ex);

		if (camel_lock_fcntl(fd2, CAMEL_LOCK_READ, ex) == 0) {
			printf("got fcntl read lock twice\n");
			camel_unlock_fcntl(fd2);
		} else {
			printf("failed to get read lock: %s\n", camel_exception_get_description(ex));
		}

		camel_exception_clear(ex);
		camel_unlock_fcntl(fd1);
	}

	close(fd1);
	close(fd2);

	return 0;
}
#endif
