/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*- */
/* camel-stream-process.c : stream over piped process */

/*
 *  Copyright (C) 2003 Ximian Inc.
 *
 *  Authors: David Woodhouse <dwmw2@infradead.org>,
 *	     Jeffrey Stedfast <fejj@ximian.com>
 *  
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "camel-stream-process.h"
#include "camel-file-utils.h"

extern int camel_verbose_debug;

static CamelObjectClass *parent_class = NULL;

/* Returns the class for a CamelStream */
#define CS_CLASS(so) CAMEL_STREAM_PROCESS_CLASS(CAMEL_OBJECT_GET_CLASS(so))

/* dummy implementations, for a PROCESS stream */
static ssize_t   stream_read       (CamelStream *stream, char *buffer, size_t n);
static ssize_t   stream_write      (CamelStream *stream, const char *buffer, size_t n);
static int       stream_close      (CamelStream *stream);
static int       stream_flush      (CamelStream *stream);

static void
camel_stream_process_finalise (CamelObject *object)
{
	/* Ensure we clean up after ourselves -- kill
	   the child process and reap it. */
	stream_close(CAMEL_STREAM (object));
}

static void
camel_stream_process_class_init (CamelStreamProcessClass *camel_stream_process_class)
{
	CamelStreamClass *camel_stream_class = (CamelStreamClass *)camel_stream_process_class;

	parent_class = camel_type_get_global_classfuncs( CAMEL_OBJECT_TYPE );

	/* virtual method definition */
	camel_stream_class->read = stream_read;
	camel_stream_class->write = stream_write;
	camel_stream_class->close = stream_close;
	camel_stream_class->flush = stream_flush;
}

static void
camel_stream_process_init (gpointer object, gpointer klass)
{
        CamelStreamProcess *stream = CAMEL_STREAM_PROCESS (object);
         
        stream->sockfd = -1;
	stream->childpid = 0;
}


CamelType
camel_stream_process_get_type (void)
{
	static CamelType camel_stream_process_type = CAMEL_INVALID_TYPE;

	if (camel_stream_process_type == CAMEL_INVALID_TYPE) {
		camel_stream_process_type = 
			camel_type_register( camel_stream_get_type(),
					     "CamelStreamProcess",
					     sizeof( CamelStreamProcess ),
					     sizeof( CamelStreamProcessClass ),
					     (CamelObjectClassInitFunc) camel_stream_process_class_init,
					     NULL,
					     (CamelObjectInitFunc) camel_stream_process_init,
					     (CamelObjectFinalizeFunc) camel_stream_process_finalise);
	}

	return camel_stream_process_type;
}

/**
 * camel_stream_process_new:
 *
 * Returns a PROCESS stream.
 *
 * Return value: the stream
 **/
CamelStream *
camel_stream_process_new(void)
{
	return (CamelStream *)camel_object_new(camel_stream_process_get_type ());
}


static ssize_t
stream_read (CamelStream *stream, char *buffer, size_t n)
{
	CamelStreamProcess *stream_process = CAMEL_STREAM_PROCESS (stream);

	return camel_read(stream_process->sockfd, buffer, n);
}

static ssize_t
stream_write (CamelStream *stream, const char *buffer, size_t n)
{
	CamelStreamProcess *stream_process = CAMEL_STREAM_PROCESS (stream);

	return camel_write(stream_process->sockfd, buffer, n);
}

static int
stream_flush (CamelStream *stream)
{
	return 0;
}

static int
stream_close (CamelStream *object)
{
	CamelStreamProcess *stream = CAMEL_STREAM_PROCESS (object);
	if (camel_verbose_debug)
		fprintf(stderr, "Process stream close. sockfd %d, childpid %d\n",
			stream->sockfd, stream->childpid);

	if (stream->sockfd != -1) {
		close(stream->sockfd);
		stream->sockfd = -1;
	}
	if (stream->childpid) {
		int ret, i;
		for (i=0; i<4; i++) {
			ret = waitpid(stream->childpid, NULL, WNOHANG);
			if (camel_verbose_debug)
				fprintf(stderr, "waitpid() for pid %d returned %d (errno %d)\n",
					stream->childpid, ret, ret==-1?errno:0);
			if (ret == stream->childpid || errno == ECHILD)
				break;
			switch(i) {
			case 0:
				if (camel_verbose_debug)
					fprintf(stderr, "Sending SIGTERM to pid %d\n",
						stream->childpid);
				kill(stream->childpid, SIGTERM);
				break;
			case 2:
				if (camel_verbose_debug)
					fprintf(stderr, "Sending SIGKILL to pid %d\n",
						stream->childpid);
				kill(stream->childpid, SIGKILL);
				break;
			case 1:
			case 3:
				sleep(1);
				break;
			}
		}
		stream->childpid = 0;
	}
	return 0;
}

static void do_exec_command(int fd, const char *command, char **env)
{
	int i, maxopen;

	/* Not a lot we can do if there's an error other than bail. */
	if (dup2(fd, 0) == -1)
		exit(1);
	if (dup2(fd, 1) == -1)
		exit(1);
	
	/* What to do with stderr? Possibly put it through a separate pipe
	   and bring up a dialog box with its output if anything does get
	   spewed to it? It'd help the user understand what was going wrong
	   with their command, but it's hard to do cleanly. For now we just
	   leave it as it is. Perhaps we should close it and reopen /dev/null? */

	maxopen = sysconf(_SC_OPEN_MAX);
	for (i=3; i < maxopen; i++)
		close(i);

	setsid();
#ifdef TIOCNOTTY
	/* Detach from the controlling tty if we have one. Otherwise, 
	   SSH might do something stupid like trying to use it instead 
	   of running $SSH_ASKPASS. Doh. */
	fd = open("/dev/tty", O_RDONLY);
	if (fd != -1) {
		ioctl(fd, TIOCNOTTY, NULL);
		close(fd);
	}
#endif /* TIOCNOTTY */

	/* Set up child's environment. We _add_ to it, don't use execle, 
	   because otherwise we'd destroy stuff like SSH_AUTH_SOCK etc. */
	for (; env && *env; env++) {
		char *eq = strchr(*env, '=');
		if (!eq) {
			unsetenv(*env);
			continue;
		}
		*eq = 0;
		eq++;
		setenv(*env, eq, 1);
	}

	execl("/bin/sh", "/bin/sh", "-c", command, NULL);

	if (camel_verbose_debug)
		fprintf(stderr, "exec failed %d\n", errno);
	exit(1);
}

int
camel_stream_process_connect(CamelStreamProcess *stream, const char *command, const char **env)
{
	int sockfds[2];

	if (stream->sockfd != -1 || stream->childpid) {
		stream_close(CAMEL_STREAM (stream));
	}
	
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockfds))
		return -1;

	stream->childpid = fork();
	if (!stream->childpid) {
		do_exec_command(sockfds[1], command, (char **)env);
	} else if (stream->childpid == -1) {
		close(sockfds[0]);
		close(sockfds[1]);
		stream->sockfd = -1;
		return -1;
	}

	close(sockfds[1]);
	stream->sockfd = sockfds[0];

	return 0;
}
