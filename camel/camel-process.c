/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
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

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "camel-process.h"


pid_t
camel_process_fork (const char *path, char **argv, int *infd, int *outfd, int *errfd, CamelException *ex)
{
	int errnosav, fd[6], i;
	pid_t pid;
	
	for (i = 0; i < 6; i++)
		fds[i] = -1;
	
	for (i = 0; i < 6; i += 2) {
		if (pipe (fd + i) == -1) {
			errnosav = errno;
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Failed to create pipe to '%s': %s"),
					      argv[0], strerror (errno));
			
			for (i = 0; i < 6; i++) {
				if (fd[i] == -1)
					break;
				close (fd[i]);
			}
			
			errno = errnosav;
			
			return -1;
		}
	}
	
	if (!(pid = fork ())) {
		/* child process */
		int maxfd, nullfd = -1;
		
		if (!outfd || !errfd)
			nullfd = open ("/dev/null", O_WRONLY);
		
		if (dup2 (fd[0], STDIN_FILENO) == -1)
			_exit (255);
		
		if (dup2 (outfd ? fd[3] : nullfd, STDOUT_FILENO) == -1)
			_exit (255);
		
		if (dup2 (errfd ? fd[5] : nullfd, STDERR_FILENO) == -1)
			_exit (255);
		
		setsid ();
		
		if ((maxfd = sysconf (_SC_OPEN_MAX)) > 0) {
			for (i = 3; i < maxfd; i++)
				fcntl (i, F_SETFD, FD_CLOEXEC);
		}
		
		execv (path, argv);
		_exit (255);
	} else if (pid == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Failed to create child process '%s': %s"),
				      argv[0], strerror (errno));
		return -1;
	}
	
	/* parent process */
	close (fd[0]);
	close (fd[3]);
	close (fd[5]);
	
	if (infd)
		*infd = fd[1];
	else
		close (fd[1]);
	
	if (outfd)
		*outfd = fd[2];
	else
		close (fd[2]);
	
	if (errfd)
		*errfd = fd[4];
	else
		close (fd[4]);
	
	return pid;
}


int
camel_process_wait (pid_t pid)
{
	sigset_t mask, omask;
	int status;
	pid_t r;
	
	sigemptyset (&mask);
	sigaddset (&mask, SIGALRM);
	sigprocmask (SIG_BLOCK, &mask, &omask);
	alarm (1);
	
	r = waitpid (pid, &status, 0);
	
	alarm (0);
	sigprocmask (SIG_SETMASK, &omask, NULL);
	
	if (r == (pid_t) -1 && errno == EINTR) {
		kill (pid, SIGTERM);
		sleep (1);
		r = waitpid (pid, &status, WNOHANG);
		if (r == (pid_t) 0) {
			kill (pid, SIGKILL);
			sleep (1);
			r = waitpid (pid, &status, WNOHANG);
		}
	}
	
	if (r != (pid_t) -1 && WIFEXITED (status))
		return WEXITSTATUS (status);
	else
		return -1;
}
