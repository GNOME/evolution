/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2001-2003 Ximian Inc.
 *
 * Authors: Michael Zucchi <notzed@ximian.com>
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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "camel-exception.h"
#include "camel-lock-helper.h"
#include "camel-lock-client.h"

#define d(x)

/* dunno where this fucking thing is got from */
/* see also camel-lock.c */
#define _(x) (x)

static pthread_mutex_t lock_lock = PTHREAD_MUTEX_INITIALIZER;
#define LOCK() pthread_mutex_lock(&lock_lock)
#define UNLOCK() pthread_mutex_unlock(&lock_lock)

static int lock_sequence;
static int lock_helper_pid = -1;
static int lock_stdin_pipe[2], lock_stdout_pipe[2];

static int read_n(int fd, void *buffer, int inlen)
{
	char *p = buffer;
	int len, left = inlen;

	do {
		len = read(fd, p, left);
		if (len == -1) {
			if (errno != EINTR)
				return -1;
		} else {
			left -= len;
			p += len;
		}
	} while (left > 0 && len != 0);

	return inlen - left;
}

static int write_n(int fd, void *buffer, int inlen)
{
	char *p = buffer;
	int len, left = inlen;

	do {
		len = write(fd, p, left);
		if (len == -1) {
			if (errno != EINTR)
				return -1;
		} else {
			left -= len;
			p += len;
		}
	} while (left > 0);

	return inlen;
}

static int camel_lock_helper_init(CamelException *ex)
{
	int i;

	if (pipe(lock_stdin_pipe) == -1
	    || pipe(lock_stdout_pipe) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot build locking helper pipe: %s"),
				      g_strerror (errno));
		return -1;
	}

	lock_helper_pid = fork();
	switch(lock_helper_pid) {
	case -1:
		close(lock_stdin_pipe[0]);
		close(lock_stdin_pipe[1]);
		close(lock_stdout_pipe[0]);
		close(lock_stdout_pipe[1]);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot fork locking helper: %s"),
				      g_strerror (errno));
		return -1;
	case 0:
		close(STDIN_FILENO);
		dup(lock_stdin_pipe[0]);
		close(STDOUT_FILENO);
		dup(lock_stdout_pipe[1]);
		close(lock_stdin_pipe[0]);
		close(lock_stdin_pipe[1]);
		close(lock_stdout_pipe[0]);
		close(lock_stdout_pipe[1]);
		for (i=3;i<255;i++)
			     close(i);
		execl(CAMEL_LIBEXECDIR "/camel-lock-helper", "camel-lock-helper", NULL);
		d(fprintf(stderr, "shit, couldn't exec lock helper!\n"));
		/* it'll pick this up when it tries to use us */
		exit(255);
	default:
		close(lock_stdin_pipe[0]);
		close(lock_stdout_pipe[1]);

		/* so the child knows when we vanish */
		fcntl(lock_stdin_pipe[1], F_SETFD, FD_CLOEXEC);
		fcntl(lock_stdout_pipe[0], F_SETFD, FD_CLOEXEC);
	}

	return 0;
}

int camel_lock_helper_lock(const char *path, CamelException *ex)
{
	struct _CamelLockHelperMsg *msg;
	int len = strlen(path);
	int res = -1;
	int retry = 3;

	LOCK();

	if (lock_helper_pid == -1) {
		if (camel_lock_helper_init(ex) == -1) {
			UNLOCK();
			return -1;
		}
	}

	msg = alloca(len + sizeof(*msg));
again:
	msg->magic = CAMEL_LOCK_HELPER_MAGIC;
	msg->seq = lock_sequence;
	msg->id = CAMEL_LOCK_HELPER_LOCK;
	msg->data = len;
	memcpy(msg+1, path, len);

	write_n(lock_stdin_pipe[1], msg, len+sizeof(*msg));

	do {
		/* should also have a timeout here?  cancellation? */
		len = read_n(lock_stdout_pipe[0], msg, sizeof(*msg));
		if (len == 0) {
			/* child quit, do we try ressurect it? */
			res = CAMEL_LOCK_HELPER_STATUS_PROTOCOL;
			/* if the child exited, this should get it, waidpid returns 0 if the child hasn't */
			if (waitpid(lock_helper_pid, NULL, WNOHANG) > 0) {
				lock_helper_pid = -1;
				close(lock_stdout_pipe[0]);
				close(lock_stdin_pipe[1]);
				lock_stdout_pipe[0] = -1;
				lock_stdin_pipe[1] = -1;
			}
			goto fail;
		}

		if (msg->magic != CAMEL_LOCK_HELPER_RETURN_MAGIC
		    || msg->seq > lock_sequence) {
			res = CAMEL_LOCK_HELPER_STATUS_PROTOCOL;
			d(printf("lock child protocol error\n"));
			camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Could not lock '%s': protocol error with lock-helper"), path);
			goto fail;
		}
	} while (msg->seq < lock_sequence);

	if (msg->seq == lock_sequence) {
		switch(msg->id) {
		case CAMEL_LOCK_HELPER_STATUS_OK:
			d(printf("lock child locked ok, id is %d\n", msg->data));
			res = msg->data;
			break;
		default:
			camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Could not lock '%s'"), path);
			d(printf("locking failed ! status = %d\n", msg->id));
			break;
		}
	} else if (retry > 0) {
		d(printf("sequence failure, lost message? retry?\n"));
		retry--;
		goto again;
	} else {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Could not lock '%s': protocol error with lock-helper"), path);
	}

fail:
	lock_sequence++;

	UNLOCK();

	return res;
}

int camel_lock_helper_unlock(int lockid)
{
	struct _CamelLockHelperMsg *msg;
	int res = -1;
	int retry = 3;
	int len;

	d(printf("unlocking lock id %d\n", lockid));

	LOCK();

	/* impossible to unlock if we haven't locked yet */
	if (lock_helper_pid == -1) {
		UNLOCK();
		return -1;
	}

	msg = alloca(sizeof(*msg));
again:
	msg->magic = CAMEL_LOCK_HELPER_MAGIC;
	msg->seq = lock_sequence;
	msg->id = CAMEL_LOCK_HELPER_UNLOCK;
	msg->data = lockid;

	write_n(lock_stdin_pipe[1], msg, sizeof(*msg));

	do {
		/* should also have a timeout here?  cancellation? */
		len = read_n(lock_stdout_pipe[0], msg, sizeof(*msg));
		if (len == 0) {
			/* child quit, do we try ressurect it? */
			res = CAMEL_LOCK_HELPER_STATUS_PROTOCOL;
			if (waitpid(lock_helper_pid, NULL, WNOHANG) > 0) {
				lock_helper_pid = -1;
				close(lock_stdout_pipe[0]);
				close(lock_stdin_pipe[1]);
				lock_stdout_pipe[0] = -1;
				lock_stdin_pipe[1] = -1;
			}
			goto fail;
		}

		if (msg->magic != CAMEL_LOCK_HELPER_RETURN_MAGIC
		    || msg->seq > lock_sequence) {
			goto fail;
		}
	} while (msg->seq < lock_sequence);

	if (msg->seq == lock_sequence) {
		switch(msg->id) {
		case CAMEL_LOCK_HELPER_STATUS_OK:
			d(printf("lock child unlocked ok\n"));
			res = 0;
			break;
		default:
			d(printf("locking failed ! \n"));
			break;
		}
	} else if (retry > 0) {
		d(printf("sequence failure, lost message? retry?\n"));
		lock_sequence++;
		retry--;
		goto again;
	}

fail:
	lock_sequence++;

	UNLOCK();

	return res;
}

#if 0
int main(int argc, char **argv)
{
	int id1, id2;

	d(printf("locking started\n"));
	camel_lock_helper_init();

	id1 = camel_lock_helper_lock("1 path 1");
	if (id1 != -1) {
		d(printf("lock ok, unlock\n"));
		camel_lock_helper_unlock(id1);
	}

	id1 = camel_lock_helper_lock("2 path 1");
	id2 = camel_lock_helper_lock("2 path 2");
	camel_lock_helper_unlock(id2);
	camel_lock_helper_unlock(id1);

	id1 = camel_lock_helper_lock("3 path 1");
	id2 = camel_lock_helper_lock("3 path 2");
	camel_lock_helper_unlock(id1);
}
#endif
